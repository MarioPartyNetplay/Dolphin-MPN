// Copyright 2017 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <QWidget>

class QCheckBox;
class QVBoxLayout;
class QSlider;
class QLabel;

namespace Core
{
enum class State;
}

class MarioPartyNetplayPane final : public QWidget
{
  Q_OBJECT
public:
  explicit MarioPartyNetplayPane(QWidget* parent = nullptr);

private:
  void CreateLayout();
  void ConnectLayout();
  void LoadConfig();
  void OnSaveConfig();
  void OnEmulationStateChanged(Core::State state);

  // Widgets
  QVBoxLayout* m_main_layout;
  QCheckBox* m_checkbox_show_turn_count;
  QCheckBox* m_checkbox_show_buttons_new;
  QCheckBox* m_checkbox_log_turn_count_to_file;
  QSlider* m_slider_hud_scale;
  QLabel* m_label_hud_scale;
  QLabel* m_label_hud_scale_value;
}; 