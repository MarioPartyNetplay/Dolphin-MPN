// Copyright 2017 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "DolphinQt/Settings/GeneralPane.h"

#include <map>

#include <QCheckBox>
#include <QComboBox>
#include <QFormLayout>
#include <QGroupBox>
#include <QLabel>
#include <QPushButton>
#include <QSlider>
#include <QVBoxLayout>
#include <QWidget>

#include "Core/AchievementManager.h"
#include "Core/Config/MainSettings.h"
#include "Core/Config/UISettings.h"
#include "Core/ConfigManager.h"
#include "Core/Core.h"
#include "Core/DolphinAnalytics.h"
#include "Core/PowerPC/PowerPC.h"
#include "Core/System.h"

#include "DolphinQt/QtUtils/ModalMessageBox.h"
#include "DolphinQt/QtUtils/NonDefaultQPushButton.h"
#include "DolphinQt/QtUtils/SetWindowDecorations.h"
#include "DolphinQt/QtUtils/SignalBlocking.h"
#include "DolphinQt/Settings.h"

#include "UICommon/AutoUpdate.h"
#ifdef USE_DISCORD_PRESENCE
#include "UICommon/DiscordPresence.h"
#endif

constexpr int AUTO_UPDATE_DISABLE_INDEX = 0;
constexpr int AUTO_UPDATE_BETA_INDEX = 1;
constexpr int AUTO_UPDATE_DEV_INDEX = 2;

constexpr const char* AUTO_UPDATE_DISABLE_STRING = "";
constexpr const char* AUTO_UPDATE_BETA_STRING = "beta";
constexpr const char* AUTO_UPDATE_DEV_STRING = "dev";

constexpr int FALLBACK_REGION_NTSCJ_INDEX = 0;
constexpr int FALLBACK_REGION_NTSCU_INDEX = 1;
constexpr int FALLBACK_REGION_PAL_INDEX = 2;
constexpr int FALLBACK_REGION_NTSCK_INDEX = 3;

GeneralPane::GeneralPane(QWidget* parent) : QWidget(parent)
{
  CreateLayout();
  LoadConfig();

  ConnectLayout();

  connect(&Settings::Instance(), &Settings::EmulationStateChanged, this,
          &GeneralPane::OnEmulationStateChanged);
  connect(&Settings::Instance(), &Settings::ConfigChanged, this, &GeneralPane::LoadConfig);

  OnEmulationStateChanged(Core::GetState(Core::System::GetInstance()));
}

void GeneralPane::CreateLayout()
{
  m_main_layout = new QVBoxLayout;
  // Create layout here
  CreateBasic();
  CreateAutoUpdate();
  CreateFallbackRegion();

#if defined(USE_ANALYTICS) && USE_ANALYTICS
  CreateAnalytics();
#endif
  CreateCheats();
  m_main_layout->addStretch(1);
  setLayout(m_main_layout);
}

void GeneralPane::OnEmulationStateChanged(Core::State state)
{
  const bool running = state != Core::State::Uninitialized;

  m_checkbox_dualcore->setEnabled(!running);
#ifdef USE_RETRO_ACHIEVEMENTS
  bool hardcore = AchievementManager::GetInstance().IsHardcoreModeActive();
  m_checkbox_cheats->setEnabled(!running && !hardcore);
#else   // USE_RETRO_ACHIEVEMENTS
  m_checkbox_cheats->setEnabled(!running);
#endif  // USE_RETRO_ACHIEVEMENTS
  m_checkbox_override_region_settings->setEnabled(!running);
#ifdef USE_DISCORD_PRESENCE
  m_checkbox_discord_presence->setEnabled(!running);
#endif
  m_combobox_fallback_region->setEnabled(!running);

  UpdateDescriptionsUsingHardcoreStatus();
}

