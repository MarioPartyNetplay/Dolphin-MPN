// Copyright 2019 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "InputCommon/DynamicInputTextureManager.h"

#include <set>

#include "Common/CommonPaths.h"
#include "Common/FileSearch.h"
#include "Common/FileUtil.h"
#include "Core/ConfigManager.h"

#include "InputCommon/DynamicInputTextures/DITConfiguration.h"
#include "VideoCommon/HiresTextures.h"

namespace InputCommon
{
DynamicInputTextureManager::DynamicInputTextureManager() = default;

DynamicInputTextureManager::~DynamicInputTextureManager() = default;

void DynamicInputTextureManager::Load()
{
  m_configuration.clear();

  const std::string& game_id = SConfig::GetInstance().GetGameID();
  std::set<std::string> dynamic_input_directories =
      GetTextureDirectoriesWithGameId(File::GetUserPath(D_DYNAMICINPUT_IDX), game_id);

  const std::set<std::string> additional_texture_directories = GetTextureDirectoriesWithGameId(File::GetSysDirectory() + "/Load/DynamicInputTextures/", game_id); 
  dynamic_input_directories.insert(additional_texture_directories.begin(), additional_texture_directories.end());


  for (const auto& dynamic_input_directory : dynamic_input_directories)
  {
    const auto json_files = Common::DoFileSearch(dynamic_input_directory, ".json");
    for (auto& file : json_files)
    {
      m_configuration.emplace_back(file);
    }
  }
}

void DynamicInputTextureManager::GenerateTextures(const Common::IniFile& file,
                                                  const std::vector<std::string>& controller_names)
{
  for (const auto& configuration : m_configuration)
  {
    (void)configuration.GenerateTextures(file, controller_names);
  }
}
}  // namespace InputCommon
