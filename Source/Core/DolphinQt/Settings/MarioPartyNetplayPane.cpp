// Copyright 2024 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "DolphinQt/Settings/MarioPartyNetplayPane.h"

#include <QCheckBox>
#include <QGroupBox>
#include <QLabel>
#include <QVBoxLayout>

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
  auto* mpn_group = new QGroupBox(tr("Mario Party Netplay Settings"));
  auto* mpn_group_layout = new QVBoxLayout;
  mpn_group->setLayout(mpn_group_layout);
  m_main_layout->addWidget(mpn_group);

  m_checkbox_show_turn_count = new QCheckBox(tr("Show Turn Count"));
  mpn_group_layout->addWidget(m_checkbox_show_turn_count);

  m_checkbox_show_buttons_new = new QCheckBox(tr("Per-controller Buttons"));
  mpn_group_layout->addWidget(m_checkbox_show_buttons_new);

  m_checkbox_log_turn_count_to_file = new QCheckBox(tr("Log Turn Count to File"));
  mpn_group_layout->addWidget(m_checkbox_log_turn_count_to_file);

  // Add descriptions
  auto* turn_count_description = new QLabel(tr("Show the current MP turn in the Dolphin HUD."));
  turn_count_description->setWordWrap(true);
  mpn_group_layout->addWidget(turn_count_description);

  auto* buttons_description = new QLabel(tr("Change the in-game MP buttons to buttons to match "
                                           "your selected controller."));
  buttons_description->setWordWrap(true);
  mpn_group_layout->addWidget(buttons_description);

  auto* log_description = new QLabel(tr("Logs the current turn count to a file for tracking purposes."));
  log_description->setWordWrap(true);
  mpn_group_layout->addWidget(log_description);

  m_main_layout->addStretch(1);
  setLayout(m_main_layout);
}

void MarioPartyNetplayPane::ConnectLayout()
{
  connect(m_checkbox_show_turn_count, &QCheckBox::toggled, this, &MarioPartyNetplayPane::OnSaveConfig);
  connect(m_checkbox_show_buttons_new, &QCheckBox::toggled, this, &MarioPartyNetplayPane::OnSaveConfig);
  connect(m_checkbox_log_turn_count_to_file, &QCheckBox::toggled, this, &MarioPartyNetplayPane::OnSaveConfig);
}

void MarioPartyNetplayPane::LoadConfig()
{
  const bool running = Core::GetState(Core::System::GetInstance()) != Core::State::Uninitialized;

  m_checkbox_show_turn_count->setChecked(Config::Get(Config::GFX_SHOW_MP_TURN));
  m_checkbox_show_buttons_new->setChecked(Config::Get(Config::PER_CTRL_BUTTONS));
  m_checkbox_log_turn_count_to_file->setChecked(Config::Get(Config::GFX_LOG_TURN_COUNT_TO_FILE));

  m_checkbox_show_turn_count->setEnabled(!running);
  m_checkbox_show_buttons_new->setEnabled(!running);
  m_checkbox_log_turn_count_to_file->setEnabled(!running);
}

void MarioPartyNetplayPane::OnSaveConfig()
{
  Config::SetBaseOrCurrent(Config::GFX_SHOW_MP_TURN, m_checkbox_show_turn_count->isChecked());
  Config::SetBaseOrCurrent(Config::PER_CTRL_BUTTONS, m_checkbox_show_buttons_new->isChecked());
  Config::SetBaseOrCurrent(Config::GFX_LOG_TURN_COUNT_TO_FILE, m_checkbox_log_turn_count_to_file->isChecked());
  Config::Save();
}

void MarioPartyNetplayPane::OnEmulationStateChanged(Core::State state)
{
  const bool running = state != Core::State::Uninitialized;

  m_checkbox_show_turn_count->setEnabled(!running);
  m_checkbox_show_buttons_new->setEnabled(!running);
  m_checkbox_log_turn_count_to_file->setEnabled(!running);
} 