// Copyright 2024 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <cstddef>
#include <functional>

#include "Common/CommonTypes.h"

namespace ExpansionInterface
{
// Transport bridge between the BBA backend and the NetPlay client/server.
//
// Host-authoritative mirror model: only the host runs real BBA/LAN logic. The host mirrors its
// ethernet stream (TX and locally generated RX) to peers via the registered sender. Peer BBAs are
// passive injectors only — they do not originate LAN traffic. Frames from netplay are delivered
// immediately to all registered injectors.

// Called by the BBA backend to push a frame onto the netplay transport.
void SendBBAFrameToNetPlay(const u8* data, u32 size);

// Registered by NetPlayClient/NetPlayServer to forward frames over ENet.
void RegisterBBAPacketSender(std::function<void(const u8*, u32)> sender);

// Called by NetPlayClient/NetPlayServer when a frame arrives from a peer.
void InjectBBAPacketFromNetPlay(const u8* data, u32 size);

// Registered by the BBA backend to receive frames coming from netplay.
u64 RegisterBBAPacketInjector(std::function<void(const u8*, u32)> injector);
void UnregisterBBAPacketInjector(u64 id);

// Virtual LAN identity. In mirror mode every player uses index 0 (same IP/MAC as the host) so
// consoles appear co-located on the LAN.
void SetBBANetPlayIndex(int index);
int GetBBANetPlayIndex();

// When true the BBA is passive: only inject frames from netplay, never originate LAN traffic.
void SetBBANetPlayMirrorMode(bool mirror);
bool GetBBANetPlayMirrorMode();
}  // namespace ExpansionInterface