void GeneralPane::ConnectLayout()
{
  connect(m_checkbox_dualcore, &QCheckBox::toggled, this, &GeneralPane::OnSaveConfig);
  connect(m_checkbox_cheats, &QCheckBox::toggled, this, &GeneralPane::OnSaveConfig);
  connect(m_checkbox_override_region_settings, &QCheckBox::stateChanged, this,
          &GeneralPane::OnSaveConfig);
  connect(m_checkbox_auto_disc_change, &QCheckBox::toggled, this, &GeneralPane::OnSaveConfig);
#ifdef USE_DISCORD_PRESENCE
  connect(m_checkbox_discord_presence, &QCheckBox::toggled, this, &GeneralPane::OnSaveConfig);
#endif

  // Advanced
  connect(m_combobox_speedlimit, &QComboBox::currentIndexChanged, [this] {
    Config::SetBaseOrCurrent(Config::MAIN_EMULATION_SPEED,
                             m_combobox_speedlimit->currentIndex() * 0.1f);
    Config::Save();
  });

  connect(m_combobox_fallback_region, &QComboBox::currentIndexChanged, this,
          &GeneralPane::OnSaveConfig);
  connect(&Settings::Instance(), &Settings::FallbackRegionChanged, this, &GeneralPane::LoadConfig);

#if defined(USE_ANALYTICS) && USE_ANALYTICS
  connect(&Settings::Instance(), &Settings::AnalyticsToggled, this, &GeneralPane::LoadConfig);
  connect(m_checkbox_enable_analytics, &QCheckBox::toggled, this, &GeneralPane::OnSaveConfig);
  connect(m_button_generate_new_identity, &QPushButton::clicked, this,
          &GeneralPane::GenerateNewIdentity);
#endif
}

void GeneralPane::CreateBasic()
{
  auto* basic_group = new QGroupBox(tr("Basic Settings"));
  auto* basic_group_layout = new QVBoxLayout;
  basic_group->setLayout(basic_group_layout);
  m_main_layout->addWidget(basic_group);

  m_checkbox_dualcore = new QCheckBox(tr("Enable Dual Core (speed-hack)"));
  basic_group_layout->addWidget(m_checkbox_dualcore);

  m_checkbox_override_region_settings = new QCheckBox(tr("Allow Mismatched Region Settings"));
  basic_group_layout->addWidget(m_checkbox_override_region_settings);

  m_checkbox_auto_disc_change = new QCheckBox(tr("Change Discs Automatically"));
  basic_group_layout->addWidget(m_checkbox_auto_disc_change);

#ifdef USE_DISCORD_PRESENCE
  m_checkbox_discord_presence = new QCheckBox(tr("Show Current Game on Discord"));
  basic_group_layout->addWidget(m_checkbox_discord_presence);
#endif

  auto* speed_limit_layout = new QFormLayout;
  speed_limit_layout->setFormAlignment(Qt::AlignLeft | Qt::AlignTop);
  speed_limit_layout->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);
  basic_group_layout->addLayout(speed_limit_layout);

  m_combobox_speedlimit = new QComboBox();

  m_combobox_speedlimit->addItem(tr("Unlimited"));
  for (int i = 10; i <= 200; i += 10)  // from 10% to 200%
  {
    QString str;
    if (i != 100)
      str = QStringLiteral("%1%").arg(i);
    else
      str = tr("%1% (Normal Speed)").arg(i);

    m_combobox_speedlimit->addItem(str);
  }

  speed_limit_layout->addRow(tr("&Speed Limit:"), m_combobox_speedlimit);
}

