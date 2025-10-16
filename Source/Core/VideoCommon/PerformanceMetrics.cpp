// Copyright 2022 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "VideoCommon/PerformanceMetrics.h"

#include <algorithm>
#include <random>
#include <vector>
#include <string>
#include <chrono>

#include <imgui.h>
#include <implot.h>

#include "Core/Config/GraphicsSettings.h"
#include "VideoCommon/VideoConfig.h"
#include "Core/MarioPartyNetplay/Gamestate.h"
#include "Core/ConfigManager.h"
#include "Common/Logging/Log.h"

PerformanceMetrics g_perf_metrics;

void PerformanceMetrics::Reset()
{
  m_fps_counter.Reset();
  m_vps_counter.Reset();

  m_time_sleeping = DT::zero();
  m_samples = {};

  m_speed = 0;
  m_max_speed = 0;
}


void PerformanceMetrics::CountFrame()
{
  m_fps_counter.Count();
}

void PerformanceMetrics::CountVBlank()
{
  m_vps_counter.Count();
}

void PerformanceMetrics::OnEmulationStateChanged([[maybe_unused]] Core::State state)
{
  m_fps_counter.InvalidateLastTime();
  m_vps_counter.InvalidateLastTime();
}

void PerformanceMetrics::CountThrottleSleep(DT sleep)
{
  m_time_sleeping += sleep;
}

void PerformanceMetrics::AdjustClockSpeed(s64 ticks, u32 new_ppc_clock, u32 old_ppc_clock)
{
  for (auto& sample : m_samples)
  {
    const s64 diff = (sample.core_ticks - ticks) * new_ppc_clock / old_ppc_clock;
    sample.core_ticks = ticks + diff;
  }
}

void PerformanceMetrics::CountPerformanceMarker(s64 core_ticks, u32 ticks_per_second)
{
  const auto clock_time = Clock::now();
  const auto work_time = clock_time - m_time_sleeping;

  m_samples.emplace_back(
      PerfSample{.clock_time = clock_time, .work_time = work_time, .core_ticks = core_ticks});

  const auto sample_window = std::chrono::microseconds{g_ActiveConfig.iPerfSampleUSec};
  while (clock_time - m_samples.front().clock_time > sample_window)
    m_samples.pop_front();

  // Avoid division by zero when we just have one sample.
  if (m_samples.size() < 2)
    return;

  const PerfSample& oldest = m_samples.front();
  const auto elapsed_core_time = DT_s(core_ticks - oldest.core_ticks) / ticks_per_second;

  m_speed.store(elapsed_core_time / (clock_time - oldest.clock_time), std::memory_order_relaxed);
  m_max_speed.store(elapsed_core_time / (work_time - oldest.work_time), std::memory_order_relaxed);
}

double PerformanceMetrics::GetFPS() const
{
  return m_fps_counter.GetHzAvg();
}

double PerformanceMetrics::GetVPS() const
{
  return m_vps_counter.GetHzAvg();
}

double PerformanceMetrics::GetSpeed() const
{
  return m_speed.load(std::memory_order_relaxed);
}

double PerformanceMetrics::GetMaxSpeed() const
{
  return m_max_speed.load(std::memory_order_relaxed);
}

