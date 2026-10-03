#pragma once

// Local diagnostic only. Not an upstream contribution or a behavior fix.
#include "Cafe/CafeSystem.h"
#include "Cafe/HW/Latte/Core/Latte.h"
#include "Cafe/HW/Latte/Core/LatteTexture.h"
#include <condition_variable>
#include <deque>
#include <cstring>
#include <set>
#include <array>
#include <atomic>

namespace SwapForceVideoTrace
{
struct Event
{
	uint64 id{}, us{};
	uint32 frame{}, draw{}, phys{}, width{}, height{}, pitch{}, reload{}, unit{}, stage{}, core{}, lr{};
	uintptr_t texture{};
	uint64 value{}, beforeHash{}, decodedHash{}, afterHash{}, endUs{};
	uint32 sp{};
	std::array<uint32, 6> stack{};
	std::string kind;
	std::vector<uint8> before, decoded, after;
};

class Sink
{
public:
	Sink()
	{
		const char* path = std::getenv("CEMU_SWAPFORCE_TRACE");
		if (!path || std::string_view(path).find("/home/deck/CemuSwapForceTest/upstream-0a2b6ff-") != 0)
			return;
		m_path = path;
		if (!fs::is_directory(m_path))
			return;
		m_enabled = true;
		m_writer = std::thread([this] { WriteEvents(); });
	}
	~Sink()
	{
		if (!m_enabled) return;
		{ std::lock_guard lock(m_mutex); m_stop = true; }
		m_ready.notify_one();
		m_writer.join();
	}
	bool Match(uint32 width, uint32 height, Latte::E_GX2SURFFMT format, uint32 tile) const
	{
		return m_enabled && CafeSystem::GetForegroundTitleId() == 0x0005000010139200ULL &&
			format == Latte::E_GX2SURFFMT::R8_UNORM && (tile == 0 || tile == 1) &&
			width >= 512 && width <= 4096 && height >= 256 && height <= 2048;
	}
	Event Begin(const char* kind, uint32 phys, uint32 width, uint32 height, uint32 pitch, uintptr_t texture = 0, uint32 reload = 0)
	{
		Event e;
		e.id = ++m_sequence;
		e.us = std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now() - m_start).count();
		// Only read the GPU thread's counters on that thread.
		if (std::string_view(kind) == "upload" || std::string_view(kind) == "draw" || std::string_view(kind) == "retire")
		{
			e.frame = LatteGPUState.frameCounter;
			e.draw = LatteGPUState.drawCallCounter;
		}
		e.kind = kind; e.phys = phys; e.width = width; e.height = height; e.pitch = pitch;
		e.texture = texture; e.reload = reload;
		if (e.kind == "bind")
		{
			m_movieStarted.store(true, std::memory_order_relaxed);
			for (auto& plane : m_planes)
			{
				uint32 expected = 0;
				if (plane.load(std::memory_order_relaxed) == phys || plane.compare_exchange_strong(expected, phys, std::memory_order_relaxed)) break;
			}
		}
		return e;
	}
	uint64 Now() const { return std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now() - m_start).count(); }
	bool Active() const { return m_enabled && m_movieStarted.load(std::memory_order_relaxed); }
	bool Enabled() const { return m_enabled; }
	bool Touches(uint32 phys, uint32 size) const
	{
		if (!Active()) return false;
		for (const auto& plane : m_planes)
		{
			uint32 base = plane.load(std::memory_order_relaxed);
			// Discovery filter deliberately covers one maximum plane size.
			if (base && uint64(phys) < uint64(base) + 2048 * 1024 && uint64(phys) + size > base) return true;
		}
		return false;
	}
	void GuestContext(Event& e)
	{
		auto* cpu = PPCInterpreter_getCurrentInstance();
		if (!cpu) return;
		e.core = cpu->spr.UPIR; e.lr = cpu->spr.LR; e.sp = cpu->gpr[1];
		uint32 sp = e.sp;
		for (auto& pc : e.stack)
		{
			if (sp < 0x10000000 || sp > 0x3ffffff8 || (sp & 3)) break;
			uint32 next = memory_readU32(sp);
			if (next <= sp || next - sp > 0x10000 || next > 0x3ffffff8) break;
			pc = memory_readU32(next + 4); sp = next;
		}
	}
	void GuestEvent(const char* kind, uint64 value = 0, uint32 phys = 0, uint32 size = 0)
	{
		if (!Active()) return;
		auto e = Begin(kind, phys, size, 0, 0); e.value = value; GuestContext(e); Submit(std::move(e));
	}
	uint64 Hash(const Event& e, const uint8* input, uint32 pitch) const
	{
		// 512 sampled bytes, not a complete plane hash or atomic snapshot.
		uint32 w = std::min(e.width, e.width >= 2048 ? 1280U : 640U);
		uint32 h = std::min(e.height, e.width >= 2048 ? 720U : 360U);
		uint64 hash = 14695981039346656037ULL;
		for (uint32 y = 0; y < 16; ++y)
			for (uint32 x = 0; x < 32; ++x)
				{ hash ^= input[size_t((y * h + h / 2) / 16) * pitch + (x * w + w / 2) / 32]; hash *= 1099511628211ULL; }
		return hash;
	}
	uint64 GuestHash(const Event& e) const
	{
		return Hash(e, static_cast<const uint8*>(memory_getPointerFromPhysicalOffset(e.phys)), e.pitch);
	}
	void DumpCallerCodeOnce(const Event& bind)
	{
		if (!std::getenv("CEMU_SWAPFORCE_DUMP_GUEST") || m_dumped.exchange(true)) return;
		std::set<uint32> pages;
		for (uint32 pc : {bind.lr, bind.stack[0], bind.stack[1], bind.stack[2], bind.stack[3]})
		{
			if (pc < 0x02000000 || pc >= 0x10000000) continue;
			pages.insert((pc & ~0xfffU) - 0x1000); pages.insert(pc & ~0xfffU); pages.insert((pc & ~0xfffU) + 0x1000);
		}
		for (uint32 page : pages)
		{
			auto e = Begin("guestcode", page, 4096, 1, 4096);
			auto* ptr = static_cast<const uint8*>(memory_getPointerFromVirtualOffset(page));
			e.decoded.assign(ptr, ptr + 4096); Submit(std::move(e));
		}
	}
	bool Sample(const Event& e)
	{
		// Sparse samples throughout the movie, rather than a fixed wall-clock scene.
		return std::getenv("CEMU_SWAPFORCE_TRACE_RAW") && e.frame % 12 == 0 && m_samples.fetch_add(1) < 900;
	}
	std::vector<uint8> GuestRows(const Event& e)
	{
		if (e.pitch < e.width || e.pitch > 8192) return {};
		std::vector<uint8> rows(size_t(e.width) * e.height);
		const auto* input = static_cast<const uint8*>(memory_getPointerFromPhysicalOffset(e.phys));
		for (uint32 y = 0; y < e.height; ++y)
			std::memcpy(rows.data() + size_t(y) * e.width, input + size_t(y) * e.pitch, e.width);
		return rows;
	}
	void Submit(Event e)
	{
		std::lock_guard lock(m_mutex);
		if (m_events.size() >= 4096) { ++m_dropped; return; }
		m_events.push_back(std::move(e));
		m_ready.notify_one();
	}