void GeneralPane::CreateFallbackRegion()
{
  auto* fallback_region_group = new QGroupBox(tr("Fallback Region"));
  auto* fallback_region_group_layout = new QVBoxLayout;
  fallback_region_group->setLayout(fallback_region_group_layout);
  m_main_layout->addWidget(fallback_region_group);

  auto* fallback_region_dropdown_layout = new QFormLayout;
  fallback_region_dropdown_layout->setFormAlignment(Qt::AlignLeft | Qt::AlignTop);
  fallback_region_dropdown_layout->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);
  fallback_region_group_layout->addLayout(fallback_region_dropdown_layout);

  m_combobox_fallback_region = new QComboBox(this);
  fallback_region_dropdown_layout->addRow(tr("Fallback Region:"), m_combobox_fallback_region);

  for (const QString& option : {tr("NTSC-J"), tr("NTSC-U"), tr("PAL"), tr("NTSC-K")})
    m_combobox_fallback_region->addItem(option);

  auto* fallback_region_description =
      new QLabel(tr("Dolphin will use this for titles whose region cannot be determined "
                    "automatically."));
  fallback_region_description->setWordWrap(true);
  fallback_region_group_layout->addWidget(fallback_region_description);
}

/*void GeneralPane::CreateAutoUpdate()
{
  if (!AutoUpdateChecker::SystemSupportsAutoUpdates())
    return;

  auto* auto_update_group = new QGroupBox(tr("Auto Update"));
  auto* auto_update_group_layout = new QVBoxLayout;
  auto_update_group->setLayout(auto_update_group_layout);
  m_main_layout->addWidget(auto_update_group);

  auto* auto_update_dropdown_layout = new QFormLayout;
  auto_update_dropdown_layout->setFormAlignment(Qt::AlignLeft | Qt::AlignTop);
  auto_update_dropdown_layout->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);
  auto_update_group_layout->addLayout(auto_update_dropdown_layout);

  m_combobox_update_track = new QComboBox(this);

  m_combobox_update_track->addItem(tr("Don't Auto-Update"));
  m_combobox_update_track->addItem(tr("Release Updates"));
  m_combobox_update_track->addItem(tr("Development Updates"));

  auto_update_dropdown_layout->addRow(tr("Auto Update:"), m_combobox_update_track);

  connect(m_combobox_update_track, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
          &GeneralPane::OnSaveConfig);
}*/

#if defined(USE_ANALYTICS) && USE_ANALYTICS
void GeneralPane::CreateAnalytics()
{
  auto* analytics_group = new QGroupBox(tr("Usage Statistics Reporting Settings"));
  auto* analytics_group_layout = new QVBoxLayout;
  analytics_group->setLayout(analytics_group_layout);
  m_main_layout->addWidget(analytics_group);

  m_checkbox_enable_analytics = new QCheckBox(tr("Enable Usage Statistics Reporting"));
  m_button_generate_new_identity =
      new NonDefaultQPushButton(tr("Generate a New Statistics Identity"));
  analytics_group_layout->addWidget(m_checkbox_enable_analytics);
  analytics_group_layout->addWidget(m_button_generate_new_identity);
}
#endif

void GeneralPane::CreateCheats()
{
  auto* cheats_group = new QGroupBox(tr("Cheats Settings"));
  auto* cheats_group_layout = new QVBoxLayout;
  cheats_group->setLayout(cheats_group_layout);
  m_main_layout->addWidget(cheats_group);

  m_checkbox_cheats = new QCheckBox(tr("Enable Cheats"));
  cheats_group_layout->addWidget(m_checkbox_cheats);

  // Add dropdown for code handler selection
  auto* code_handler_layout = new QFormLayout();
  auto* code_handler_label = new QLabel(tr("Code Handler:"));
  m_combobox_codehandler = new QComboBox();
  
  m_combobox_codehandler->addItem(tr("Dolphin (Stock)"), QVariant(0));
  m_combobox_codehandler->addItem(tr("MPN (Extended)"), QVariant(1));

  code_handler_layout->addRow(code_handler_label, m_combobox_codehandler);
  cheats_group_layout->addLayout(code_handler_layout);

  // Add a label to inform users about NetPlay settings
  auto* netplay_info_label = new QLabel(tr("<b>Note:</b> Codehandler needs to be Extended if code limit is reached."));
  cheats_group_layout->addWidget(netplay_info_label);

  // Add a label to define the different code handlers
  auto* code_handler_info_label = new QLabel(tr("<b>Dolphin (Stock)</b>: Compatibility with legacy and non Dolphin-MPN builds <br>(around 3,200 bytes / 400 lines of code.)<br><br>"
    "<b>MPN (Extended)</b>: Enhanced code handler that uses hacks to give certain games<br>currently Mario Party 4, 5, 6, 7, and 8 way more code room<br>(around 30,000 bytes / 3,750 lines of codes)."));
  
  code_handler_info_label->setWordWrap(true);
  cheats_group_layout->addWidget(code_handler_info_label);

  cheats_group_layout->addSpacing(10);

  connect(m_combobox_codehandler, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &GeneralPane::OnCodeHandlerChanged);

  code_handler_layout->setFormAlignment(Qt::AlignLeft | Qt::AlignTop);
  code_handler_layout->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);
}

