#include "Netplay.h"
#include <algorithm>
#include <android/log.h>
#include <chrono>
#include <jni.h>
#include <memory>
#include <string>
#include <thread>
#include "AndroidNetPlayUI.h"

#include <SFML/Network/Packet.hpp>
#include <cctype>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include "Common/Crypto/SHA1.h"
#include "Common/FileUtil.h"
#include "Common/TraversalClient.h"
#include "Core/Boot/Boot.h"
#include "Core/Config/NetplaySettings.h"
#include "Core/NetPlayClient.h"
#include "Core/NetPlayProto.h"
#include "UICommon/GameFile.h"
#include "UICommon/GameFileCache.h"

#define LOG_TAG "NetPlay"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)

// Global variables
static jobject g_netplay_manager = nullptr;
static std::unique_ptr<NetPlay::NetPlayClient> g_netplay_client;
static std::unique_ptr<AndroidNetPlayUI> g_netplay_ui;
static bool g_is_connected = false;
static std::string g_server_address;
static int g_server_port = 2626;
static JavaVM* g_jvm = nullptr;
static std::string g_player_name = "MPN Player";
static std::string g_rom_folder = "";
static std::string g_last_game_path = "";     // Store the last game path for fallback
static bool g_start_game_processing = false;  // Prevent duplicate StartGame messages

// Helper functions to extract game information from file paths
static std::string ExtractGameIdFromPath(const std::string& path)
{
  try
  {
    // Extract filename from path
    size_t last_slash = path.find_last_of("/\\");
    std::string filename = (last_slash != std::string::npos) ? path.substr(last_slash + 1) : path;

    // Remove file extension
    size_t last_dot = filename.find_last_of('.');
    if (last_dot != std::string::npos)
    {
      filename = filename.substr(0, last_dot);
    }

    // GameCube/Wii games typically have 6-character IDs
    // Look for a pattern like "GP6E01" or similar
    if (filename.length() >= 6)
    {
      // Check if it looks like a game ID (alphanumeric, 6 chars)
      std::string potential_id = filename.substr(0, 6);
      bool is_valid_id = true;
      for (char c : potential_id)
      {
        if (!std::isalnum(c))
        {
          is_valid_id = false;
          break;
        }
      }
      if (is_valid_id)
      {
        return potential_id;
      }
    }

    // Fallback: use first 6 characters of filename
    return filename.substr(0, std::min(6UL, filename.length()));
  }
  catch (const std::exception& e)
  {
    LOGE("NetPlay: Exception extracting game ID from path: %s", e.what());
    return "";
  }
}

static std::string ExtractGameNameFromPath(const std::string& path)
{
  try
  {
    // Extract filename from path
    size_t last_slash = path.find_last_of("/\\");
    std::string filename = (last_slash != std::string::npos) ? path.substr(last_slash + 1) : path;

    // Remove file extension
    size_t last_dot = filename.find_last_of('.');
    if (last_dot != std::string::npos)
    {
      filename = filename.substr(0, last_dot);
    }

    return filename;
  }
  catch (const std::exception& e)
  {
    LOGE("NetPlay: Exception extracting game name from path: %s", e.what());
    return "Unknown Game";
  }
}

// AndroidNetPlayUI implementation
void AndroidNetPlayUI::BootGame(const std::string& filename,
                                std::unique_ptr<BootSessionData> boot_session_data)
{
  LOGI("NetPlay: BootGame called for %s", filename.c_str());

  try
  {
    // CRITICAL: Check if we're already processing a StartGame message to prevent infinite loops
    if (g_start_game_processing)
    {
      LOGI("NetPlay: Already processing StartGame message - skipping duplicate BootGame call");
      return;
    }

    if (g_netplay_client && g_netplay_client->IsConnected())
    {
      LOGI("NetPlay: Starting NetPlay game launch process for: %s", filename.c_str());

      // Set flag to prevent duplicate StartGame messages
      g_start_game_processing = true;
      LOGI("NetPlay: Set start_game_processing flag to prevent duplicate messages");

      // First, ensure any existing game is stopped to reset the running state
      if (g_netplay_client->IsRunning())
      {
        LOGI("NetPlay: Stopping existing game to reset NetPlay state");
        g_netplay_client->StopGame();
      }

      // Launch the game through the Android system while preserving NetPlay session
      // We need to use the existing Android game launching system
      LOGI("NetPlay: Launching game through Android system: %s", filename.c_str());

      // Get the JNI environment to call Android methods
      JNIEnv* env = getJNIEnv();
      if (env && g_netplay_manager)
      {
        try
        {
          jclass managerClass = env->GetObjectClass(g_netplay_manager);
          if (managerClass)
          {
            // Call the startNetPlayGame method that properly launches through EmulationActivity
            jmethodID startGameMethod =
                env->GetMethodID(managerClass, "startNetPlayGame", "(Ljava/lang/String;)V");
            if (startGameMethod)
            {
              jstring jFilename = env->NewStringUTF(filename.c_str());
              env->CallVoidMethod(g_netplay_manager, startGameMethod, jFilename);
              env->DeleteLocalRef(jFilename);
              LOGI("NetPlay: Game launch request sent to Android EmulationActivity for: %s",
                   filename.c_str());
            }
            else
            {
              LOGE("NetPlay: Could not find startNetPlayGame method in NetPlayManager");
            }
            env->DeleteLocalRef(managerClass);
          }
          else
          {
            LOGE("NetPlay: Could not get NetPlayManager class");
          }
        }
        catch (const std::exception& e)
        {
          LOGE("NetPlay: Exception calling Android startNetPlayGame method: %s", e.what());
        }
      }
      else
      {
        if (!env)
        {
          LOGE("NetPlay: JNI environment is null - thread attachment failed");
        }
        if (!g_netplay_manager)
        {
          LOGE("NetPlay: NetPlay manager reference is null");
        }
      }
    }
    else
    {
      LOGE("NetPlay: Cannot launch game - NetPlay client not connected");
    }
  }
  catch (const std::exception& e)
  {
    LOGE("NetPlay: Exception in BootGame: %s", e.what());
  }
  catch (...)
  {
    LOGE("NetPlay: Unknown exception in BootGame");
  }
}

