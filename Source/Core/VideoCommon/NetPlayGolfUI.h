// Copyright 2019 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <atomic>
#include <memory>

namespace NetPlay
{
class NetPlayClient;
}

class NetPlayGolfUI
{
public:
  explicit NetPlayGolfUI(std::weak_ptr<NetPlay::NetPlayClient> netplay_client);
  ~NetPlayGolfUI();

  void Display();

private:
  std::weak_ptr<NetPlay::NetPlayClient> m_netplay_client;
};

extern std::atomic<std::shared_ptr<NetPlayGolfUI>> g_netplay_golf_ui;
