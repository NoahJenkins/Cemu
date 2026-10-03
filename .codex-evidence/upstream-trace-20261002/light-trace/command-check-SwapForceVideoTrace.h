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
	uint32 job{}, outputDescriptor{}, frameIndex{}, outputIndex{}, jobY{}, jobV{}, jobU{};
	bool jobProbe{};
	std::array<uint32, 6> stack{};
	std::string kind;
	std::vector<uint8> before, decoded, after;
};

class Sink
{
public:
	Sink()
	{
		const char* configRoot = std::getenv("XDG_CONFIG_HOME");
		m_postMovieWait = std::getenv("CEMU_SWAPFORCE_POST_MOVIE_SUBMIT_WAIT") && configRoot &&
			std::string_view(configRoot).find("/home/deck/CemuSwapForceTest/upstream-0a2b6ff-") == 0;
		if (configRoot && std::string_view(configRoot).find("/home/deck/CemuSwapForceTest/upstream-0a2b6ff-") == 0)
		{
			m_commandCheck = std::getenv("CEMU_SWAPFORCE_COMMAND_CHECK") != nullptr;
			if (const char* value = std::getenv("CEMU_SWAPFORCE_POST_MOVIE_DELAY_US"))
			{
				char* end{};
				unsigned long delay = std::strtoul(value, &end, 10);
				if (end != value && *end == '\0' && delay >= 1 && delay <= 10000)
					m_postMovieDelayUs = static_cast<uint32>(delay);
			}
		}
		const char* path = std::getenv("CEMU_SWAPFORCE_TRACE");
		if (!path || std::string_view(path).find("/home/deck/CemuSwapForceTest/upstream-0a2b6ff-") != 0)
			return;
		m_path = path;
		if (!fs::is_directory(m_path))
			return;
		m_enabled = true;
		if (const char* v = std::getenv("CEMU_SWAPFORCE_TRACE_START_US")) m_windowStartUs = std::strtoull(v, nullptr, 10);
		if (const char* v = std::getenv("CEMU_SWAPFORCE_TRACE_END_US")) m_windowEndUs = std::strtoull(v, nullptr, 10);
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
			width >= 512 && width <= 4096 && height >= 256 && height <= 2048 && Window();
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
	bool Window() const { if (!m_windowEndUs) return true; uint64 t = Now(); return t >= m_windowStartUs && t < m_windowEndUs; }
	bool Active() const { return m_enabled && m_movieStarted.load(std::memory_order_relaxed) && Window(); }
	bool Enabled() const { return m_enabled; }
	bool PostMovieWaitEnabled() const { return (m_postMovieWait || m_postMovieDelayUs) && CafeSystem::GetForegroundTitleId() == 0x0005000010139200ULL; }
	uint32 PostMovieDelayUs() const { return m_postMovieDelayUs; }
	bool CommandCheckEnabled() const { return m_commandCheck && CafeSystem::GetForegroundTitleId() == 0x0005000010139200ULL; }
	bool TrackMovieLists() const { return PostMovieWaitEnabled() || CommandCheckEnabled(); }
	void NoteMovieList(uintptr_t address)
	{
		for (auto& entry : m_movieLists)
		{
			uintptr_t expected = 0;
			if (entry.load(std::memory_order_relaxed) == address || entry.compare_exchange_strong(expected, address, std::memory_order_relaxed)) break;
		}
	}
	bool IsMovieList(uintptr_t address) const
	{
		if (!TrackMovieLists()) return false;
		for (const auto& entry : m_movieLists) if (entry.load(std::memory_order_relaxed) == address) return true;
		return false;
	}
	uint32 CountPostMovieWait() { return ++m_postMovieWaitCount; }
	struct CommandCheck
	{
		uint64 id{}, submittedHash{}, enterHash{};
		uint32 phys{}, words{}, nested{};
	};
	uint64 CommandHash(const uint32be* ptr, uint32 words) const
	{
		// Hash the whole bounded root list. No guest command bytes leave the process.
		const volatile uint8* bytes = reinterpret_cast<const volatile uint8*>(ptr);
		uint64 hash = 14695981039346656037ULL;
		for (uint32 i = 0; i < words * 4; ++i) hash = (hash ^ bytes[i]) * 1099511628211ULL;
		return hash;
	}
	void CommandSubmit(uint32be* ptr, uint32 phys, uint32 words)
	{
		if (!CommandCheckEnabled() || !IsMovieList(reinterpret_cast<uintptr_t>(ptr))) return;
		if (!words || words > 16384) { ++m_commandSkipped; return; }
		CommandCheck record;
		record.id = ++m_commandSequence; record.phys = phys; record.words = words;
		record.submittedHash = CommandHash(ptr, words);
		std::lock_guard lock(m_commandMutex);
		if (m_commandPending.size() >= 64) { ++m_commandSkipped; return; }
		m_commandPending.push_back(record);
	}
	CommandCheck CommandEnter(uint32be* ptr, uint32 phys, uint32 words)
	{
		CommandCheck record;
		if (!CommandCheckEnabled()) return record;
		{
			std::lock_guard lock(m_commandMutex);
			for (auto it = m_commandPending.begin(); it != m_commandPending.end(); ++it)
				if (it->phys == phys && it->words == words)
				{
					record = *it; m_commandPending.erase(it); break;
				}
		}
		if (record.id) record.enterHash = CommandHash(ptr, words);
		return record;
	}
	void CommandExit(const CommandCheck& record, uint32be* ptr)
	{
		if (!record.id) return;
		uint64 endHash = CommandHash(ptr, record.words);
		bool changed = record.submittedHash != record.enterHash || record.enterHash != endHash;
		++m_commandChecked;
		if (changed) ++m_commandChanged;
		m_commandNested += record.nested;
		if (changed || m_commandChecked == 1 || m_commandChecked % 120 == 0)
			cemuLog_log(LogType::Force, "Diagnostic command-check id={} us={} phys={:08x} words={} submit={:016x} enter={:016x} exit={:016x} checked={} changed={} nested={} skipped={}",
				record.id, Now(), record.phys, record.words, record.submittedHash, record.enterHash, endHash,
				m_commandChecked, m_commandChanged, m_commandNested, m_commandSkipped.load());
	}
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
		auto e = Begin(kind, phys, size, 0, 0); e.value = value; GuestContext(e);
		// Observed queue ABI in this title's owned executable. Diagnostic metadata only.
		if (e.kind == "mutex-unlock-enter" && e.lr == 0x02bb873c && (e.stack[1] == 0x02bb938c || e.stack[1] == 0x02bb93d0))
		{
			e.jobProbe = true;
			// Read the published ring entry, not shadow nonvolatile CPU registers.
			uint32 queue = phys - 0x80;
			if (queue >= 0x10000000 && queue < 0x3ffff800)
			{
				uint32 writeIndex = memory_readU32(queue + 0x108);
				uint32 payload = queue + 0x110 + ((writeIndex - 4) & 0xff) * 4;
				uint32 job = memory_readU32(payload) & ~7U;
				e.job = job;
				if (job >= 0x10000000 && job < 0x3ffffe00)
				{
					e.width = memory_readU32(job); e.height = memory_readU32(job + 4);
					uint32 descriptor = memory_readU32(job + 0xe0);
					e.outputDescriptor = descriptor;
					if (descriptor >= 0x10000000 && descriptor < 0x3fffff80 && memory_readU32(descriptor) == 2)
					{
						uint32 index = memory_readU32(descriptor + 0x14);
						if (index < 2)
						{
							e.job = job; e.outputDescriptor = descriptor; e.frameIndex = index;
							// Decoder reads selector, then chooses the other slot when allocated.
							e.outputIndex = memory_readU32(descriptor + (index ^ 1) * 0x30 + 0x1c) ? (index ^ 1) : index;
							uint32 slot = descriptor + 0x18 + e.outputIndex * 0x30;
							e.jobY = memory_readU32(slot + 4); e.jobV = memory_readU32(slot + 0x10); e.jobU = memory_readU32(slot + 0x1c);
							for (const auto& plane : m_planes) if (plane.load(std::memory_order_relaxed) == e.jobY) e.unit = 1;
						}
					}
				}
			}
		}
		Submit(std::move(e));
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
			if (e.jobProbe)
				log << fmt::format("{{\"extra_for\":{},\"job\":{},\"outputDescriptor\":{},\"frameIndex\":{},\"outputIndex\":{},\"jobY\":{},\"jobV\":{},\"jobU\":{}}}\n",
					e.id,e.job,e.outputDescriptor,e.frameIndex,e.outputIndex,e.jobY,e.jobV,e.jobU);
			log.flush();
		}
	}
	bool m_enabled{}, m_stop{};
	bool m_postMovieWait{};
	uint32 m_postMovieDelayUs{};
	bool m_commandCheck{};
	std::mutex m_commandMutex;
	std::deque<CommandCheck> m_commandPending;
	std::atomic<uint64> m_commandSequence{}, m_commandSkipped{};
	uint64 m_commandChecked{}, m_commandChanged{}, m_commandNested{}; // GPU thread only
	std::array<std::atomic<uintptr_t>, 8> m_movieLists{};
	std::atomic<uint32> m_postMovieWaitCount{};
	fs::path m_path;
	std::chrono::steady_clock::time_point m_start = std::chrono::steady_clock::now();
	std::atomic<uint64> m_sequence{}, m_samples{}, m_dropped{};
	std::atomic<bool> m_movieStarted{}, m_dumped{};
	uint64 m_windowStartUs{}, m_windowEndUs{};
	std::array<std::atomic<uint32>, 12> m_planes{};
	std::mutex m_mutex;
	std::condition_variable m_ready;
	std::deque<Event> m_events;
	std::thread m_writer;
};

inline Sink& Get() { static Sink sink; return sink; }
}
