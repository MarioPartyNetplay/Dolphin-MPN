// Copyright 2024 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "Core/HW/EXI/BBA/NetPlayBBA.h"

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

static std::deque<std::vector<u8>> g_bba_packet_queue;
static std::mutex g_bba_packet_queue_mutex;
static constexpr size_t MAX_BBA_PACKET_QUEUE = 512;

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

void QueueBBAPacketFromNetPlay(const u8* data, u32 size)
{
  if (!data || size == 0 || size > 1518)
    return;

  std::lock_guard lock(g_bba_packet_queue_mutex);
  if (g_bba_packet_queue.size() >= MAX_BBA_PACKET_QUEUE)
    g_bba_packet_queue.pop_front();
  g_bba_packet_queue.emplace_back(data, data + size);
}

void DrainBBAPacketQueue()
{
  std::vector<u8> packet;
  {
    std::lock_guard lock(g_bba_packet_queue_mutex);
    if (g_bba_packet_queue.empty())
      return;

    packet = std::move(g_bba_packet_queue.front());
    g_bba_packet_queue.pop_front();
  }

  InjectToBackends(packet.data(), static_cast<u32>(packet.size()));
}

void ClearBBAPacketQueue()
{
  std::lock_guard lock(g_bba_packet_queue_mutex);
  g_bba_packet_queue.clear();
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