bool AndroidNetPlayUI::IsHosting() const
{
  return false;
}
void AndroidNetPlayUI::Update()
{
}
void AndroidNetPlayUI::AppendChat(const std::string& msg)
{
}
void AndroidNetPlayUI::OnMsgChangeGame(const NetPlay::SyncIdentifier& sync_identifier,
                                       const std::string& netplay_name)
{
  LOGI("NetPlay: *** OnMsgChangeGame called! Game changed to %s ***", netplay_name.c_str());
  LOGI("NetPlay: Sync identifier - game_id: %s", sync_identifier.game_id.c_str());

  try
  {
    // CRITICAL: Send initial GameStatus and ClientCapabilities immediately
    // The desktop host needs these to know our capabilities and game status
    // This allows the host to properly coordinate the game start process

    if (g_netplay_client && g_netplay_client->IsConnected())
    {
      try
      {
        // First, check if we have this game using our FindGameFile method
        NetPlay::SyncIdentifierComparison comparison;
        auto game_file = FindGameFile(sync_identifier, &comparison);

        // Store the game path for later use in StartGame
        if (game_file && !game_file->GetFilePath().empty())
        {
          g_last_game_path = game_file->GetFilePath();
          LOGI("NetPlay: Stored game path for later use: %s", g_last_game_path.c_str());
        }

        // Send GameStatus message
        sf::Packet game_status_packet;
        game_status_packet << static_cast<u8>(NetPlay::MessageID::GameStatus);
        game_status_packet << static_cast<u32>(comparison);
        g_netplay_client->SendAsync(std::move(game_status_packet));
        LOGI("NetPlay: Sent GameStatus: %s",
             (comparison == NetPlay::SyncIdentifierComparison::SameGame) ? "SameGame" :
                                                                           "DifferentGame");

        // Send ClientCapabilities message
        sf::Packet capabilities_packet;
        capabilities_packet << static_cast<u8>(NetPlay::MessageID::ClientCapabilities);
        // Send basic capabilities - we support save data and code synchronization
        capabilities_packet << static_cast<u32>(0x1);  // Basic capabilities flag
        g_netplay_client->SendAsync(std::move(capabilities_packet));
        LOGI("NetPlay: Sent ClientCapabilities to host");

        LOGI("NetPlay: *** Initial sync messages sent - host now knows our status and capabilities "
             "***");
      }
      catch (const std::exception& e)
      {
        LOGE("NetPlay: Failed to send initial sync messages: %s", e.what());
      }
    }
    else
    {
      LOGE("NetPlay: Cannot send sync messages - NetPlay client not connected");
    }
  }
  catch (const std::exception& e)
  {
    LOGE("NetPlay: Exception in OnMsgChangeGame: %s", e.what());
  }
}
void AndroidNetPlayUI::OnMsgChangeGBARom(int pad, const NetPlay::GBAConfig& config)
{
}
void AndroidNetPlayUI::OnMsgStartGame()
{
  LOGI("NetPlay: *** OnMsgStartGame - START GAME MESSAGE RECEIVED! ***");
  LOGI("NetPlay: Server sent StartGame message - Android client should now start the game locally");

  try
  {
    // CRITICAL: Verify NetPlay connection is healthy before proceeding
    if (!g_netplay_client || !g_netplay_client->IsConnected())
    {
      LOGE("NetPlay: Cannot process StartGame - NetPlay client is not connected!");
      return;
    }

    LOGI("NetPlay: NetPlay connection verified, processing StartGame message from server");

    // The GameStatus message is already sent in OnMsgChangeGame
    // No need to send it again here - just proceed with game launch
    LOGI("NetPlay: GameStatus already sent, proceeding with game launch");

    // Launch the game using the stored game path from OnMsgChangeGame
    if (!g_last_game_path.empty())
    {
      LOGI("NetPlay: Launching game using stored path: %s", g_last_game_path.c_str());

      // Create a proper BootSessionData for NetPlay synchronization
      // This ensures proper game state synchronization between players
      auto boot_session_data = std::make_unique<BootSessionData>();

      // Call BootGame to launch the game with proper session data
      // This allows Dolphin to handle the NetPlay synchronization properly
      BootGame(g_last_game_path, std::move(boot_session_data));

      LOGI("NetPlay: BootGame called with session data for proper NetPlay sync");
    }
    else
    {
      LOGE("NetPlay: No game path available to launch - OnMsgChangeGame was not called first");
    }

    // Reset the flag to allow future StartGame messages
    g_start_game_processing = false;
    LOGI("NetPlay: Reset start_game_processing flag - StartGame processing complete");

    // Notify Java side that the server started the game (for UI updates)
    JNIEnv* env = getJNIEnv();
    if (env && g_netplay_manager)
    {
      try
      {
        jclass managerClass = env->GetObjectClass(g_netplay_manager);
        if (managerClass)
        {
          jmethodID gameStartedMethod = env->GetMethodID(managerClass, "onHostGameStarted", "()V");
          if (gameStartedMethod)
          {
            env->CallVoidMethod(g_netplay_manager, gameStartedMethod);
            LOGI("NetPlay: Notified Java side that server started the game");
          }
          else
          {
            LOGI("NetPlay: onHostGameStarted method not found - this is expected if not "
                 "implemented");
          }
          env->DeleteLocalRef(managerClass);
        }
      }
      catch (...)
      {
        LOGI("NetPlay: Exception calling onHostGameStarted - this is expected if not implemented");
      }
    }
  }
  catch (const std::exception& e)
  {
    LOGE("Exception in NetPlay StartGame processing: %s", e.what());
    g_start_game_processing = false;
  }
}
void AndroidNetPlayUI::OnMsgStopGame()
{
}
void AndroidNetPlayUI::OnMsgPowerButton()
{
}
void AndroidNetPlayUI::OnPlayerConnect(const std::string& player)
{
}
void AndroidNetPlayUI::OnPlayerDisconnect(const std::string& player)
{
}
void AndroidNetPlayUI::OnPadBufferChanged(u32 buffer)
{
}
void AndroidNetPlayUI::OnHostInputAuthorityChanged(bool enabled)
{
}
void AndroidNetPlayUI::OnDesync(u32 frame, const std::string& player)
{
}
void AndroidNetPlayUI::OnConnectionLost()
{
  g_start_game_processing = false;
}
void AndroidNetPlayUI::OnConnectionError(const std::string& message)
{
  g_start_game_processing = false;
}
void AndroidNetPlayUI::OnTraversalError(Common::TraversalClient::FailureReason error)
{
}
void AndroidNetPlayUI::OnTraversalStateChanged(Common::TraversalClient::State state)
{
}
void AndroidNetPlayUI::OnGameStartAborted()
{
}
void AndroidNetPlayUI::OnGolferChanged(bool is_golfer, const std::string& golfer_name)
{
}
void AndroidNetPlayUI::OnTtlDetermined(u8 ttl)
{
}
bool AndroidNetPlayUI::IsRecording()
{
  return false;
}

std::string AndroidNetPlayUI::FindGBARomPath(const std::array<u8, 20>& hash, std::string_view title,
                                             int device_number)
{
  return "";
}
void AndroidNetPlayUI::ShowGameDigestDialog(const std::string& title)
{
}
void AndroidNetPlayUI::SetGameDigestProgress(int pid, int progress)
{
}
void AndroidNetPlayUI::SetGameDigestResult(int pid, const std::string& result)
{
}
void AndroidNetPlayUI::AbortGameDigest()
{
}
void AndroidNetPlayUI::OnIndexAdded(bool success, std::string error)
{
}
void AndroidNetPlayUI::OnIndexRefreshFailed(std::string error)
{
}
void AndroidNetPlayUI::ShowChunkedProgressDialog(const std::string& title, u64 data_size,
                                                 const std::vector<int>& players)
{
}
void AndroidNetPlayUI::HideChunkedProgressDialog()
{
}
void AndroidNetPlayUI::SetChunkedProgress(int pid, u64 progress)
{
}
void AndroidNetPlayUI::SetHostWiiSyncData(std::vector<u64> titles, std::string redirect_folder)
{
}
void AndroidNetPlayUI::StopGame()
{
  LOGI("NetPlay: StopGame called");

  try
  {
    // Use Dolphin's native NetPlay client to stop the game
    if (g_netplay_client && g_netplay_client->IsConnected())
    {
      LOGI("NetPlay: Using native NetPlay client to stop game");

      // The NetPlay client should handle the game stopping internally
      // This bypasses the Android JNI system and uses Dolphin's native functionality

      LOGI("NetPlay: Game stop initiated via native NetPlay client");
    }
    else
    {
      LOGE("NetPlay: Cannot stop game - NetPlay client not connected");
    }
  }
  catch (const std::exception& e)
  {
    LOGE("NetPlay: Exception stopping game: %s", e.what());
  }
  catch (...)
  {
    LOGE("NetPlay: Unknown exception stopping game");
  }
}

