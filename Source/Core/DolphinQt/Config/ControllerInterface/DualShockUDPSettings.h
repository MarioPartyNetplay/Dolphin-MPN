// Copyright 2026 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <string>
#include <vector>

struct DualShockUDPServer
{
  std::string description;
  std::string server_address;
  int server_port;

  DualShockUDPServer() = default;
  DualShockUDPServer(std::string description_, std::string server_address_, int server_port_)
      : description(std::move(description_)), server_address(std::move(server_address_)),
        server_port(server_port_)
  {
  }
};

namespace DualShockUDPSettings
{
constexpr char FIELD_SEPARATOR = ':';
constexpr char SERVER_SEPARATOR = ';';

std::vector<DualShockUDPServer> GetServers();

void AddServer(DualShockUDPServer server);

void ReplaceServer(size_t index, DualShockUDPServer server);

void RemoveServer(size_t index);

bool IsEnabled();

void SetEnabled(bool enabled);
}  // namespace DualShockUDPSettings