void PerformanceMetrics::DrawImGuiStats(const float backbuffer_scale)
{
  m_vps_counter.UpdateStats();
  m_fps_counter.UpdateStats();

  const bool movable_overlays = Config::Get(Config::GFX_MOVABLE_PERFORMANCE_METRICS);
  const int movable_flag = movable_overlays ? ImGuiWindowFlags_None : ImGuiWindowFlags_NoMove;

  const float bg_alpha = 0.7f;
  const auto imgui_flags = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoSavedSettings |
                           ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoNav | movable_flag |
                           ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoFocusOnAppearing;

  const double fps = GetFPS();
  const double vps = GetVPS();
  const double speed = GetSpeed();

  static ImVec2 last_display_size(-1.0f, -1.0f);

  // Change Color based on % Speed
  float r = 1.0f, g = 0.55f, b = 0.00f;

  if (g_ActiveConfig.bShowSpeedColors)
  {
    r = 1.0 - (speed - 0.8) / 0.2;
    g = speed / 0.8;
    b = (speed - 0.9) / 0.1;
  }

  // Get HUD scale for consistent scaling across all performance metrics
  const float hud_scale = Config::Get(Config::GFX_MPN_HUD_SCALE);
  const float window_padding = 8.f * backbuffer_scale * hud_scale;
  const float window_width = 93.f * backbuffer_scale;

  const ImVec2& display_size = ImGui::GetIO().DisplaySize;
  const bool display_size_changed =
      display_size.x != last_display_size.x || display_size.y != last_display_size.y;
  last_display_size = display_size;
  // There are too many edge cases to reasonably handle when the display size changes, so just reset
  // the layout to default. Hopefully users aren't changing window sizes or resolutions too often.
  const ImGuiCond set_next_position_condition =
      (display_size_changed || !movable_overlays) ? ImGuiCond_Always : ImGuiCond_FirstUseEver;

  float window_y = window_padding;
  float window_x = ImGui::GetIO().DisplaySize.x - window_padding;

  const auto clamp_window_position = [&] {
    const ImVec2 position = ImGui::GetWindowPos();
    const ImVec2 size = ImGui::GetWindowSize();
    const float window_min_x = window_padding;
    const float window_max_x = display_size.x - window_padding - size.x;
    const float window_min_y = window_padding;
    const float window_max_y = display_size.y - window_padding - size.y;

    if (window_min_x > window_max_x || window_min_y > window_max_y)
      return;

    const float clamped_window_x = std::clamp(position.x, window_min_x, window_max_x);
    const float clamped_window_y = std::clamp(position.y, window_min_y, window_max_y);
    const bool window_needs_clamping =
        (clamped_window_x != position.x) || (clamped_window_y != position.y);

    if (window_needs_clamping)
      ImGui::SetWindowPos(ImVec2(clamped_window_x, clamped_window_y), ImGuiCond_Always);
  };
  
  const float graph_width = 50.f * backbuffer_scale + 3.f * window_width + 2.f * window_padding;
  const float graph_height =
      std::min(200.f * backbuffer_scale, display_size.y - 85.f * backbuffer_scale);

  const bool stack_vertically = !g_ActiveConfig.bShowGraphs;

  ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.f);
  ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 14.f * backbuffer_scale);
  if (g_ActiveConfig.bShowGraphs)
  {
    // A font size of 13 is small enough to keep the tick numbers from overlapping too much.
    ImGui::PushFont(NULL, 13.0f);
    ImGui::PushStyleColor(ImGuiCol_ResizeGrip, 0);
    const auto graph_flags = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoSavedSettings |
                             ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoNav | movable_flag |
                             ImGuiWindowFlags_NoFocusOnAppearing;
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 4.f * backbuffer_scale));

    // Position in the top-right corner of the screen.
    ImGui::SetNextWindowPos(ImVec2(window_x, window_y), set_next_position_condition,
                            ImVec2(1.0f, 0.0f));
    ImGui::SetNextWindowSize(ImVec2(graph_width, graph_height), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowBgAlpha(bg_alpha);
    if (ImGui::Begin("PerformanceGraphs", nullptr, graph_flags))
    {
      static constexpr std::size_t num_ticks = 17;
      static constexpr std::array<double, num_ticks> tick_marks = {0.0,
                                                                   1000.0 / 360.0,
                                                                   1000.0 / 240.0,
                                                                   1000.0 / 180.0,
                                                                   1000.0 / 120.0,
                                                                   1000.0 / 90.00,
                                                                   1000.0 / 59.94,
                                                                   1000.0 / 40.00,
                                                                   1000.0 / 29.97,
                                                                   1000.0 / 24.00,
                                                                   1000.0 / 20.00,
                                                                   1000.0 / 15.00,
                                                                   1000.0 / 10.00,
                                                                   1000.0 / 5.000,
                                                                   1000.0 / 2.000,
                                                                   1000.0,
                                                                   2000.0};

      clamp_window_position();
      window_y += ImGui::GetWindowHeight();

      const DT vblank_time = m_vps_counter.GetDtAvg() + 2 * m_vps_counter.GetDtStd();
      const DT frame_time = m_fps_counter.GetDtAvg() + 2 * m_fps_counter.GetDtStd();
      const double target_max_time = DT_ms(vblank_time + frame_time).count();
      const double a =
          std::max(0.0, 1.0 - std::exp(-4.0 * (DT_s(m_fps_counter.GetLastRawDt()) /
                                               DT_s(m_fps_counter.GetSampleWindow()))));

      if (std::isfinite(m_graph_max_time))
        m_graph_max_time += a * (target_max_time - m_graph_max_time);
      else
        m_graph_max_time = target_max_time;

      const double total_frame_time =
          DT_ms(std::max(m_fps_counter.GetSampleWindow(), m_vps_counter.GetSampleWindow())).count();

      if (ImPlot::BeginPlot("PerformanceGraphs", ImVec2(-1.0, -1.0),
                            ImPlotFlags_NoFrame | ImPlotFlags_NoTitle | ImPlotFlags_NoMenus |
                                ImPlotFlags_NoInputs))
      {
        ImPlot::PushStyleColor(ImPlotCol_PlotBg, {0, 0, 0, 0});
        ImPlot::PushStyleColor(ImPlotCol_LegendBg, {0, 0, 0, 0.2f});
        ImPlot::PushStyleVar(ImPlotStyleVar_FitPadding, ImVec2(0.f, 0.f));
        ImPlot::PushStyleVar(ImPlotStyleVar_LineWeight, 1.5f * backbuffer_scale);
        ImPlot::SetupAxes(nullptr, nullptr,
                          ImPlotAxisFlags_Lock | ImPlotAxisFlags_Invert |
                              ImPlotAxisFlags_NoDecorations | ImPlotAxisFlags_NoHighlight,
                          ImPlotAxisFlags_Lock | ImPlotAxisFlags_Invert | ImPlotAxisFlags_NoLabel |
                              ImPlotAxisFlags_NoHighlight);
        ImPlot::SetupAxisFormat(ImAxis_Y1, "%.1f");
        ImPlot::SetupAxisTicks(ImAxis_Y1, tick_marks.data(), num_ticks);
        ImPlot::SetupAxesLimits(0, total_frame_time, 0, m_graph_max_time, ImGuiCond_Always);
        ImPlot::SetupLegend(ImPlotLocation_SouthEast, ImPlotLegendFlags_None);
        m_vps_counter.ImPlotPlotLines("V-Blank (ms)");
        m_fps_counter.ImPlotPlotLines("Frame (ms)");
        ImPlot::EndPlot();
        ImPlot::PopStyleVar(2);
        ImPlot::PopStyleColor(2);
      }
      ImGui::PopStyleVar();
    }
    ImGui::End();
    ImGui::PopFont();
    ImGui::PopStyleColor();
  }

  if (g_ActiveConfig.bShowSpeed)
  {
    // Position in the top-right corner of the screen.
    float window_height = 47.f * backbuffer_scale * hud_scale;
    float scaled_window_width = window_width * hud_scale;

    ImGui::SetNextWindowPos(ImVec2(window_x, window_y), set_next_position_condition,
                            ImVec2(1.0f, 0.0f));
    ImGui::SetNextWindowSize(ImVec2(scaled_window_width, window_height));
    ImGui::SetNextWindowBgAlpha(bg_alpha);

    if (stack_vertically)
      window_y += window_height + window_padding;
    else
      window_x -= scaled_window_width + window_padding;

    if (ImGui::Begin("SpeedStats", nullptr, imgui_flags))
    {
      if (stack_vertically)
        window_y += ImGui::GetWindowHeight() + window_padding;
      else
        window_x -= ImGui::GetWindowWidth() + window_padding;
      clamp_window_position();
      // Scale the text size using ImGui's font scaling
      ImGui::SetWindowFontScale(hud_scale);
      ImGui::TextColored(ImVec4(r, g, b, 1.0f), "Speed:%4.0lf%%", 100.0 * speed);
      ImGui::TextColored(ImVec4(r, g, b, 1.0f), "Max:%6.0lf%%", 100.0 * GetMaxSpeed());
      ImGui::SetWindowFontScale(1.0f);  // Reset to normal scale
    }
    ImGui::End();
  }

  if (g_ActiveConfig.bShowFPS || g_ActiveConfig.bShowFTimes)
  {
    int count = g_ActiveConfig.bShowFPS + 2 * g_ActiveConfig.bShowFTimes;
    float window_height = (12.f + 17.f * count) * backbuffer_scale * hud_scale;
    float scaled_window_width = window_width * hud_scale;

    // Position in the top-right corner of the screen.
    ImGui::SetNextWindowPos(ImVec2(window_x, window_y), set_next_position_condition,
                            ImVec2(1.0f, 0.0f));
    ImGui::SetNextWindowSize(ImVec2(scaled_window_width, window_height));
    ImGui::SetNextWindowBgAlpha(bg_alpha);

    if (stack_vertically)
      window_y += window_height + window_padding;
    else
      window_x -= scaled_window_width + window_padding;

    if (ImGui::Begin("FPSStats", nullptr, imgui_flags))
    {
      if (stack_vertically)
        window_y += ImGui::GetWindowHeight() + window_padding;
      else
        window_x -= ImGui::GetWindowWidth() + window_padding;
      clamp_window_position();
      // Scale the text size using ImGui's font scaling
      ImGui::SetWindowFontScale(hud_scale);
      if (g_ActiveConfig.bShowFPS)
        ImGui::TextColored(ImVec4(r, g, b, 1.0f), "FPS:%7.2lf", fps);
      if (g_ActiveConfig.bShowFTimes)
      {
        ImGui::TextColored(ImVec4(r, g, b, 1.0f), "dt:%6.2lfms",
                           DT_ms(m_fps_counter.GetDtAvg()).count());
        ImGui::TextColored(ImVec4(r, g, b, 1.0f), " ±:%6.2lfms",
                           DT_ms(m_fps_counter.GetDtStd()).count());
      }
      ImGui::SetWindowFontScale(1.0f);  // Reset to normal scale
    }
    ImGui::End();
  }

  if (g_ActiveConfig.bShowMPTurn && CurrentState.IsMarioParty && CurrentState.Board && CurrentState.Boards)
  {
    // Apply HUD scale to the turn counter
    float window_height = (30.f) * backbuffer_scale * hud_scale;
    float scaled_window_width = window_width * hud_scale;  // Scale the width as well

    // Position in the top-left corner of the screen, below the FPS stats.
    ImGui::SetNextWindowPos(ImVec2(100.0f, window_y), ImGuiCond_FirstUseEver, ImVec2(1.0f, 0.0f));
    ImGui::SetNextWindowSize(ImVec2(scaled_window_width, window_height));
    ImGui::SetNextWindowBgAlpha(bg_alpha);
   
    if (ImGui::Begin("MPStats", nullptr, imgui_flags))
    {
      if (g_ActiveConfig.bShowMPTurn && CurrentState.IsMarioParty && CurrentState.Board && CurrentState.Boards)
      {
        // Use safe turn reading function
        if (g_ActiveConfig.bShowMPTurn && CurrentState.IsMarioParty && CurrentState.Board &&
            CurrentState.Boards)
        {
          // Scale the text size using ImGui's font scaling
          ImGui::SetWindowFontScale(hud_scale);
          ImGui::TextColored(ImVec4(r, g, b, 1.0f), "Turn: %d/%d",
                             mpn_read_value(CurrentState.Addresses->CurrentTurn, 1),
                             mpn_read_value(CurrentState.Addresses->TotalTurns, 1));
          ImGui::SetWindowFontScale(1.0f);  // Reset to normal scale
        }
        else
        {
          // Show "Loading..." or similar when turn info isn't available
          ImGui::SetWindowFontScale(hud_scale);
          ImGui::TextColored(ImVec4(r, g, b, 0.7f), "Turn: Loading...");
          ImGui::SetWindowFontScale(1.0f);  // Reset to normal scale
        }
      }
      ImGui::End();
    }
  }

  // Task Master Mode HUD Display - Single draggable widget in bottom-left
  if (g_ActiveConfig.bMPNTaskMasterMode && CurrentState.IsMarioParty && 
      (SConfig::GetInstance().GetGameID() == "GMPE01" || SConfig::GetInstance().GetGameID() == "GMPEDX"))
  {
    // Apply HUD scale to the Task Master widget
    float window_height = (30.f * 4 + 20.f) * backbuffer_scale * hud_scale;  // 4 players + padding
    float scaled_window_width = window_width * hud_scale;
    
    // Define bottom-left position
    const ImVec2& display_size = ImGui::GetIO().DisplaySize;
    const float corner_padding = 20.f * backbuffer_scale * hud_scale;
    
    // Player colors for each player
    const ImVec4 player_colors[4] = {
      ImVec4(1.0f, 0.2f, 0.2f, 1.0f),  // Red - Player 1
      ImVec4(0.2f, 0.2f, 1.0f, 1.0f),  // Blue - Player 2
      ImVec4(0.2f, 1.0f, 0.2f, 1.0f),  // Green - Player 3
      ImVec4(1.0f, 1.0f, 0.2f, 1.0f)   // Yellow - Player 4
    };
    
    const char* player_names[4] = {"P1", "P2", "P3", "P4"};
    
    // Static player scores (starting at 0)
    static int player_scores[4] = {0, 0, 0, 0};
    
    // Task management
    static std::string current_task = "Not started";
    static int last_board_scene_id = -1;
    static bool task_generated = false;
    static auto task_completion_time = std::chrono::steady_clock::now();
    static bool task_completed = false;
    
    // Task checking variables
    static int last_coin_counts[4] = {0, 0, 0, 0};
    static auto last_coin_check_time = std::chrono::steady_clock::now();
    static bool coin_task_active = false;
    static int coin_gains_this_window[4] = {0, 0, 0, 0}; // Track cumulative gains in 2-second window
    
    // Check if we're in an active game session using total turns
    // TotalTurns = 0xFF means no active game, anything else means we're in a game
    bool in_active_game = false;
    if (CurrentState.Addresses && CurrentState.Addresses->TotalTurns != 0)
    {
      int total_turns = mpn_read_value(CurrentState.Addresses->TotalTurns, 1);
      in_active_game = (total_turns != 0xFF);
    }
    
    // Also check if we're on a board (scene IDs 0x59-0x61 for MP4 boards) for additional safety
    bool on_board = false;
    if (CurrentState.CurrentSceneId >= 0x59 && CurrentState.CurrentSceneId <= 0x61)
    {
      on_board = true;
    }
    
    // Use active game status as the primary indicator
    bool game_active = in_active_game || on_board;
    
    // Generate new task when entering a board OR after task completion
    bool should_generate_task = false;
    
    if (on_board && CurrentState.CurrentSceneId != last_board_scene_id)
    {
      should_generate_task = true;
      last_board_scene_id = CurrentState.CurrentSceneId;
    }
    else if (task_completed && on_board)
    {
      // Check if 3 seconds have passed since task completion
      auto current_time = std::chrono::steady_clock::now();
      auto time_since_completion = std::chrono::duration_cast<std::chrono::milliseconds>(current_time - task_completion_time);
      if (time_since_completion.count() >= 3000) // 3 seconds
      {
        should_generate_task = true;
      }
    }
    
    if (should_generate_task)
    {      
      // Use the correct coin addresses for Mario Party 4
      static const uint32_t coin_addresses[4] = {0x0018FC54, 0x0018FC84, 0x0018FCB4, 0x0018FCE4};
      
      // Random task list for Mario Party 4
      static const std::vector<std::string> tasks = {
        "Collect 3 coins",
        "Collect 5 coins",
        "Collect 10 coins",
        "Collect 15 coins",
        "Collect 20 coins",
        "Collect 25 coins",
        "Collect 30 coins",
        "Collect 35 coins",
        "Collect 40 coins",
        "Collect 50 coins",
        "Collect 69 coins",
        "Lose 3 coins",
        "Lose 5 coins",
        "Lose 10 coins",
        "Lose 15 coins",
        "Lose 20 coins",
        "Lose 25 coins",
        "Lose 30 coins",
        "Lose 35 coins",
        "Lose 40 coins",
        "Lose 50 coins",
        "Lose 69 coins"
      };
      
      // Read current coin counts to validate task achievability
      int current_coin_counts[4];
      for (int i = 0; i < 4; i++)
      {
        current_coin_counts[i] = mpn_read_value(coin_addresses[i], 2);
      }
      
      // Filter tasks to only include achievable ones
      std::vector<std::string> achievable_tasks;
      for (const auto& task : tasks)
      {
        bool is_achievable = true;
        
        // Extract target amount from task
        int target_amount = 3; // Default
        size_t coins_pos = task.find(" coins");
        if (coins_pos != std::string::npos) {
          size_t num_start = coins_pos;
          while (num_start > 0 && std::isdigit(task[num_start - 1])) {
            num_start--;
          }
          if (num_start < coins_pos) {
            std::string num_str = task.substr(num_start, coins_pos - num_start);
            target_amount = std::atoi(num_str.c_str());
          }
        }
        
        if (task.find("Lose") != std::string::npos)
        {
          // For lose tasks, check if ANY player has enough coins to lose
          // Since everyone gets the same task, only one player needs to be able to complete it
          bool any_player_can_lose = false;
          for (int i = 0; i < 4; i++)
          {
            if (current_coin_counts[i] >= target_amount)
            {
              any_player_can_lose = true;
              break;
            }
          }
          is_achievable = any_player_can_lose;
        }
        else if (task.find("Collect") != std::string::npos)
        {
          // For collect tasks, check if ANY player can gain that many coins (not at 999 limit)
          // Since everyone gets the same task, only one player needs to be able to complete it
          bool any_player_can_gain = false;
          for (int i = 0; i < 4; i++)
          {
            if (current_coin_counts[i] <= (999 - target_amount))
            {
              any_player_can_gain = true;
              break;
            }
          }
          is_achievable = any_player_can_gain;
        }
        
        if (is_achievable)
        {
          achievable_tasks.push_back(task);
        }
      }
      
      // Generate random task from achievable tasks only
      if (!achievable_tasks.empty())
      {
        static std::random_device rd;
        static std::mt19937 gen(rd());
        std::uniform_int_distribution<> dis(0, achievable_tasks.size() - 1);
        current_task = achievable_tasks[dis(gen)];
      }
      else
      {
        // Fallback: all players are at extremes (0 coins or 999 coins), use a safe task
        current_task = "Collect 1 coin"; // Always achievable
      }
      
      task_generated = true;
      task_completed = false;
      
      // Reset coin gains for new task and set proper baseline
      for (int i = 0; i < 4; i++)
      {
        coin_gains_this_window[i] = 0;
        // Set baseline to current coin counts so we only track gains from this point forward
        last_coin_counts[i] = current_coin_counts[i];
      }
      
    }
    
    // Only reset task when completely leaving the game (main menu, etc.)
    // Keep task active during minigames and other board-related scenes
    if (!game_active) // Reset only when not in an active game
    {
      current_task = "Not started";
      task_generated = false;
      task_completed = false;
      coin_task_active = false;
      // Reset cumulative gains
      for (int i = 0; i < 4; i++)
      {
        coin_gains_this_window[i] = 0;
      }
    }
    
    // Check if current task involves coin collection or loss
    bool is_coin_task = (current_task.find("Collect") != std::string::npos || 
                        current_task.find("Lose") != std::string::npos) && 
                       current_task.find("coins") != std::string::npos;
    bool is_lose_task = current_task.find("Lose") != std::string::npos;
    
    // Extract target amount from task string using robust parsing
    int target_amount = 3; // Default
    if (is_coin_task) {
      // Try to extract number before "coins"
      size_t coins_pos = current_task.find(" coins");
      if (coins_pos != std::string::npos) {
        // Find the start of the number
        size_t num_start = coins_pos;
        while (num_start > 0 && std::isdigit(current_task[num_start - 1])) {
          num_start--;
        }
        if (num_start < coins_pos) {
          std::string num_str = current_task.substr(num_start, coins_pos - num_start);
          // Simple conversion without exceptions
          target_amount = std::atoi(num_str.c_str());
          if (target_amount <= 0) {
            target_amount = 3; // Fallback to default if invalid
          }
        }
      }
    }
    
    
    
    // Task checking logic - simplified to prevent lockups
    if (game_active && task_generated)
    {
      auto current_time = std::chrono::steady_clock::now();
      auto time_diff = std::chrono::duration_cast<std::chrono::milliseconds>(current_time - last_coin_check_time);
      
      // Check every 2 seconds for more responsive coin tracking
      if (time_diff.count() >= 2000)
      {
        // Use the correct coin addresses for Mario Party 4
        static const uint32_t coin_addresses[4] = {0x0018FC54, 0x0018FC84, 0x0018FCB4, 0x0018FCE4};
        
        // Read current coin counts
        int current_coin_counts[4];
        for (int i = 0; i < 4; i++)
        {
          current_coin_counts[i] = mpn_read_value(coin_addresses[i], 2);
        }
        
        // Track which players completed the task this round
        bool task_completed_this_round = false;
        std::vector<int> players_who_completed;
        
        // Calculate coin changes by comparing with previous counts
        for (int i = 0; i < 4; i++)
        {
          int coin_change = current_coin_counts[i] - last_coin_counts[i];
          
          // Handle both gain and loss tasks
          if (is_lose_task && coin_change < 0)
          {
            // For "Lose X coins" tasks, track losses (negative changes)
            coin_gains_this_window[i] += (-coin_change); // Convert negative to positive for tracking
          }
          else if (!is_lose_task && coin_change > 0)
          {
            // For "Collect X coins" tasks, track gains (positive changes)
            coin_gains_this_window[i] += coin_change;
          }
          
          // Check for task completion
          if (coin_gains_this_window[i] >= target_amount)
          {
            // Validate that the task was actually achievable
            bool task_was_achievable = false;
            
            if (is_lose_task)
            {
              // For lose tasks, check if player had enough coins to lose
              // We need to check the baseline coin count at task start
              int baseline_coins = last_coin_counts[i] - coin_gains_this_window[i]; // Reconstruct baseline
              if (baseline_coins >= target_amount)
              {
                task_was_achievable = true;
              }
            }
            else
            {
              // For collect tasks, check if player could actually gain that many coins
              // Check if current coins + target_amount would exceed 999 limit
              if (current_coin_counts[i] <= (999 - target_amount))
              {
                task_was_achievable = true;
              }
            }
            
            // Only award points if the task was actually achievable
            if (task_was_achievable)
            {
              // Award points to the player who completed the task
              player_scores[i] += 1; // Award 1 point for completing a task
              players_who_completed.push_back(i);
              task_completed_this_round = true;
            }
            else
            {
              // Task was impossible, reset progress and continue
              coin_gains_this_window[i] = 0;
            }
          }
          // Update the last coin counts for next comparison
          last_coin_counts[i] = current_coin_counts[i];
        }
        
        // If any players completed the task, mark it as completed and reset
        if (task_completed_this_round)
        {
          // Mark task as completed and set timer for new task
          current_task = "COMPLETED! New task in 3 seconds...";
          task_completed = true;
          task_completion_time = std::chrono::steady_clock::now();
          
          // Reset all coin gains for new task and update baseline to current coin counts
          for (int j = 0; j < 4; j++)
          {
            coin_gains_this_window[j] = 0;
            last_coin_counts[j] = current_coin_counts[j]; // Update baseline to current coin count
          }
        }
        
        coin_task_active = true;
        last_coin_check_time = current_time;
      }
    }
    
    // Position in bottom-left corner, draggable
    ImGui::SetNextWindowPos(ImVec2(corner_padding, display_size.y - corner_padding), ImGuiCond_FirstUseEver, ImVec2(0.0f, 1.0f));
    ImGui::SetNextWindowSize(ImVec2(scaled_window_width, window_height), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowBgAlpha(bg_alpha);
    
    // Make it draggable and resizable
    const auto taskmaster_flags = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoSavedSettings |
                                 ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoNav |
                                 ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoFocusOnAppearing;
    
    if (ImGui::Begin("TaskMasterWidget", nullptr, taskmaster_flags))
    {
      ImGui::SetWindowFontScale(hud_scale);
      
      // Header
      ImGui::TextColored(ImVec4(1.0f, 1.0f, 1.0f, 1.0f), "Task Master");
      ImGui::Separator();
      
      // Current Task
      if (current_task.find("COMPLETED") != std::string::npos)
      {
        ImGui::TextColored(ImVec4(0.0f, 1.0f, 0.0f, 1.0f), "Current Task: %s", current_task.c_str());
      }
      else
      {
        ImGui::TextColored(ImVec4(0.8f, 0.8f, 0.8f, 1.0f), "Current Task: %s", current_task.c_str());
      }
      ImGui::Separator();
      
      
      if (is_coin_task && coin_task_active)
      {
        // Grid layout for coin progress
        if (is_lose_task)
        {
          ImGui::TextColored(ImVec4(0.9f, 0.9f, 0.9f, 1.0f), "Coin Loss Progress:");
        }
        else
        {
          ImGui::TextColored(ImVec4(0.9f, 0.9f, 0.9f, 1.0f), "Coin Progress:");
        }
        
        // Create a 2x2 grid for the 4 players
        if (ImGui::BeginTable("CoinProgress", 2, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg))
        {
          // Top row: P1 and P2
          ImGui::TableNextRow();
          ImGui::TableNextColumn();
          ImGui::TextColored(player_colors[0], "P1: %d/%d", coin_gains_this_window[0], target_amount);
          ImGui::TableNextColumn();
          ImGui::TextColored(player_colors[1], "P2: %d/%d", coin_gains_this_window[1], target_amount);
          
          // Bottom row: P3 and P4
          ImGui::TableNextRow();
          ImGui::TableNextColumn();
          ImGui::TextColored(player_colors[2], "P3: %d/%d", coin_gains_this_window[2], target_amount);
          ImGui::TableNextColumn();
          ImGui::TextColored(player_colors[3], "P4: %d/%d", coin_gains_this_window[3], target_amount);
          
          ImGui::EndTable();
        }
        ImGui::Separator();
      }
      
      // Display scores when there's an active task (always show scores during tasks)
      if (task_generated)
      {
        // Display all 4 players stacked vertically with scores
        ImGui::TextColored(ImVec4(0.9f, 0.9f, 0.9f, 1.0f), "Scores:");
        
        // Create a 2x2 grid for the scores
        if (ImGui::BeginTable("Scores", 2, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg))
        {
          // Top row: P1 and P2
          ImGui::TableNextRow();
          ImGui::TableNextColumn();
          ImGui::TextColored(player_colors[0], "P1: %d pts", player_scores[0]);
          ImGui::TableNextColumn();
          ImGui::TextColored(player_colors[1], "P2: %d pts", player_scores[1]);
          
          // Bottom row: P3 and P4
          ImGui::TableNextRow();
          ImGui::TableNextColumn();
          ImGui::TextColored(player_colors[2], "P3: %d pts", player_scores[2]);
          ImGui::TableNextColumn();
          ImGui::TextColored(player_colors[3], "P4: %d pts", player_scores[3]);
          
          ImGui::EndTable();
        }
      }
      
      ImGui::SetWindowFontScale(1.0f);
      ImGui::End();
    }
  }

  if (g_ActiveConfig.bShowVPS || g_ActiveConfig.bShowVTimes)
  {
    int count = g_ActiveConfig.bShowVPS + 2 * g_ActiveConfig.bShowVTimes;
    float window_height = (12.f + 17.f * count) * backbuffer_scale * hud_scale;
    float scaled_window_width = window_width * hud_scale;

    // Position in the top-right corner of the screen.
    ImGui::SetNextWindowPos(ImVec2(window_x, window_y), set_next_position_condition,
                            ImVec2(1.0f, 0.0f));
    ImGui::SetNextWindowSize(ImVec2(scaled_window_width, window_height));
    ImGui::SetNextWindowBgAlpha(bg_alpha);

    if (stack_vertically)
      window_y += window_height + window_padding;
    else
      window_x -= scaled_window_width + window_padding;

    if (ImGui::Begin("VPSStats", nullptr, imgui_flags))
    {
      if (stack_vertically)
        window_y += ImGui::GetWindowHeight() + window_padding;
      else
        window_x -= ImGui::GetWindowWidth() + window_padding;
      clamp_window_position();
      // Scale the text size using ImGui's font scaling
      ImGui::SetWindowFontScale(hud_scale);
      if (g_ActiveConfig.bShowVPS)
        ImGui::TextColored(ImVec4(r, g, b, 1.0f), "VPS:%7.2lf", vps);
      if (g_ActiveConfig.bShowVTimes)
      {
        ImGui::TextColored(ImVec4(r, g, b, 1.0f), "dt:%6.2lfms",
                           DT_ms(m_vps_counter.GetDtAvg()).count());
        ImGui::TextColored(ImVec4(r, g, b, 1.0f), " ±:%6.2lfms",
                           DT_ms(m_vps_counter.GetDtStd()).count());
      }
      ImGui::SetWindowFontScale(1.0f);  // Reset to normal scale
    }
    ImGui::End();
  }

  ImGui::PopStyleVar(2);
}