void GeneralPane::LoadConfig()
{
  const QSignalBlocker blocker(this);

  if (AutoUpdateChecker::SystemSupportsAutoUpdates())
  {
    const QString current_track = Settings::Instance().GetAutoUpdateTrack();
    int index = AUTO_UPDATE_DISABLE_INDEX;
    if (current_track == QString::fromStdString(AUTO_UPDATE_BETA_STRING))
      index = AUTO_UPDATE_BETA_INDEX;
    else if (current_track == QString::fromStdString(AUTO_UPDATE_DEV_STRING))
      index = AUTO_UPDATE_DEV_INDEX;
    SignalBlocking(m_combobox_update_track)->setCurrentIndex(index);
  }

#if defined(USE_ANALYTICS) && USE_ANALYTICS
  SignalBlocking(m_checkbox_enable_analytics)
      ->setChecked(Settings::Instance().IsAnalyticsEnabled());
#endif

  SignalBlocking(m_checkbox_dualcore)->setChecked(Config::Get(Config::MAIN_CPU_THREAD));
  SignalBlocking(m_checkbox_cheats)->setChecked(Settings::Instance().GetCheatsEnabled());
  SignalBlocking(m_combobox_codehandler)->setCurrentIndex(Config::Get(Config::MAIN_CODE_HANDLER));
  SignalBlocking(m_checkbox_override_region_settings)
      ->setChecked(Config::Get(Config::MAIN_OVERRIDE_REGION_SETTINGS));
  SignalBlocking(m_checkbox_auto_disc_change)
      ->setChecked(Config::Get(Config::MAIN_AUTO_DISC_CHANGE));

#ifdef USE_DISCORD_PRESENCE
  SignalBlocking(m_checkbox_discord_presence)
      ->setChecked(Config::Get(Config::MAIN_USE_DISCORD_PRESENCE));
#endif
  int selection = qRound(Config::Get(Config::MAIN_EMULATION_SPEED) * 10);
  if (selection < m_combobox_speedlimit->count())
    SignalBlocking(m_combobox_speedlimit)->setCurrentIndex(selection);

  const auto fallback = Settings::Instance().GetFallbackRegion();
  if (fallback == DiscIO::Region::NTSC_J)
    SignalBlocking(m_combobox_fallback_region)->setCurrentIndex(FALLBACK_REGION_NTSCJ_INDEX);
  else if (fallback == DiscIO::Region::NTSC_U)
    SignalBlocking(m_combobox_fallback_region)->setCurrentIndex(FALLBACK_REGION_NTSCU_INDEX);
  else if (fallback == DiscIO::Region::PAL)
    SignalBlocking(m_combobox_fallback_region)->setCurrentIndex(FALLBACK_REGION_PAL_INDEX);
  else if (fallback == DiscIO::Region::NTSC_K)
    SignalBlocking(m_combobox_fallback_region)->setCurrentIndex(FALLBACK_REGION_NTSCK_INDEX);
  else
    SignalBlocking(m_combobox_fallback_region)->setCurrentIndex(FALLBACK_REGION_NTSCJ_INDEX);
}

