// Copyright 2009 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "VideoCommon/HiresTextures.h"

#include <algorithm>
#include <memory>
#include <mutex>
#include <string>
#include <string_view>
#include <thread>
#include <unordered_map>
#include <utility>
#include <vector>
#include <xxhash.h>
#include <algorithm>
#include <string>
#include <set>
#include <filesystem>
#include <iostream>
#include <fmt/format.h>

#include "Common/CommonPaths.h"
#include "Common/FileSearch.h"
#include "Common/FileUtil.h"
#include "Common/Logging/Log.h"
#include "Common/StringUtil.h"
#include "Core/Config/GraphicsSettings.h"
#include "Core/ConfigManager.h"
#include "Core/System.h"
#include "VideoCommon/Assets/CustomAsset.h"
#include "VideoCommon/Assets/DirectFilesystemAssetLibrary.h"
#include "VideoCommon/OnScreenDisplay.h"
#include "VideoCommon/Resources/CustomResourceManager.h"
#include "VideoCommon/VideoConfig.h"

constexpr std::string_view s_format_prefix{"tex1_"};

static std::unordered_map<std::string, std::shared_ptr<HiresTexture>> s_hires_texture_cache;
static std::unordered_map<std::string, bool> s_hires_texture_id_to_arbmipmap;

static auto s_file_library = std::make_shared<VideoCommon::DirectFilesystemAssetLibrary>();

namespace
{
std::pair<std::string, bool> GetNameArbPair(const TextureInfo& texture_info)
{
  if (s_hires_texture_id_to_arbmipmap.empty())
    return {"", false};

  const auto texture_name_details = texture_info.CalculateTextureName();
  // look for an exact match first
  const std::string full_name = texture_name_details.GetFullName();
  if (auto iter = s_hires_texture_id_to_arbmipmap.find(full_name);
      iter != s_hires_texture_id_to_arbmipmap.end())
  {
    return {full_name, iter->second};
  }

  // Single wildcard ignoring the tlut hash
  const std::string texture_name_single_wildcard_tlut =
      fmt::format("{}_{}_$_{}", texture_name_details.base_name, texture_name_details.texture_name,
                  texture_name_details.format_name);
  if (auto iter = s_hires_texture_id_to_arbmipmap.find(texture_name_single_wildcard_tlut);
      iter != s_hires_texture_id_to_arbmipmap.end())
  {
    return {texture_name_single_wildcard_tlut, iter->second};
  }

  // Single wildcard ignoring the texture hash
  const std::string texture_name_single_wildcard_tex =
      fmt::format("{}_${}_{}", texture_name_details.base_name, texture_name_details.tlut_name,
                  texture_name_details.format_name);
  if (auto iter = s_hires_texture_id_to_arbmipmap.find(texture_name_single_wildcard_tex);
      iter != s_hires_texture_id_to_arbmipmap.end())
  {
    return {texture_name_single_wildcard_tex, iter->second};
  }

  return {"", false};
}
}  // namespace

void HiresTexture::Shutdown()
{
  Clear();
}

bool ContainsGeneratedInFileName(const std::filesystem::path& filePath) {
  std::string filename = filePath.filename().string();
  
  // Convert to lowercase for case-insensitive comparison
  std::transform(filename.begin(), filename.end(), filename.begin(), ::tolower);
  
  // Check if "generated" exists in the filename
  return filename.find("generated") != std::string::npos;
}

// Function to check if a directory can be accessed (if it's valid)
bool CanAccessDirectory(const std::filesystem::path& dir) {
  return std::filesystem::exists(dir) && std::filesystem::is_directory(dir);
}

