// Copyright 2024 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "Core/HW/EXI/BBA/NetPlayBBA.h"

#include <algorithm>
#include <chrono>
#include <cstring>

#include "Common/Logging/Log.h"
#include "Core/NetPlayClient.h"
#include "Core/NetPlayServer.h"
#include "Core/Config/NetplaySettings.h"
#include "Core/Config/MainSettings.h"
#include "Core/System.h"
#include "Core/HW/EXI/EXI.h"

namespace ExpansionInterface
{

// Global callback function pointer for sending BBA packets through NetPlay
// This will be set by the NetPlay system when it initializes
static std::function<void(const u8*, u32)> g_bba_packet_sender = nullptr;

// Global list of active BBA interfaces for injecting packets from NetPlay
struct InjectorEntry
{
  u64 id;
  std::function<void(const u8*, u32)> fn;
};
static std::vector<InjectorEntry> g_bba_packet_injectors;
static std::mutex g_bba_injectors_mutex;
static std::atomic<u64> g_bba_injector_next_id{1};

// Function to register the BBA packet sender callback
void RegisterBBAPacketSender(std::function<void(const u8*, u32)> sender)
{
  g_bba_packet_sender = sender;
}

// Function to register the BBA packet sender callback for NetPlay clients
void RegisterBBAPacketSenderForClient(std::function<void(const u8*, u32)> sender)
{
  g_bba_packet_sender = sender;
}

// Function to register the BBA packet injector callback
u64 RegisterBBAPacketInjector(std::function<void(const u8*, u32)> injector)
{
  if (!injector)
    return 0;
  std::lock_guard<std::mutex> lock(g_bba_injectors_mutex);
  const u64 id = g_bba_injector_next_id.fetch_add(1, std::memory_order_relaxed);
  g_bba_packet_injectors.push_back(InjectorEntry{.id = id, .fn = std::move(injector)});
  return id;
}

// Function to unregister the BBA packet injector callback
void UnregisterBBAPacketInjector(u64 id)
{
  if (id == 0)
    return;
  std::lock_guard<std::mutex> lock(g_bba_injectors_mutex);
  std::erase_if(g_bba_packet_injectors, [id](const InjectorEntry& e) { return e.id == id; });
}

bool CEXIETHERNET::NetPlayBBAInterface::Activate()
{
  if (m_active)
    return false;

  INFO_LOG_FMT(SP1, "NetPlay BBA Interface activated");
  m_active = true;
  m_shutdown = false;
  
  // Initialize packet buffer
  m_packet_buffer.clear();
  
  // Create and store the injector callback for this interface
  m_injector_callback = [this](const u8* data, u32 size) {
    if (m_active && !m_shutdown)
      InjectPacket(data, size);
  };
  
  // Register this interface's injector callback
  m_injector_id = RegisterBBAPacketInjector(m_injector_callback);
  
  // Log that we're ready to handle BBA packets
  INFO_LOG_FMT(SP1, "NetPlay BBA Interface ready to handle packets");
  
  return true;
}

void CEXIETHERNET::NetPlayBBAInterface::Deactivate()
{
  INFO_LOG_FMT(SP1, "NetPlay BBA Interface deactivated");
  
  // Unregister the injector callback
  if (m_injector_id != 0)
  {
    UnregisterBBAPacketInjector(m_injector_id);
    m_injector_id = 0;
    m_injector_callback = nullptr;
  }
  
  {
    std::lock_guard<std::mutex> lock(m_buffer_mutex);
    m_shutdown = true;
    m_buffer_cv.notify_all();
  }
  
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

  INFO_LOG_FMT(SP1, "NetPlay BBA sending frame: {} bytes", size);
  
  // Try to send the BBA packet through NetPlay if the callback is registered
  if (g_bba_packet_sender)
  {
    g_bba_packet_sender(frame, size);
    // Signal DMA/transfer completion to the emulated BBA
    m_eth_ref->SendComplete();
    return true;
  }
  
  // If no callback is registered, just buffer the packet locally
  {
    std::lock_guard<std::mutex> lock(m_buffer_mutex);
    m_packet_buffer.push_back({frame, frame + size});
  }
  // Signal DMA/transfer completion to the emulated BBA even if we buffered locally
  m_eth_ref->SendComplete();
  
  return true;
}

bool CEXIETHERNET::NetPlayBBAInterface::RecvInit()
{
  if (!m_active || m_shutdown)
    return false;
  
  INFO_LOG_FMT(SP1, "NetPlay BBA RecvInit");
  return true;
}

void CEXIETHERNET::NetPlayBBAInterface::RecvStart()
{
  if (!m_active || m_shutdown)
    return;
  
  INFO_LOG_FMT(SP1, "NetPlay BBA RecvStart");
  m_receiving = true;
}

void CEXIETHERNET::NetPlayBBAInterface::RecvStop()
{
  if (!m_active || m_shutdown)
    return;
  
  INFO_LOG_FMT(SP1, "NetPlay BBA RecvStop");
  m_receiving = false;
}

void CEXIETHERNET::NetPlayBBAInterface::RecvRead(u8* dest, u32 size)
{
  if (!m_active || m_shutdown || !m_receiving)
    return;
  
  std::lock_guard<std::mutex> lock(m_buffer_mutex);
  
  if (!m_packet_buffer.empty())
  {
    const auto& packet = m_packet_buffer.front();
    const u32 copy_size = std::min(size, static_cast<u32>(packet.size()));
    
    std::memcpy(dest, packet.data(), copy_size);
    m_packet_buffer.pop_front();
    
    INFO_LOG_FMT(SP1, "NetPlay BBA received packet: {} bytes", copy_size);
  }
}

void CEXIETHERNET::NetPlayBBAInterface::RecvReadDone()
{
  if (!m_active || m_shutdown)
    return;
  
  INFO_LOG_FMT(SP1, "NetPlay BBA RecvReadDone");
}

// Called from NetPlay to inject received BBA packets
void CEXIETHERNET::NetPlayBBAInterface::InjectPacket(const u8* data, u32 size)
{
  if (!m_active || m_shutdown)
    return;
  
  INFO_LOG_FMT(SP1, "NetPlay BBA injecting packet: {} bytes", size);
  
  // If receive is running, deliver immediately into the BBA buffer like the BuiltIn path
  if (m_receiving)
  {
    const u32 min_frame = 64;
    const u32 copy_size = std::max(size, min_frame);
    if (copy_size > BBA_RECV_SIZE)
    {
      WARN_LOG_FMT(SP1, "Injected frame too large ({}), dropping", copy_size);
      return;
    }

    // Copy and pad to minimum Ethernet frame size
    std::memcpy(m_eth_ref->mRecvBuffer.get(), data, size);
    if (size < min_frame)
      std::memset(m_eth_ref->mRecvBuffer.get() + size, 0, min_frame - size);

    m_eth_ref->mRecvBufferLength = copy_size;
    (void)m_eth_ref->RecvHandlePacket();
    return;
  }

  // Otherwise buffer it for later consumption
  {
    std::lock_guard<std::mutex> lock(m_buffer_mutex);
    m_packet_buffer.push_back({data, data + size});
  }
}

// Function to inject BBA packets from NetPlay
void InjectBBAPacketFromNetPlay(const u8* data, u32 size)
{
  if (!data || size == 0)
  {
    WARN_LOG_FMT(SP1, "Invalid packet data or size, cannot inject BBA packet");
    return;
  }
  
  INFO_LOG_FMT(SP1, "Injecting BBA packet from NetPlay: {} bytes", size);
  
  // Inject the packet into all registered BBA interfaces
  std::lock_guard<std::mutex> lock(g_bba_injectors_mutex);
  if (!g_bba_packet_injectors.empty())
  {
    for (const auto& entry : g_bba_packet_injectors)
    {
      entry.fn(data, size);
    }
    INFO_LOG_FMT(SP1, "Packet injected into {} BBA interfaces", g_bba_packet_injectors.size());
  }
  else
  {
    WARN_LOG_FMT(SP1, "No BBA packet injectors registered, packet not injected");
  }
}

}  // namespace ExpansionInterface
