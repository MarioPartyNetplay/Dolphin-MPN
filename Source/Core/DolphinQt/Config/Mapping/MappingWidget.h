// Copyright 2017 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <memory>
#include <vector>

#include <QString>
#include <QFormLayout>
#include <QWidget>

class ControlGroupBox;
class InputConfig;
class MappingButton;
class MappingNumeric;
class MappingWindow;
class QFormLayout;
class QPushButton;
class QGroupBox;

namespace ControllerEmu
{
class Control;
class ControlGroup;
class EmulatedController;
class NumericSettingBase;
enum class SettingVisibility;
}  // namespace ControllerEmu

constexpr int INDICATOR_UPDATE_FREQ = 30;

class MappingWidget : public QWidget
{
  Q_OBJECT
public:
  explicit MappingWidget(MappingWindow* window);

  ControllerEmu::EmulatedController* GetController() const;

  MappingWindow* GetParent() const;

  virtual void LoadSettings() = 0;
  virtual void SaveSettings() = 0;
  virtual InputConfig* GetConfig() = 0;

signals:
  void Update();
  void ConfigChanged();

protected:
  int GetPort() const;

  void RefreshSettingsEnabled();

  QGroupBox* CreateGroupBox(ControllerEmu::ControlGroup* group);
  QGroupBox* CreateGroupBox(const QString& name, ControllerEmu::ControlGroup* group);
  QGroupBox* CreateControlsBox(const QString& name, ControllerEmu::ControlGroup* group,
                               int columns);
  void CreateControl(const ControllerEmu::Control* control, QFormLayout* layout, bool indicator);
  QPushButton* CreateSettingAdvancedMappingButton(ControllerEmu::NumericSettingBase& setting);
  void AddSettingWidgets(QFormLayout* layout, ControllerEmu::ControlGroup* group,
                         ControllerEmu::SettingVisibility visibility);
  void ShowAdvancedControlGroupDialog(ControllerEmu::ControlGroup* group);

private:
  MappingWindow* const m_parent;

  std::vector<std::tuple<const ControllerEmu::NumericSettingBase*, QFormLayout::TakeRowResult,
                         const ControllerEmu::ControlGroup*>>
      m_edit_condition_numeric_settings;
};
