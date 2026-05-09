// Copyright 2017 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <QWidget>

class QCheckBox;
class QComboBox;
class QLabel;
class QPushButton;
class QRadioButton;
class QSlider;
class QVBoxLayout;

#include "DolphinQt/Config/ToolTipControls/ToolTipCheckBox.h"
#include "DolphinQt/Config/ToolTipControls/ToolTipComboBox.h"
#include "DolphinQt/Config/ToolTipControls/ToolTipPushButton.h"
#include "DolphinQt/Config/ConfigControls/ConfigBool.h"

namespace Core
{
enum class State;
}

class GeneralPane final : public QWidget
{
  Q_OBJECT
public:
  explicit GeneralPane(QWidget* parent = nullptr);

private:
  void CreateLayout();
  void ConnectLayout();
  void CreateBasic();
  void CreateFallbackRegion();
  void LoadConfig();
  void OnSaveConfig();
  void OnEmulationStateChanged(Core::State state);
  void CreateCheats();
  void OnCodeHandlerChanged(int index);
  void UpdateDescriptionsUsingHardcoreStatus();

  // Widgets
  QVBoxLayout* m_main_layout;
  ToolTipComboBox* m_combobox_speedlimit;
  ToolTipComboBox* m_combobox_update_track;
  ToolTipComboBox* m_combobox_fallback_region;
  ToolTipComboBox* m_combobox_codehandler;
  ToolTipCheckBox* m_checkbox_dualcore;
  ToolTipCheckBox* m_checkbox_cheats;
  ConfigBool* m_checkbox_load_games_into_memory;
  ConfigBool* m_checkbox_override_region_settings;
  ToolTipCheckBox* m_checkbox_auto_disc_change;
#ifdef USE_DISCORD_PRESENCE
  ToolTipCheckBox* m_checkbox_discord_presence;
#endif
  QLabel* m_label_speedlimit;

};
