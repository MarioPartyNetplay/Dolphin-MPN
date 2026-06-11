// Copyright 2017 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <array>

#include <QDialog>

#include "Core/NetPlayProto.h"

class QDialogButtonBox;
class QLabel;
class QPushButton;
class QTableWidget;
class QTableWidgetItem;
class QCheckBox;

namespace NetPlay
{
class Player;
}

class PadMappingDialog : public QDialog
{
  Q_OBJECT
public:
  explicit PadMappingDialog(QWidget* parent = nullptr);

  int exec() override;
  void accept() override;

  void SetIsWiiGame(bool is_wii_game);

  NetPlay::PadMappingArray GetGCPadArray();
  NetPlay::GBAConfigArray GetGBAArray();
  NetPlay::PadMappingArray GetWiimoteArray();

private:
  enum class Column : int
  {
    Player = 0,
    GC1,
    GC2,
    GC3,
    GC4,
    Wii1,
    Wii2,
    Wii3,
    Wii4,
    Count
  };

  void CreateWidgets();
  void ConnectWidgets();
  void ReloadFromServer();
  void RefreshTable();
  void RefreshGbaRow();
  void SyncMappingsFromWidgets();
  void UpdateSummary();
  void AutoAssign();
  void ClearAll();

  static int PortForColumn(int column);
  static bool IsGcColumn(int column);
  static bool IsWiiColumn(int column);

  QTableWidget* m_mapping_table = nullptr;
  QLabel* m_help_label = nullptr;
  QLabel* m_summary_label = nullptr;
  QPushButton* m_auto_assign_button = nullptr;
  QPushButton* m_clear_button = nullptr;
  QDialogButtonBox* m_button_box = nullptr;
  std::array<QCheckBox*, 4> m_gba_boxes{};

  NetPlay::PadMappingArray m_pad_mapping;
  NetPlay::GBAConfigArray m_gba_config;
  NetPlay::PadMappingArray m_wii_mapping;
  std::vector<const NetPlay::Player*> m_players;
  bool m_is_wii_game = false;
};
