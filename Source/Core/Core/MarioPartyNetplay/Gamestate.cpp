/*
 *  Dolphin for Mario Party Netplay
 *  Copyright (C) 2025 Tabitha Hanegan <tabithahanegan.com>
 */

#include "Gamestate.h"
#include "Core/ConfigManager.h"
#include "Core/System.h"
#include "TurnCountLogger.h"

#include <chrono>
static auto lastTriggerTime = std::chrono::steady_clock::now();
static bool waiting = false;
static int storedSceneId = -1;  // Variable to store the previous scene ID
static MarioPartyNetplay::TurnCountLogger s_turn_count_logger;
static u32 s_last_current_turn = 0;
static u32 s_last_total_turns = 0;
mpn_state_t CurrentState;

bool mpn_init_state()
{
  auto& system = Core::System::GetInstance();
  auto& memory = system.GetMemory();
  if (!memory.IsInitialized())
    return false;

  switch (mpn_read_value(0x00000000, 4))
  {
  case MPN_GAMEID_MP4:
    CurrentState.Addresses = &MP4_ADDRESSES;
    CurrentState.Boards = MP4_BOARDS;
    CurrentState.Image = "box-mp4";
    CurrentState.IsMarioParty = true;
    CurrentState.Scenes = MP4_GAMESTATES;
    CurrentState.Title = "Mario Party 4";
    break;
  case MPN_GAMEID_MP4DX:
    CurrentState.Addresses = &MP4_ADDRESSES;
    CurrentState.Boards = MP4_BOARDS;
    CurrentState.Image = "box-mp4dx";
    CurrentState.IsMarioParty = true;
    CurrentState.Scenes = MP4_GAMESTATES;
    CurrentState.Title = "Mario Party 4";
    break;

  case MPN_GAMEID_MP5:
    CurrentState.Addresses = &MP5_ADDRESSES;
    CurrentState.Boards = MP5_BOARDS;
    CurrentState.Image = "box-mp5";
    CurrentState.IsMarioParty = true;
    CurrentState.Scenes = MP5_GAMESTATES;
    CurrentState.Title = "Mario Party 5";
    break;
  case MPN_GAMEID_MP6:
    CurrentState.Addresses = &MP6_ADDRESSES;
    CurrentState.Boards = MP6_BOARDS;
    CurrentState.Image = "box-mp6";
    CurrentState.IsMarioParty = true;
    CurrentState.Scenes = MP6_GAMESTATES;
    CurrentState.Title = "Mario Party 6";
    break;
  case MPN_GAMEID_MP7:
    CurrentState.Addresses = &MP7_ADDRESSES;
    CurrentState.Boards = MP7_BOARDS;
    CurrentState.Image = "box-mp7";
    CurrentState.IsMarioParty = true;
    CurrentState.Scenes = MP7_GAMESTATES;
    CurrentState.Title = "Mario Party 7";
    break;
  case MPN_GAMEID_MP8:
    CurrentState.Addresses = &MP8_ADDRESSES;
    CurrentState.Boards = MP8_BOARDS;
    CurrentState.Image = "box-mp8";
    CurrentState.IsMarioParty = true;
    CurrentState.Scenes = MP8_GAMESTATES;
    CurrentState.Title = "Mario Party 8";
    break;
  case MPN_GAMEID_MP9: /* TODO */
  default:
    CurrentState.Addresses = NULL;
    CurrentState.Boards = NULL;
    CurrentState.Image = "box-mp9";
    CurrentState.IsMarioParty = false;
    CurrentState.Scenes = NULL;
  }

  return CurrentState.Scenes != NULL;
}

bool mpn_update_board()
{
  uint8_t i;

  if (CurrentState.Boards == NULL)
    CurrentState.Board = NULL;
  else if (CurrentState.CurrentSceneId != CurrentState.PreviousSceneId)
  {
    for (i = 0;; i++)
    {
      /* Unknown scene ID */
      if (CurrentState.Boards[i].SceneId == NONE)
        break;
      if (CurrentState.Boards[i].SceneId == CurrentState.CurrentSceneId)
      {
        CurrentState.Board = &CurrentState.Boards[i];
        return true;
      }
    }
  }

  return false;
}

uint8_t mpn_get_needs(uint16_t StateId, bool IsSceneId)
{
  uint16_t i;

  if (CurrentState.Scenes == NULL)
    return MPN_NEEDS_NOTHING;
  else if (CurrentState.CurrentSceneId != CurrentState.PreviousSceneId)
  {
    for (i = 0;; i++)
    {
      /* Unknown scene ID */
      if (CurrentState.Scenes[i].SceneId == NONE)
        return MPN_NEEDS_NOTHING;

      /* Scene ID found in array */
      if ((IsSceneId && StateId == CurrentState.Scenes[i].SceneId) ||
          (StateId == CurrentState.Scenes[i].MiniGameId))
        return CurrentState.Scenes[i].Needs;
    }
  }

  return MPN_NEEDS_NOTHING;
}

