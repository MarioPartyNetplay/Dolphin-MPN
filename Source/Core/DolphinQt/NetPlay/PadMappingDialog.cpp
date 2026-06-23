// Copyright 2017 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "DolphinQt/NetPlay/PadMappingDialog.h"

#include <algorithm>

#include <QCheckBox>
#include <QDialogButtonBox>
#include <QGridLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QMessageBox>
#include <QPushButton>
#include <QSignalBlocker>
#include <QTableWidget>
#include <QVBoxLayout>

#include "Core/NetPlayClient.h"
#include "Core/NetPlayServer.h"
#include "Core/NetPlayProto.h"

#include "DolphinQt/Settings.h"

PadMappingDialog::PadMappingDialog(QWidget* parent) : QDialog(parent)
{
  setWindowTitle(tr("Assign Controllers"));
  setMinimumSize(720, 360);

  CreateWidgets();
  ConnectWidgets();
}

void PadMappingDialog::CreateWidgets()
{
  m_help_label = new QLabel;
  m_help_label->setWordWrap(true);

  m_gc_ports_checkbox = new QCheckBox(tr("GameCube controllers (optional)"));
  m_gc_ports_checkbox->setToolTip(
      tr("Show GameCube port columns for Wii games that support the GameCube Controller."));
  m_gc_ports_checkbox->setVisible(false);

  m_mapping_table = new QTableWidget;
  m_mapping_table->setColumnCount(static_cast<int>(Column::Count));
  m_mapping_table->setHorizontalHeaderLabels(
      {tr("Player"), tr("GC 1"), tr("GC 2"), tr("GC 3"), tr("GC 4"), tr("Wii 1"), tr("Wii 2"),
       tr("Wii 3"), tr("Wii 4")});
  m_mapping_table->verticalHeader()->setVisible(false);
  m_mapping_table->setSelectionMode(QAbstractItemView::NoSelection);
  m_mapping_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
  m_mapping_table->horizontalHeader()->setSectionResizeMode(static_cast<int>(Column::Player),
                                                            QHeaderView::Stretch);
  for (int col = static_cast<int>(Column::GC1); col < static_cast<int>(Column::Count); ++col)
    m_mapping_table->horizontalHeader()->setSectionResizeMode(col, QHeaderView::ResizeToContents);

  m_summary_label = new QLabel;
  m_summary_label->setWordWrap(true);

  m_auto_assign_button = new QPushButton(tr("Auto-Assign"));
  m_auto_assign_button->setToolTip(
      tr("Assign each player to the matching port number (player 1 → port 1, and so on)."));
  m_clear_button = new QPushButton(tr("Clear All"));
  m_clear_button->setToolTip(tr("Remove all controller assignments."));

  auto* action_layout = new QHBoxLayout;
  action_layout->addWidget(m_auto_assign_button);
  action_layout->addWidget(m_clear_button);
  action_layout->addStretch();

  auto* gba_layout = new QGridLayout;
  auto* gba_group = new QGroupBox(tr("GBA Link"));
  gba_group->setLayout(gba_layout);
  for (unsigned int i = 0; i < m_gba_boxes.size(); ++i)
  {
    m_gba_boxes[i] = new QCheckBox(tr("Port %1").arg(i + 1));
    gba_layout->addWidget(m_gba_boxes[i], 0, static_cast<int>(i));
  }

  m_button_box = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);

  auto* main_layout = new QVBoxLayout;
  main_layout->addWidget(m_help_label);
  main_layout->addWidget(m_gc_ports_checkbox);
  main_layout->addWidget(m_mapping_table, 1);
#ifdef HAS_LIBMGBA
  main_layout->addWidget(gba_group);
#else
  gba_group->hide();
#endif
  main_layout->addLayout(action_layout);
  main_layout->addWidget(m_summary_label);
  main_layout->addWidget(m_button_box);
  setLayout(main_layout);
}