static QString UpdateTrackFromIndex(int index)
{
  QString value;

  switch (index)
  {
  case AUTO_UPDATE_DISABLE_INDEX:
    value = QString::fromStdString(AUTO_UPDATE_DISABLE_STRING);
    break;
  case AUTO_UPDATE_BETA_INDEX:
    value = QString::fromStdString(AUTO_UPDATE_BETA_STRING);
    break;
  case AUTO_UPDATE_DEV_INDEX:
    value = QString::fromStdString(AUTO_UPDATE_DEV_STRING);
    break;
  }

  return value;
}

static DiscIO::Region UpdateFallbackRegionFromIndex(int index)
{
  DiscIO::Region value = DiscIO::Region::Unknown;

  switch (index)
  {
  case FALLBACK_REGION_NTSCJ_INDEX:
    value = DiscIO::Region::NTSC_J;
    break;
  case FALLBACK_REGION_NTSCU_INDEX:
    value = DiscIO::Region::NTSC_U;
    break;
  case FALLBACK_REGION_PAL_INDEX:
    value = DiscIO::Region::PAL;
    break;
  case FALLBACK_REGION_NTSCK_INDEX:
    value = DiscIO::Region::NTSC_K;
    break;
  default:
    value = DiscIO::Region::NTSC_J;
  }

  return value;
}

void GeneralPane::OnSaveConfig()
{
  Config::ConfigChangeCallbackGuard config_guard;

  auto& settings = SConfig::GetInstance();
  if (AutoUpdateChecker::SystemSupportsAutoUpdates())
  {
    Settings::Instance().SetAutoUpdateTrack(
        UpdateTrackFromIndex(m_combobox_update_track->currentIndex()));
  }

#ifdef USE_DISCORD_PRESENCE
  Discord::SetDiscordPresenceEnabled(m_checkbox_discord_presence->isChecked());
#endif

#if defined(USE_ANALYTICS) && USE_ANALYTICS
  Settings::Instance().SetAnalyticsEnabled(m_checkbox_enable_analytics->isChecked());
  DolphinAnalytics::Instance().ReloadConfig();
#endif
  Config::SetBaseOrCurrent(Config::MAIN_CPU_THREAD, m_checkbox_dualcore->isChecked());
  Settings::Instance().SetCheatsEnabled(m_checkbox_cheats->isChecked());
  Config::SetBaseOrCurrent(Config::MAIN_OVERRIDE_REGION_SETTINGS,
                           m_checkbox_override_region_settings->isChecked());
  Config::SetBase(Config::MAIN_AUTO_DISC_CHANGE, m_checkbox_auto_disc_change->isChecked());
  Config::SetBaseOrCurrent(Config::MAIN_ENABLE_CHEATS, m_checkbox_cheats->isChecked());
  Settings::Instance().SetFallbackRegion(
      UpdateFallbackRegionFromIndex(m_combobox_fallback_region->currentIndex()));

  settings.SaveSettings();
}

#if defined(USE_ANALYTICS) && USE_ANALYTICS
void GeneralPane::GenerateNewIdentity()
{
  DolphinAnalytics::Instance().GenerateNewIdentity();
  DolphinAnalytics::Instance().ReloadConfig();
  ModalMessageBox message_box(this);
  message_box.setIcon(QMessageBox::Information);
  message_box.setWindowTitle(tr("Identity Generation"));
  message_box.setText(tr("New identity generated."));
  message_box.exec();
}
#endif

