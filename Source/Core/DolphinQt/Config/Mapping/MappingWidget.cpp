// Copyright 2017 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "DolphinQt/Config/Mapping/MappingWidget.h"

#include <fmt/core.h>

#include <QCheckBox>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>

#include "DolphinQt/Config/Mapping/IOWindow.h"
#include "DolphinQt/Config/Mapping/MappingButton.h"
#include "DolphinQt/Config/Mapping/MappingIndicator.h"
#include "DolphinQt/Config/Mapping/MappingNumeric.h"
#include "DolphinQt/Config/Mapping/MappingWindow.h"
#include "DolphinQt/QtUtils/SetWindowDecorations.h"

#include "InputCommon/ControlReference/ControlReference.h"
#include "InputCommon/ControllerEmu/Control/Control.h"
#include "InputCommon/ControllerEmu/ControlGroup/ControlGroup.h"
#include "InputCommon/ControllerEmu/ControlGroup/MixedTriggers.h"
#include "InputCommon/ControllerEmu/ControllerEmu.h"
#include "InputCommon/ControllerEmu/Setting/NumericSetting.h"
#include "InputCommon/ControllerEmu/StickGate.h"
#include "InputCommon/ControllerInterface/ControllerInterface.h"

MappingWidget::MappingWidget(MappingWindow* parent) : m_parent(parent)
{
  connect(parent, &MappingWindow::Update, this, &MappingWidget::Update);
  connect(parent, &MappingWindow::Save, this, &MappingWidget::SaveSettings);
  connect(parent, &MappingWindow::ConfigChanged, this, &MappingWidget::ConfigChanged);
}

MappingWindow* MappingWidget::GetParent() const
{
  return m_parent;
}

int MappingWidget::GetPort() const
{
  return m_parent->GetPort();
}

QGroupBox* MappingWidget::CreateGroupBox(ControllerEmu::ControlGroup* group)
{
  return CreateGroupBox(tr(group->ui_name.c_str()), group);
}

QGroupBox* MappingWidget::CreateGroupBox(const QString& name, ControllerEmu::ControlGroup* group)
{
  QGroupBox* group_box = new QGroupBox(name);
  QFormLayout* form_layout = new QFormLayout();

  group_box->setLayout(form_layout);

  MappingIndicator* indicator = nullptr;

  switch (group->type)
  {
  case ControllerEmu::GroupType::Shake:
    indicator = new ShakeMappingIndicator(*static_cast<ControllerEmu::Shake*>(group));
    break;

  case ControllerEmu::GroupType::MixedTriggers:
    indicator = new MixedTriggersIndicator(*static_cast<ControllerEmu::MixedTriggers*>(group));
    break;

  case ControllerEmu::GroupType::Tilt:
    indicator = new TiltIndicator(*static_cast<ControllerEmu::Tilt*>(group));
    break;

  case ControllerEmu::GroupType::Cursor:
    indicator = new CursorIndicator(*static_cast<ControllerEmu::Cursor*>(group));
    break;

  case ControllerEmu::GroupType::Force:
    indicator = new SwingIndicator(*static_cast<ControllerEmu::Force*>(group));
    break;

  case ControllerEmu::GroupType::IMUAccelerometer:
    indicator =
        new AccelerometerMappingIndicator(*static_cast<ControllerEmu::IMUAccelerometer*>(group));
    break;

  case ControllerEmu::GroupType::IMUGyroscope:
    indicator = new GyroMappingIndicator(*static_cast<ControllerEmu::IMUGyroscope*>(group));
    break;

  case ControllerEmu::GroupType::Stick:
    indicator = new AnalogStickIndicator(*static_cast<ControllerEmu::ReshapableInput*>(group));
    break;

  default:
    break;
  }

  if (indicator)
  {
    const auto indicator_layout = new QBoxLayout(QBoxLayout::Direction::Down);
    indicator_layout->addWidget(indicator);
    indicator_layout->setAlignment(Qt::AlignCenter);
    form_layout->addRow(indicator_layout);

    connect(this, &MappingWidget::Update, indicator, qOverload<>(&MappingIndicator::update));

    const bool need_calibration = group->type == ControllerEmu::GroupType::Cursor ||
                                  group->type == ControllerEmu::GroupType::Stick ||
                                  group->type == ControllerEmu::GroupType::Tilt ||
                                  group->type == ControllerEmu::GroupType::Force;

    if (need_calibration)
    {
      const auto calibrate =
          new CalibrationWidget(*static_cast<ControllerEmu::ReshapableInput*>(group),
                                *static_cast<ReshapableInputIndicator*>(indicator));

      form_layout->addRow(calibrate);
    }
  }

  for (auto& control : group->controls)
    CreateControl(control.get(), form_layout, !indicator);

  AddSettingWidgets(form_layout, group, ControllerEmu::SettingVisibility::Normal);

  if (group->default_value != ControllerEmu::ControlGroup::DefaultValue::AlwaysEnabled)
  {
    QLabel* group_enable_label = new QLabel(tr("Enable"));
    QCheckBox* group_enable_checkbox = new QCheckBox();
    group_enable_checkbox->setChecked(group->enabled);
    form_layout->insertRow(0, group_enable_label, group_enable_checkbox);
    auto enable_group_by_checkbox = [group, form_layout, group_enable_label,
                                     group_enable_checkbox] {
      group->enabled = group_enable_checkbox->isChecked();
      for (int i = 0; i < form_layout->count(); ++i)
      {
        QWidget* widget = form_layout->itemAt(i)->widget();
        if (widget != nullptr && widget != group_enable_label && widget != group_enable_checkbox)
          widget->setEnabled(group->enabled);
      }
    };
    enable_group_by_checkbox();
    connect(group_enable_checkbox, &QCheckBox::toggled, this, enable_group_by_checkbox);
    connect(this, &MappingWidget::ConfigChanged, this,
            [group_enable_checkbox, group] { group_enable_checkbox->setChecked(group->enabled); });
  }

  const auto advanced_setting_count = std::count_if(
      group->numeric_settings.begin(), group->numeric_settings.end(), [](auto& setting) {
        return setting->GetVisibility() == ControllerEmu::SettingVisibility::Advanced;
      });

  if (advanced_setting_count != 0)
  {
    const auto advanced_button = new QPushButton(tr("Advanced"));
    form_layout->addRow(advanced_button);
    connect(advanced_button, &QPushButton::clicked,
            [this, group] { ShowAdvancedControlGroupDialog(group); });
  }

  if (group->type == ControllerEmu::GroupType::Cursor)
  {
    QPushButton* mouse_button = new QPushButton(tr("Use Mouse Controlled Pointing"));
    form_layout->insertRow(2, mouse_button);
    connect(mouse_button, &QCheckBox::clicked, [this, group] {
      std::string default_device = g_controller_interface.GetDefaultDeviceString() + ":";
      const std::string controller_device = GetController()->GetDefaultDevice().ToString() + ":";
      if (default_device == controller_device)
      {
        default_device.clear();
      }
      group->SetControlExpression(0, fmt::format("`{}Cursor Y-`", default_device));
      group->SetControlExpression(1, fmt::format("`{}Cursor Y+`", default_device));
      group->SetControlExpression(2, fmt::format("`{}Cursor X-`", default_device));
      group->SetControlExpression(3, fmt::format("`{}Cursor X+`", default_device));
      emit ConfigChanged();
      GetController()->UpdateReferences(g_controller_interface);
    });
  }

  return group_box;
}

