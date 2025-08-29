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

// Global callback function pointer for injecting BBA packets from NetPlay
// This will be set by the NetPlay BBA interface when it activates
static std::function<void(const u8*, u32)> g_bba_packet_injector = nullptr;

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
void RegisterBBAPacketInjector(std::function<void(const u8*, u32)> injector)
{
  g_bba_packet_injector = injector;
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
  
  // Register this interface's injector callback
  RegisterBBAPacketInjector([this](const u8* data, u32 size) {
    if (m_active && !m_shutdown)
      InjectPacket(data, size);
  });
  
  // Log that we're ready to handle BBA packets
  INFO_LOG_FMT(SP1, "NetPlay BBA Interface ready to handle packets");
  
  return true;
}

void CEXIETHERNET::NetPlayBBAInterface::Deactivate()
{
  INFO_LOG_FMT(SP1, "NetPlay BBA Interface deactivated");
  
  // Unregister the injector callback
  RegisterBBAPacketInjector(nullptr);
  
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
    return true;
  }
  
  // If no callback is registered, just buffer the packet locally
  {
    std::lock_guard<std::mutex> lock(m_buffer_mutex);
    m_packet_buffer.push_back({frame, frame + size});
  }
  
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
  
  {
    std::lock_guard<std::mutex> lock(m_buffer_mutex);
    m_packet_buffer.push_back({data, data + size});
  }
  
  // Notify that we have data available
  m_buffer_cv.notify_one();
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
  
  // Use the registered injector callback if available
  if (g_bba_packet_injector)
  {
    g_bba_packet_injector(data, size);
    INFO_LOG_FMT(SP1, "Packet injected via callback");
  }
  else
  {
    WARN_LOG_FMT(SP1, "No BBA packet injector registered, packet not injected");
  }
}

}  // namespace ExpansionInterface