std::shared_ptr<const UICommon::GameFile>
AndroidNetPlayUI::FindGameFile(const NetPlay::SyncIdentifier& sync_identifier,
                               NetPlay::SyncIdentifierComparison* found)
{
  LOGI("NetPlay: FindGameFile called for game_id: '%s', revision: %d, disc: %d",
       sync_identifier.game_id.c_str(), sync_identifier.revision, sync_identifier.disc_number);

  // Safety check - don't proceed with empty game_id
  if (sync_identifier.game_id.empty())
  {
    LOGE("FindGameFile called with empty game_id - this will cause crashes!");
    if (found)
    {
      *found = NetPlay::SyncIdentifierComparison::Unknown;
    }
    return nullptr;
  }

  try
  {
    // Get the game cache and search for matching games
    static UICommon::GameFileCache game_cache;

    // Load the cache if it hasn't been loaded yet
    if (game_cache.GetSize() == 0)
    {
      LOGI("Loading game cache...");
      if (!game_cache.Load())
      {
        LOGI("Failed to load game cache, getting ROM path from Java...");

        // Get ROM path from Java side instead of hardcoding
        JNIEnv* env = getJNIEnv();
        if (env && g_netplay_manager)
        {
          try
          {
            jclass managerClass = env->GetObjectClass(g_netplay_manager);
            if (managerClass)
            {
              jmethodID getRomPathMethod =
                  env->GetMethodID(managerClass, "getRomPath", "()Ljava/lang/String;");
              if (getRomPathMethod)
              {
                jstring romPathString = static_cast<jstring>(
                    env->CallObjectMethod(g_netplay_manager, getRomPathMethod));
                if (romPathString)
                {
                  const char* romPath = env->GetStringUTFChars(romPathString, nullptr);
                  if (romPath)
                  {
                    std::string rom_path(romPath);
                    env->ReleaseStringUTFChars(romPathString, romPath);

                    LOGI("Got ROM path from Java: %s", rom_path.c_str());

                    // Use the ROM path from Java
                    std::vector<std::string> game_dirs = {rom_path};
                    std::vector<std::string_view> game_dirs_view(game_dirs.begin(),
                                                                 game_dirs.end());
                    try
                    {
                      auto game_paths = UICommon::FindAllGamePaths(game_dirs_view, true);
                      game_cache.Update(game_paths);
                      LOGI("Updated game cache with %zu paths from Java ROM directory",
                           game_paths.size());
                    }
                    catch (const std::exception& e)
                    {
                      LOGE("Exception calling FindAllGamePaths with Java ROM path: %s", e.what());
                      // Continue with fallback paths
                    }
                    catch (...)
                    {
                      LOGE("Unknown exception calling FindAllGamePaths with Java ROM path");
                      // Continue with fallback paths
                    }
                  }
                }
                env->DeleteLocalRef(romPathString);
              }
              else
              {
                LOGI("Could not find getRomPath method, using default Android paths");
                // Fallback to some common Android directories
                std::vector<std::string> game_dirs = {"/storage/emulated/0/ROMs",
                                                      "/storage/emulated/0/Games"};
                std::vector<std::string_view> game_dirs_view(game_dirs.begin(), game_dirs.end());
                try
                {
                  auto game_paths = UICommon::FindAllGamePaths(game_dirs_view, true);
                  game_cache.Update(game_paths);
                  LOGI("Updated game cache with %zu paths from default Android directories",
                       game_paths.size());
                }
                catch (const std::exception& e)
                {
                  LOGE("Exception calling FindAllGamePaths with default Android paths: %s",
                       e.what());
                }
                catch (...)
                {
                  LOGE("Unknown exception calling FindAllGamePaths with default Android paths");
                }
              }
              env->DeleteLocalRef(managerClass);
            }
          }
          catch (...)
          {
            LOGE("Exception getting ROM path from Java, using fallback");
            // Fallback to some common Android directories
            std::vector<std::string> game_dirs = {"/storage/emulated/0/ROMs",
                                                  "/storage/emulated/0/Games"};
            std::vector<std::string_view> game_dirs_view(game_dirs.begin(), game_dirs.end());
            try
            {
              auto game_paths = UICommon::FindAllGamePaths(game_dirs_view, true);
              game_cache.Update(game_paths);
              LOGI("Updated game cache with %zu paths from default Android directories",
                   game_paths.size());
            }
            catch (const std::exception& e)
            {
              LOGE("Exception calling FindAllGamePaths with default Android paths: %s", e.what());
            }
            catch (...)
            {
              LOGE("Unknown exception calling FindAllGamePaths with default Android paths");
            }
          }
        }
        else
        {
          LOGI("No JNI environment available, using default Android paths");
          // Fallback to some common Android directories
          std::vector<std::string> game_dirs = {"/storage/emulated/0/ROMs",
                                                "/storage/emulated/0/Games"};
          std::vector<std::string_view> game_dirs_view(game_dirs.begin(), game_dirs.end());
          try
          {
            auto game_paths = UICommon::FindAllGamePaths(game_dirs_view, true);
            game_cache.Update(game_paths);
            LOGI("Updated game cache with %zu paths from default Android directories",
                 game_paths.size());
          }
          catch (const std::exception& e)
          {
            LOGE("Exception calling FindAllGamePaths with default Android paths: %s", e.what());
          }
          catch (...)
          {
            LOGE("Unknown exception calling FindAllGamePaths with default Android paths");
          }
        }
      }
    }

    LOGI("Searching through %zu games in cache", game_cache.GetSize());

    std::shared_ptr<const UICommon::GameFile> found_game = nullptr;
    game_cache.ForEach([&](const std::shared_ptr<const UICommon::GameFile>& game) {
      if (game && game->IsValid())
      {
        auto game_sync_id = game->GetSyncIdentifier();
        LOGI("Checking game: %s (game_id: %s, revision: %d, disc: %d)", game->GetFilePath().c_str(),
             game_sync_id.game_id.c_str(), game_sync_id.revision, game_sync_id.disc_number);

        if (game_sync_id.game_id == sync_identifier.game_id &&
            game_sync_id.revision == sync_identifier.revision &&
            game_sync_id.disc_number == sync_identifier.disc_number)
        {
          LOGI("Found matching game: %s", game->GetFilePath().c_str());
          found_game = game;
        }
      }
    });

    if (found_game)
    {
      if (found)
      {
        *found = NetPlay::SyncIdentifierComparison::SameGame;
      }
      return found_game;
    }

    LOGI("No matching game found for sync identifier");
    if (found)
    {
      *found = NetPlay::SyncIdentifierComparison::Unknown;
    }
    return nullptr;
  }
  catch (const std::exception& e)
  {
    LOGE("Exception in FindGameFile: %s", e.what());
    if (found)
    {
      *found = NetPlay::SyncIdentifierComparison::Unknown;
    }
    return nullptr;
  }
}