void MappingWidget::AddSettingWidgets(QFormLayout* layout, ControllerEmu::ControlGroup* group,
                                      ControllerEmu::SettingVisibility visibility)
{
  for (auto& setting : group->numeric_settings)
  {
    if (setting->GetVisibility() != visibility)
      continue;

    QWidget* setting_widget = nullptr;
    bool has_edit_condition = false;

    switch (setting->GetType())
    {
    case ControllerEmu::SettingType::Double:
      setting_widget = new MappingDouble(
          this, static_cast<ControllerEmu::NumericSetting<double>*>(setting.get()));
      has_edit_condition =
          static_cast<ControllerEmu::NumericSetting<double>*>(setting.get())->GetEditCondition() !=
          nullptr;
      break;

    case ControllerEmu::SettingType::Bool:
      setting_widget =
          new MappingBool(this, static_cast<ControllerEmu::NumericSetting<bool>*>(setting.get()));
      has_edit_condition =
          static_cast<ControllerEmu::NumericSetting<bool>*>(setting.get())->GetEditCondition() !=
          nullptr;
      break;

    default:
      // FYI: Widgets for additional types can be implemented as needed.
      break;
    }

    if (setting_widget)
    {
      const auto hbox = new QHBoxLayout;

      hbox->addWidget(setting_widget);
      hbox->addWidget(CreateSettingAdvancedMappingButton(*setting));

      form_layout->addRow(tr(setting->GetUIName()), hbox);

      QFormLayout::TakeRowResult row;
      row.fieldItem = form_layout->itemAt(form_layout->rowCount() - 1, QFormLayout::FieldRole);
      row.labelItem = form_layout->itemAt(form_layout->rowCount() - 1, QFormLayout::LabelRole);
      row.labelItem->widget()->setToolTip(tr(setting->GetUIDescription()));

      if (has_edit_condition)
      {
        m_edit_condition_numeric_settings.emplace_back(std::make_tuple(setting.get(), row, group));
      }
    }
  }
}

