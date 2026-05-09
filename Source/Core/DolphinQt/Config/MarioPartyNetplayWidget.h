// Copyright 2017 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <QWidget>

class ConfigBool;
class GameConfigWidget;

namespace Config
{
class Layer;
}  // namespace Config

class MarioPartyNetplayWidget final : public QWidget
{
  Q_OBJECT
public:
  explicit MarioPartyNetplayWidget(GameConfigWidget* parent, Config::Layer* layer);

private:
  void CreateWidgets();
  void ConnectWidgets();
  void AddDescriptions();

  // MPN Settings
  ConfigBool* m_show_turn_count;
  ConfigBool* m_show_buttons_new;
  ConfigBool* m_log_turn_count_to_file;
  Config::Layer* m_game_layer = nullptr;
}; 