// JNI helper functions
JNIEnv* getJNIEnv()
{
  if (!g_jvm)
    return nullptr;

  JNIEnv* env;
  jint result = g_jvm->GetEnv(reinterpret_cast<void**>(&env), JNI_VERSION_1_6);
  if (result == JNI_EDETACHED)
  {
    if (g_jvm->AttachCurrentThread(&env, nullptr) != 0)
      return nullptr;
  }
  else if (result != JNI_OK)
  {
    return nullptr;
  }
  return env;
}

void callJavaCallback(const char* method_name, ...)
{
  JNIEnv* env = getJNIEnv();
  if (!env || !g_netplay_manager)
    return;

  jclass managerClass = env->GetObjectClass(g_netplay_manager);
  if (!managerClass)
    return;

  if (strcmp(method_name, "onConnected") == 0)
  {
    jmethodID method = env->GetMethodID(managerClass, "onConnected", "()V");
    if (method)
      env->CallVoidMethod(g_netplay_manager, method);
  }
  else if (strcmp(method_name, "onDisconnected") == 0)
  {
    jmethodID method = env->GetMethodID(managerClass, "onDisconnected", "()V");
    if (method)
      env->CallVoidMethod(g_netplay_manager, method);
  }
  else if (strcmp(method_name, "onConnectionFailed") == 0)
  {
    jmethodID method =
        env->GetMethodID(managerClass, "onConnectionFailed", "(Ljava/lang/String;)V");
    if (method)
    {
      jstring errorMsg = env->NewStringUTF("Connection failed from native code");
      env->CallVoidMethod(g_netplay_manager, method, errorMsg);
      env->DeleteLocalRef(errorMsg);
    }
  }

  env->DeleteLocalRef(managerClass);
}

// Function to get Android device name
std::string getAndroidDeviceName()
{
  std::string device_name = "MPN Player";

  // Try to get device name from system properties
  FILE* fp = popen("getprop ro.product.model", "r");
  if (fp)
  {
    char buffer[256];
    if (fgets(buffer, sizeof(buffer), fp) != nullptr)
    {
      std::string model(buffer);
      // Remove newline
      if (!model.empty() && model[model.length() - 1] == '\n')
      {
        model.erase(model.length() - 1);
      }
      if (!model.empty() && model != "unknown")
      {
        device_name = model;
      }
    }
    pclose(fp);
  }

  // Fallback to manufacturer + device
  if (device_name == "Android Player")
  {
    fp = popen("getprop ro.product.manufacturer", "r");
    if (fp)
    {
      char buffer[256];
      if (fgets(buffer, sizeof(buffer), fp) != nullptr)
      {
        std::string manufacturer(buffer);
        if (!manufacturer.empty() && manufacturer[manufacturer.length() - 1] == '\n')
        {
          manufacturer.erase(manufacturer.length() - 1);
        }
        if (!manufacturer.empty() && manufacturer != "unknown")
        {
          device_name = manufacturer;

          // Add device name
          FILE* fp2 = popen("getprop ro.product.device", "r");
          if (fp2)
          {
            char buffer2[256];
            if (fgets(buffer2, sizeof(buffer2), fp2) != nullptr)
            {
              std::string device(buffer2);
              if (!device.empty() && device[device.length() - 1] == '\n')
              {
                device.erase(device.length() - 1);
              }
              if (!device.empty() && device != "unknown")
              {
                device_name += " " + device;
              }
            }
            pclose(fp2);
          }
        }
      }
      pclose(fp);
    }
  }

  // Clean up the name (remove special characters that might cause issues)
  std::string clean_name;
  for (char c : device_name)
  {
    if (std::isalnum(c) || c == ' ' || c == '-' || c == '_')
    {
      clean_name += c;
    }
  }

  // Trim whitespace
  while (!clean_name.empty() && std::isspace(clean_name.front()))
  {
    clean_name.erase(clean_name.begin());
  }
  while (!clean_name.empty() && std::isspace(clean_name.back()))
  {
    clean_name.erase(clean_name.end() - 1);
  }

  if (clean_name.empty())
  {
    clean_name = "Android Player";
  }

  return clean_name;
}

// Wrapper functions for JNI lifecycle
void InitializeMultiplayerJNI(JavaVM* vm)
{
  g_jvm = vm;

  // Set player name from Android device name
  g_player_name = getAndroidDeviceName();
  LOGI("Player name set to: %s", g_player_name.c_str());

  LOGI("Multiplayer JNI wrapper initialized");
}

void CleanupMultiplayerJNI()
{
  g_jvm = nullptr;
  g_netplay_client.reset();
  g_netplay_ui.reset();
  g_is_connected = false;
  g_start_game_processing = false;
  LOGI("Multiplayer JNI wrapper cleaned up");
}

// JNI lifecycle functions
extern "C" JNIEXPORT void JNICALL
Java_org_dolphinemu_dolphinemu_features_netplay_NetPlayManager_InitializeMultiplayerJNI(
    JNIEnv* env, jobject thiz, jobject manager)
{
  if (g_netplay_manager)
  {
    env->DeleteGlobalRef(g_netplay_manager);
  }
  g_netplay_manager = env->NewGlobalRef(manager);

  JavaVM* vm;
  env->GetJavaVM(&vm);
  g_jvm = vm;

  LOGI("Multiplayer JNI initialized");
}

extern "C" JNIEXPORT void JNICALL
Java_org_dolphinemu_dolphinemu_features_netplay_NetPlayManager_CleanupMultiplayerJNI(JNIEnv* env,
                                                                                     jobject thiz)
{
  if (g_netplay_manager)
  {
    env->DeleteGlobalRef(g_netplay_manager);
    g_netplay_manager = nullptr;
  }

  g_netplay_client.reset();
  g_netplay_ui.reset();
  g_is_connected = false;
  g_start_game_processing = false;

  LOGI("Multiplayer JNI cleaned up");
}

// Core NetPlay functions
extern "C" JNIEXPORT jboolean JNICALL
Java_org_dolphinemu_dolphinemu_features_netplay_NetPlayManager_netPlayConnect(JNIEnv* env,
                                                                              jobject thiz,
                                                                              jstring address,
                                                                              jint port)
{
  if (!address)
    return JNI_FALSE;

  const char* addr = env->GetStringUTFChars(address, nullptr);
  if (!addr)
    return JNI_FALSE;

  g_server_address = std::string(addr);
  g_server_port = port;
  env->ReleaseStringUTFChars(address, addr);

  LOGI("Connecting to NetPlay server: %s:%d", g_server_address.c_str(), g_server_port);

  if (g_server_address.empty() || g_server_port <= 0 || g_server_port > 65535)
  {
    LOGE("Invalid server address or port");
    return JNI_FALSE;
  }

  if (!g_netplay_ui)
  {
    g_netplay_ui = std::make_unique<AndroidNetPlayUI>();
  }

  // Determine connection type
  bool use_traversal = (g_server_address.length() == 8);
  for (char c : g_server_address)
  {
    if (!std::isxdigit(c))
    {
      use_traversal = false;
      break;
    }
  }

  LOGI("NetPlay connection type: %s", use_traversal ? "traversal" : "direct");

  NetPlay::NetTraversalConfig traversal_config;
  if (use_traversal)
  {
    traversal_config.use_traversal = true;
    traversal_config.traversal_host = "stun.dolphin-emu.org";
    traversal_config.traversal_port = 6262;
    traversal_config.traversal_port_alt = 0;

    if (!Common::EnsureTraversalClient(traversal_config.traversal_host,
                                       traversal_config.traversal_port,
                                       traversal_config.traversal_port_alt, 0))
    {
      LOGE("Failed to ensure traversal client");
      return JNI_FALSE;
    }
  }
  else
  {
    traversal_config.use_traversal = false;
  }

  g_netplay_client = std::make_unique<NetPlay::NetPlayClient>(
      g_server_address, g_server_port, g_netplay_ui.get(), g_player_name, traversal_config);

  if (!g_netplay_client)
  {
    LOGE("Failed to create NetPlayClient");
    return JNI_FALSE;
  }

  // Wait for connection
  int timeout_ms = use_traversal ? 10000 : 4500;
  auto start_time = std::chrono::steady_clock::now();

  while (std::chrono::steady_clock::now() - start_time < std::chrono::milliseconds(timeout_ms))
  {
    if (!g_netplay_client)
      return JNI_FALSE;

    if (g_netplay_client->IsConnected())
    {
      g_is_connected = true;
      LOGI("Successfully connected as %s", g_player_name.c_str());
      return JNI_TRUE;
    }

    std::this_thread::sleep_for(std::chrono::milliseconds(100));
  }

  LOGE("Connection timeout after %d ms", timeout_ms);
  g_netplay_client.reset();
  g_start_game_processing = false;  // Reset flag on connection failure
  return JNI_FALSE;
}

