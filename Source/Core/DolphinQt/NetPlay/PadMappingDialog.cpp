// Copyright 2017 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "DolphinQt/NetPlay/PadMappingDialog.h"

#include <QCheckBox>
#include <QDialogButtonBox>
#include <QGridLayout>
#include <QLabel>
#include <QListWidget>
#include <QListWidgetItem>
#include <QSignalBlocker>
#include <QVariant>

#include "Core/NetPlayClient.h"
#include "Core/NetPlayServer.h"
#include "Core/NetPlayProto.h"

#include "DolphinQt/Settings.h"

PadMappingDialog::PadMappingDialog(QWidget* parent) : QDialog(parent)
{
  setWindowTitle(tr("Assign Controllers"));
  setMinimumSize(800, 600);

  CreateWidgets();
  ConnectWidgets();
}

void PadMappingDialog::CreateWidgets()
{
  m_main_layout = new QGridLayout;
  m_button_box = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);

  // Create headers
  m_main_layout->addWidget(new QLabel(tr("Port")), 0, 0);
  m_main_layout->addWidget(new QLabel(tr("GC Players")), 0, 1);
  m_main_layout->addWidget(new QLabel(tr("Wii Players")), 0, 2);
  m_main_layout->addWidget(new QLabel(tr("GBA")), 0, 3);

  for (unsigned int i = 0; i < 4; i++)
  {
    // Port number
    m_main_layout->addWidget(new QLabel(tr("Port %1").arg(i + 1)), i + 1, 0);

    // GC Player List
    m_gc_player_lists[i] = new QListWidget;
    m_gc_player_lists[i]->setSelectionMode(QAbstractItemView::MultiSelection);
    m_gc_player_lists[i]->setMaximumHeight(120);
    m_main_layout->addWidget(m_gc_player_lists[i], i + 1, 1);

    // Wii Player List
    m_wii_player_lists[i] = new QListWidget;
    m_wii_player_lists[i]->setSelectionMode(QAbstractItemView::MultiSelection);
    m_wii_player_lists[i]->setMaximumHeight(120);
    m_main_layout->addWidget(m_wii_player_lists[i], i + 1, 2);

    // GBA Checkbox
    m_gba_boxes[i] = new QCheckBox(tr("GBA Port %1").arg(i + 1));
    m_main_layout->addWidget(m_gba_boxes[i], i + 1, 3);
  }

  m_main_layout->addWidget(m_button_box, 5, 0, 1, 4);

  setLayout(m_main_layout);
}

void PadMappingDialog::ConnectWidgets()
{
  connect(m_button_box, &QDialogButtonBox::accepted, this, &QDialog::accept);
  connect(m_button_box, &QDialogButtonBox::rejected, this, &QDialog::reject);

  // Connect all player lists to the same slot
  for (auto* list : m_gc_player_lists)
  {
    connect(list, &QListWidget::itemChanged, this, &PadMappingDialog::OnPlayerSelectionChanged);
  }
  for (auto* list : m_wii_player_lists)
  {
    connect(list, &QListWidget::itemChanged, this, &PadMappingDialog::OnPlayerSelectionChanged);
  }

  // Connect GBA checkboxes
  for (auto* checkbox : m_gba_boxes)
  {
    connect(checkbox, &QCheckBox::toggled, this, &PadMappingDialog::OnPlayerSelectionChanged);
  }
}

int PadMappingDialog::exec()
{
  const auto client = Settings::Instance().GetNetPlayClient();
  const auto server = Settings::Instance().GetNetPlayServer();
  
  // Load Settings
  m_players = client->GetPlayers();
  m_pad_mapping = server->GetPadMapping();
  m_gba_config = server->GetGBAConfig();
  m_wii_mapping = server->GetWiimoteMapping();

  UpdatePlayerLists();

  return QDialog::exec();
}

