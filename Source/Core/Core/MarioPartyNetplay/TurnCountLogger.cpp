// Copyright 2024 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "TurnCountLogger.h"

#include <fstream>
#include <string>

#include "Common/CommonPaths.h"
#include "Common/FileUtil.h"
#include "Common/Logging/Log.h"
#include "Core/ConfigManager.h"
#include "Core/Core.h"
#include "Core/PowerPC/MMU.h"
#include "Gamestate.h"

namespace MarioPartyNetplay
{
TurnCountLogger::TurnCountLogger() = default;
TurnCountLogger::~TurnCountLogger() = default;

void TurnCountLogger::Initialize()
{
  if (!m_initialized)
  {
    DetermineLogFilePath();
    m_initialized = true;
    NOTICE_LOG_FMT(MPN, "TurnCountLogger initialized. Log file: {}", m_log_file_path);
  }
}

void TurnCountLogger::DetermineLogFilePath()
{
  std::string user_path = File::GetUserPath(D_USER_IDX);
  
  // Check if running in portable mode by looking for portable.txt in executable directory
  std::string exe_path = File::GetExeDirectory();
  std::string portable_file = exe_path + DIR_SEP + "portable.txt";
  
  if (File::Exists(portable_file))
  {
    // Portable mode - create TurnCount.txt in executable directory
    m_log_file_path = exe_path + DIR_SEP + "TurnCount.txt";
  }
  else
  {
    // Non-portable mode - create TurnCount.txt in User directory
    m_log_file_path = user_path + "TurnCount.txt";
  }
}

void TurnCountLogger::LogTurnCount(u32 current_turn, u32 total_turns)
{
  if (!m_initialized)
    Initialize();

  std::ofstream log_file(m_log_file_path, std::ios::trunc);
  if (!log_file.is_open())
  {
    ERROR_LOG_FMT(MPN, "Failed to open log file: {}", m_log_file_path);
    return;
  }

  log_file << "Turn: " << current_turn << " / " << total_turns;
  log_file.close();
}

std::string TurnCountLogger::GetLogFilePath() const
{
  return m_log_file_path;
}

void TurnCountLogger::ClearLog()
{
  if (!m_initialized)
    Initialize();

  std::ofstream log_file(m_log_file_path, std::ios::trunc);
  if (log_file.is_open())
  {
    log_file << "Turn: 0 / 0";
    log_file.close();
  }
}

} // namespace MarioPartyNetplay 
