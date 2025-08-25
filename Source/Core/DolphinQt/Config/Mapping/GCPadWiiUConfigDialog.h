// Copyright 2017 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <QDialog>
#include <optional>

class QCheckBox;
class QLabel;
class QDialogButtonBox;
class QVBoxLayout;

#include "InputCommon/ControllerInterface/ControllerInterface.h"

class GCPadWiiUConfigDialog final : public QDialog
{
  Q_OBJECT
public:
  explicit GCPadWiiUConfigDialog(int port, QWidget* parent = nullptr);
  ~GCPadWiiUConfigDialog() override;

private:
  void LoadSettings();
  void SaveSettings();

  void CreateLayout();
  void ConnectWidgets();

private:
  void UpdateAdapterStatus();
  bool IsGCAdapterAvailable() const;

  int m_port;

  QVBoxLayout* m_layout;
  QLabel* m_status_label;
  QDialogButtonBox* m_button_box;

  // Checkboxes
  QCheckBox* m_rumble;
  QCheckBox* m_simulate_bongos;

  // ControllerInterface callback handle
  std::optional<ControllerInterface::HotplugCallbackHandle> m_devices_changed_handle;
};