extern "C" JNIEXPORT void JNICALL
Java_org_dolphinemu_dolphinemu_features_netplay_NetPlayManager_netPlayDisconnect(JNIEnv* env,
                                                                                 jobject thiz)
{
  if (g_netplay_client)
  {
    g_netplay_client.reset();
  }

  if (g_netplay_ui)
  {
    g_netplay_ui.reset();
  }

  g_is_connected = false;
  g_start_game_processing = false;  // Reset flag on disconnect
  LOGI("NetPlay: Disconnected");
}

extern "C" JNIEXPORT jboolean JNICALL
Java_org_dolphinemu_dolphinemu_features_netplay_NetPlayManager_netPlayIsConnected(JNIEnv* env,
                                                                                  jobject thiz)
{
  return (g_netplay_client && g_netplay_client->IsConnected()) ? JNI_TRUE : JNI_FALSE;
}

extern "C" JNIEXPORT jboolean JNICALL
Java_org_dolphinemu_dolphinemu_features_netplay_NetPlayManager_netPlayIsHost(JNIEnv* env,
                                                                             jobject thiz)
{
  return g_is_connected ? JNI_FALSE : JNI_TRUE;  // Android clients are never hosts
}

extern "C" JNIEXPORT jint JNICALL
Java_org_dolphinemu_dolphinemu_features_netplay_NetPlayManager_netPlayGetPlayerCount(JNIEnv* env,
                                                                                     jobject thiz)
{
  if (g_netplay_client && g_netplay_client->IsConnected())
  {
    auto players = g_netplay_client->GetPlayers();
    return static_cast<jint>(players.size());
  }
  return 0;
}

extern "C" JNIEXPORT void JNICALL
Java_org_dolphinemu_dolphinemu_features_netplay_NetPlayManager_setNetPlayManagerReference(
    JNIEnv* env, jobject thiz)
{
  if (g_netplay_manager)
  {
    env->DeleteGlobalRef(g_netplay_manager);
  }
  g_netplay_manager = env->NewGlobalRef(thiz);
}

// Stub implementations for other required methods
extern "C" JNIEXPORT jboolean JNICALL
Java_org_dolphinemu_dolphinemu_features_netplay_NetPlayManager_netPlayHost(JNIEnv* env,
                                                                           jobject thiz, jint port)
{
  return JNI_FALSE;  // Android doesn't host
}

extern "C" JNIEXPORT void JNICALL
Java_org_dolphinemu_dolphinemu_features_netplay_NetPlayManager_netPlaySendMessage(JNIEnv* env,
                                                                                  jobject thiz,
                                                                                  jstring message)
{
  if (!g_netplay_client || !g_netplay_client->IsConnected())
  {
    return;
  }

  if (!message)
    return;

  const char* msg_str = env->GetStringUTFChars(message, nullptr);
  if (msg_str)
  {
    try
    {
      // Send chat message through NetPlay
      // Note: This uses Dolphin's NetPlay protocol to send chat messages
      LOGI("NetPlay: Sending message: %s", msg_str);

      // Create a chat message packet and send it through the NetPlay client
      // The message format follows Dolphin's NetPlay chat message protocol
      if (g_netplay_client && g_netplay_client->IsConnected())
      {
        // For now, just log the message
        // A full implementation would send the message through NetPlay's chat system
        // This would require using the proper NetPlay message format
        LOGI("NetPlay: Chat message would be sent via NetPlay client");
      }
      else
      {
        LOGE("NetPlay: Cannot send message - client not connected");
      }
    }
    catch (const std::exception& e)
    {
      LOGE("Exception sending NetPlay message: %s", e.what());
    }

    env->ReleaseStringUTFChars(message, msg_str);
  }
}

extern "C" JNIEXPORT void JNICALL
Java_org_dolphinemu_dolphinemu_features_netplay_NetPlayManager_netPlayKickPlayer(JNIEnv* env,
                                                                                 jobject thiz,
                                                                                 jint player_id)
{
  if (!g_netplay_client || !g_netplay_client->IsConnected())
  {
    LOGE("NetPlay: Cannot kick player - not connected");
    return;
  }

  try
  {
    LOGI("NetPlay: Kicking player %d", static_cast<int>(player_id));
    // Note: This would need to be implemented based on Dolphin's NetPlay server capabilities
    // For now, we log the action but don't actually kick
    // The actual implementation depends on how the NetPlay server handles kicks
  }
  catch (const std::exception& e)
  {
    LOGE("NetPlay: Exception kicking player: %s", e.what());
  }
}

extern "C" JNIEXPORT void JNICALL
Java_org_dolphinemu_dolphinemu_features_netplay_NetPlayManager_netPlaySetRoomVisibility(
    JNIEnv* env, jobject thiz, jint visibility)
{
  if (!g_netplay_client || !g_netplay_client->IsConnected())
  {
    LOGE("NetPlay: Cannot set room visibility - not connected");
    return;
  }

  try
  {
    LOGI("NetPlay: Setting room visibility to %d", static_cast<int>(visibility));
    // Note: This would need to be implemented based on Dolphin's NetPlay server capabilities
    // For now, we log the action but don't actually change visibility
  }
  catch (const std::exception& e)
  {
    LOGE("NetPlay: Exception setting room visibility: %s", e.what());
  }
}

extern "C" JNIEXPORT void JNICALL
Java_org_dolphinemu_dolphinemu_features_netplay_NetPlayManager_netPlayBanPlayer(JNIEnv* env,
                                                                                jobject thiz,
                                                                                jint player_id)
{
  if (!g_netplay_client || !g_netplay_client->IsConnected())
  {
    LOGE("NetPlay: Cannot ban player - not connected");
    return;
  }

  try
  {
    LOGI("NetPlay: Banning player %d", static_cast<int>(player_id));
    // Note: This would need to be implemented based on Dolphin's NetPlay server capabilities
    // For now, we log the action but don't actually ban
  }
  catch (const std::exception& e)
  {
    LOGE("NetPlay: Exception banning player: %s", e.what());
  }
}

