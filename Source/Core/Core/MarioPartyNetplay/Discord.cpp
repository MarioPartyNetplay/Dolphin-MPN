/*
*  Dolphin for Mario Party Netplay
*  Copyright (C) 2025 Tabitha Hanegan <tabithahanegan.com>
*/

#ifdef USE_DISCORD_PRESENCE
#include "Discord.h"
#include <Core/State.h>
#include "Core/Config/NetplaySettings.h"
#include "Core/Core.h"
#include "Core/IOS/DolphinDevice.h"
#include <Core/State.h>
#include "Core/System.h"
#include "Common/Logging/Log.h"

static int previousSceneId = -1;
static bool hasSaved = false; 

bool mpn_update_discord()
{
  DiscordRichPresence RichPresence = {};

  RichPresence.largeImageKey = CurrentState.Image ? CurrentState.Image : "default";
  RichPresence.largeImageText = CurrentState.Title ? CurrentState.Title : "In-Game";

  if (CurrentState.Scenes != NULL && CurrentState.Scene != NULL)
    RichPresence.state = CurrentState.Scene->Name.c_str();

  int gameID = mpn_read_value(0x00000000, 4);
  int sceneValue = -1;
  bool shouldSave = false;

  if (gameID == MPN_GAMEID_MP4)
  {
    sceneValue = mpn_read_value(0x001D3CE3, 1);
    shouldSave = (sceneValue == 0x4E);
  }
  else if (gameID == MPN_GAMEID_MP5)
  {
    sceneValue = mpn_read_value(0x00288863, 1);
    shouldSave = (sceneValue == 0x69);
  }
  else if (gameID == MPN_GAMEID_MP6)
  {
    sceneValue = mpn_read_value(0x002C0257, 1);
    shouldSave = (sceneValue == 0x5C);
  }
  else if (gameID == MPN_GAMEID_MP7)
  {
    sceneValue = mpn_read_value(0x002F2F3F, 1);
    shouldSave = (sceneValue == 0x1);
  }

  
  if (shouldSave) {
    // Check if the Scene ID hasn't changed and we haven't already saved
    if (previousSceneId == CurrentState.CurrentSceneId && !hasSaved) {
        // Scene ID hasn't changed, and we haven't saved yet, so save the state
        State::Save(Core::System::GetInstance(), 1);
        hasSaved = true;  // Mark as saved to prevent further saves until conditions change
    }
    
    // Store the current Scene ID for future checks
    previousSceneId = CurrentState.CurrentSceneId;
  } else {
      // Reset the save flag if conditions change (optional)
      hasSaved = false;
  }

  if (CurrentState.Addresses != NULL)
  {

    // Add controller port values
    int value1 = mpn_read_value(CurrentState.Addresses->ControllerPortAddress1, 1);
    int value2 = mpn_read_value(CurrentState.Addresses->ControllerPortAddress2, 1);
    int value3 = mpn_read_value(CurrentState.Addresses->ControllerPortAddress3, 1);
    int value4 = mpn_read_value(CurrentState.Addresses->ControllerPortAddress4, 1);

    // Modify values based on the given conditions
    value1 = (value1 == 0) ? 1 : 0;
    value2 = (value2 == 0) ? 1 : 0;
    value3 = (value3 == 0) ? 1 : 0;
    value4 = (value4 == 0) ? 1 : 0;

    int controller_port_sum = value1 + value2 + value3 + value4;

    char Details[128] = "";

    if (CurrentState.Boards && CurrentState.Board)
    {
      snprintf(Details, sizeof(Details), "Players: %d/4 Turn: %d/%d", controller_port_sum,
               mpn_read_value(CurrentState.Addresses->CurrentTurn, 1),
               mpn_read_value(CurrentState.Addresses->TotalTurns, 1));

      RichPresence.smallImageKey = CurrentState.Board->Icon.c_str();
      RichPresence.smallImageText = CurrentState.Board->Name.c_str();
    }
    else
    {
      snprintf(Details, sizeof(Details), "Players: %d/4", controller_port_sum);
      RichPresence.smallImageKey = "";
      RichPresence.smallImageText = "";
    }
    RichPresence.details = Details;
  }
  else
  {
    // Handle the case where CurrentState.Addresses is NULL
    RichPresence.details = "Invalid state: Addresses are NULL";
    RichPresence.smallImageKey = "";
    RichPresence.smallImageText = "";
  }

  RichPresence.startTimestamp = std::time(nullptr);
  Discord_UpdatePresence(&RichPresence);

  return true;
}
#endif
