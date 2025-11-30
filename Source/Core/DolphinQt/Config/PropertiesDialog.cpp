// Copyright 2016 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "DolphinQt/Config/PropertiesDialog.h"

#include <memory>

#include <QDialogButtonBox>
#include <QHBoxLayout>
#include <QPushButton>
#include <QVBoxLayout>

#include "DiscIO/Enums.h"
#include "DiscIO/Volume.h"

#include "DolphinQt/Config/ARCodeWidget.h"
#include "DolphinQt/Config/FilesystemWidget.h"
#include "DolphinQt/Config/GameConfigWidget.h"
#include "DolphinQt/Config/GeckoCodeWidget.h"
#include "DolphinQt/Config/GraphicsModListWidget.h"
#include "DolphinQt/Config/InfoWidget.h"
#include "DolphinQt/Config/PatchesWidget.h"
#include "DolphinQt/Config/VerifyWidget.h"
#include "DolphinQt/QtUtils/WrapInScrollArea.h"

#include "UICommon/GameFile.h"

PropertiesDialog::PropertiesDialog(QWidget* parent, const UICommon::GameFile& game)
    : StackedSettingsWindow{parent}, m_filepath(game.GetFilePath())
{
  setWindowTitle(QStringLiteral("%1: %2 - %3")
                     .arg(QString::fromStdString(game.GetFileName()),
                          QString::fromStdString(game.GetGameID()),
                          QString::fromStdString(game.GetLongName())));

  auto* const info = new InfoWidget(game);
  auto* const ar = new ARCodeWidget(game.GetGameID(), game.GetRevision());
  auto* const gecko =
      new GeckoCodeWidget(game.GetGameID(), game.GetGameTDBID(), game.GetRevision());
  auto* const patches = new PatchesWidget(game);
  auto* const game_config = new GameConfigWidget(game);
  auto* const graphics_mod_list = new GraphicsModListWidget(game);

  connect(gecko, &GeckoCodeWidget::OpenGeneralSettings, this,
          &PropertiesDialog::OpenGeneralSettings);

  connect(ar, &ARCodeWidget::OpenGeneralSettings, this, &PropertiesDialog::OpenGeneralSettings);
#ifdef USE_RETRO_ACHIEVEMENTS
  connect(ar, &ARCodeWidget::OpenAchievementSettings, this,
          &PropertiesDialog::OpenAchievementSettings);
  connect(gecko, &GeckoCodeWidget::OpenAchievementSettings, this,
          &PropertiesDialog::OpenAchievementSettings);
  connect(patches, &PatchesWidget::OpenAchievementSettings, this,
          &PropertiesDialog::OpenAchievementSettings);
#endif  // USE_RETRO_ACHIEVEMENTS

  connect(graphics_mod_list, &GraphicsModListWidget::OpenGraphicsSettings, this,
          &PropertiesDialog::OpenGraphicsSettings);

  AddWrappedPane(info, tr("Info"));
  AddWrappedPane(game_config, tr("Game Config"));
  AddWrappedPane(patches, tr("Patches"));
  AddWrappedPane(ar, tr("AR Codes"));
  AddWrappedPane(gecko, tr("Gecko Codes"));
  AddWrappedPane(graphics_mod_list, tr("Graphics Mods"));

  if (game.GetPlatform() != DiscIO::Platform::ELFOrDOL)
  {
    std::shared_ptr<DiscIO::Volume> volume = DiscIO::CreateVolume(game.GetFilePath());
    if (volume)
    {
      auto* const verify = new VerifyWidget(volume);
      AddPane(verify, tr("Verify"));

      if (DiscIO::IsDisc(game.GetPlatform()))
      {
        auto* const filesystem = new FilesystemWidget(volume);
        AddPane(filesystem, tr("Filesystem"));
      }
    }
  }

  connect(this, &QDialog::rejected, graphics_mod_list, &GraphicsModListWidget::SaveToDisk);

  QDialogButtonBox* close_box = new QDialogButtonBox(QDialogButtonBox::Close);

  connect(close_box, &QDialogButtonBox::rejected, this, &QDialog::reject);
  connect(close_box, &QDialogButtonBox::rejected, graphics_mod_list,
          &GraphicsModListWidget::SaveToDisk);

  // Get the layout from the base class and add the close button
  auto* layout = qobject_cast<QHBoxLayout*>(this->layout());
  if (layout)
  {
    auto* right_side = qobject_cast<QVBoxLayout*>(layout->itemAt(1)->layout());
    if (right_side)
    {
      right_side->addWidget(close_box);
    }
  }

  OnDoneCreatingPanes();
}

GeckoDialog::GeckoDialog(QWidget* parent) : StackedSettingsWindow(parent)
{
  setWindowTitle(QStringLiteral("%1").arg(QString::fromStdString("Modifications")));
  setWindowFlags(windowFlags() & ~Qt::WindowContextHelpButtonHint);
  resize(300, 400);

  // Create Gecko code widgets for each Mario Party game
  GeckoCodeWidget* mp4_gecko = new GeckoCodeWidget("GMPE01", "GMPE01", 0);
  GeckoCodeWidget* mp4dx_gecko = new GeckoCodeWidget("GMPDX2", "GMPDX2", 0);
  GeckoCodeWidget* mp5_gecko = new GeckoCodeWidget("GP5E01", "GP5E01", 0);
  GeckoCodeWidget* mp6_gecko = new GeckoCodeWidget("GP6E01", "GP6E01", 0);
  GeckoCodeWidget* mp7_gecko = new GeckoCodeWidget("GP7E01", "GP7E01", 0);
  GeckoCodeWidget* mp8_gecko = new GeckoCodeWidget("RM8E01", "RM8E01", 0);

  connect(mp4_gecko, &GeckoCodeWidget::OpenGeneralSettings, this,
          &GeckoDialog::OpenGeneralSettings);
  connect(mp4dx_gecko, &GeckoCodeWidget::OpenGeneralSettings, this,
          &GeckoDialog::OpenGeneralSettings);
  connect(mp5_gecko, &GeckoCodeWidget::OpenGeneralSettings, this,
          &GeckoDialog::OpenGeneralSettings);
  connect(mp6_gecko, &GeckoCodeWidget::OpenGeneralSettings, this,
          &GeckoDialog::OpenGeneralSettings);
  connect(mp7_gecko, &GeckoCodeWidget::OpenGeneralSettings, this,
          &GeckoDialog::OpenGeneralSettings);
  connect(mp8_gecko, &GeckoCodeWidget::OpenGeneralSettings, this,
          &GeckoDialog::OpenGeneralSettings);

  // Add each Mario Party game as a pane using the new list layout
  AddWrappedPane(mp4_gecko, tr("Mario Party 4"));
  AddWrappedPane(mp4dx_gecko, tr("Mario Party 4 DX"));
  AddWrappedPane(mp5_gecko, tr("Mario Party 5"));
  AddWrappedPane(mp6_gecko, tr("Mario Party 6"));
  AddWrappedPane(mp7_gecko, tr("Mario Party 7"));
  AddWrappedPane(mp8_gecko, tr("Mario Party 8"));

  OnDoneCreatingPanes();
}