void MappingWidget::ShowAdvancedControlGroupDialog(ControllerEmu::ControlGroup* group)
{
  QDialog dialog{this};
  dialog.setWindowTitle(tr(group->ui_name.c_str()));

  const auto group_box = new QGroupBox(tr("Advanced Settings"));

  QFormLayout* form_layout = new QFormLayout();

  AddSettingWidgets(form_layout, group, ControllerEmu::SettingVisibility::Advanced);

  const auto reset_button = new QPushButton(tr("Reset All"));
  form_layout->addRow(reset_button);

  connect(reset_button, &QPushButton::clicked, [this, group] {
    for (auto& setting : group->numeric_settings)
    {
      if (setting->GetVisibility() != ControllerEmu::SettingVisibility::Advanced)
        continue;

      setting->SetToDefault();
    }

    emit ConfigChanged();
  });

  const auto main_layout = new QVBoxLayout();
  const auto button_box = new QDialogButtonBox(QDialogButtonBox::Close);

  group_box->setLayout(form_layout);

  main_layout->addWidget(group_box);
  main_layout->addWidget(button_box);

  dialog.setLayout(main_layout);

  // Focusing something else by default instead of the first spin box.
  // Dynamically changing expression-backed settings pause when taking input.
  // This just avoids that weird edge case behavior when the dialog is first open.
  button_box->setFocus();

  // Signal the newly created numeric setting widgets to display the current values.
  emit ConfigChanged();

  // Enable "Close" button functionality.
  connect(button_box, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);

  SetQWidgetWindowDecorations(&dialog);
  dialog.exec();
}

QGroupBox* MappingWidget::CreateControlsBox(const QString& name, ControllerEmu::ControlGroup* group,
                                            int columns)
{
  auto* group_box = new QGroupBox(name);
  auto* hbox_layout = new QHBoxLayout();

  group_box->setLayout(hbox_layout);

  std::vector<QFormLayout*> form_layouts;
  for (int i = 0; i < columns; ++i)
  {
    QLabel* group_enable_label = new QLabel(tr("Enable"));
    QCheckBox* group_enable_checkbox = new QCheckBox();
    group_enable_checkbox->setChecked(group->enabled);
    form_layout->insertRow(0, group_enable_label, group_enable_checkbox);
    auto enable_group_by_checkbox = [group, form_layout, group_enable_label,
                                     group_enable_checkbox] {
      group->enabled = group_enable_checkbox->isChecked();
      for (int i = 0; i < form_layout->count(); ++i)
      {
        QWidget* widget = form_layout->itemAt(i)->widget();
        if (widget != nullptr && widget != group_enable_label && widget != group_enable_checkbox)
        {
          widget->setEnabled(group->enabled);
        }
        // Layouts could be even more nested but they likely never will
        else if (QLayout* nested_layout = form_layout->itemAt(i)->layout())
        {
          for (int k = 0; k < nested_layout->count(); ++k)
          {
            if (widget = nested_layout->itemAt(k)->widget())
            {
              widget->setEnabled(group->enabled);
            }
          }
        }
      }
    };
    enable_group_by_checkbox();
    connect(group_enable_checkbox, &QCheckBox::toggled, this, enable_group_by_checkbox);
    connect(this, &MappingWidget::ConfigChanged, this,
            [group_enable_checkbox, group] { group_enable_checkbox->setChecked(group->enabled); });
  }

  // This isn't called immediately when the edit condition changes
  // but it's called at frequent regular intervals so it's fine.
  // Connecting checkbox changes events would have been quite complicated
  // given that groups have the ability to override the enabled value as well
  connect(this, &MappingWidget::Update, this, &MappingWidget::RefreshSettingsEnabled);

  return group_box;
}

void MappingWidget::RefreshSettingsEnabled()
{
  for (auto& numeric_settings : m_edit_condition_numeric_settings)
  {
    bool enabled = true;
    const ControllerEmu::NumericSettingBase* setting = std::get<0>(numeric_settings);
    switch (setting->GetType())
    {
    case ControllerEmu::SettingType::Double:
      enabled = static_cast<const ControllerEmu::NumericSetting<double>*>(setting)->IsEnabled();
      break;

    case ControllerEmu::SettingType::Bool:
      enabled = static_cast<const ControllerEmu::NumericSetting<bool>*>(setting)->IsEnabled();
      break;
    }
    enabled = enabled && std::get<2>(numeric_settings)->enabled;

    if (QWidget* widget = std::get<1>(numeric_settings).labelItem->widget())
    {
      widget->setEnabled(enabled);
    }
    if (QLayout* layout = std::get<1>(numeric_settings).fieldItem->layout())
    {
      for (int i = 0; i < layout->count(); ++i)
      {
        if (QWidget* widget = layout->itemAt(i)->widget())
        {
          widget->setEnabled(enabled);
        }
      }
    }
  }
}

ControllerEmu::EmulatedController* MappingWidget::GetController() const
{
  return m_parent->GetController();
}

QPushButton*
MappingWidget::CreateSettingAdvancedMappingButton(ControllerEmu::NumericSettingBase& setting)
{
  const auto button = new QPushButton(tr("..."));
  button->setFixedWidth(QFontMetrics(font()).boundingRect(button->text()).width() * 2);

  button->connect(button, &QPushButton::clicked, [this, &setting]() {
    if (setting.IsSimpleValue())
      setting.SetExpressionFromValue();

    IOWindow io(this, GetController(), &setting.GetInputReference(), IOWindow::Type::Input);
    SetQWidgetWindowDecorations(&io);
    io.exec();

    setting.SimplifyIfPossible();

    ConfigChanged();
    SaveSettings();
  });

  return button;
}
