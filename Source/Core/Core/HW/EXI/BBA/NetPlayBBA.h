// Copyright 2024 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include "Core/HW/EXI/EXI_DeviceEthernet.h"

namespace ExpansionInterface
{

// Forward declaration
class CEXIETHERNET;

// Function to inject BBA packets from NetPlay
void InjectBBAPacketFromNetPlay(const u8* data, u32 size);

// Function to register the BBA packet sender callback for NetPlay clients
void RegisterBBAPacketSenderForClient(std::function<void(const u8*, u32)> sender);

}  // namespace ExpansionInterface