void PadMappingDialog::ConnectWidgets()
{
  connect(m_button_box, &QDialogButtonBox::accepted, this, &QDialog::accept);
  connect(m_button_box, &QDialogButtonBox::rejected, this, &QDialog::reject);
  connect(m_mapping_table, &QTableWidget::itemChanged, this,
          &PadMappingDialog::OnMappingItemChanged);
  connect(m_gc_ports_checkbox, &QCheckBox::toggled, this, [this](bool checked) {
    m_show_gc_ports = checked;
    if (!checked)
      ClearGcMappingsInTable();
    UpdateGcColumnVisibility();
    UpdateSummary();
  });
  connect(m_auto_assign_button, &QPushButton::clicked, this, [this] {
    AutoAssign();
    UpdateSummary();
  });
  connect(m_clear_button, &QPushButton::clicked, this, [this] {
    ClearAll();
    UpdateSummary();
  });
#ifdef HAS_LIBMGBA
  for (QCheckBox* box : m_gba_boxes)
  {
    connect(box, &QCheckBox::toggled, this, [this] {
      SyncMappingsFromWidgets();
      UpdateSummary();
    });
  }
#endif
}

void PadMappingDialog::SetIsWiiGame(bool is_wii_game)
{
  m_is_wii_game = is_wii_game;

  if (!m_mapping_table)
    return;

  if (m_gc_ports_checkbox)
    m_gc_ports_checkbox->setVisible(is_wii_game);

  if (is_wii_game)
  {
    m_help_label->setText(
        tr("Assign each player to a Wii Remote port. Enable \"%1\" below for games that also "
           "support the GameCube Controller. Multiple players can share the same Wii Remote port "
           "(inputs are combined).")
            .arg(tr("GameCube controllers (optional)")));
    for (int col = static_cast<int>(Column::Wii1); col <= static_cast<int>(Column::Wii4); ++col)
      m_mapping_table->setColumnHidden(col, false);
    UpdateGcColumnVisibility();
  }
  else
  {
    m_show_gc_ports = true;
    if (m_gc_ports_checkbox)
    {
      const QSignalBlocker blocker(m_gc_ports_checkbox);
      m_gc_ports_checkbox->setChecked(false);
    }
    m_help_label->setText(
        tr("Check which GameCube ports each player uses. Multiple players can share the same port "
           "(inputs are combined)."));
    for (int col = static_cast<int>(Column::GC1); col <= static_cast<int>(Column::GC4); ++col)
      m_mapping_table->setColumnHidden(col, false);
    for (int col = static_cast<int>(Column::Wii1); col <= static_cast<int>(Column::Wii4); ++col)
      m_mapping_table->setColumnHidden(col, true);
  }
}

int PadMappingDialog::exec()
{
  ReloadFromServer();
  return QDialog::exec();
}

