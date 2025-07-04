// Copyright 2017 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <QDialog>
#include <QDialogButtonBox>
#include <QGridLayout>
#include <QLabel>
#include <QListWidget>
#include <QCheckBox>

#include "Core/NetPlayProto.h"

// Forward declarations
namespace NetPlay
{
class Player;
}

class QDialogButtonBox;
class QGridLayout;
class QLabel;
class QListWidget;
class QCheckBox;

class PadMappingDialog : public QDialog
{
  Q_OBJECT
public:
  explicit PadMappingDialog(QWidget* parent = nullptr);

  int exec() override;

  NetPlay::PadMappingArray GetGCPadArray();
  NetPlay::GBAConfigArray GetGBAArray();
  NetPlay::PadMappingArray GetWiimoteArray();

private:
  void CreateWidgets();
  void ConnectWidgets();
  void UpdatePlayerLists();
  void OnPlayerSelectionChanged();

  QGridLayout* m_main_layout;
  QDialogButtonBox* m_button_box;

  // Multi-selection lists for each port
  std::array<QListWidget*, 4> m_gc_player_lists;
  std::array<QListWidget*, 4> m_wii_player_lists;
  std::array<QCheckBox*, 4> m_gba_boxes;

  // Current mappings
  NetPlay::PadMappingArray m_pad_mapping;
  NetPlay::GBAConfigArray m_gba_config;
  NetPlay::PadMappingArray m_wii_mapping;
  std::vector<const NetPlay::Player*> m_players;
};