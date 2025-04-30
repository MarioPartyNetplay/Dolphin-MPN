// Copyright 2017 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "DolphinQt/NetPlay/PadMappingDialog.h"

#include <QCheckBox>
#include <QDialogButtonBox>
#include <QGridLayout>
#include <QLabel>
#include <QListWidget>
#include <QSignalBlocker>

#include "Core/NetPlayClient.h"
#include "Core/NetPlayServer.h"

#include "DolphinQt/Settings.h"

PadMappingDialog::PadMappingDialog(QWidget* parent) : QDialog(parent)
{
  setWindowFlags(windowFlags() & ~Qt::WindowContextHelpButtonHint);
  setWindowTitle(tr("Assign Controllers"));

  CreateWidgets();
  ConnectWidgets();
}

void PadMappingDialog::CreateWidgets()
{
  m_main_layout = new QGridLayout;
  m_button_box = new QDialogButtonBox(QDialogButtonBox::Ok);

  for (unsigned int i = 0; i < m_wii_boxes.size(); i++)
  {
    m_gc_boxes[i] = new QListWidget;
    m_gc_boxes[i]->setSelectionMode(QAbstractItemView::MultiSelection);
    m_gba_boxes[i] = new QCheckBox(tr("GBA Port %1").arg(i + 1));
    m_wii_boxes[i] = new QListWidget;
    m_wii_boxes[i]->setSelectionMode(QAbstractItemView::MultiSelection);

    m_main_layout->addWidget(new QLabel(tr("GC Port %1").arg(i + 1)), 0, i);
    m_main_layout->addWidget(m_gc_boxes[i], 1, i);
#ifdef HAS_LIBMGBA
    m_main_layout->addWidget(m_gba_boxes[i], 2, i);
#endif
    m_main_layout->addWidget(new QLabel(tr("Wii Remote %1").arg(i + 1)), 3, i);
    m_main_layout->addWidget(m_wii_boxes[i], 4, i);
  }

  m_main_layout->addWidget(m_button_box, 5, 0, 1, -1);

  setLayout(m_main_layout);
}

void PadMappingDialog::ConnectWidgets()
{
  connect(m_button_box, &QDialogButtonBox::accepted, this, &QDialog::accept);
  for (const auto& list_group : {m_gc_boxes, m_wii_boxes})
  {
    for (const auto& list : list_group)
    {
      connect(list, &QListWidget::itemSelectionChanged, this, &PadMappingDialog::OnMappingChanged);
    }
  }
  for (const auto& checkbox : m_gba_boxes)
  {
    connect(checkbox, &QCheckBox::stateChanged, this, &PadMappingDialog::OnMappingChanged);
  }
}

int PadMappingDialog::exec()
{
  auto client = Settings::Instance().GetNetPlayClient();
  auto server = Settings::Instance().GetNetPlayServer();
  // Load Settings
  m_players = client->GetPlayers();
  m_pad_mapping = server->GetPadMapping();
  m_multi_pad_mapping = server->GetMultiPadMapping();
  m_gba_config = server->GetGBAConfig();
  m_wii_mapping = server->GetWiimoteMapping();

  for (auto& list_group : {m_gc_boxes, m_wii_boxes})
  {
    bool gc = list_group == m_gc_boxes;
    for (size_t i = 0; i < list_group.size(); i++)
    {
      auto& list = list_group[i];
      const QSignalBlocker blocker(list);

      list->clear();
      list->addItem(tr("None"));

      for (const auto& player : m_players)
      {
        auto* item = new QListWidgetItem(
            QStringLiteral("%1 (%2)").arg(QString::fromStdString(player->name)).arg(player->pid));
        list->addItem(item);

        // Select the item if this player is mapped to this port
        if (gc)
        {
          if (m_multi_pad_mapping[i].find(player->pid) != m_multi_pad_mapping[i].end())
          {
            item->setSelected(true);
          }
        }
        else
        {
          if (m_wii_mapping[i] == player->pid)
          {
            item->setSelected(true);
          }
        }
      }
    }
  }

  for (size_t i = 0; i < m_gba_boxes.size(); i++)
  {
    const QSignalBlocker blocker(m_gba_boxes[i]);
    m_gba_boxes[i]->setChecked(m_gba_config[i].enabled);
  }

  return QDialog::exec();
}