void PadMappingDialog::accept()
{
  SyncMappingsFromWidgets();

  if (m_is_wii_game)
  {
    QStringList dual_mapped;
    for (const NetPlay::Player* player : m_players)
    {
      const NetPlay::PlayerId pid = player->pid;
      const bool has_gc = std::ranges::any_of(m_pad_mapping, [pid](const NetPlay::PadMapping& mapping) {
        return std::ranges::find(mapping.players, pid) != mapping.players.end();
      });
      const bool has_wii =
          std::ranges::any_of(m_wii_mapping, [pid](const NetPlay::PadMapping& mapping) {
            return std::ranges::find(mapping.players, pid) != mapping.players.end();
          });
      if (has_gc && has_wii)
        dual_mapped.append(QString::fromStdString(player->name));
    }

    if (!dual_mapped.isEmpty())
    {
      QMessageBox::warning(
          this, tr("Invalid controller layout"),
          tr("Each player can use either a GameCube port or a Wii Remote port, not both:\n%1")
              .arg(dual_mapped.join(QStringLiteral("\n"))));
      return;
    }
  }

  std::vector<const NetPlay::Player*> unmapped;
  for (const NetPlay::Player* player : m_players)
  {
    const NetPlay::PlayerId pid = player->pid;
    const bool has_gc = std::ranges::any_of(m_pad_mapping, [pid](const NetPlay::PadMapping& mapping) {
      return std::ranges::find(mapping.players, pid) != mapping.players.end();
    });
    const bool has_wii =
        std::ranges::any_of(m_wii_mapping, [pid](const NetPlay::PadMapping& mapping) {
          return std::ranges::find(mapping.players, pid) != mapping.players.end();
        });
    if (!has_gc && !has_wii)
      unmapped.push_back(player);
  }

  if (!unmapped.empty())
  {
    QStringList names;
    for (const NetPlay::Player* player : unmapped)
      names.append(QStringLiteral("%1 (%2)").arg(QString::fromStdString(player->name)).arg(player->pid));

    const auto answer = QMessageBox::warning(
        this, tr("Unassigned players"),
        tr("These players have no GameCube or Wii Remote port assigned:\n%1\n\nStart anyway?")
            .arg(names.join(QLatin1Char('\n'))),
        QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
    if (answer != QMessageBox::Yes)
      return;
  }

  QDialog::accept();
}

void PadMappingDialog::ReloadFromServer()
{
  const auto client = Settings::Instance().GetNetPlayClient();
  const auto server = Settings::Instance().GetNetPlayServer();

  m_players = client->GetPlayers();
  m_pad_mapping = server->GetPadMapping();
  m_gba_config = server->GetGBAConfig();
  m_wii_mapping = server->GetWiimoteMapping();

  if (m_is_wii_game)
  {
    m_show_gc_ports = std::ranges::any_of(m_pad_mapping, [](const NetPlay::PadMapping& mapping) {
      return !mapping.players.empty();
    });
    if (m_gc_ports_checkbox)
    {
      const QSignalBlocker blocker(m_gc_ports_checkbox);
      m_gc_ports_checkbox->setChecked(m_show_gc_ports);
    }
    UpdateGcColumnVisibility();
  }

  RefreshTable();
  RefreshGbaRow();
  UpdateSummary();
}

void PadMappingDialog::RefreshTable()
{
  const QSignalBlocker blocker(m_mapping_table);

  m_mapping_table->setRowCount(static_cast<int>(m_players.size()));

  for (int row = 0; row < static_cast<int>(m_players.size()); ++row)
  {
    const NetPlay::Player* player = m_players[row];
    const QString player_label =
        QStringLiteral("%1 (%2)").arg(QString::fromStdString(player->name)).arg(player->pid);

    auto* name_item = new QTableWidgetItem(player_label);
    name_item->setFlags(Qt::ItemIsEnabled);
    name_item->setData(Qt::UserRole, static_cast<int>(player->pid));
    m_mapping_table->setItem(row, static_cast<int>(Column::Player), name_item);

    for (int col = static_cast<int>(Column::GC1); col < static_cast<int>(Column::Count); ++col)
    {
      const int port = PortForColumn(col);
      const NetPlay::PlayerId pid = player->pid;
      const bool checked =
          IsGcColumn(col) ?
              std::ranges::find(m_pad_mapping[port].players, pid) != m_pad_mapping[port].players.end() :
              std::ranges::find(m_wii_mapping[port].players, pid) !=
                  m_wii_mapping[port].players.end();

      auto* item = new QTableWidgetItem;
      item->setFlags(Qt::ItemIsEnabled | Qt::ItemIsUserCheckable);
      item->setCheckState(checked ? Qt::Checked : Qt::Unchecked);
      m_mapping_table->setItem(row, col, item);
    }
  }

  m_mapping_table->resizeRowsToContents();
}

void PadMappingDialog::RefreshGbaRow()
{
#ifdef HAS_LIBMGBA
  for (unsigned int i = 0; i < m_gba_boxes.size(); ++i)
  {
    const QSignalBlocker blocker(m_gba_boxes[i]);
    m_gba_boxes[i]->setChecked(m_gba_config[i].enabled);
  }
#endif
}

void PadMappingDialog::SyncMappingsFromWidgets()
{
  m_pad_mapping.fill(NetPlay::PadMapping{});
  m_wii_mapping.fill(NetPlay::PadMapping{});

  for (int row = 0; row < m_mapping_table->rowCount(); ++row)
  {
    const auto* name_item = m_mapping_table->item(row, static_cast<int>(Column::Player));
    if (!name_item)
      continue;

    const NetPlay::PlayerId pid = static_cast<NetPlay::PlayerId>(name_item->data(Qt::UserRole).toInt());

    for (int col = static_cast<int>(Column::GC1); col < static_cast<int>(Column::Count); ++col)
    {
      const auto* item = m_mapping_table->item(row, col);
      if (!item || item->checkState() != Qt::Checked)
        continue;

      const int port = PortForColumn(col);
      auto& players = IsGcColumn(col) ? m_pad_mapping[port].players : m_wii_mapping[port].players;
      if (std::ranges::find(players, pid) == players.end())
        players.push_back(pid);
    }
  }

#ifdef HAS_LIBMGBA
  for (unsigned int i = 0; i < m_gba_boxes.size(); ++i)
    m_gba_config[i].enabled = m_gba_boxes[i]->isChecked();
#endif

  if (m_is_wii_game)
  {
    // GC and Wii Remote ports are mutually exclusive per player.
    for (const NetPlay::Player* player : m_players)
    {
      const NetPlay::PlayerId pid = player->pid;
      const bool has_gc = std::ranges::any_of(m_pad_mapping, [pid](const NetPlay::PadMapping& mapping) {
        return std::ranges::find(mapping.players, pid) != mapping.players.end();
      });
      if (!has_gc)
        continue;

      for (auto& mapping : m_wii_mapping)
      {
        auto& players = mapping.players;
        const auto it = std::ranges::find(players, pid);
        if (it != players.end())
          players.erase(it);
      }
    }
  }
}

void PadMappingDialog::UpdateSummary()
{
  SyncMappingsFromWidgets();

  QStringList port_summaries;
  for (unsigned int port = 0; port < m_pad_mapping.size(); ++port)
  {
    QStringList gc_names;
    for (NetPlay::PlayerId pid : m_pad_mapping[port].players)
    {
      for (const NetPlay::Player* player : m_players)
      {
        if (player->pid == pid)
        {
          gc_names.append(QString::fromStdString(player->name));
          break;
        }
      }
    }

    QStringList wii_names;
    for (NetPlay::PlayerId pid : m_wii_mapping[port].players)
    {
      for (const NetPlay::Player* player : m_players)
      {
        if (player->pid == pid)
        {
          wii_names.append(QString::fromStdString(player->name));
          break;
        }
      }
    }

    if (gc_names.empty() && wii_names.empty())
      continue;

    QString line = tr("Port %1:").arg(port + 1);
    if (!gc_names.empty())
      line += QStringLiteral(" GC [%1]").arg(gc_names.join(QStringLiteral(", ")));
    if (!wii_names.empty())
      line += QStringLiteral(" Wii [%1]").arg(wii_names.join(QStringLiteral(", ")));
#ifdef HAS_LIBMGBA
    if (m_gba_config[port].enabled)
      line += tr(" GBA");
#endif
    port_summaries.append(line);
  }

  if (port_summaries.isEmpty())
  {
    m_summary_label->setText(
        tr("<b>No controllers assigned.</b> Use Auto-Assign or check ports in the table."));
    m_summary_label->setStyleSheet(QStringLiteral("color: #c0392b;"));
  }
  else
  {
    m_summary_label->setText(tr("<b>Current layout</b><br>%1").arg(port_summaries.join(QStringLiteral("<br>"))));
    m_summary_label->setStyleSheet({});
  }
}

void PadMappingDialog::AutoAssign()
{
  ClearAll();

  const QSignalBlocker blocker(m_mapping_table);

  std::array<bool, 4> port_used{};
  const int base_col = (m_is_wii_game && !m_show_gc_ports) ? static_cast<int>(Column::Wii1) :
                                                             static_cast<int>(Column::GC1);

  for (int row = 0; row < m_mapping_table->rowCount(); ++row)
  {
    const auto* name_item = m_mapping_table->item(row, static_cast<int>(Column::Player));
    if (!name_item)
      continue;

    const NetPlay::PlayerId pid =
        static_cast<NetPlay::PlayerId>(name_item->data(Qt::UserRole).toInt());
    int port = -1;
    if (pid >= 1 && pid <= 4)
      port = static_cast<int>(pid) - 1;
    else
    {
      for (int i = 0; i < 4; ++i)
      {
        if (!port_used[i])
        {
          port = i;
          break;
        }
      }
    }

    if (port < 0 || port >= 4)
      continue;

    if (auto* item = m_mapping_table->item(row, base_col + port))
      item->setCheckState(Qt::Checked);
    port_used[port] = true;
  }

  SyncMappingsFromWidgets();
  RefreshGbaRow();
}

void PadMappingDialog::ClearAll()
{
  const QSignalBlocker table_blocker(m_mapping_table);

  for (int row = 0; row < m_mapping_table->rowCount(); ++row)
  {
    for (int col = static_cast<int>(Column::GC1); col < static_cast<int>(Column::Count); ++col)
    {
      if (auto* item = m_mapping_table->item(row, col))
        item->setCheckState(Qt::Unchecked);
    }
  }

#ifdef HAS_LIBMGBA
  for (QCheckBox* box : m_gba_boxes)
  {
    const QSignalBlocker blocker(box);
    box->setChecked(false);
  }
#endif

  SyncMappingsFromWidgets();
}

int PadMappingDialog::PortForColumn(int column)
{
  if (IsGcColumn(column))
    return column - static_cast<int>(Column::GC1);
  if (IsWiiColumn(column))
    return column - static_cast<int>(Column::Wii1);
  return -1;
}

bool PadMappingDialog::IsGcColumn(int column)
{
  return column >= static_cast<int>(Column::GC1) && column <= static_cast<int>(Column::GC4);
}

bool PadMappingDialog::IsWiiColumn(int column)
{
  return column >= static_cast<int>(Column::Wii1) && column <= static_cast<int>(Column::Wii4);
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

void PadMappingDialog::UpdateGcColumnVisibility()
{
  if (!m_mapping_table)
    return;

  const bool show_gc = !m_is_wii_game || m_show_gc_ports;
  for (int col = static_cast<int>(Column::GC1); col <= static_cast<int>(Column::GC4); ++col)
    m_mapping_table->setColumnHidden(col, !show_gc);
}

void PadMappingDialog::ClearGcMappingsInTable()
{
  const QSignalBlocker blocker(m_mapping_table);

  for (int row = 0; row < m_mapping_table->rowCount(); ++row)
  {
    for (int col = static_cast<int>(Column::GC1); col <= static_cast<int>(Column::GC4); ++col)
    {
      if (auto* item = m_mapping_table->item(row, col))
        item->setCheckState(Qt::Unchecked);
    }
  }

  SyncMappingsFromWidgets();
}

void PadMappingDialog::OnMappingItemChanged(QTableWidgetItem* item)
{
  if (!item || item->column() == static_cast<int>(Column::Player))
    return;

  if (m_is_wii_game && item->checkState() == Qt::Checked)
  {
    const int row = item->row();
    const QSignalBlocker blocker(m_mapping_table);
    if (IsGcColumn(item->column()))
    {
      for (int col = static_cast<int>(Column::Wii1); col <= static_cast<int>(Column::Wii4); ++col)
      {
        if (auto* wii_item = m_mapping_table->item(row, col))
          wii_item->setCheckState(Qt::Unchecked);
      }
    }
    else if (IsWiiColumn(item->column()))
    {
      for (int col = static_cast<int>(Column::GC1); col <= static_cast<int>(Column::GC4); ++col)
      {
        if (auto* gc_item = m_mapping_table->item(row, col))
          gc_item->setCheckState(Qt::Unchecked);
      }
    }
  }

  SyncMappingsFromWidgets();

  UpdateSummary();
}
