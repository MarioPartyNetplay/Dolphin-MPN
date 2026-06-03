// Copyright 2024 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "DolphinQt/Settings/MarioPartyNetplayPane.h"

#include <QCheckBox>
#include <QGroupBox>
#include <QLabel>
#include <QVBoxLayout>
#include <QSlider>
#include <QHBoxLayout>

#include "Core/Config/GraphicsSettings.h"
#include "Core/Core.h"
#include "Core/System.h"

#include "DolphinQt/Settings.h"

MarioPartyNetplayPane::MarioPartyNetplayPane(QWidget* parent) : QWidget(parent)
{
  CreateLayout();
  LoadConfig();

  ConnectLayout();

  connect(&Settings::Instance(), &Settings::EmulationStateChanged, this,
          &MarioPartyNetplayPane::OnEmulationStateChanged);
  connect(&Settings::Instance(), &Settings::ConfigChanged, this, &MarioPartyNetplayPane::LoadConfig);

  OnEmulationStateChanged(Core::GetState(Core::System::GetInstance()));
}

void MarioPartyNetplayPane::CreateLayout()
{
  m_main_layout = new QVBoxLayout;

  // Mario Party Netplay Settings
  auto* mpn_group = new QGroupBox(tr("MPN Settings"));
  auto* mpn_group_layout = new QVBoxLayout;
  mpn_group->setLayout(mpn_group_layout);
  m_main_layout->addWidget(mpn_group);

  m_checkbox_show_turn_count = new QCheckBox(tr("Show Turn Count"));
  m_checkbox_show_turn_count->setToolTip(tr("Show the current MP turn in the Dolphin HUD."));
  mpn_group_layout->addWidget(m_checkbox_show_turn_count);

  m_checkbox_show_buttons_new = new QCheckBox(tr("Per-controller Buttons"));
  m_checkbox_show_buttons_new->setToolTip(tr("Change the in-game MP buttons to buttons to match your selected controller."));
  mpn_group_layout->addWidget(m_checkbox_show_buttons_new);

  m_checkbox_log_turn_count_to_file = new QCheckBox(tr("Log Turn Count to File"));
  m_checkbox_log_turn_count_to_file->setToolTip(tr("Logs the current turn count to a file for tracking purposes."));
  mpn_group_layout->addWidget(m_checkbox_log_turn_count_to_file);

  // HUD Scale setting
  auto* hud_scale_layout = new QHBoxLayout;
  m_label_hud_scale = new QLabel(tr("HUD Scale:"));
  m_slider_hud_scale = new QSlider(Qt::Horizontal);
  m_slider_hud_scale->setRange(10, 500);
  m_slider_hud_scale->setValue(100);
  m_slider_hud_scale->setToolTip(tr("Adjust the size of the Mario Party HUD elements (turn counter, etc.)."));
  
  m_label_hud_scale_value = new QLabel(QStringLiteral("100%"));
  m_label_hud_scale_value->setMinimumWidth(50);
  
  hud_scale_layout->addWidget(m_label_hud_scale);
  hud_scale_layout->addWidget(m_slider_hud_scale);
  hud_scale_layout->addWidget(m_label_hud_scale_value);
  mpn_group_layout->addLayout(hud_scale_layout);

  mpn_group_layout->addStretch(1);
  m_main_layout->addStretch(1);
  setLayout(m_main_layout);
}

void MarioPartyNetplayPane::ConnectLayout()
{
  connect(m_checkbox_show_turn_count, &QCheckBox::toggled, this, &MarioPartyNetplayPane::OnSaveConfig);
  connect(m_checkbox_show_buttons_new, &QCheckBox::toggled, this, &MarioPartyNetplayPane::OnSaveConfig);
  connect(m_checkbox_log_turn_count_to_file, &QCheckBox::toggled, this, &MarioPartyNetplayPane::OnSaveConfig);
  connect(m_slider_hud_scale, &QSlider::valueChanged, this, [this](int value) {
    // Update the value label
    m_label_hud_scale_value->setText(QString::number(value) + QStringLiteral("%"));
    // Save the config
    OnSaveConfig();
  });

}

void MarioPartyNetplayPane::LoadConfig()
{
  const bool running = Core::GetState(Core::System::GetInstance()) != Core::State::Uninitialized;

  m_checkbox_show_turn_count->setChecked(Config::Get(Config::GFX_SHOW_MP_TURN));
  m_checkbox_show_buttons_new->setChecked(Config::Get(Config::PER_CTRL_BUTTONS));
  m_checkbox_log_turn_count_to_file->setChecked(Config::Get(Config::GFX_LOG_TURN_COUNT_TO_FILE));
  
  // Load HUD scale value (convert from float 0.1-5.0 to slider 10-500)
  const float hud_scale = Config::Get(Config::GFX_MPN_HUD_SCALE);
  const int slider_value = static_cast<int>(hud_scale * 100);
  m_slider_hud_scale->setValue(slider_value);
  m_label_hud_scale_value->setText(QString::number(slider_value) + QStringLiteral("%"));

  m_checkbox_show_turn_count->setEnabled(!running);
  m_checkbox_show_buttons_new->setEnabled(!running);
  m_checkbox_log_turn_count_to_file->setEnabled(!running);
  m_slider_hud_scale->setEnabled(!running);


}

void MarioPartyNetplayPane::OnSaveConfig()
{
  Config::SetBaseOrCurrent(Config::GFX_SHOW_MP_TURN, m_checkbox_show_turn_count->isChecked());
  Config::SetBaseOrCurrent(Config::PER_CTRL_BUTTONS, m_checkbox_show_buttons_new->isChecked());
  Config::SetBaseOrCurrent(Config::GFX_LOG_TURN_COUNT_TO_FILE, m_checkbox_log_turn_count_to_file->isChecked());
  
  // Save HUD scale value (convert from slider 10-500 to float 0.1-5.0)
  const float hud_scale = m_slider_hud_scale->value() / 100.0f;
  Config::SetBaseOrCurrent(Config::GFX_MPN_HUD_SCALE, hud_scale);

  Config::Save();
}

void MarioPartyNetplayPane::OnEmulationStateChanged(Core::State state)
{
  const bool running = state != Core::State::Uninitialized;

  m_checkbox_show_turn_count->setEnabled(!running);
  m_checkbox_show_buttons_new->setEnabled(!running);
  m_checkbox_log_turn_count_to_file->setEnabled(!running);
  m_slider_hud_scale->setEnabled(!running);
} 