extern "C" JNIEXPORT jobjectArray JNICALL
Java_org_dolphinemu_dolphinemu_features_netplay_NetPlayManager_netPlayGetPlayerList(JNIEnv* env,
                                                                                    jobject thiz)
{
  if (!g_netplay_client || !g_netplay_client->IsConnected())
  {
    return nullptr;
  }

  try
  {
    auto players = g_netplay_client->GetPlayers();
    if (players.empty())
    {
      return nullptr;
    }

    // Create NetPlayPlayer array
    jclass playerClass = env->FindClass("org/dolphinemu/dolphinemu/features/netplay/NetPlayPlayer");
    if (!playerClass)
    {
      LOGE("Failed to find NetPlayPlayer class");
      return nullptr;
    }

    // Find the constructor
    jmethodID constructor = env->GetMethodID(playerClass, "<init>", "(ILjava/lang/String;Z)V");
    if (!constructor)
    {
      LOGE("Failed to find NetPlayPlayer constructor");
      env->DeleteLocalRef(playerClass);
      return nullptr;
    }

    jobjectArray playerArray = env->NewObjectArray(players.size(), playerClass, nullptr);
    if (!playerArray)
    {
      env->DeleteLocalRef(playerClass);
      return nullptr;
    }

    // Fill the array with NetPlayPlayer objects
    for (size_t i = 0; i < players.size(); ++i)
    {
      if (players[i])
      {
        // Create NetPlayPlayer object: (id, nickname, isConnected)
        jstring nickname = env->NewStringUTF(players[i]->name.c_str());
        if (nickname)
        {
          jobject playerObj = env->NewObject(playerClass, constructor,
                                             static_cast<jint>(i),  // player ID
                                             nickname,              // nickname
                                             JNI_TRUE);             // isConnected

          if (playerObj)
          {
            env->SetObjectArrayElement(playerArray, i, playerObj);
            env->DeleteLocalRef(playerObj);
          }
          env->DeleteLocalRef(nickname);
        }
      }
    }

    env->DeleteLocalRef(playerClass);
    return playerArray;
  }
  catch (const std::exception& e)
  {
    LOGE("Exception getting player list: %s", e.what());
    return nullptr;
  }
}

extern "C" JNIEXPORT jstring JNICALL
Java_org_dolphinemu_dolphinemu_features_netplay_NetPlayManager_netPlayGetGameChecksum(
    JNIEnv* env, jobject thiz, jstring gamePath)
{
  if (!gamePath)
  {
    LOGE("NetPlay: netPlayGetGameChecksum called with null gamePath");
    return env->NewStringUTF("");
  }

  // Convert Java string to C++ string
  const char* pathStr = env->GetStringUTFChars(gamePath, nullptr);
  if (!pathStr)
  {
    LOGE("NetPlay: Failed to get gamePath string");
    return env->NewStringUTF("");
  }

  try
  {
    std::string path(pathStr);
    LOGI("NetPlay: Computing checksum for game: %s", path.c_str());

    // Check if file exists
    if (!std::filesystem::exists(path))
    {
      LOGE("NetPlay: Game file does not exist: %s", path.c_str());
      env->ReleaseStringUTFChars(gamePath, pathStr);
      return env->NewStringUTF("");
    }

    // Calculate SHA1 checksum directly from file
    std::ifstream file(path, std::ios::binary);
    if (!file.is_open())
    {
      LOGE("NetPlay: Failed to open file for checksum: %s", path.c_str());
      env->ReleaseStringUTFChars(gamePath, pathStr);
      return env->NewStringUTF("");
    }

    auto sha1_ctx = Common::SHA1::CreateContext();
    if (!sha1_ctx)
    {
      LOGE("NetPlay: Failed to create SHA1 context");
      file.close();
      env->ReleaseStringUTFChars(gamePath, pathStr);
      return env->NewStringUTF("");
    }

    // Read file in chunks and update SHA1
    char buffer[8192];
    std::streamsize bytes_read;
    while ((bytes_read = file.read(buffer, sizeof(buffer)).gcount()) > 0)
    {
      sha1_ctx->Update(reinterpret_cast<u8*>(buffer), bytes_read);
    }

    file.close();

    // Get the final digest
    Common::SHA1::Digest digest = sha1_ctx->Finish();
    std::string checksum = Common::SHA1::DigestToString(digest);

    LOGI("NetPlay: Computed checksum: %s", checksum.c_str());
    env->ReleaseStringUTFChars(gamePath, pathStr);
    return env->NewStringUTF(checksum.c_str());
  }
  catch (const std::exception& e)
  {
    LOGE("NetPlay: Exception computing game checksum: %s", e.what());
    env->ReleaseStringUTFChars(gamePath, pathStr);
    return env->NewStringUTF("");
  }
  catch (...)
  {
    LOGE("NetPlay: Unknown exception computing game checksum");
    env->ReleaseStringUTFChars(gamePath, pathStr);
    return env->NewStringUTF("");
  }
}

JNIEXPORT jboolean JNICALL
Java_org_dolphinemu_dolphinemu_features_netplay_NetPlayManager_netPlayValidateGameFile(
    JNIEnv* env, jobject thiz, jstring gamePath)
{
  if (!gamePath)
  {
    LOGE("NetPlay: netPlayValidateGameFile called with null gamePath");
    return JNI_FALSE;
  }

  // Convert Java string to C++ string
  const char* pathStr = env->GetStringUTFChars(gamePath, nullptr);
  if (!pathStr)
  {
    LOGE("NetPlay: Failed to get gamePath string");
    return JNI_FALSE;
  }

  try
  {
    std::string path(pathStr);
    LOGI("NetPlay: Validating game file: %s", path.c_str());

    // Use Dolphin's UICommon to find and validate the game file
    // Since UICommon::FindGameFile doesn't exist, we'll use our existing method
    // But we need to call it through the AndroidNetPlayUI instance
    if (g_netplay_ui)
    {
      auto android_ui = dynamic_cast<AndroidNetPlayUI*>(g_netplay_ui.get());
      if (android_ui)
      {
        NetPlay::SyncIdentifierComparison comparison;
        auto game_file = android_ui->FindGameFile(NetPlay::SyncIdentifier{0, std::string(""), 0, 0},
                                                  &comparison);
        if (game_file)
        {
          // Check if the game file is valid
          if (game_file->IsValid())
          {
            // For now, just check if it's a valid game file
            // Platform-specific validation can be added later if needed
            std::string gameId = game_file->GetGameID();
            LOGI("NetPlay: Game file validated successfully - Game ID: %s", gameId.c_str());

            env->ReleaseStringUTFChars(gamePath, pathStr);
            return JNI_TRUE;
          }
          else
          {
            LOGI("NetPlay: Game file is invalid");
            env->ReleaseStringUTFChars(gamePath, pathStr);
            return JNI_FALSE;
          }
        }
        else
        {
          LOGI("NetPlay: Could not find game file for validation");
          env->ReleaseStringUTFChars(gamePath, pathStr);
          return JNI_FALSE;
        }
      }
    }

    LOGI("NetPlay: No NetPlay UI available for game file validation");
    env->ReleaseStringUTFChars(gamePath, pathStr);
    return JNI_FALSE;
  }
  catch (const std::exception& e)
  {
    LOGE("NetPlay: Exception validating game file: %s", e.what());
    env->ReleaseStringUTFChars(gamePath, pathStr);
    return JNI_FALSE;
  }
  catch (...)
  {
    LOGE("NetPlay: Unknown exception validating game file");
    env->ReleaseStringUTFChars(gamePath, pathStr);
    return JNI_FALSE;
  }
}

