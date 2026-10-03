#pragma once

// Local diagnostic only. Not an upstream contribution or a behavior fix.
#include "Cafe/CafeSystem.h"
#include "Cafe/HW/Latte/Core/Latte.h"
#include "Cafe/HW/Latte/Core/LatteTexture.h"
#include <condition_variable>
#include <deque>
#include <cstring>

namespace SwapForceVideoTrace
{
struct Event
{
	uint64 id{}, us{};
	uint32 frame{}, draw{}, phys{}, width{}, height{}, pitch{}, reload{}, unit{}, stage{}, core{}, lr{};
	uintptr_t texture{};
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
		e.frame = LatteGPUState.frameCounter;
		e.draw = LatteGPUState.drawCallCounter;
		e.kind = kind; e.phys = phys; e.width = width; e.height = height; e.pitch = pitch;
		e.texture = texture; e.reload = reload;
		return e;
	}
	bool Sample(const Event& e)
	{
		// Sparse samples throughout the movie, rather than a fixed wall-clock scene.
		return e.frame % 12 == 0 && m_samples.fetch_add(1) < 900;
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
		if (m_events.size() >= 128) { ++m_dropped; return; }
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
			log.flush();
		}
	}
	bool m_enabled{}, m_stop{};
	fs::path m_path;
	std::chrono::steady_clock::time_point m_start = std::chrono::steady_clock::now();
	std::atomic<uint64> m_sequence{}, m_samples{}, m_dropped{};
	std::mutex m_mutex;
	std::condition_variable m_ready;
	std::deque<Event> m_events;
	std::thread m_writer;
};

inline Sink& Get() { static Sink sink; return sink; }
}
