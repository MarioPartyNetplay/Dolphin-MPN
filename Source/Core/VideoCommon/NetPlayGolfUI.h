// Copyright 2019 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

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

std::shared_ptr<NetPlayGolfUI> GetNetPlayGolfUI();
void SetNetPlayGolfUI(std::shared_ptr<NetPlayGolfUI> ui);