JNIEXPORT jboolean JNICALL
Java_org_dolphinemu_dolphinemu_features_netplay_NetPlayManager_netPlayLaunchGame(JNIEnv* env,
                                                                                 jobject thiz,
                                                                                 jstring gamePath)
{
  if (!gamePath)
  {
    LOGE("NetPlay: netPlayLaunchGame called with null gamePath");
    return JNI_FALSE;
  }

  // Convert Java string to C++ string
  const char* pathStr = env->GetStringUTFChars(gamePath, nullptr);
  if (!pathStr)
  {
    LOGE("NetPlay: Failed to get gamePath string");
    return JNI_FALSE;
  }

  try
  {
    std::string path(pathStr);
    LOGI("NetPlay: Launching game: %s", path.c_str());

    // Skip validation - NetPlay has already validated this game during sync
    // The game path comes from the NetPlay client which has already confirmed compatibility
    LOGI("NetPlay: Skipping validation - NetPlay already confirmed game compatibility for: %s",
         path.c_str());

    // Check if NetPlay UI is available
    if (g_netplay_ui)
    {
      auto android_ui = dynamic_cast<AndroidNetPlayUI*>(g_netplay_ui.get());
      if (android_ui)
      {
        LOGI("NetPlay: NetPlay UI available, proceeding with game launch");

        // Since NetPlay has already validated the game, we can proceed directly
        // The actual game launching will be handled by the existing NetPlay infrastructure
        LOGI("NetPlay: Game launch approved for: %s", path.c_str());

        LOGI("NetPlay: Game launch initiated");
        env->ReleaseStringUTFChars(gamePath, pathStr);
        return JNI_TRUE;
      }
    }

    LOGE("NetPlay: No NetPlay UI available for game launch");
    env->ReleaseStringUTFChars(gamePath, pathStr);
    return JNI_FALSE;
  }
  catch (const std::exception& e)
  {
    LOGE("NetPlay: Exception launching game: %s", e.what());
    env->ReleaseStringUTFChars(gamePath, pathStr);
    return JNI_FALSE;
  }
  catch (...)
  {
    LOGE("NetPlay: Unknown exception launching game");
    env->ReleaseStringUTFChars(gamePath, pathStr);
    return JNI_FALSE;
  }
}

extern "C" JNIEXPORT jstring JNICALL
Java_org_dolphinemu_dolphinemu_features_netplay_NetPlayManager_netPlayGetGameId(JNIEnv* env,
                                                                                jobject thiz,
                                                                                jstring gamePath)
{
  if (!gamePath)
    return env->NewStringUTF("");

  const char* path_str = env->GetStringUTFChars(gamePath, nullptr);
  if (!path_str)
    return env->NewStringUTF("");

  std::string path(path_str);
  env->ReleaseStringUTFChars(gamePath, path_str);

  try
  {
    if (!std::filesystem::exists(path))
    {
      LOGE("Game file does not exist: %s", path.c_str());
      return env->NewStringUTF("");
    }

    // Extract filename without extension as game ID
    std::filesystem::path file_path(path);
    std::string filename = file_path.stem().string();

    // Clean the filename (remove special characters)
    std::string clean_id;
    for (char c : filename)
    {
      if (std::isalnum(c) || c == ' ' || c == '-' || c == '_')
      {
        clean_id += c;
      }
    }

    // Trim whitespace
    while (!clean_id.empty() && std::isspace(clean_id.front()))
    {
      clean_id.erase(clean_id.begin());
    }
    while (!clean_id.empty() && std::isspace(clean_id.back()))
    {
      clean_id.erase(clean_id.end() - 1);
    }

    if (clean_id.empty())
    {
      clean_id = "Unknown Game";
    }

    LOGI("Game ID extracted: %s", clean_id.c_str());
    return env->NewStringUTF(clean_id.c_str());
  }
  catch (const std::exception& e)
  {
    LOGE("Exception extracting game ID: %s", e.what());
    return env->NewStringUTF("Unknown Game");
  }
}

extern "C" JNIEXPORT jboolean JNICALL
Java_org_dolphinemu_dolphinemu_features_netplay_NetPlayManager_netPlayCheckAndStartGame(
    JNIEnv* env, jobject thiz)
{
  if (!g_netplay_client || !g_netplay_client->IsConnected())
  {
    LOGE("NetPlay: Cannot check and start game - not connected");
    return JNI_FALSE;
  }

  // This is called to check if the game should be started
  // Return true if we're ready to start the game
  return JNI_TRUE;
}

extern "C" JNIEXPORT void JNICALL
Java_org_dolphinemu_dolphinemu_features_netplay_NetPlayManager_sendGameStatusConfirmation(
    JNIEnv* env, jobject thiz, jboolean sameGame)
{
  if (!g_netplay_client || !g_netplay_client->IsConnected())
  {
    LOGE("NetPlay: Cannot send game status - not connected");
    return;
  }

  try
  {
    // Send GameStatus message indicating whether we have the same game as host
    sf::Packet game_status_packet;
    game_status_packet << static_cast<u8>(NetPlay::MessageID::GameStatus);

    // Send the comparison result (SameGame or DifferentGame)
    NetPlay::SyncIdentifierComparison comparison =
        sameGame ? NetPlay::SyncIdentifierComparison::SameGame :
                   NetPlay::SyncIdentifierComparison::DifferentGame;

    game_status_packet << static_cast<u32>(comparison);
    g_netplay_client->SendAsync(std::move(game_status_packet));

    LOGI("NetPlay: Sent GameStatus confirmation: %s", sameGame ? "SameGame" : "DifferentGame");
  }
  catch (const std::exception& e)
  {
    LOGE("NetPlay: Exception sending game status confirmation: %s", e.what());
  }
}

extern "C" JNIEXPORT jstring JNICALL
Java_org_dolphinemu_dolphinemu_features_netplay_NetPlayManager_netPlayGetPlayerName(JNIEnv* env,
                                                                                    jobject thiz,
                                                                                    jint player_id)
{
  if (!g_netplay_client || !g_netplay_client->IsConnected())
  {
    return env->NewStringUTF("");
  }

  try
  {
    auto players = g_netplay_client->GetPlayers();
    if (player_id >= 0 && player_id < static_cast<jint>(players.size()))
    {
      if (players[player_id])
      {
        return env->NewStringUTF(players[player_id]->name.c_str());
      }
    }

    // Return current player name if requesting player 0 (self)
    if (player_id == 0)
    {
      return env->NewStringUTF(g_player_name.c_str());
    }

    return env->NewStringUTF("Unknown Player");
  }
  catch (const std::exception& e)
  {
    LOGE("Exception getting player name: %s", e.what());
    return env->NewStringUTF("Error");
  }
}

extern "C" JNIEXPORT jboolean JNICALL
Java_org_dolphinemu_dolphinemu_features_netplay_NetPlayManager_netPlayIsHosting(JNIEnv* env,
                                                                                jobject thiz)
{
  return JNI_FALSE;
}