void HiresTexture::Update()
{
    const std::string& game_id = SConfig::GetInstance().GetGameID();
    std::set<std::string> texture_directories =
        GetTextureDirectoriesWithGameId(File::GetUserPath(D_HIRESTEXTURES_IDX), game_id);

    const std::set<std::string> additional_texture_directories =
        GetTextureDirectoriesWithGameId(File::GetSysDirectory() + "/Load/Textures/", game_id);

    texture_directories.insert(additional_texture_directories.begin(), additional_texture_directories.end());

    if (!g_ActiveConfig.bHiresTextures1)
    {
      // Iterate over texture_directories and remove directories containing "Generated" in their name
      for (auto it = texture_directories.begin(); it != texture_directories.end(); ) {
          bool contains_generated = false;

          // Log the current directory being checked
          ERROR_LOG_FMT(VIDEO, "Checking directory: '{}'", *it);

          // Check if the directory can be accessed
          if (!CanAccessDirectory(*it)) {
              // Log warning for inaccessible directory
              ERROR_LOG_FMT(VIDEO, "Warning: Unable to access directory: '{}'", *it);
              it = texture_directories.erase(it);  // Remove the directory and move to the next
              continue;  // Skip to the next iteration
          }

          // Check if the directory name itself contains "generated"
          if (ContainsGeneratedInFileName(*it)) {
              contains_generated = true;
          }

          // Remove the directory if it contains "Generated" in the directory name
          if (contains_generated) {
              ERROR_LOG_FMT(VIDEO, "Warning: Directory contains 'Generated' in its name and will be removed: '{}'", *it);
              it = texture_directories.erase(it);  // Erase and move to the next directory
          } else {
              ++it;  // Move to the next directory if it's valid
          }
      }
    }
    // After the loop, print all remaining directories in texture_directories
    ERROR_LOG_FMT(VIDEO, "Remaining texture directories:");
    for (const auto& dir : texture_directories) {
        ERROR_LOG_FMT(VIDEO, "  '{}'", dir);  // Prints each remaining directory
    }

    const std::vector<std::string> extensions{".png", ".dds"};

    for (const auto& texture_directory : texture_directories)
    {
        const auto texture_paths =
            Common::DoFileSearch({texture_directory}, extensions, /*recursive*/ true);

        bool failed_insert = false;
        for (auto& path : texture_paths)
        {
            std::string filename;
            SplitPath(path, nullptr, &filename, nullptr);

            if (filename.substr(0, s_format_prefix.length()) == s_format_prefix)
            {
                const size_t arb_index = filename.rfind("_arb");
                const bool has_arbitrary_mipmaps = arb_index != std::string::npos;
                if (has_arbitrary_mipmaps)
                    filename.erase(arb_index, 4);

                const auto [it, inserted] =
                    s_hires_texture_id_to_arbmipmap.try_emplace(filename, has_arbitrary_mipmaps);
                if (!inserted)
                {
                    failed_insert = true;
                }
                else
                {
                    // Map the texture data
                    s_file_library->SetAssetIDMapData(filename, std::map<std::string, std::filesystem::path>{
                                                            {"texture", StringToPath(path)}});

                    // Cache the texture if necessary
                    if (g_ActiveConfig.bCacheHiresTextures)
                    {
                      auto hires_texture =
                          std::make_shared<HiresTexture>(has_arbitrary_mipmaps, std::move(filename));
                      static_cast<void>(hires_texture->LoadTexture());
                      s_hires_texture_cache.try_emplace(hires_texture->GetId(), hires_texture);                       
                    }
                    
                }
            }
        }

        // Log if any textures failed to be inserted
        if (failed_insert)
        {
            ERROR_LOG_FMT(VIDEO, "One or more textures at path '{}' were already inserted", texture_directory);
        }
    }

    // Display message depending on whether textures are being cached or not
    if (g_ActiveConfig.bCacheHiresTextures)
    {
        OSD::AddMessage(fmt::format("Loading '{}' custom textures", s_hires_texture_cache.size()), 10000);
    }
    else
    {
        OSD::AddMessage(fmt::format("Found '{}' custom textures", s_hires_texture_id_to_arbmipmap.size()), 10000);
    }
}

void HiresTexture::Clear()
{
  s_hires_texture_cache.clear();
  s_hires_texture_id_to_arbmipmap.clear();
  s_file_library = std::make_shared<VideoCommon::DirectFilesystemAssetLibrary>();
}

std::shared_ptr<HiresTexture> HiresTexture::Search(const TextureInfo& texture_info)
{
  auto [base_filename, has_arb_mipmaps] = GetNameArbPair(texture_info);
  if (base_filename == "")
    return nullptr;

  if (auto iter = s_hires_texture_cache.find(base_filename); iter != s_hires_texture_cache.end())
  {
    return iter->second;
  }
  else
  {
    auto hires_texture = std::make_shared<HiresTexture>(has_arb_mipmaps, std::move(base_filename));
    if (g_ActiveConfig.bCacheHiresTextures)
    {
      s_hires_texture_cache.try_emplace(hires_texture->GetId(), hires_texture);
    }
    return hires_texture;
  }
}

HiresTexture::HiresTexture(bool has_arbitrary_mipmaps, std::string id)
    : m_has_arbitrary_mipmaps(has_arbitrary_mipmaps), m_id(std::move(id))
{
}

VideoCommon::TextureDataResource* HiresTexture::LoadTexture() const
{
  auto& system = Core::System::GetInstance();
  auto& custom_resource_manager = system.GetCustomResourceManager();
  return custom_resource_manager.GetTextureDataFromAsset(m_id, s_file_library);
}

std::set<std::string> GetTextureDirectoriesWithGameId(const std::string& root_directory,
                                                      const std::string& game_id)
{
  std::set<std::string> result;
  const std::string texture_directory = root_directory + game_id;

  if (File::Exists(texture_directory))
  {
    result.insert(texture_directory);
  }
  else
  {
    // If there's no directory with the region-specific ID, look for a 3-character region-free one
    const std::string region_free_directory = root_directory + game_id.substr(0, 3);

    if (File::Exists(region_free_directory))
    {
      result.insert(region_free_directory);
    }
  }

  const auto match_gameid_or_all = [game_id](const std::string& filename) {
    std::string basename;
    SplitPath(filename, nullptr, &basename, nullptr);
    return basename == game_id || basename == game_id.substr(0, 3) || basename == "all";
  };

  // Look for any other directories that might be specific to the given gameid
  const auto files = Common::DoFileSearch({root_directory}, {".txt"}, true);
  for (const auto& file : files)
  {
    if (match_gameid_or_all(file))
    {
      // The following code is used to calculate the top directory
      // of a found gameid.txt file
      // ex:  <root directory>/My folder/gameids/<gameid>.txt
      // would insert "<root directory>/My folder"
      const auto directory_path = file.substr(root_directory.size());
      const std::size_t first_path_separator_position = directory_path.find_first_of(DIR_SEP_CHR);
      result.insert(root_directory + directory_path.substr(0, first_path_separator_position));
    }
  }

  return result;
}
