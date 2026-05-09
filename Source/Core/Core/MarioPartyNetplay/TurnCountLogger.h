// Copyright 2024 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <string>

#include "Common/CommonTypes.h"

namespace MarioPartyNetplay
{
/**
 * TurnCountLogger - A utility class for logging turn count data to a TurnCount.txt file.
 * 
 * This class handles writing turn count data to a log file in a simple format:
 * "Turn: CurrentTurn / TotalTurns"
 * 
 * The log file is created either in the executable directory (if running in portable mode)
 * or in the User directory (if not portable).
 */
class TurnCountLogger
{
public:
  TurnCountLogger();
  ~TurnCountLogger();

  /**
   * Initialize the logger and determine the log file path
   */
  void Initialize();

  /**
   * Log turn count values in simple format
   * 
   * @param current_turn The current turn number
   * @param total_turns The total number of turns
   */
  void LogTurnCount(u32 current_turn, u32 total_turns);

  /**
   * Get the current log file path
   * 
   * @return The path to the log file
   */
  std::string GetLogFilePath() const;

  /**
   * Clear the log file and reset to "Turn: 0 / 0"
   */
  void ClearLog();

private:
  /**
   * Determine the appropriate log file path based on portable mode
   */
  void DetermineLogFilePath();

  std::string m_log_file_path;
  bool m_initialized = false;
};

} // namespace MarioPartyNetplay 