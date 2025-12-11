// Copyright 2024 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "DolphinNoGUI/Platform.h"

#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#include <emscripten/html5.h>
#endif

#include <cstdio>
#include <iostream>

#include "Core/Core.h"
#include "Core/System.h"

namespace
{
class PlatformWeb final : public Platform
{
public:
  bool Init() override;
  void SetTitle(const std::string& title) override;
  void MainLoop() override;

  WindowSystemInfo GetWindowSystemInfo() const override;

private:
  static void EmscriptenMainLoop(void* arg);
};

bool PlatformWeb::Init()
{
  if (!Platform::Init())
    return false;

  std::fprintf(stdout, "PlatformWeb::Init()\n");
  return true;
}

void PlatformWeb::SetTitle(const std::string& title)
{
  std::fprintf(stdout, "Title: %s\n", title.c_str());
#ifdef __EMSCRIPTEN__
  emscripten_set_window_title(title.c_str());
#endif
}

void PlatformWeb::MainLoop()
{
#ifdef __EMSCRIPTEN__
  std::fprintf(stdout, "Starting Emscripten MainLoop\n");
  // 0 fps means use browser's requestAnimationFrame
  emscripten_set_main_loop_arg(EmscriptenMainLoop, this, 0, 1);
#else
  std::fprintf(stdout, "Not an Emscripten build. Exiting MainLoop.\n");
#endif
}

void PlatformWeb::EmscriptenMainLoop(void* arg)
{
  PlatformWeb* platform = static_cast<PlatformWeb*>(arg);

  if (!platform->IsRunning())
  {
#ifdef __EMSCRIPTEN__
    emscripten_cancel_main_loop();
#endif
    return;
  }

  platform->UpdateRunningFlag();
  Core::HostDispatchJobs(Core::System::GetInstance());
}

WindowSystemInfo PlatformWeb::GetWindowSystemInfo() const
{
  WindowSystemInfo wsi;
  // Using Headless type for now as we don't have a dedicated Web type yet
  // Ideally we would add WindowSystemType::Web
  wsi.type = WindowSystemType::Headless;
  wsi.display_connection = nullptr;
  wsi.render_window = nullptr;
  wsi.render_surface = nullptr;
  return wsi;
}

}  // namespace

std::unique_ptr<Platform> Platform::CreateWebPlatform()
{
  return std::make_unique<PlatformWeb>();
}