void GeneralPane::OnCodeHandlerChanged(int index)
{
  int code_handler_value = m_combobox_codehandler->itemData(index).toInt();
  Config::SetBaseOrCurrent(Config::MAIN_CODE_HANDLER, code_handler_value);
  Config::Save();
  static constexpr char TR_DUALCORE_DESCRIPTION[] =
      QT_TR_NOOP("Separates CPU and GPU emulation work to separate threads. Reduces single-thread "
                 "burden by spreading Dolphin's heaviest load across two cores, which usually "
                 "improves performance. However, it can result in glitches and crashes."
                 "<br><br>This setting cannot be changed while emulation is active."
                 "<br><br><dolphin_emphasis>If unsure, leave this unchecked.</dolphin_emphasis>");
  static constexpr char TR_CHEATS_DESCRIPTION[] = QT_TR_NOOP(
      "Enables the use of AR and Gecko cheat codes which can be used to modify games' behavior. "
      "These codes can be configured with the Cheats Manager in the Tools menu."
      "<br><br>This setting cannot be changed while emulation is active."
      "<br><br><dolphin_emphasis>If unsure, leave this unchecked.</dolphin_emphasis>");
  static constexpr char TR_OVERRIDE_REGION_SETTINGS_DESCRIPTION[] =
      QT_TR_NOOP("Lets you use languages and other region-related settings that the game may not "
                 "be designed for. May cause various crashes and bugs."
                 "<br><br>This setting cannot be changed while emulation is active."
                 "<br><br><dolphin_emphasis>If unsure, leave this unchecked.</dolphin_emphasis>");
  static constexpr char TR_AUTO_DISC_CHANGE_DESCRIPTION[] = QT_TR_NOOP(
      "Automatically changes the game disc when requested by games with two discs. This feature "
      "requires the game to be launched in one of the following ways:"
      "<br>- From the game list, with both discs being present in the game list."
      "<br>- With File > Open or the command line interface, with the paths to both discs being "
      "provided."
      "<br>- By launching an M3U file with File > Open or the command line interface."
      "<br><br><dolphin_emphasis>If unsure, leave this unchecked.</dolphin_emphasis>");
#ifdef USE_DISCORD_PRESENCE
  static constexpr char TR_DISCORD_PRESENCE_DESCRIPTION[] =
      QT_TR_NOOP("Shows which game is active and the duration of your current play session in your "
                 "Discord status."
                 "<br><br>This setting cannot be changed while emulation is active."
                 "<br><br><dolphin_emphasis>If unsure, leave this checked.</dolphin_emphasis>");
#endif
  static constexpr char TR_UPDATE_TRACK_DESCRIPTION[] = QT_TR_NOOP(
      "Selects which update track Dolphin uses when checking for updates at startup. If a new "
      "update is available, Dolphin will show a list of changes made since your current version "
      "and ask you if you want to update."
      "<br><br>The Dev track has the latest version of Dolphin which often updates multiple times "
      "per day. Select this track if you want the newest features and fixes."
      "<br><br>The Releases track has an update every few months. Some reasons you might prefer to "
      "use this track:"
      "<br>- You prefer using versions that have had additional testing."
      "<br>- NetPlay requires players to have the same Dolphin version, and the latest Release "
      "version will have the most players to match with."
      "<br>- You frequently use Dolphin's savestate system, which doesn't guarantee backward "
      "compatibility of savestates between Dolphin versions. If this applies to you, make sure you "
      "make an in-game save before updating (i.e. save your game in the same way you would on a "
      "physical GameCube or Wii), then load the in-game save after updating Dolphin and before "
      "making any new savestates."
      "<br><br>Selecting \"Don't Update\" will prevent Dolphin from automatically checking for "
      "updates."
      "<br><br><dolphin_emphasis>If unsure, select Releases.</dolphin_emphasis>");
  static constexpr char TR_FALLBACK_REGION_DESCRIPTION[] =
      QT_TR_NOOP("Sets the region used for titles whose region cannot be determined automatically."
                 "<br><br>This setting cannot be changed while emulation is active.");
#if defined(USE_ANALYTICS) && USE_ANALYTICS
  static constexpr char TR_ENABLE_ANALYTICS_DESCRIPTION[] = QT_TR_NOOP(
      "If selected, Dolphin can collect data on its performance, feature usage, emulated games, "
      "and configuration, as well as data on your system's hardware and operating system."
      "<br><br>No private data is ever collected. This data helps us understand how people and "
      "emulated games use Dolphin and prioritize our efforts. It also helps us identify rare "
      "configurations that are causing bugs, performance and stability issues.");
  static constexpr char TR_GENERATE_NEW_IDENTITY_DESCRIPTION[] =
      QT_TR_NOOP("Generate a new anonymous ID for your usage statistics. This will cause any "
                 "future statistics to be unassociated with your previous statistics.");
#endif

  m_checkbox_dualcore->SetDescription(tr(TR_DUALCORE_DESCRIPTION));

  m_checkbox_cheats->SetDescription(tr(TR_CHEATS_DESCRIPTION));

  m_checkbox_override_region_settings->SetDescription(tr(TR_OVERRIDE_REGION_SETTINGS_DESCRIPTION));

  m_checkbox_auto_disc_change->SetDescription(tr(TR_AUTO_DISC_CHANGE_DESCRIPTION));

#ifdef USE_DISCORD_PRESENCE
  m_checkbox_discord_presence->SetDescription(tr(TR_DISCORD_PRESENCE_DESCRIPTION));
#endif

  m_combobox_speedlimit->SetTitle(tr("Speed Limit"));

  if (AutoUpdateChecker::SystemSupportsAutoUpdates())
  {
    m_combobox_update_track->SetTitle(tr("Auto Update"));
    m_combobox_update_track->SetDescription(tr(TR_UPDATE_TRACK_DESCRIPTION));
  }

  m_combobox_fallback_region->SetTitle(tr("Fallback Region"));
  m_combobox_fallback_region->SetDescription(tr(TR_FALLBACK_REGION_DESCRIPTION));

#if defined(USE_ANALYTICS) && USE_ANALYTICS
  m_checkbox_enable_analytics->SetDescription(tr(TR_ENABLE_ANALYTICS_DESCRIPTION));

  m_button_generate_new_identity->SetTitle(tr("Generate a New Statistics Identity"));
  m_button_generate_new_identity->SetDescription(tr(TR_GENERATE_NEW_IDENTITY_DESCRIPTION));
#endif
}