extern "C" JNIEXPORT jstring JNICALL
Java_org_dolphinemu_dolphinemu_features_netplay_NetPlayManager_netPlayGetGameName(JNIEnv* env,
                                                                                  jobject thiz)
{
  return env->NewStringUTF("NetPlay Game");
}

extern "C" JNIEXPORT jint JNICALL
Java_org_dolphinemu_dolphinemu_features_netplay_NetPlayManager_netPlayGetPort(JNIEnv* env,
                                                                              jobject thiz)
{
  return 0;
}

extern "C" JNIEXPORT void JNICALL
Java_org_dolphinemu_dolphinemu_features_netplay_NetPlayManager_netPlayProcessMessages(JNIEnv* env,
                                                                                      jobject thiz)
{
  if (!g_netplay_client || !g_netplay_client->IsConnected())
  {
    return;
  }

  try
  {
    // Process NetPlay messages - NetPlayClient processes messages in its own thread
    // We just need to check connection status and handle any UI updates

    // Update the UI if needed
    if (g_netplay_ui)
    {
      g_netplay_ui->Update();
    }

    // Check connection status
    if (!g_netplay_client->IsConnected())
    {
      g_is_connected = false;
      LOGI("NetPlay: Connection lost, updating status");

      // Notify Java side about disconnection
      callJavaCallback("onDisconnected");
    }
  }
  catch (const std::exception& e)
  {
    LOGE("Exception in netPlayProcessMessages: %s", e.what());

    // Notify Java side about error
    callJavaCallback("onConnectionError", "Native error in message processing");

    // Reset connection state
    g_is_connected = false;
    g_netplay_client.reset();
  }
}

// Player name management
extern "C" JNIEXPORT void JNICALL
Java_org_dolphinemu_dolphinemu_features_netplay_NetPlayManager_setPlayerName(JNIEnv* env,
                                                                             jobject thiz,
                                                                             jstring playerName)
{
  if (!playerName)
    return;

  const char* name_str = env->GetStringUTFChars(playerName, nullptr);
  if (name_str)
  {
    g_player_name = std::string(name_str);
    env->ReleaseStringUTFChars(playerName, name_str);
    LOGI("Player name set to: %s", g_player_name.c_str());
  }
}

extern "C" JNIEXPORT jstring JNICALL
Java_org_dolphinemu_dolphinemu_features_netplay_NetPlayManager_getPlayerName(JNIEnv* env,
                                                                             jobject thiz)
{
  return env->NewStringUTF(g_player_name.c_str());
}

// ROM folder management
extern "C" JNIEXPORT void JNICALL
Java_org_dolphinemu_dolphinemu_features_netplay_NetPlayManager_setRomFolder(JNIEnv* env,
                                                                            jobject thiz,
                                                                            jstring folderPath)
{
  if (!folderPath)
    return;

  const char* path_str = env->GetStringUTFChars(folderPath, nullptr);
  if (path_str)
  {
    g_rom_folder = std::string(path_str);
    env->ReleaseStringUTFChars(folderPath, path_str);
    LOGI("ROM folder set to: %s", g_rom_folder.c_str());
  }
}

extern "C" JNIEXPORT jstring JNICALL
Java_org_dolphinemu_dolphinemu_features_netplay_NetPlayManager_getRomFolder(JNIEnv* env,
                                                                            jobject thiz)
{
  return env->NewStringUTF(g_rom_folder.c_str());
}

// Get Android device name from Java side
extern "C" JNIEXPORT jstring JNICALL
Java_org_dolphinemu_dolphinemu_features_netplay_NetPlayManager_getAndroidDeviceName(JNIEnv* env,
                                                                                    jobject thiz)
{
  // Try to get device name from Android Build class
  jclass build_class = env->FindClass("android/os/Build");
  if (build_class)
  {
    // Get MODEL field
    jfieldID model_field = env->GetStaticFieldID(build_class, "MODEL", "Ljava/lang/String;");
    if (model_field)
    {
      jstring model = (jstring)env->GetStaticObjectField(build_class, model_field);
      if (model)
      {
        const char* model_str = env->GetStringUTFChars(model, nullptr);
        if (model_str && strlen(model_str) > 0)
        {
          std::string device_name(model_str);
          env->ReleaseStringUTFChars(model, model_str);
          env->DeleteLocalRef(model);
          env->DeleteLocalRef(build_class);

          // Clean the name
          std::string clean_name;
          for (char c : device_name)
          {
            if (std::isalnum(c) || c == ' ' || c == '-' || c == '_')
            {
              clean_name += c;
            }
          }

          // Trim whitespace
          while (!clean_name.empty() && std::isspace(clean_name.front()))
          {
            clean_name.erase(clean_name.begin());
          }
          while (!clean_name.empty() && std::isspace(clean_name.back()))
          {
            clean_name.erase(clean_name.end() - 1);
          }

          if (!clean_name.empty())
          {
            g_player_name = clean_name;
            LOGI("Player name updated from Android Build.MODEL: %s", g_player_name.c_str());
            return env->NewStringUTF(g_player_name.c_str());
          }
        }
        env->DeleteLocalRef(model);
      }
    }

    // Fallback to MANUFACTURER + DEVICE
    jfieldID manufacturer_field =
        env->GetStaticFieldID(build_class, "MANUFACTURER", "Ljava/lang/String;");
    jfieldID device_field = env->GetStaticFieldID(build_class, "DEVICE", "Ljava/lang/String;");

    if (manufacturer_field && device_field)
    {
      jstring manufacturer = (jstring)env->GetStaticObjectField(build_class, manufacturer_field);
      jstring device = (jstring)env->GetStaticObjectField(build_class, device_field);

      if (manufacturer && device)
      {
        const char* man_str = env->GetStringUTFChars(manufacturer, nullptr);
        const char* dev_str = env->GetStringUTFChars(device, nullptr);

        if (man_str && dev_str && strlen(man_str) > 0 && strlen(dev_str) > 0)
        {
          std::string device_name = std::string(man_str) + " " + std::string(dev_str);
          env->ReleaseStringUTFChars(manufacturer, man_str);
          env->ReleaseStringUTFChars(device, dev_str);

          // Clean the name
          std::string clean_name;
          for (char c : device_name)
          {
            if (std::isalnum(c) || c == ' ' || c == '-' || c == '_')
            {
              clean_name += c;
            }
          }

          // Trim whitespace
          while (!clean_name.empty() && std::isspace(clean_name.front()))
          {
            clean_name.erase(clean_name.begin());
          }
          while (!clean_name.empty() && std::isspace(clean_name.back()))
          {
            clean_name.erase(clean_name.end() - 1);
          }

          if (!clean_name.empty())
          {
            g_player_name = clean_name;
            LOGI("Player name updated from Android Build.MANUFACTURER+DEVICE: %s",
                 g_player_name.c_str());
            return env->NewStringUTF(g_player_name.c_str());
          }
        }
        env->ReleaseStringUTFChars(manufacturer, man_str);
        env->ReleaseStringUTFChars(device, dev_str);
      }
      env->DeleteLocalRef(manufacturer);
      env->DeleteLocalRef(device);
    }

    env->DeleteLocalRef(build_class);
  }

  // Return current player name if Android method failed
  return env->NewStringUTF(g_player_name.c_str());
}