private:
	void WriteEvents()
	{
		std::ofstream log(m_path / "events.jsonl");
		for (;;)
		{
			Event e;
			{
				std::unique_lock lock(m_mutex);
				m_ready.wait(lock, [this] { return m_stop || !m_events.empty(); });
				if (m_events.empty() && m_stop) break;
				e = std::move(m_events.front()); m_events.pop_front();
			}
			for (const auto& [label, data] : {std::pair{"before", &e.before}, {"decoded", &e.decoded}, {"after", &e.after}})
			{
				if (data->empty()) continue;
				std::ofstream out(m_path / fmt::format("{:08}_{}_{}.r8", e.id, e.kind, label), std::ios::binary);
				out.write(reinterpret_cast<const char*>(data->data()), data->size());
			}
			log << fmt::format("{{\"id\":{},\"us\":{},\"kind\":\"{}\",\"frame\":{},\"draw\":{},\"phys\":{},\"width\":{},\"height\":{},\"pitch\":{},\"texture\":{},\"reload\":{},\"unit\":{},\"stage\":{},\"core\":{},\"lr\":{},\"before\":{},\"decoded\":{},\"after\":{},\"dropped\":{}}}\n",
				e.id,e.us,e.kind,e.frame,e.draw,e.phys,e.width,e.height,e.pitch,e.texture,e.reload,e.unit,e.stage,e.core,e.lr,e.before.size(),e.decoded.size(),e.after.size(),m_dropped.load());
			log << fmt::format("{{\"extra_for\":{},\"value\":{},\"beforeHash\":{},\"decodedHash\":{},\"afterHash\":{},\"endUs\":{},\"sp\":{},\"stack\":[{},{},{},{},{},{}]}}\n",
				e.id,e.value,e.beforeHash,e.decodedHash,e.afterHash,e.endUs,e.sp,e.stack[0],e.stack[1],e.stack[2],e.stack[3],e.stack[4],e.stack[5]);
			log.flush();
		}
	}
	bool m_enabled{}, m_stop{};
	fs::path m_path;
	std::chrono::steady_clock::time_point m_start = std::chrono::steady_clock::now();
	std::atomic<uint64> m_sequence{}, m_samples{}, m_dropped{};
	std::atomic<bool> m_movieStarted{}, m_dumped{};
	std::array<std::atomic<uint32>, 12> m_planes{};
	std::mutex m_mutex;
	std::condition_variable m_ready;
	std::deque<Event> m_events;
	std::thread m_writer;
};

inline Sink& Get() { static Sink sink; return sink; }
}
