// Copyright 2017 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "DolphinQt/Config/MarioPartyNetplayWidget.h"

#include <QGridLayout>
#include <QGroupBox>
#include <QVBoxLayout>

#include "Core/Config/GraphicsSettings.h"

#include "DolphinQt/Config/ConfigControls/ConfigBool.h"
#include "DolphinQt/Config/GameConfigWidget.h"

MarioPartyNetplayWidget::MarioPartyNetplayWidget(GameConfigWidget* parent, Config::Layer* layer)
    : m_game_layer(layer)
{
  CreateWidgets();
  ConnectWidgets();
  AddDescriptions();
}

void MarioPartyNetplayWidget::CreateWidgets()
{
  auto* main_layout = new QVBoxLayout;

  // MPN Settings
  auto* mpn_box = new QGroupBox(tr("MPN Settings"));
  auto* mpn_layout = new QGridLayout();

  m_show_turn_count = new ConfigBool(tr("Show Turn Count"), Config::GFX_SHOW_MP_TURN, m_game_layer);
  m_show_buttons_new = new ConfigBool(tr("Per-controller Buttons"), Config::PER_CTRL_BUTTONS, m_game_layer);
  m_log_turn_count_to_file = new ConfigBool(tr("Log Turn Count to File"), Config::GFX_LOG_TURN_COUNT_TO_FILE, m_game_layer);

  mpn_box->setLayout(mpn_layout);
  mpn_layout->addWidget(m_show_turn_count, 0, 0);
  mpn_layout->addWidget(m_show_buttons_new, 0, 1);
  mpn_layout->addWidget(m_log_turn_count_to_file, 1, 0, 1, 2);

  main_layout->addWidget(mpn_box);
  main_layout->addStretch();

  setLayout(main_layout);
}

void MarioPartyNetplayWidget::ConnectWidgets()
{
  // No special connections needed for ConfigBool widgets
}

void MarioPartyNetplayWidget::AddDescriptions()
{
  static const char TR_TURN_DESCRIPTION[] =
      QT_TR_NOOP("Show the current MP turn in the Dolphin HUD.");

  static const char TR_BUTTON_DESCRIPTION[] =
      QT_TR_NOOP("Change the in-game MP buttons to buttons to match "
                 "your selected controller.");

  static const char TR_LOG_TURN_COUNT_DESCRIPTION[] =
      QT_TR_NOOP("Logs the current turn count to a file for tracking purposes.");

  m_show_turn_count->SetDescription(tr(TR_TURN_DESCRIPTION));
  m_show_buttons_new->SetDescription(tr(TR_BUTTON_DESCRIPTION));
  m_log_turn_count_to_file->SetDescription(tr(TR_LOG_TURN_COUNT_DESCRIPTION));
} 