void PadMappingDialog::OnMappingChanged()
{
  for (unsigned int i = 0; i < m_wii_boxes.size(); i++)
  {
    // Handle GC port mappings
    QList<QListWidgetItem*> gc_selected = m_gc_boxes[i]->selectedItems();
    m_multi_pad_mapping[i].clear();

    bool has_none = false;
    bool has_players = false;

    // First pass - check what's selected
    for (auto* item : gc_selected)
    {
      if (item->text() == tr("None"))
        has_none = true;
      else
        has_players = true;
    }

    // Handle selections with signal blocking
    {
      const QSignalBlocker blocker(m_gc_boxes[i]);

      // If both None and players are selected, enforce the proper state
      if (has_none && has_players)
      {
        if (gc_selected.size() > 1)  // If multiple items selected, prefer players over None
        {
          m_gc_boxes[i]->item(0)->setSelected(false);  // Deselect None
          has_none = false;
        }
        else  // If only one item, prefer None
        {
          for (int j = 1; j < m_gc_boxes[i]->count(); j++)
            m_gc_boxes[i]->item(j)->setSelected(false);
          has_players = false;
        }
      }

      // Update mappings based on final state
      if (has_players)
      {
        for (auto* item : gc_selected)
        {
          if (item->text() != tr("None"))
          {
            int player_index = m_gc_boxes[i]->row(item) - 1;
            if (player_index >= 0 && player_index < static_cast<int>(m_players.size()))
            {
              m_multi_pad_mapping[i].insert(m_players[player_index]->pid);
            }
          }
        }
      }

      // If nothing is selected or only None is selected, ensure None is selected
      if (!has_players)
      {
        m_gc_boxes[i]->item(0)->setSelected(true);
      }
    }

    // Update legacy single-player mapping
    m_pad_mapping[i] = m_multi_pad_mapping[i].empty() ? 0 : *m_multi_pad_mapping[i].begin();

    // Handle Wii Remote mappings
    QList<QListWidgetItem*> wii_selected = m_wii_boxes[i]->selectedItems();
    m_wii_mapping[i] = 0;

    has_none = false;
    has_players = false;

    // First pass - check what's selected
    for (auto* item : wii_selected)
    {
      if (item->text() == tr("None"))
        has_none = true;
      else
        has_players = true;
    }

    // Handle selections with signal blocking
    {
      const QSignalBlocker blocker(m_wii_boxes[i]);

      // For Wii remotes, we only want one selection
      if (has_none || !has_players)
      {
        // Select None and clear other selections
        for (int j = 0; j < m_wii_boxes[i]->count(); j++)
          m_wii_boxes[i]->item(j)->setSelected(j == 0);
      }
      else
      {
        // Use the last selected player
        auto* last_selected = wii_selected.last();
        if (last_selected->text() != tr("None"))
        {
          int player_index = m_wii_boxes[i]->row(last_selected) - 1;
          if (player_index >= 0 && player_index < static_cast<int>(m_players.size()))
          {
            // Clear all selections except the last player
            for (int j = 0; j < m_wii_boxes[i]->count(); j++)
              m_wii_boxes[i]->item(j)->setSelected(m_wii_boxes[i]->item(j) == last_selected);
            
            m_wii_mapping[i] = m_players[player_index]->pid;
          }
        }
      }
    }

    m_gba_config[i].enabled = m_gba_boxes[i]->isChecked();
  }

  // Update server with new mappings
  if (auto server = Settings::Instance().GetNetPlayServer())
  {
    server->SetPadMapping(m_pad_mapping);
    server->SetMultiPadMapping(m_multi_pad_mapping);
    server->SetWiimoteMapping(m_wii_mapping);
    server->SetGBAConfig(m_gba_config, false);
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