void GeneralPane::UpdateDescriptionsUsingHardcoreStatus()
{
  const bool hardcore_enabled = AchievementManager::GetInstance().IsHardcoreModeActive();

  static constexpr char TR_SPEEDLIMIT_DESCRIPTION[] =
      QT_TR_NOOP("Controls how fast emulation runs relative to the original hardware."
                 "<br><br>Values higher than 100% will emulate faster than the original hardware "
                 "can run, if your hardware is able to keep up. Values lower than 100% will slow "
                 "emulation instead. Unlimited will emulate as fast as your hardware is able to."
                 "<br><br><dolphin_emphasis>If unsure, select 100%.</dolphin_emphasis>");
  static constexpr char TR_SPEEDLIMIT_RESTRICTION_IN_HARDCORE_DESCRIPTION[] =
      QT_TR_NOOP("<dolphin_emphasis>When Hardcore Mode is enabled, Speed Limit values less than "
                 "100% will be treated as 100%.</dolphin_emphasis>");

  if (hardcore_enabled)
  {
    m_combobox_speedlimit->SetDescription(
        tr("%1<br><br>%2")
            .arg(tr(TR_SPEEDLIMIT_DESCRIPTION))
            .arg(tr(TR_SPEEDLIMIT_RESTRICTION_IN_HARDCORE_DESCRIPTION)));
  }
  else
  {
    m_combobox_speedlimit->SetDescription(tr(TR_SPEEDLIMIT_DESCRIPTION));
  }
}
