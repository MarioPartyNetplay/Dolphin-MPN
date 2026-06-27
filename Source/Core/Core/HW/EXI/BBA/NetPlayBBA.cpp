// Copyright 2024 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "Core/HW/EXI/BBA/NetPlayBBA.h"

#include <algorithm>
#include <atomic>
#include <deque>
#include <functional>
#include <mutex>
#include <vector>

namespace ExpansionInterface
{
static std::function<void(const u8*, u32)> g_bba_packet_sender;
static std::mutex g_bba_sender_mutex;

struct InjectorEntry
{
  u64 id;
  std::function<void(const u8*, u32)> fn;
};
static std::vector<InjectorEntry> g_bba_packet_injectors;
static std::mutex g_bba_injectors_mutex;
static std::atomic<u64> g_bba_injector_next_id{1};

static std::atomic<int> g_bba_netplay_index{0};

static std::deque<std::vector<u8>> g_bba_packet_buffer;
static std::mutex g_bba_packet_mutex;
static std::atomic<u32> g_bba_packet_buffer_size{5};

static u32 GetBBAPacketQueueCap()
{
  const u32 target = g_bba_packet_buffer_size.load(std::memory_order_relaxed);
  return std::min(std::max(target + 8, 16u), 128u);
}

static void InjectToBackends(const u8* data, const u32 size)
{
  std::lock_guard lock(g_bba_injectors_mutex);
  for (const InjectorEntry& entry : g_bba_packet_injectors)
    entry.fn(data, size);
}

void RegisterBBAPacketSender(std::function<void(const u8*, u32)> sender)
{
  std::lock_guard lock(g_bba_sender_mutex);
  g_bba_packet_sender = std::move(sender);
}

void SendBBAFrameToNetPlay(const u8* data, u32 size)
{
  if (!data || size == 0)
    return;

  std::function<void(const u8*, u32)> sender;
  {
    std::lock_guard lock(g_bba_sender_mutex);
    sender = g_bba_packet_sender;
  }

  if (sender)
    sender(data, size);
}

u64 RegisterBBAPacketInjector(std::function<void(const u8*, u32)> injector)
{
  if (!injector)
    return 0;

  std::lock_guard lock(g_bba_injectors_mutex);
  const u64 id = g_bba_injector_next_id.fetch_add(1, std::memory_order_relaxed);
  g_bba_packet_injectors.push_back(InjectorEntry{.id = id, .fn = std::move(injector)});
  return id;
}

void UnregisterBBAPacketInjector(const u64 id)
{
  if (id == 0)
    return;

  std::lock_guard lock(g_bba_injectors_mutex);
  std::erase_if(g_bba_packet_injectors, [id](const InjectorEntry& e) { return e.id == id; });
}

void InjectBBAPacketFromNetPlay(const u8* data, u32 size)
{
  if (!data || size == 0)
    return;

  InjectToBackends(data, size);
}

void QueueBBAPacketFromNetPlay(const u8* data, u32 size)
{
  if (!data || size == 0 || size > 1518)
    return;

  const u32 cap = GetBBAPacketQueueCap();
  std::lock_guard lock(g_bba_packet_mutex);
  while (g_bba_packet_buffer.size() >= cap)
    g_bba_packet_buffer.pop_front();
  g_bba_packet_buffer.emplace_back(data, data + size);
}

void DrainBBAPacketBuffer()
{
  std::vector<u8> packet;
  {
    std::lock_guard lock(g_bba_packet_mutex);
    if (g_bba_packet_buffer.empty())
      return;

    const u32 target = g_bba_packet_buffer_size.load(std::memory_order_relaxed);
    // Hold up to `target` frames before delivering to absorb jitter, but never block forever.
    if (g_bba_packet_buffer.size() <= target)
      return;

    packet = std::move(g_bba_packet_buffer.front());
    g_bba_packet_buffer.pop_front();
  }

  InjectToBackends(packet.data(), static_cast<u32>(packet.size()));
}

void SetBBAPacketBufferSize(const u32 size)
{
  // BBA LAN traffic is bursty; keep jitter buffering modest even if the input buffer is higher.
  g_bba_packet_buffer_size.store(std::min(size, 8u), std::memory_order_relaxed);
}

void ClearBBAPacketBuffer()
{
  std::lock_guard lock(g_bba_packet_mutex);
  g_bba_packet_buffer.clear();
}

void SetBBANetPlayIndex(const int index)
{
  g_bba_netplay_index.store(index, std::memory_order_relaxed);
}

int GetBBANetPlayIndex()
{
  return g_bba_netplay_index.load(std::memory_order_relaxed);
}

}  // namespace ExpansionInterface
