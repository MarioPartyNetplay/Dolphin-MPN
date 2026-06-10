// Copyright 2024 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "Core/HW/EXI/BBA/NetPlayBBA.h"

#include <algorithm>
#include <atomic>
#include <cstring>
#include <deque>
#include <functional>
#include <mutex>
#include <vector>

#include "Common/Logging/Log.h"
#include "Core/Core.h"
#include "Core/HW/EXI/EXI_DeviceEthernet.h"
#include "Core/System.h"

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

void RegisterBBAPacketSender(std::function<void(const u8*, u32)> sender)
{
  std::lock_guard lock(g_bba_sender_mutex);
  g_bba_packet_sender = std::move(sender);
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

bool CEXIETHERNET::NetPlayBBAInterface::Activate()
{
  if (m_active)
    return false;

  INFO_LOG_FMT(SP1, "NetPlay BBA interface activated");
  m_active = true;
  m_shutdown = false;

  m_injector_callback = [this](const u8* data, u32 size) {
    if (m_active && !m_shutdown)
      InjectPacket(data, size);
  };
  m_injector_id = RegisterBBAPacketInjector(m_injector_callback);
  return true;
}

void CEXIETHERNET::NetPlayBBAInterface::Deactivate()
{
  INFO_LOG_FMT(SP1, "NetPlay BBA interface deactivated");

  if (m_injector_id != 0)
  {
    UnregisterBBAPacketInjector(m_injector_id);
    m_injector_id = 0;
    m_injector_callback = nullptr;
  }

  m_shutdown = true;
  m_active = false;
}

bool CEXIETHERNET::NetPlayBBAInterface::IsActivated()
{
  return m_active;
}

bool CEXIETHERNET::NetPlayBBAInterface::SendFrame(const u8* frame, u32 size)
{
  if (!m_active || m_shutdown)
    return false;

  std::function<void(const u8*, u32)> sender;
  {
    std::lock_guard lock(g_bba_sender_mutex);
    sender = g_bba_packet_sender;
  }

  if (sender)
  {
    sender(frame, size);
    return true;
  }

  return false;
}

bool CEXIETHERNET::NetPlayBBAInterface::RecvInit()
{
  return m_active && !m_shutdown;
}

void CEXIETHERNET::NetPlayBBAInterface::RecvStart()
{
  m_receiving = true;
}

void CEXIETHERNET::NetPlayBBAInterface::RecvStop()
{
  m_receiving = false;
}

void CEXIETHERNET::NetPlayBBAInterface::InjectPacket(const u8* data, u32 size)
{
  if (!m_active || m_shutdown || !m_receiving || !data || size == 0)
    return;

  const u32 copy_size = std::min(size, static_cast<u32>(BBA_RECV_SIZE));
  std::vector<u8> packet(data, data + copy_size);
  CEXIETHERNET* eth = m_eth_ref;

  Core::RunOnCPUThread(Core::System::GetInstance(), [eth, packet = std::move(packet)]() mutable {
    if (!eth)
      return;

    const u32 length = std::min(static_cast<u32>(packet.size()), static_cast<u32>(BBA_RECV_SIZE));
    std::memcpy(eth->mRecvBuffer.get(), packet.data(), length);
    eth->mRecvBufferLength = length;
    eth->RecvHandlePacket();
  });
}

void InjectBBAPacketFromNetPlay(const u8* data, u32 size)
{
  if (!data || size == 0)
    return;

  std::lock_guard lock(g_bba_injectors_mutex);
  for (const InjectorEntry& entry : g_bba_packet_injectors)
    entry.fn(data, size);
}

}  // namespace ExpansionInterface
