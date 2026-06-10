// Copyright 2024 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <cstddef>
#include <functional>

#include "Common/CommonTypes.h"

namespace ExpansionInterface
{
void InjectBBAPacketFromNetPlay(const u8* data, u32 size);
void RegisterBBAPacketSender(std::function<void(const u8*, u32)> sender);
u64 RegisterBBAPacketInjector(std::function<void(const u8*, u32)> injector);
void UnregisterBBAPacketInjector(u64 id);
}  // namespace ExpansionInterface