void mpn_push_osd_message(const std::string& Message)
{
#ifdef MPN_USE_OSD
  OSD::AddMessage(Message, OSD::Duration::SHORT, MPN_OSD_COLOR);
#endif
}

bool mpn_update_state()
{
  if (CurrentState.Scenes == NULL && !mpn_init_state())
    return false;
  auto& system = Core::System::GetInstance();
  auto& memory = system.GetMemory();
  if (!memory.IsInitialized())
    return false;

  if (CurrentState.Addresses == NULL)
    return false;

  CurrentState.PreviousSceneId = CurrentState.CurrentSceneId;
  CurrentState.CurrentSceneId = mpn_read_value(CurrentState.Addresses->SceneIdAddress, 2);

  for (uint16_t i = 0;; i++)
  {
    if (CurrentState.Scenes[i].SceneId == NONE)
      break;
    if (CurrentState.CurrentSceneId == CurrentState.Scenes[i].SceneId)
    {
      CurrentState.Scene = &CurrentState.Scenes[i];
      return true;
    }
  }

  return false;
}

#define OSD_PUSH(a) mpn_push_osd_message("Adjusting #a for " + CurrentState.Scene->Name);
void mpn_per_frame()
{
  if (SConfig::GetInstance().GetGameID() == "GMPE01" ||
      SConfig::GetInstance().GetGameID() == "GP5E01" ||
      SConfig::GetInstance().GetGameID() == "GP6E01" ||
      SConfig::GetInstance().GetGameID() == "GP7E01" ||
      SConfig::GetInstance().GetGameID() == "RM8E01" ||
      SConfig::GetInstance().GetGameID() == "GMPEDX" ||
      SConfig::GetInstance().GetGameID() == "GMPDX2")
  {
    uint8_t Needs = 0;

    // Initialize turn count logger if not already done
    s_turn_count_logger.Initialize();

    if (!mpn_update_state() || CurrentState.PreviousSceneId == CurrentState.CurrentSceneId)
    {
      if (!waiting)
      {
        lastTriggerTime = std::chrono::steady_clock::now();
        storedSceneId = CurrentState.PreviousSceneId;
        waiting = true;
      }

      if (std::chrono::steady_clock::now() - lastTriggerTime < std::chrono::duration<double>(0.05))
      {
        return;
      }
      waiting = false;
    }

    mpn_update_board();
#ifdef USE_DISCORD_PRESENCE
    mpn_update_discord();
#endif

    // Log turn count changes
    if (CurrentState.IsMarioParty && CurrentState.Addresses)
    {
      u32 current_turn = mpn_read_value(CurrentState.Addresses->CurrentTurn, 1);
      u32 total_turns = mpn_read_value(CurrentState.Addresses->TotalTurns, 1);

      // Log if turn count has changed
      if (current_turn != s_last_current_turn || total_turns != s_last_total_turns)
      {
        // Log turn count in simple format
        s_turn_count_logger.LogTurnCount(current_turn, total_turns);

        s_last_current_turn = current_turn;
        s_last_total_turns = total_turns;
      }
    }

    if (CurrentState.Addresses == NULL)
      Needs = MPN_NEEDS_NOTHING;
    else
      Needs = mpn_get_needs(mpn_read_value(CurrentState.Addresses->SceneIdAddress, 2), true);

    if (Needs != MPN_NEEDS_NOTHING)
    {
      if (Needs & MPN_NEEDS_SAFE_TEX_CACHE)
      {
        OSD_PUSH(GFX_SAFE_TEXTURE_CACHE_COLOR_SAMPLES)
        Config::SetCurrent(Config::GFX_SAFE_TEXTURE_CACHE_COLOR_SAMPLES, 0);
      }
      else
        Config::SetCurrent(Config::GFX_SAFE_TEXTURE_CACHE_COLOR_SAMPLES, 128);

      if (Needs & MPN_NEEDS_NATIVE_RES)
      {
        OSD_PUSH(GFX_EFB_SCALE)
        Config::SetCurrent(Config::GFX_EFB_SCALE, 1);
      }
      else
        Config::SetCurrent(Config::GFX_EFB_SCALE, Config::GetBase(Config::GFX_EFB_SCALE));

      if (Needs & MPN_NEEDS_EFB_TO_TEXTURE)
      {
        OSD_PUSH(GFX_HACK_SKIP_EFB_COPY_TO_RAM)
        Config::SetCurrent(Config::GFX_HACK_SKIP_EFB_COPY_TO_RAM, false);
      }
      else
        Config::SetCurrent(Config::GFX_HACK_SKIP_EFB_COPY_TO_RAM, true);
      UpdateActiveConfig();
    }
  }
}

uint32_t mpn_read_value(uint32_t Address, uint8_t Size)
{
  uint32_t Value = 0;
  auto& system = Core::System::GetInstance();
  auto& memory = system.GetMemory();
  for (int8_t i = 0; i < Size; i++)
    Value += memory.GetRAM()[Address + i] * pow(256, Size - i - 1);

  return Value;
}