void PadMappingDialog::UpdatePlayerLists()
{
  // Clear all lists
  for (auto* list : m_gc_player_lists)
  {
    list->clear();
  }
  for (auto* list : m_wii_player_lists)
  {
    list->clear();
  }

  // Add players to all lists
  for (size_t player_idx = 0; player_idx < m_players.size(); player_idx++)
  {
    const auto* player = m_players[player_idx];
    QString player_text = QStringLiteral("%1 (%2)").arg(QString::fromUtf8(player->name.c_str())).arg(player->pid);
    
    for (unsigned int i = 0; i < 4; i++)
    {
      // GC List
      auto* gc_item = new QListWidgetItem(player_text);
      gc_item->setFlags(gc_item->flags() | Qt::ItemIsUserCheckable);
      gc_item->setCheckState(Qt::Unchecked);
      gc_item->setData(Qt::UserRole, QVariant(static_cast<int>(player->pid)));
      m_gc_player_lists[i]->addItem(gc_item);

      // Wii List
      auto* wii_item = new QListWidgetItem(player_text);
      wii_item->setFlags(wii_item->flags() | Qt::ItemIsUserCheckable);
      wii_item->setCheckState(Qt::Unchecked);
      wii_item->setData(Qt::UserRole, QVariant(static_cast<int>(player->pid)));
      m_wii_player_lists[i]->addItem(wii_item);
    }
  }

  // Set current selections based on mappings
  for (unsigned int i = 0; i < 4; i++)
  {
    // Set GC selections
    for (int j = 0; j < m_gc_player_lists[i]->count(); j++)
    {
      auto* item = m_gc_player_lists[i]->item(j);
      NetPlay::PlayerId pid = static_cast<NetPlay::PlayerId>(item->data(Qt::UserRole).toInt());
      
      bool is_selected = std::find(m_pad_mapping[i].players.begin(), 
                                  m_pad_mapping[i].players.end(), pid) != m_pad_mapping[i].players.end();
      item->setCheckState(is_selected ? Qt::Checked : Qt::Unchecked);
    }

    // Set Wii selections
    for (int j = 0; j < m_wii_player_lists[i]->count(); j++)
    {
      auto* item = m_wii_player_lists[i]->item(j);
      NetPlay::PlayerId pid = static_cast<NetPlay::PlayerId>(item->data(Qt::UserRole).toInt());
      
      bool is_selected = std::find(m_wii_mapping[i].players.begin(), 
                                  m_wii_mapping[i].players.end(), pid) != m_wii_mapping[i].players.end();
      item->setCheckState(is_selected ? Qt::Checked : Qt::Unchecked);
    }

    // Set GBA checkbox
    m_gba_boxes[i]->setChecked(m_gba_config[i].enabled);
  }
}

void PadMappingDialog::OnPlayerSelectionChanged()
{
  // Update mappings based on current selections
  for (unsigned int i = 0; i < 4; i++)
  {
    // Update GC mapping
    m_pad_mapping[i].players.clear();
    for (int j = 0; j < m_gc_player_lists[i]->count(); j++)
    {
      auto* item = m_gc_player_lists[i]->item(j);
      if (item->checkState() == Qt::Checked)
      {
        NetPlay::PlayerId pid = static_cast<NetPlay::PlayerId>(item->data(Qt::UserRole).toInt());
        m_pad_mapping[i].players.push_back(pid);
      }
    }

    // Update Wii mapping
    m_wii_mapping[i].players.clear();
    for (int j = 0; j < m_wii_player_lists[i]->count(); j++)
    {
      auto* item = m_wii_player_lists[i]->item(j);
      if (item->checkState() == Qt::Checked)
      {
        NetPlay::PlayerId pid = item->data(Qt::UserRole).toUInt();
        m_wii_mapping[i].players.push_back(pid);
      }
    }

    // Update GBA config
    m_gba_config[i].enabled = m_gba_boxes[i]->isChecked();
  }
}

NetPlay::PadMappingArray PadMappingDialog::GetGCPadArray()
{
  return m_pad_mapping;
}

NetPlay::GBAConfigArray PadMappingDialog::GetGBAArray()
{
  return m_gba_config;
}

NetPlay::PadMappingArray PadMappingDialog::GetWiimoteArray()
{
  return m_wii_mapping;
}