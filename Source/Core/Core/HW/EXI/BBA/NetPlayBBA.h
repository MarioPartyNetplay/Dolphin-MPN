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
// The BBA backend (BuiltIn HLE adapter in netplay mode) decides which ethernet frames are
// LAN/peer traffic and bridges them to the other player(s) via the registered sender. Frames
// received from netplay are fanned out to all registered injectors (the active BBA backend).

// Called by the BBA backend to push a frame onto the netplay transport.
void SendBBAFrameToNetPlay(const u8* data, u32 size);

// Registered by NetPlayClient/NetPlayServer to forward frames over ENet.
void RegisterBBAPacketSender(std::function<void(const u8*, u32)> sender);

// Immediately inject a frame into the active BBA backend(s).
void InjectBBAPacketFromNetPlay(const u8* data, u32 size);

// Queue a frame from the netplay transport for jitter-buffered delivery to the BBA backend.
void QueueBBAPacketFromNetPlay(const u8* data, u32 size);

// Drain at most one queued frame per emulated frame once the queue exceeds the jitter target.
// Never blocks the CPU thread. The queue is capped to prevent latency spikes from exhausting memory.
void DrainBBAPacketBuffer();

// Registered by the BBA backend to receive frames coming from netplay.
u64 RegisterBBAPacketInjector(std::function<void(const u8*, u32)> injector);
void UnregisterBBAPacketInjector(u64 id);

// Local player index on the shared virtual LAN (host = 0, client = 1, ...). Used by the BBA
// backend to assign a distinct IP/MAC per player.
void SetBBANetPlayIndex(int index);
int GetBBANetPlayIndex();

// Uses the same value as the netplay input buffer setting.
void SetBBAPacketBufferSize(u32 size);
void ClearBBAPacketBuffer();
}  // namespace ExpansionInterface
