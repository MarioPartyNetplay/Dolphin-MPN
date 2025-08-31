// Copyright 2024 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <atomic>
#include <functional>

#include "Core/HW/EXI/EXI_DeviceEthernet.h"

namespace ExpansionInterface
{

// Forward declaration
class CEXIETHERNET;

// Function to inject BBA packets from NetPlay
void InjectBBAPacketFromNetPlay(const u8* data, u32 size);

// Function to inject packets into BBA interfaces (internal use)
void InjectPacket(const u8* data, u32 size);

// Function to register the BBA packet sender callback for NetPlay clients
void RegisterBBAPacketSenderForClient(std::function<void(const u8*, u32)> sender);

// Register/unregister BBA packet injector callbacks (multi-subscriber)
// Returns a registration id that can be used to unregister later.
u64 RegisterBBAPacketInjector(std::function<void(const u8*, u32)> injector);
void UnregisterBBAPacketInjector(u64 id);

// Global flag to track if we're the first user (host)
extern std::atomic<bool> g_is_first_user;

}  // namespace ExpansionInterface
