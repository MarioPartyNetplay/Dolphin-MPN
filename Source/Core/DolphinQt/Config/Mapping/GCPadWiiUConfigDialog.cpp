// Copyright 2017 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "DolphinQt/Config/Mapping/GCPadWiiUConfigDialog.h"

#include <QCheckBox>
#include <QDialogButtonBox>
#include <QLabel>
#include <QVBoxLayout>

#include "Core/Config/MainSettings.h"
#include "Core/ConfigManager.h"
#include "DolphinQt/QtUtils/QueueOnObject.h"

#include "InputCommon/GCAdapter.h"
#include "InputCommon/ControllerInterface/ControllerInterface.h"

GCPadWiiUConfigDialog::GCPadWiiUConfigDialog(int port, QWidget* parent)
    : QDialog(parent), m_port{port}
{
  CreateLayout();

  LoadSettings();
  ConnectWidgets();
}

GCPadWiiUConfigDialog::~GCPadWiiUConfigDialog()
{
  // Clean up ControllerInterface callback if we registered one
  if (m_devices_changed_handle.has_value())
  {
    g_controller_interface.UnregisterDevicesChangedCallback(*m_devices_changed_handle);
  }
  else
  {
    // Fallback to old API cleanup
    GCAdapter::SetAdapterCallback(nullptr);
  }
}

void GCPadWiiUConfigDialog::CreateLayout()
{
  setWindowTitle(tr("GameCube Controller Adapter at Port %1").arg(m_port + 1));

  m_layout = new QVBoxLayout();
  m_status_label = new QLabel();
  m_rumble = new QCheckBox(tr("Enable Rumble"));
  m_simulate_bongos = new QCheckBox(tr("Simulate DK Bongos"));
  m_button_box = new QDialogButtonBox(QDialogButtonBox::Ok);

  // Verify all widgets were created successfully
  if (!m_layout || !m_status_label || !m_rumble || !m_simulate_bongos || !m_button_box)
  {
    // If any widget creation failed, clean up and return
    delete m_layout;
    delete m_status_label;
    delete m_rumble;
    delete m_simulate_bongos;
    delete m_button_box;
    return;
  }

  UpdateAdapterStatus();

  // Use the new ControllerInterface API for device change notifications
  if (g_controller_interface.IsInit())
  {
    m_devices_changed_handle = g_controller_interface.RegisterDevicesChangedCallback(
        [this] { QueueOnObject(this, &GCPadWiiUConfigDialog::UpdateAdapterStatus); });
  }
  else
  {
    // Fallback to old API if ControllerInterface is not available
    auto callback = [this] { QueueOnObject(this, &GCPadWiiUConfigDialog::UpdateAdapterStatus); };
    GCAdapter::SetAdapterCallback(callback);
  }

  m_layout->addWidget(m_status_label);
  m_layout->addWidget(m_rumble);
  m_layout->addWidget(m_simulate_bongos);
  m_layout->addWidget(m_button_box);

  setLayout(m_layout);
}

void GCPadWiiUConfigDialog::ConnectWidgets()
{
  // Add safety checks to prevent crashes on Windows
  if (!m_rumble || !m_simulate_bongos || !m_button_box)
    return;

  connect(m_rumble, &QCheckBox::toggled, this, &GCPadWiiUConfigDialog::SaveSettings);
  connect(m_simulate_bongos, &QCheckBox::toggled, this, &GCPadWiiUConfigDialog::SaveSettings);
  connect(m_button_box, &QDialogButtonBox::accepted, this, &GCPadWiiUConfigDialog::accept);
}

void GCPadWiiUConfigDialog::UpdateAdapterStatus()
{
  // Add safety checks to prevent crashes on Windows
  if (!m_status_label || !m_rumble || !m_simulate_bongos)
    return;

  try
  {
    // Use the new helper method to detect GC adapter
    bool detected = IsGCAdapterAvailable();
    QString status_text;
    
    // For error messages, we still need to check the old API
    if (!detected)
    {
      const char* error_message = nullptr;
      GCAdapter::IsDetected(&error_message);
      
      if (error_message)
      {
        status_text = tr("Error Opening Adapter: %1").arg(QString::fromUtf8(error_message));
      }
    }

    if (detected)
    {
      status_text = tr("Adapter Detected");
    }
    else if (status_text.isEmpty())
    {
      status_text = tr("No Adapter Detected");
    }

    m_status_label->setText(status_text);

    m_rumble->setEnabled(detected);
    m_simulate_bongos->setEnabled(detected);
  }
  catch (...)
  {
    // Handle any Windows-specific exceptions gracefully
    m_status_label->setText(tr("Error: Unable to detect adapter"));
    m_rumble->setEnabled(false);
    m_simulate_bongos->setEnabled(false);
  }
}

void GCPadWiiUConfigDialog::LoadSettings()
{
  // Add safety checks to prevent crashes on Windows
  if (!m_rumble || !m_simulate_bongos)
    return;

  m_rumble->setChecked(Config::Get(Config::GetInfoForAdapterRumble(m_port)));
  m_simulate_bongos->setChecked(Config::Get(Config::GetInfoForSimulateKonga(m_port)));
}

void GCPadWiiUConfigDialog::SaveSettings()
{
  // Add safety checks to prevent crashes on Windows
  if (!m_rumble || !m_simulate_bongos)
    return;

  Config::SetBaseOrCurrent(Config::GetInfoForAdapterRumble(m_port), m_rumble->isChecked());
  Config::SetBaseOrCurrent(Config::GetInfoForSimulateKonga(m_port), m_simulate_bongos->isChecked());
}

bool GCPadWiiUConfigDialog::IsGCAdapterAvailable() const
{
  // First try the new ControllerInterface API
  if (g_controller_interface.IsInit())
  {
    const auto& devices = g_controller_interface.GetAllDevices();
    for (const auto& device : devices)
    {
      if (device->GetSource() == "GCAdapter")
      {
        return true;
      }
    }
  }
  
  // Fallback to old API
  const char* error_message = nullptr;
  return GCAdapter::IsDetected(&error_message);
}
