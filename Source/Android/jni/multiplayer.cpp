#include "multiplayer.h"
#include <jni.h>
#include <android/log.h>
#include <string>
#include <vector>
#include <memory>
#include <thread>
#include <chrono>
#include <cstdarg>

// Include Dolphin's NetPlay headers
#include "Core/NetPlayClient.h"
#include "Core/NetPlayServer.h"
#include "Core/NetPlayProto.h"
#include "Core/Config/NetplaySettings.h"
#include <SFML/Network/Packet.hpp>
#include "UICommon/NetPlayIndex.h"
#include "UICommon/GameFile.h"
#include "UICommon/GameFileCache.h"
#include "Common/Config/Config.h"
#include "Common/CommonTypes.h"
#include "Core/Boot/Boot.h"
#include "Core/Core.h"
#include "Core/System.h"

#define LOG_TAG "NetPlay"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)

// Global variables
static jobject g_netplay_manager = nullptr;

// Static NetPlay objects
static std::unique_ptr<NetPlay::NetPlayClient> g_netplay_client;
static std::unique_ptr<NetPlay::NetPlayServer> g_netplay_server;

// NetPlayUI implementation for Android
class AndroidNetPlayUI : public NetPlay::NetPlayUI
{
public:
    AndroidNetPlayUI() = default;
    ~AndroidNetPlayUI() = default;

    void BootGame(const std::string& filename, std::unique_ptr<BootSessionData> boot_session_data) override {
        LOGI("NetPlay: BootGame called for %s - starting game for NetPlay sync", filename.c_str());
        
        try {
            // CRITICAL: Before starting the game, verify our NetPlay connection is still valid
            if (!g_netplay_client || !g_netplay_client->IsConnected()) {
                LOGE("NetPlay: Cannot start game - NetPlay client is not connected!");
                return;
            }
            
            LOGI("NetPlay: NetPlay connection verified, starting game locally for NetPlay client synchronization");
            
            // Call Java method to start the game with NetPlay settings
        JNIEnv* env = getJNIEnv();
        if (env && g_netplay_manager) {
            try {
                jclass managerClass = env->GetObjectClass(g_netplay_manager);
                if (managerClass) {
                        // Try to call a method to start the game with NetPlay
                    jmethodID startGameMethod = env->GetMethodID(managerClass, "startNetPlayGame", "(Ljava/lang/String;)V");
                    if (startGameMethod) {
                        jstring gameFile = env->NewStringUTF(filename.c_str());
                        env->CallVoidMethod(g_netplay_manager, startGameMethod, gameFile);
                        env->DeleteLocalRef(gameFile);
                        LOGI("NetPlay: Called startNetPlayGame for %s", filename.c_str());
                            
                            // CRITICAL: After launching the game, verify the connection is still valid
                            // This helps catch any connection issues that might occur during activity transition
                            if (g_netplay_client && g_netplay_client->IsConnected()) {
                                LOGI("NetPlay: Connection still valid after game launch");
                    } else {
                                LOGE("NetPlay: WARNING - Connection lost during game launch!");
                            }
                            
        } else {
                            LOGI("NetPlay: startNetPlayGame method not found - trying alternative method");
                            
                            // Try alternative method name
                            jmethodID altMethod = env->GetMethodID(managerClass, "startGame", "(Ljava/lang/String;)V");
                            if (altMethod) {
                                jstring gameFile = env->NewStringUTF(filename.c_str());
                                env->CallVoidMethod(g_netplay_manager, altMethod, gameFile);
                                env->DeleteLocalRef(gameFile);
                                LOGI("NetPlay: Called startGame for %s", filename.c_str());
                                
                                // Verify connection after alternative method call too
                                if (g_netplay_client && g_netplay_client->IsConnected()) {
                                    LOGI("NetPlay: Connection still valid after alternative game launch");
        } else {
                                    LOGE("NetPlay: WARNING - Connection lost during alternative game launch!");
                                }
                                
        } else {
                                LOGI("NetPlay: No game start method found - NetPlay sync may not work properly");
                            }
                    }
                    env->DeleteLocalRef(managerClass);
                }
        } catch (const std::exception& e) {
                    LOGE("NetPlay: Exception starting game: %s", e.what());
            } catch (...) {
                    LOGE("NetPlay: Unknown exception starting game");
                }
            }
            
            LOGI("NetPlay: Game boot initiated for NetPlay synchronization");
            
            // Final connection check after everything is done
            if (g_netplay_client && g_netplay_client->IsConnected()) {
                LOGI("NetPlay: Final connection check - NetPlay client is still connected and ready for sync");
        } else {
                LOGE("NetPlay: CRITICAL ERROR - NetPlay client lost connection during game boot process!");
            }
            
        } catch (const std::exception& e) {
            LOGE("NetPlay: Exception in BootGame: %s", e.what());
        }
    }

    bool IsHosting() const override {
        return m_is_hosting;
    }

    void Update() override {
        // Called periodically to update NetPlay state
        // Simplified for stability - removed frequent connection checks
    }

    void AppendChat(const std::string& msg) override {
        LOGI("NetPlay: Chat message: %s", msg.c_str());
    }

    void OnMsgChangeGame(const NetPlay::SyncIdentifier& sync_identifier, const std::string& netplay_name) override {
        LOGI("NetPlay: *** OnMsgChangeGame called! Game changed to %s ***", netplay_name.c_str());
        LOGI("NetPlay: Sync identifier - game_id: %s", sync_identifier.game_id.c_str());
        m_current_sync_identifier = sync_identifier;
        
        // CRITICAL: Send initial GameStatus and ClientCapabilities immediately
        // The desktop host needs these to know our capabilities and game status
        // This allows the host to properly coordinate the game start process
        
        if (g_netplay_client && g_netplay_client->IsConnected()) {
            try {
                // First, check if we have this game
                NetPlay::SyncIdentifierComparison comparison;
                auto game_file = FindGameFile(sync_identifier, &comparison);
                
                // Send GameStatus message
                sf::Packet game_status_packet;
                game_status_packet << static_cast<u8>(NetPlay::MessageID::GameStatus);
                game_status_packet << static_cast<u32>(comparison);
                g_netplay_client->SendAsync(std::move(game_status_packet));
                LOGI("NetPlay: Sent GameStatus: %s", 
                     (comparison == NetPlay::SyncIdentifierComparison::SameGame) ? "SameGame" : "DifferentGame");
                
                // Send ClientCapabilities message
                sf::Packet capabilities_packet;
                capabilities_packet << static_cast<u8>(NetPlay::MessageID::ClientCapabilities);
                // Send basic capabilities - we support save data and code synchronization
                capabilities_packet << static_cast<u32>(0x1); // Basic capabilities flag
                g_netplay_client->SendAsync(std::move(capabilities_packet));
                LOGI("NetPlay: Sent ClientCapabilities to host");
                
                LOGI("NetPlay: *** Initial sync messages sent - host now knows our status and capabilities ***");
                
            } catch (const std::exception& e) {
                LOGE("NetPlay: Failed to send initial sync messages: %s", e.what());
            }
        } else {
            LOGE("NetPlay: Cannot send sync messages - NetPlay client not connected");
        }
    }

    void OnMsgChangeGBARom(int pad, const NetPlay::GBAConfig& config) override {
        LOGI("NetPlay: GBA ROM changed for pad %d", pad);
    }

    void OnMsgStartGame() override {
        LOGI("NetPlay: *** OnMsgStartGame - START GAME MESSAGE RECEIVED! ***");
        LOGI("NetPlay: Server sent StartGame message - Android client should now start the game locally");
        
        try {
            // CRITICAL: Verify NetPlay connection is healthy before proceeding
            if (!g_netplay_client || !g_netplay_client->IsConnected()) {
                LOGE("NetPlay: Cannot process StartGame - NetPlay client is not connected!");
                return;
            }
            
            LOGI("NetPlay: NetPlay connection verified, processing StartGame message from server");
            
            if (!m_current_sync_identifier.game_id.empty()) {
                LOGI("NetPlay: Server wants to start game %s - Android client starting game locally", m_current_sync_identifier.game_id.c_str());
                
                // CRITICAL: This is the REAL StartGame message from the server
                // Now we should actually start the game locally, just like the desktop client does
                
                try {
                    // Process the StartGame packet data like the desktop client does
                    // The packet contains important configuration settings that we need to extract
                    LOGI("NetPlay: Processing StartGame packet data from server");
                    
                    // IMPORTANT: This is when we actually start the game
                    // The server has coordinated all clients and is telling us to start
                    
                } catch (const std::exception& e) {
                    LOGE("NetPlay: Failed to process StartGame packet data: %s", e.what());
                    return;
                }
                
                // CRITICAL: NOW we start the game locally for NetPlay sync
                LOGI("NetPlay: Starting game locally for NetPlay synchronization");

                // Try to find the game file and boot it
                NetPlay::SyncIdentifierComparison comparison;
                auto game_file = FindGameFile(m_current_sync_identifier, &comparison);

                if (game_file && comparison == NetPlay::SyncIdentifierComparison::SameGame) {
                    LOGI("NetPlay: Found matching game file, booting: %s", game_file->GetFilePath().c_str());

                    // Create NetPlay boot session data
                    auto boot_session = std::make_unique<BootSessionData>();
                    if (g_netplay_client) {
                        // Get client settings from the connected client and create a unique_ptr
                        auto netplay_settings = g_netplay_client->GetNetSettings();
                        auto settings_ptr = std::make_unique<NetPlay::NetSettings>(netplay_settings);
                        boot_session->SetNetplaySettings(std::move(settings_ptr));
                    }

                                        // Boot the game using Android's own mechanism
                    BootGame(game_file->GetFilePath(), std::move(boot_session));
                    LOGI("NetPlay: Game boot initiated for NetPlay sync");

                    // CRITICAL: GameStatus was already sent in OnMsgChangeGame
                    // Now we just need to notify the Java side that the game is starting
                    LOGI("NetPlay: Game boot initiated - host already knows our status from OnMsgChangeGame");

                    // Notify Java side that the server started the game
                    JNIEnv* env = getJNIEnv();
                    if (env && g_netplay_manager) {
                        try {
                            jclass managerClass = env->GetObjectClass(g_netplay_manager);
                            if (managerClass) {
                                jmethodID gameStartedMethod = env->GetMethodID(managerClass, "onHostGameStarted", "()V");
                                if (gameStartedMethod) {
                                    env->CallVoidMethod(g_netplay_manager, gameStartedMethod);
                                    LOGI("NetPlay: Notified Java side that server started the game");
        } else {
                                    LOGI("NetPlay: onHostGameStarted method not found - this is expected if not implemented");
                                }
                                env->DeleteLocalRef(managerClass);
                            }
                        } catch (...) {
                            LOGI("NetPlay: Exception calling onHostGameStarted - this is expected if not implemented");
                        }
                    }

        } else {
                    LOGE("NetPlay: Could not find matching game file for NetPlay start");

                    // Send DifferentGame status if we don't have the game
                    if (g_netplay_client && g_netplay_client->IsConnected()) {
                        try {
                            sf::Packet game_status_packet;
                            game_status_packet << static_cast<u8>(NetPlay::MessageID::GameStatus);
                            game_status_packet << static_cast<u32>(NetPlay::SyncIdentifierComparison::DifferentGame);
                            g_netplay_client->SendAsync(std::move(game_status_packet));
                            LOGI("NetPlay: Sent DifferentGame status - we don't have this game");
        } catch (const std::exception& e) {
                            LOGE("NetPlay: Failed to send DifferentGame status: %s", e.what());
                        }
                    }
                }

                LOGI("NetPlay: StartGame processing completed - game should now be starting locally");

            } else {
                LOGE("No game ID available for StartGame message");
            }
        } catch (const std::exception& e) {
            LOGE("Exception in NetPlay StartGame processing: %s", e.what());
        }
    }

    void OnMsgStopGame() override {
        LOGI("NetPlay: *** OnMsgStopGame - STOP GAME MESSAGE RECEIVED! ***");
        LOGI("NetPlay: Game stopped");
        
        try {
            // Send acknowledgment that we received the stop game message
            if (g_netplay_client && g_netplay_client->IsConnected()) {
                LOGI("NetPlay: Acknowledging game stop to host");
                
                // Send GameStatus message to host (desktop doesn't support NotReady messages)
                try {
                    sf::Packet game_status_packet;
                    game_status_packet << static_cast<u8>(NetPlay::MessageID::GameStatus);
                    game_status_packet << static_cast<u32>(NetPlay::SyncIdentifierComparison::DifferentGame);
                    g_netplay_client->SendAsync(std::move(game_status_packet));
                    
                    LOGI("NetPlay: GameStatus message sent to host");
        } catch (const std::exception& e) {
                    LOGE("NetPlay: Failed to send GameStatus message: %s", e.what());
                }
            }
        } catch (const std::exception& e) {
            LOGE("Exception in NetPlay stop game: %s", e.what());
        }
    }

    void OnMsgPowerButton() override {
        LOGI("NetPlay: Power button pressed");
        // For now, just log the power button event
        // In a full implementation, this would integrate with Dolphin's emulator control
    }

    // Missing pure virtual methods from NetPlay::NetPlayUI
    void ShowChunkedProgressDialog(const std::string& title, u64 data_size,
                                   const std::vector<int>& players) override {
        LOGI("NetPlay: *** ShowChunkedProgressDialog called! Title: %s, data_size: %lu, players: %zu ***", 
             title.c_str(), data_size, players.size());
        // For now, just log - this is called during save data/code synchronization
    }

    void HideChunkedProgressDialog() override {
        LOGI("NetPlay: HideChunkedProgressDialog called");
    }

    void SetChunkedProgress(int pid, u64 progress) override {
        LOGI("NetPlay: SetChunkedProgress called for player %d, progress: %lu", pid, progress);
    }

    void OnIndexAdded(bool success, std::string error) override {
        if (success) {
            LOGI("NetPlay: *** OnIndexAdded - SUCCESS! ***");
        } else {
            LOGE("NetPlay: *** OnIndexAdded - FAILED: %s ***", error.c_str());
        }
        // This is called when save data or codes are indexed for synchronization
    }

    void OnIndexRefreshFailed(std::string error) override {
        LOGE("NetPlay: Index refresh failed: %s", error.c_str());
        // This is called when save data or code indexing fails
    }

    // Game digest methods
    void ShowGameDigestDialog(const std::string& title) override {
        LOGI("NetPlay: *** ShowGameDigestDialog called for %s ***", title.c_str());
        // This is called when the host wants to compute a game digest
    }

    void SetGameDigestProgress(int pid, int progress) override {
        LOGI("NetPlay: SetGameDigestProgress called for pid %d: %d%%", pid, progress);
        // This is called to update progress during game digest computation
    }

    void SetGameDigestResult(int pid, const std::string& result) override {
        LOGI("NetPlay: SetGameDigestResult called for pid %d: %s", pid, result.c_str());
        // This is called when game digest computation completes
    }

    void AbortGameDigest() override {
        LOGI("NetPlay: AbortGameDigest called");
        // This is called when game digest computation is aborted
    }

    void StopGame() override {
        LOGI("NetPlay: StopGame called");
        
        try {
            // Use Dolphin's Core API to stop the game
            // This is the same logic used in desktop Dolphin
            auto& system = Core::System::GetInstance();
            if (Core::IsRunning(system)) {
                LOGI("NetPlay: Stopping running game via Core API");
                Core::Stop(system);
                LOGI("NetPlay: Game stopped successfully");
            } else {
                LOGI("NetPlay: No game currently running");
            }
        } catch (const std::exception& e) {
            LOGE("NetPlay: Exception stopping game: %s", e.what());
        } catch (...) {
            LOGE("NetPlay: Unknown exception stopping game");
        }
    }

    void OnPlayerConnect(const std::string& player) override {
        LOGI("NetPlay: Player connected: %s", player.c_str());
        
        // Call Java callback to notify about new player
        callJavaCallback("onPlayerJoined", 0, player.c_str());
        
        LOGI("Player connected: %s", player.c_str());
    }

    void OnPlayerDisconnect(const std::string& reason) override {
        LOGI("NetPlay: OnPlayerDisconnect called: %s", reason.c_str());
        AppendChat("Player disconnected: " + reason);
    }

    void OnPadBufferChanged(u32 buffer) override {
        LOGI("NetPlay: Pad buffer changed to %u", buffer);
    }

    void OnHostInputAuthorityChanged(bool enabled) override {
        LOGI("NetPlay: Host input authority changed to %s", enabled ? "enabled" : "disabled");
    }

    void OnDesync(u32 frame, const std::string& player) override {
        LOGE("NetPlay: Desync detected at frame %u from player %s", frame, player.c_str());
    }

    void OnConnectionLost() override {
        LOGE("NetPlay: Connection lost");
        
        // Try to recover the connection if possible
        if (g_netplay_client) {
            LOGI("NetPlay: Attempting connection recovery...");
            // The NetPlay client will handle reconnection automatically
            // We just need to notify the Java side
        }
        
        callJavaCallback("onDisconnected");
    }

    void OnConnectionError(const std::string& message) override {
        LOGE("NetPlay: Connection error: %s", message.c_str());
        callJavaCallback("onConnectionFailed");
    }

    // Additional required pure virtual methods
    void OnTraversalError(Common::TraversalClient::FailureReason error) override {
        LOGE("NetPlay: Traversal error: %d", static_cast<int>(error));
    }

    void OnTraversalStateChanged(Common::TraversalClient::State state) override {
        LOGI("NetPlay: Traversal state changed to %d", static_cast<int>(state));
    }

    void OnGameStartAborted() override {
        LOGI("NetPlay: Game start aborted");
    }

    void OnGolferChanged(bool is_golfer, const std::string& golfer_name) override {
        LOGI("NetPlay: Golfer changed to %s (is_golfer: %s)", golfer_name.c_str(), is_golfer ? "true" : "false");
    }

    void OnTtlDetermined(u8 ttl) override {
        LOGI("NetPlay: TTL determined: %u", ttl);
    }

    bool IsRecording() override {
        return false; // Not recording by default
    }

    // Public getter for the current sync identifier
    const NetPlay::SyncIdentifier& GetCurrentSyncIdentifier() const {
        return m_current_sync_identifier;
    }

    std::shared_ptr<const UICommon::GameFile> FindGameFile(const NetPlay::SyncIdentifier& sync_identifier,
                                                           NetPlay::SyncIdentifierComparison* found = nullptr) override {
        LOGI("NetPlay: FindGameFile called for game_id: %s, revision: %d, disc: %d", 
             sync_identifier.game_id.c_str(), sync_identifier.revision, sync_identifier.disc_number);
        
        // Safety check - don't proceed with empty game_id
        if (sync_identifier.game_id.empty()) {
            LOGE("FindGameFile called with empty game_id - this will cause crashes!");
            return nullptr;
        }
        
        try {
            // Get the game cache and search for matching games
            static UICommon::GameFileCache game_cache;
            
            // Load the cache if it hasn't been loaded yet
            if (game_cache.GetSize() == 0) {
                LOGI("Loading game cache...");
                if (!game_cache.Load()) {
                    LOGI("Failed to load game cache, getting ROM path from Java...");
                    
                    // Get ROM path from Java side instead of hardcoding
                    JNIEnv* env = getJNIEnv();
                    if (env && g_netplay_manager) {
                        try {
                            jclass managerClass = env->GetObjectClass(g_netplay_manager);
                            if (managerClass) {
                                jmethodID getRomPathMethod = env->GetMethodID(managerClass, "getRomPath", "()Ljava/lang/String;");
                                if (getRomPathMethod) {
                                    jstring romPathString = static_cast<jstring>(env->CallObjectMethod(g_netplay_manager, getRomPathMethod));
                                    if (romPathString) {
                                        const char* romPath = env->GetStringUTFChars(romPathString, nullptr);
                                        if (romPath) {
                                            std::string rom_path(romPath);
                                            env->ReleaseStringUTFChars(romPathString, romPath);
                                            
                                            LOGI("Got ROM path from Java: %s", rom_path.c_str());
                                            
                                            // Use the ROM path from Java
                                            std::vector<std::string> game_dirs = {rom_path};
                                            try {
                    auto game_paths = UICommon::FindAllGamePaths(game_dirs, true);
                    game_cache.Update(game_paths);
                                                LOGI("Updated game cache with %zu paths from Java ROM directory", game_paths.size());
                                            } catch (const std::exception& e) {
                                                LOGE("Exception calling FindAllGamePaths with Java ROM path: %s", e.what());
                                                // Continue with fallback paths
                                            } catch (...) {
                                                LOGE("Unknown exception calling FindAllGamePaths with Java ROM path");
                                                // Continue with fallback paths
                                            }
                                        }
                                    }
                                    env->DeleteLocalRef(romPathString);
                                } else {
                                    LOGI("Could not find getRomPath method, using default Android paths");
                                    // Fallback to some common Android directories
                                    std::vector<std::string> game_dirs = {
                                        "/storage/emulated/0/ROMs", 
                                        "/storage/emulated/0/Games"
                                    };
                                    try {
                                        auto game_paths = UICommon::FindAllGamePaths(game_dirs, true);
                                        game_cache.Update(game_paths);
                                        LOGI("Updated game cache with %zu paths from default Android directories", game_paths.size());
                                    } catch (const std::exception& e) {
                                        LOGE("Exception calling FindAllGamePaths with default Android paths: %s", e.what());
                                    } catch (...) {
                                        LOGE("Unknown exception calling FindAllGamePaths with default Android paths");
                                    }
                                }
                                env->DeleteLocalRef(managerClass);
                            }
                        } catch (...) {
                            LOGE("Exception getting ROM path from Java, using fallback");
                            // Fallback to some common Android directories
                            std::vector<std::string> game_dirs = {
                                "/storage/emulated/0/ROMs", 
                                "/storage/emulated/0/Games"
                            };
                            try {
                                auto game_paths = UICommon::FindAllGamePaths(game_dirs, true);
                                game_cache.Update(game_paths);
                                LOGI("Updated game cache with %zu paths from default Android directories", game_paths.size());
        } catch (const std::exception& e) {
                                LOGE("Exception calling FindAllGamePaths with default Android paths: %s", e.what());
                            } catch (...) {
                                LOGE("Unknown exception calling FindAllGamePaths with default Android paths");
                            }
                        }
                    } else {
                        LOGI("No JNI environment available, using default Android paths");
                        // Fallback to some common Android directories
                        std::vector<std::string> game_dirs = {
                            "/storage/emulated/0/ROMs", 
                            "/storage/emulated/0/Games"
                        };
                        try {
                            auto game_paths = UICommon::FindAllGamePaths(game_dirs, true);
                            game_cache.Update(game_paths);
                            LOGI("Updated game cache with %zu paths from default Android directories", game_paths.size());
                        } catch (const std::exception& e) {
                            LOGE("Exception calling FindAllGamePaths with default Android paths: %s", e.what());
                        } catch (...) {
                            LOGE("Unknown exception calling FindAllGamePaths with default Android paths");
                        }
                    }
                }
            }
            
            LOGI("Searching through %zu games in cache", game_cache.GetSize());
            
            std::shared_ptr<const UICommon::GameFile> found_game = nullptr;
            game_cache.ForEach([&](const std::shared_ptr<const UICommon::GameFile>& game) {
                if (game && game->IsValid()) {
                    auto game_sync_id = game->GetSyncIdentifier();
                    LOGI("Checking game: %s (game_id: %s, revision: %d, disc: %d)", 
                         game->GetFilePath().c_str(), game_sync_id.game_id.c_str(), 
                         game_sync_id.revision, game_sync_id.disc_number);
                    
                    if (game_sync_id.game_id == sync_identifier.game_id &&
                        game_sync_id.revision == sync_identifier.revision &&
                        game_sync_id.disc_number == sync_identifier.disc_number) {
                        
                        LOGI("Found matching game: %s", game->GetFilePath().c_str());
                        found_game = game;
                    }
                }
            });
            
            if (found_game) {
                if (found) {
                    *found = NetPlay::SyncIdentifierComparison::SameGame;
                }
                return found_game;
            }
            
            LOGI("No matching game found for sync identifier");
            if (found) {
                *found = NetPlay::SyncIdentifierComparison::Unknown;
            }
            return nullptr;
            
        } catch (const std::exception& e) {
            LOGE("Exception in FindGameFile: %s", e.what());
            if (found) {
                *found = NetPlay::SyncIdentifierComparison::Unknown;
            }
            return nullptr;
        }
    }

    std::string FindGBARomPath(const std::array<u8, 20>& hash, std::string_view title,
                               int device_number) override {
        LOGI("NetPlay: FindGBARomPath called for %s (device: %d)", std::string(title).c_str(), device_number);
        
        // Get ROM path from Java side instead of hardcoding
        JNIEnv* env = getJNIEnv();
        if (env && g_netplay_manager) {
            try {
                jclass managerClass = env->GetObjectClass(g_netplay_manager);
                if (managerClass) {
                    jmethodID getRomPathMethod = env->GetMethodID(managerClass, "getRomPath", "()Ljava/lang/String;");
                    if (getRomPathMethod) {
                        jstring romPathString = static_cast<jstring>(env->CallObjectMethod(g_netplay_manager, getRomPathMethod));
                        if (romPathString) {
                            const char* romPath = env->GetStringUTFChars(romPathString, nullptr);
                            if (romPath) {
                                std::string rom_path(romPath);
                                env->ReleaseStringUTFChars(romPathString, romPath);
                                
                                LOGI("Got ROM path from Java for GBA search: %s", rom_path.c_str());
                                
                                // Look for GBA ROMs in the Java-provided ROM directory
                                std::vector<std::string> gba_dirs = {
                                    rom_path + "/GBA",
                                    rom_path + "/gba",
                                    rom_path
                                };
                                
                                for (const auto& dir : gba_dirs) {
                                    LOGI("Checking GBA directory: %s", dir.c_str());
                                    // In a real implementation, you'd scan these directories for matching ROMs
                                }
                                
                                env->DeleteLocalRef(romPathString);
                                env->DeleteLocalRef(managerClass);
                                return rom_path; // Return the base ROM path for now
                            }
                            env->DeleteLocalRef(romPathString);
                        }
            } else {
                        LOGI("Could not find getRomPath method for GBA search");
                    }
                    env->DeleteLocalRef(managerClass);
                }
            } catch (...) {
                LOGE("Exception getting ROM path from Java for GBA search");
            }
        }
        
        // Fallback to empty string if Java call fails
        LOGI("Using fallback for GBA ROM path");
        return "";
    }




    
    void SetHostWiiSyncData(std::vector<u64> titles, std::string redirect_folder) override {
        LOGI("NetPlay: SetHostWiiSyncData called with %zu titles, redirect folder: %s", 
             titles.size(), redirect_folder.c_str());
        
        // Store the Wii sync data for the host
        // This would typically be used for save data synchronization
        for (size_t i = 0; i < titles.size(); i++) {
            LOGI("NetPlay: Host Wii title %zu: %016lx", i, titles[i]);
        }
        
        // Create redirect folder if needed for Android
        if (!redirect_folder.empty()) {
            std::string full_path = "/storage/emulated/0/Android/data/org.dolphinemu.dolphinmpn/files/" + redirect_folder;
            system(("mkdir -p " + full_path).c_str());
            LOGI("NetPlay: Created redirect folder: %s", full_path.c_str());
        }
    }

    // Helper methods
    void SetHosting(bool hosting) {
        m_is_hosting = hosting;
    }
    
    bool ShouldStartGame() const {
        return m_should_start_game;
    }
    
    void ClearStartGameFlag() {
        m_should_start_game = false;
    }

private:
    bool m_is_hosting = false;
    bool m_should_start_game = false;
    NetPlay::SyncIdentifier m_current_sync_identifier;
};

// Global variables for NetPlay state
static std::unique_ptr<AndroidNetPlayUI> g_netplay_ui;
static bool g_is_connected = false;
static bool g_is_host = false;
static std::string g_server_address;
static int g_server_port = 2626;

// JNI environment and object references
static JavaVM* g_jvm = nullptr;

// Safe JNI helper functions
JNIEnv* getJNIEnv() {
    if (!g_jvm) {
        LOGE("JVM not initialized");
        return nullptr;
    }
    
    JNIEnv* env;
    jint result = g_jvm->GetEnv(reinterpret_cast<void**>(&env), JNI_VERSION_1_6);
    if (result == JNI_EDETACHED) {
        // Thread not attached, try to attach
        if (g_jvm->AttachCurrentThread(&env, nullptr) != 0) {
            LOGE("Failed to attach current thread to JVM");
            return nullptr;
        }
    } else if (result != JNI_OK) {
        LOGE("Failed to get JNI environment, result: %d", result);
        return nullptr;
    }
    
    return env;
}

// Essential callback function for basic NetPlay events
void callJavaCallback(const char* method_name, ...) {
    JNIEnv* env = getJNIEnv();
    if (!env) {
        LOGE("Could not get JNI environment for callback: %s", method_name);
        return;
    }
    
    if (!g_netplay_manager) {
        LOGE("NetPlay manager object not available for callback: %s", method_name);
        return;
    }
    
    // Log the callback attempt
    LOGI("Attempting Java callback: %s", method_name);
    
    // Global exception handler for the entire function
    if (env->ExceptionCheck()) {
        env->ExceptionDescribe();
        env->ExceptionClear();
        LOGE("JNI exception detected at start of callback %s", method_name);
    }
    
    // Try to call the appropriate Java method based on the callback name
    try {
        if (strcmp(method_name, "onConnected") == 0) {
            // Find the onConnected method in the NetPlayManager class
            jclass managerClass = env->GetObjectClass(g_netplay_manager);
            if (managerClass) {
                jmethodID onConnectedMethod = env->GetMethodID(managerClass, "onConnected", "()V");
                if (onConnectedMethod) {
                    // Add extra safety checks before calling
                    if (env->IsSameObject(g_netplay_manager, nullptr)) {
                        LOGE("NetPlay manager object is null, cannot call onConnected");
                        env->DeleteLocalRef(managerClass);
                        return;
                    }
                    
                    // Check if the object is still valid
                    if (env->ExceptionCheck()) {
                        env->ExceptionDescribe();
                        env->ExceptionClear();
                        LOGE("JNI exception detected before calling onConnected");
                        env->DeleteLocalRef(managerClass);
                        return;
                    }
                    
                    env->CallVoidMethod(g_netplay_manager, onConnectedMethod);
                    
                    // Check for exceptions after the call
                    if (env->ExceptionCheck()) {
                        env->ExceptionDescribe();
                        env->ExceptionClear();
                        LOGE("Exception occurred while calling onConnected");
                    } else {
                        LOGI("Successfully called onConnected callback");
                    }
                } else {
                    LOGE("Could not find onConnected method");
                }
                env->DeleteLocalRef(managerClass);
            } else {
                LOGE("Could not get NetPlayManager class for onConnected callback");
            }
        } else if (strcmp(method_name, "onDisconnected") == 0) {
            // Find the onDisconnected method
            jclass managerClass = env->GetObjectClass(g_netplay_manager);
            if (managerClass) {
                jmethodID onDisconnectedMethod = env->GetMethodID(managerClass, "onDisconnected", "()V");
                if (onDisconnectedMethod) {
                    // Add extra safety checks before calling
                    if (env->IsSameObject(g_netplay_manager, nullptr)) {
                        LOGE("NetPlay manager object is null, cannot call onDisconnected");
                        env->DeleteLocalRef(managerClass);
                        return;
                    }
                    
                    // Check if the object is still valid
                    if (env->ExceptionCheck()) {
                        env->ExceptionDescribe();
                        env->ExceptionClear();
                        LOGE("JNI exception detected before calling onDisconnected");
                        env->DeleteLocalRef(managerClass);
                        return;
                    }
                    
                    env->CallVoidMethod(g_netplay_manager, onDisconnectedMethod);
                    
                    // Check for exceptions after the call
                    if (env->ExceptionCheck()) {
                        env->ExceptionDescribe();
                        env->ExceptionClear();
                        LOGE("Exception occurred while calling onDisconnected");
                    } else {
                        LOGI("Successfully called onDisconnected callback");
                    }
                } else {
                    LOGE("Could not find onDisconnected method");
                }
                env->DeleteLocalRef(managerClass);
            } else {
                LOGE("Could not get NetPlayManager class for onDisconnected callback");
            }
        } else if (strcmp(method_name, "onConnectionFailed") == 0) {
            // Handle connection failed callback
            jclass managerClass = env->GetObjectClass(g_netplay_manager);
            if (managerClass) {
                jmethodID onConnectionFailedMethod = env->GetMethodID(managerClass, "onConnectionFailed", "(Ljava/lang/String;)V");
                if (onConnectionFailedMethod) {
                    jstring errorMsg = env->NewStringUTF("Connection failed from native code");
                    env->CallVoidMethod(g_netplay_manager, onConnectionFailedMethod, errorMsg);
                    env->DeleteLocalRef(errorMsg);
                    LOGI("Successfully called onConnectionFailed callback");
                } else {
                    LOGE("Could not find onConnectionFailed method");
                }
                env->DeleteLocalRef(managerClass);
            }
        } else if (strcmp(method_name, "onPlayerJoined") == 0) {
            // Handle onPlayerJoined callback
            jclass managerClass = env->GetObjectClass(g_netplay_manager);
            if (managerClass) {
                jmethodID onPlayerJoinedMethod = env->GetMethodID(managerClass, "onPlayerJoined", "(ILjava/lang/String;)V");
                if (onPlayerJoinedMethod) {
                    va_list args;
                    va_start(args, method_name);
                    int player_id = va_arg(args, int);
                    const char* player_name = va_arg(args, const char*);
                    va_end(args);

                    jstring playerNameString = env->NewStringUTF(player_name);
                    env->CallVoidMethod(g_netplay_manager, onPlayerJoinedMethod, player_id, playerNameString);
                    env->DeleteLocalRef(playerNameString);
                    LOGI("Successfully called onPlayerJoined callback for player %d: %s", player_id, player_name);
                } else {
                    LOGE("Could not find onPlayerJoined method");
                }
                env->DeleteLocalRef(managerClass);
            }
        } else {
            LOGI("Unknown callback method: %s", method_name);
        }
    } catch (const std::exception& e) {
        LOGE("Exception in callback %s: %s", method_name, e.what());
    } catch (...) {
        LOGE("Unknown exception in callback %s", method_name);
    }
}

// Function to get device name using Android APIs
std::string getDeviceName() {
    JNIEnv* env = getJNIEnv();
    if (!env) {
        LOGE("Could not get JNI environment for device name");
        return "Android Player";
    }
    
    try {
        // Get Build class
        jclass buildClass = env->FindClass("android/os/Build");
        if (!buildClass) {
            LOGE("Could not find Build class");
            return "Android Player";
        }
        
        // Get Build.MODEL field (device model like "Pixel 7", "Galaxy S23")
        jfieldID modelField = env->GetStaticFieldID(buildClass, "MODEL", "Ljava/lang/String;");
        if (!modelField) {
            LOGE("Could not find Build.MODEL field");
            return "Android Player";
        }
        
        jstring modelString = static_cast<jstring>(env->GetStaticObjectField(buildClass, modelField));
        if (!modelString) {
            LOGE("Build.MODEL is null");
            return "Android Player";
        }
        
        const char* modelChars = env->GetStringUTFChars(modelString, nullptr);
        if (!modelChars) {
            LOGE("Failed to get model string chars");
            return "Android Player";
        }
        
        std::string deviceName = std::string(modelChars);
        env->ReleaseStringUTFChars(modelString, modelChars);
        
        // Clean up the model string (remove special characters that might cause issues)
        std::string cleanName;
        for (char c : deviceName) {
            if (std::isalnum(c) || c == ' ' || c == '-') {
                cleanName += c;
            }
        }
        
        // Trim whitespace
        cleanName.erase(0, cleanName.find_first_not_of(" \t"));
        cleanName.erase(cleanName.find_last_not_of(" \t") + 1);
        
        if (cleanName.empty()) {
            cleanName = "Android Player";
        }
        
        LOGI("Device name: %s", cleanName.c_str());
        return cleanName;
        
    } catch (const std::exception& e) {
        LOGE("Exception getting device name: %s", e.what());
        return "Android Player";
    } catch (...) {
        LOGE("Unknown exception getting device name");
        return "Android Player";
    }
}

// NetPlay implementation using Dolphin's classes
extern "C" JNIEXPORT jboolean JNICALL
Java_org_dolphinemu_dolphinemu_features_netplay_NetPlayManager_netPlayConnect(
    JNIEnv* env, jobject thiz, jstring address, jint port) {
    
    if (!address) {
        LOGE("Address parameter is null");
        return JNI_FALSE;
    }
    
    const char* addr = env->GetStringUTFChars(address, nullptr);
    if (!addr) {
        LOGE("Failed to get address string chars");
        return JNI_FALSE;
    }
    
    g_server_address = std::string(addr);
    g_server_port = port;
    env->ReleaseStringUTFChars(address, addr);
    
    LOGI("Connecting to NetPlay server: %s:%d", g_server_address.c_str(), g_server_port);
    
    try {
        // Validate input parameters
        if (g_server_address.empty() || g_server_port <= 0 || g_server_port > 65535) {
            LOGE("Invalid server address or port");
            return JNI_FALSE;
        }
        
        // Get device name for player identification
        std::string playerName = getDeviceName();
        
        // Create NetPlayUI if not exists
        if (!g_netplay_ui) {
            g_netplay_ui = std::make_unique<AndroidNetPlayUI>();
        }
        
        // Create NetPlayClient using Dolphin's implementation
        LOGI("Creating NetPlayClient for %s:%d as %s", g_server_address.c_str(), g_server_port, playerName.c_str());
        
        // Add safety check for NetPlayUI
        if (!g_netplay_ui) {
            LOGE("NetPlayUI is null, cannot create NetPlayClient");
            return JNI_FALSE;
        }
        
        // Determine if this is a traversal code (8-character hex string) or direct IP
        bool use_traversal = false;
        if (g_server_address.length() == 8) {
            // Check if it's a valid hex string (traversal code)
            bool is_hex = true;
            for (char c : g_server_address) {
                if (!std::isxdigit(c)) {
                    is_hex = false;
                    break;
                }
            }
            use_traversal = is_hex;
            if (use_traversal) {
                LOGI("Valid 8-character hex traversal code detected: %s", g_server_address.c_str());
            } else {
                LOGI("8-character input but not valid hex - treating as direct connection: %s", g_server_address.c_str());
            }
        } else {
            // Not 8 characters, so it's a direct IP address
            use_traversal = false;
            LOGI("Input length %zu - treating as direct IP connection: %s", g_server_address.length(), g_server_address.c_str());
        }
        
        LOGI("NetPlay connection type: %s", use_traversal ? "traversal" : "direct");
        
        // Configure traversal settings with mobile network optimizations
        NetPlay::NetTraversalConfig traversal_config;
        if (use_traversal) {
            traversal_config.use_traversal = true;
            traversal_config.traversal_host = "stun.dolphin-emu.org";  // Official Dolphin traversal server
            traversal_config.traversal_port = 6262;  // Official port
            LOGI("Using traversal server: %s:%d", traversal_config.traversal_host.c_str(), traversal_config.traversal_port);
            
            // Validate traversal code format
            if (g_server_address.length() != 8) {
                LOGE("Invalid traversal code length: %zu (expected 8)", g_server_address.length());
                return JNI_FALSE;
            }
            
            // Check if traversal code is valid hex
            for (char c : g_server_address) {
                if (!std::isxdigit(c)) {
                    LOGE("Invalid traversal code character: %c (not hex)", c);
                    return JNI_FALSE;
                }
            }
            
            LOGI("Traversal code validation passed: %s", g_server_address.c_str());
        } else {
            traversal_config.use_traversal = false;
            traversal_config.traversal_host = "";
            traversal_config.traversal_port = 0;
            LOGI("Using direct connection to %s:%d", g_server_address.c_str(), g_server_port);
        }
        
        g_netplay_client = std::make_unique<NetPlay::NetPlayClient>(
            g_server_address, g_server_port, g_netplay_ui.get(), playerName,
            traversal_config
        );
        
        if (g_netplay_client) {
            LOGI("NetPlayClient created successfully - UI pointer: %p", g_netplay_ui.get());
            LOGI("NetPlay: *** CLIENT CREATED - READY TO RECEIVE MESSAGES ***");
            
            // Wait for connection to establish
            int timeout_ms = use_traversal ? 10000 : 4500;  // 10 seconds for traversal, 4.5 for direct 
            LOGI("Waiting %d ms for connection to establish...", timeout_ms);
            
            auto start_time = std::chrono::steady_clock::now();
            bool connected = false;
            
            while (std::chrono::steady_clock::now() - start_time < std::chrono::milliseconds(timeout_ms)) {
                // Add safety check for NetPlayClient
                if (!g_netplay_client) {
                    LOGE("NetPlayClient became null during connection attempt");
                    return JNI_FALSE;
                }
                
                try {
                if (g_netplay_client->IsConnected()) {
                    connected = true;
                    break;
                }
                } catch (const std::exception& e) {
                    LOGE("Exception checking connection status: %s", e.what());
                    break;
                } catch (...) {
                    LOGE("Unknown exception checking connection status");
                    break;
                }
                
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
            }
            
            if (connected) {
                g_is_connected = true;
                g_is_host = false;
                g_netplay_ui->SetHosting(false);
                
                LOGI("Successfully connected to NetPlay server as %s", playerName.c_str());
                
                // CRITICAL: Don't send initial status messages - wait for ChangeGame from host
                // The host will send ChangeGame when it has a game selected, and then we'll respond
                // This prevents timing issues and ensures proper handshake sequence
                
                LOGI("NetPlay: Connected successfully - waiting for host to send game information");
                
                return JNI_TRUE;
            } else {
                LOGE("Connection timeout after %d ms", timeout_ms);
                g_netplay_client.reset();
                return JNI_FALSE;
            }
        } else {
            LOGE("Failed to create NetPlayClient");
            return JNI_FALSE;
        }
        
    } catch (const std::exception& e) {
        LOGE("Exception during NetPlay connection: %s", e.what());
        g_netplay_client.reset();
        return JNI_FALSE;
    } catch (...) {
        LOGE("Unknown exception during NetPlay connection");
        g_netplay_client.reset();
        return JNI_FALSE;
    }
}

extern "C" JNIEXPORT jboolean JNICALL
Java_org_dolphinemu_dolphinemu_features_netplay_NetPlayManager_netPlayHost(
    JNIEnv* env, jobject thiz, jint port) {
    
    g_server_port = port;
    LOGI("Hosting NetPlay server on port: %d", g_server_port);
    
    try {
        // Validate port number
        if (port <= 0 || port > 65535) {
            LOGE("Invalid port number: %d", port);
            return JNI_FALSE;
        }
        
        // Get device name for host identification
        std::string hostName = getDeviceName() + " (Host)";
        
        // Create NetPlayUI if not exists
        if (!g_netplay_ui) {
            g_netplay_ui = std::make_unique<AndroidNetPlayUI>();
        }
        
        LOGI("Creating NetPlayServer on port %d as %s", g_server_port, hostName.c_str());
        
        // Create NetPlayServer using Dolphin's implementation
        g_netplay_server = std::make_unique<NetPlay::NetPlayServer>(
            g_server_port, false, g_netplay_ui.get(),
            NetPlay::NetTraversalConfig{false, "", 0, 0}
        );
        
        if (g_netplay_server && g_netplay_server->is_connected) {
            g_is_connected = true;
            g_is_host = true;
            g_netplay_ui->SetHosting(true);
            
            LOGI("Successfully hosting NetPlay server as %s", hostName.c_str());
            return JNI_TRUE;
        } else {
            LOGE("Failed to host NetPlay server");
            g_netplay_server.reset();
            return JNI_FALSE;
        }
        
    } catch (const std::exception& e) {
        LOGE("Exception during NetPlay hosting: %s", e.what());
        g_netplay_server.reset();
        return JNI_FALSE;
    } catch (...) {
        LOGE("Unknown exception during NetPlay hosting");
        g_netplay_server.reset();
        return JNI_FALSE;
    }
}

extern "C" JNIEXPORT void JNICALL
Java_org_dolphinemu_dolphinemu_features_netplay_NetPlayManager_netPlayDisconnect(
    JNIEnv* env, jobject thiz) {
    
    LOGI("Disconnecting from NetPlay server");
    
    // Clean up Dolphin's NetPlay objects
    g_netplay_client.reset();
    g_netplay_server.reset();
    
    g_is_connected = false;
    g_is_host = false;
    
    LOGI("Disconnected from NetPlay server");
}

extern "C" JNIEXPORT jboolean JNICALL
Java_org_dolphinemu_dolphinemu_features_netplay_NetPlayManager_netPlayIsHost(
    JNIEnv* env, jobject thiz) {
    
    return g_is_host ? JNI_TRUE : JNI_FALSE;
}

extern "C" JNIEXPORT jboolean JNICALL
Java_org_dolphinemu_dolphinemu_features_netplay_NetPlayManager_netPlayIsConnected(
    JNIEnv* env, jobject thiz) {
    
    return g_is_connected ? JNI_TRUE : JNI_FALSE;
}

extern "C" JNIEXPORT jint JNICALL
Java_org_dolphinemu_dolphinemu_features_netplay_NetPlayManager_netPlayGetPlayerCount(
    JNIEnv* env, jobject thiz) {
    
    LOGI("NetPlay: Getting player count");
    
    try {
        if (g_netplay_client && g_netplay_client->IsConnected()) {
            // Get the actual player list from the NetPlay client
            auto players = g_netplay_client->GetPlayers();
            LOGI("NetPlay: Found %zu players connected", players.size());
            return static_cast<jint>(players.size());
        } else if (g_netplay_server && g_netplay_server->is_connected) {
            return 1; // For now, return 1 as the host
        } else {
            LOGI("NetPlay: Not connected, returning 0 players");
        return 0;
        }
    } catch (const std::exception& e) {
        LOGE("Exception getting player count: %s", e.what());
        return 0;
    } catch (...) {
        LOGE("Unknown exception getting player count");
        return 0;
    }
}

extern "C" JNIEXPORT jobjectArray JNICALL
Java_org_dolphinemu_dolphinemu_features_netplay_NetPlayManager_netPlayGetPlayerList(
    JNIEnv* env, jobject thiz) {
    
    LOGI("NetPlay: Getting player list");
    
    try {
        if (g_netplay_client && g_netplay_client->IsConnected()) {
            // Get the actual player list from the NetPlay client
            auto players = g_netplay_client->GetPlayers();
            LOGI("NetPlay: Retrieved %zu players from NetPlayClient", players.size());
            
            // Find the NetPlayPlayer class
            jclass playerClass = env->FindClass("org/dolphinemu/dolphinemu/features/netplay/NetPlayPlayer");
            if (!playerClass) {
            LOGE("Could not find NetPlayPlayer class");
            return nullptr;
        }
        
            // Create array with actual player count
            jobjectArray playerArray = env->NewObjectArray(static_cast<jsize>(players.size()), playerClass, nullptr);
            if (!playerArray) {
                LOGE("Failed to create player array");
                env->DeleteLocalRef(playerClass);
            return nullptr;
        }
        
            // Find the NetPlayPlayer constructor (id, nickname, isConnected)
            jmethodID constructor = env->GetMethodID(playerClass, "<init>", "(ILjava/lang/String;Z)V");
            if (!constructor) {
                LOGE("Could not find NetPlayPlayer constructor");
                env->DeleteLocalRef(playerClass);
            return nullptr;
        }
        
            // Populate the array with actual player data
            for (size_t i = 0; i < players.size(); i++) {
                const auto* player = players[i];
                jstring playerName = env->NewStringUTF(player->name.c_str());
                jobject playerObj = env->NewObject(playerClass, constructor, static_cast<jint>(player->pid), playerName, JNI_TRUE);
                
                env->SetObjectArrayElement(playerArray, static_cast<jsize>(i), playerObj);
                env->DeleteLocalRef(playerName);
                env->DeleteLocalRef(playerObj);
                
                LOGI("NetPlay: Added player %d: %s", player->pid, player->name.c_str());
            }
            
            env->DeleteLocalRef(playerClass);
            LOGI("NetPlay: Returning player list with %zu players", players.size());
            return playerArray;
                } else {
            // Return empty array if not connected
            jclass playerClass = env->FindClass("org/dolphinemu/dolphinemu/features/netplay/NetPlayPlayer");
            if (!playerClass) {
                LOGE("Could not find NetPlayPlayer class");
            return nullptr;
        }
        
            jobjectArray playerArray = env->NewObjectArray(0, playerClass, nullptr);
            env->DeleteLocalRef(playerClass);
            
            LOGI("NetPlay: Not connected, returning empty player list");
            return playerArray;
        }
        
        } catch (const std::exception& e) {
        LOGE("Exception getting player list: %s", e.what());
        return nullptr;
    } catch (...) {
        LOGE("Unknown exception getting player list");
        return nullptr;
    }
}

extern "C" JNIEXPORT jstring JNICALL
Java_org_dolphinemu_dolphinemu_features_netplay_NetPlayManager_netPlayGetPlayerName(
    JNIEnv* env, jobject thiz, jint player_id) {
    
    LOGI("NetPlay: Getting player name for player %d", player_id);
    
    try {
        // For now, return a default player name to prevent crashes
        // This is a simplified implementation - you might need to implement proper player name retrieval
        return env->NewStringUTF("Player");
        } catch (const std::exception& e) {
        LOGE("Exception in netPlayGetPlayerName: %s", e.what());
        return env->NewStringUTF("Unknown");
    } catch (...) {
        LOGE("Unknown exception in netPlayGetPlayerName");
        return env->NewStringUTF("Unknown");
    }
}

extern "C" JNIEXPORT jboolean JNICALL
Java_org_dolphinemu_dolphinemu_features_netplay_NetPlayManager_netPlayIsHosting(
    JNIEnv* env, jobject thiz) {
    
    LOGI("NetPlay: Checking if hosting");
    
    try {
        // Check if we're hosting a NetPlay session
        if (g_netplay_server) {
                        return JNI_TRUE;
        }
            return JNI_FALSE;
    } catch (const std::exception& e) {
        LOGE("Exception in netPlayIsHosting: %s", e.what());
        return JNI_FALSE;
    } catch (...) {
        LOGE("Unknown exception in netPlayIsHosting");
        return JNI_FALSE;
    }
}

extern "C" JNIEXPORT jstring JNICALL
Java_org_dolphinemu_dolphinemu_features_netplay_NetPlayManager_netPlayGetGameName(
    JNIEnv* env, jobject thiz) {
    
    LOGI("NetPlay: Getting game name");
    
    try {
        // For now, return a default game name to prevent crashes
        // This is a simplified implementation - you might need to implement proper game name retrieval
        return env->NewStringUTF("Unknown Game");
    } catch (const std::exception& e) {
        LOGE("Exception in netPlayGetGameName: %s", e.what());
        return env->NewStringUTF("Unknown Game");
    } catch (...) {
        LOGE("Unknown exception in netPlayGetGameName");
        return env->NewStringUTF("Unknown Game");
    }
}

extern "C" JNIEXPORT jint JNICALL
Java_org_dolphinemu_dolphinemu_features_netplay_NetPlayManager_netPlayGetPort(
    JNIEnv* env, jobject thiz) {
    
    LOGI("NetPlay: Getting port");
    
    try {
        // Return the port we're using for NetPlay
        if (g_netplay_server) {
            return g_server_port;
        }
        return 0;
        } catch (const std::exception& e) {
        LOGE("Exception in netPlayGetPort: %s", e.what());
        return 0;
    } catch (...) {
        LOGE("Unknown exception in netPlayGetPort");
        return 0;
    }
}

// Function to set the NetPlay manager reference
extern "C" JNIEXPORT void JNICALL
Java_org_dolphinemu_dolphinemu_features_netplay_NetPlayManager_setNetPlayManagerReference(
    JNIEnv* env, jobject thiz) {
    
    LOGI("Setting NetPlay manager reference");
    
    // Store the reference to the NetPlay manager object
    if (g_netplay_manager) {
        env->DeleteGlobalRef(g_netplay_manager);
    }
    
    g_netplay_manager = env->NewGlobalRef(thiz);
    if (g_netplay_manager) {
        LOGI("NetPlay manager reference set successfully");
    } else {
        LOGE("Failed to set NetPlay manager reference");
    }
}

// Multiplayer initialization function - called from IDCache.cpp JNI_OnLoad
void InitializeMultiplayerJNI(JavaVM* vm) {
    if (!vm) {
        LOGE("Invalid JVM pointer passed to InitializeMultiplayerJNI");
        return;
    }
    
    g_jvm = vm;
    LOGI("Multiplayer JNI initialized successfully");
}

// Multiplayer cleanup function - called from IDCache.cpp JNI_Unload
void CleanupMultiplayerJNI() {
    // Clean up any remaining connections
    if (g_is_connected) {
        LOGI("Cleaning up active NetPlay connection");
        g_netplay_client.reset();
        g_netplay_server.reset();
        g_netplay_ui.reset();
        g_is_connected = false;
        g_is_host = false;
    }
    
    g_jvm = nullptr;
    g_netplay_manager = nullptr;
    
    LOGI("Multiplayer JNI cleaned up successfully");
}

extern "C" JNIEXPORT jboolean JNICALL
Java_org_dolphinemu_dolphinemu_features_netplay_NetPlayManager_netPlayCheckAndStartGame(
    JNIEnv* env, jobject thiz) {
    
    LOGI("NetPlay: Checking and starting game");
    
    try {
        // Check if we have an active NetPlay connection
        if (!g_netplay_client || !g_netplay_client->IsConnected()) {
            LOGE("No active NetPlay connection");
            return JNI_FALSE;
        }
        
        // Check if we have a game selected
        auto android_ui = dynamic_cast<AndroidNetPlayUI*>(g_netplay_ui.get());
        if (!android_ui || android_ui->GetCurrentSyncIdentifier().game_id.empty()) {
            LOGE("No game selected for NetPlay");
            return JNI_FALSE;
        }
        
        LOGI("NetPlay session ready - game ID: %s", android_ui->GetCurrentSyncIdentifier().game_id.c_str());
        
        // The actual game starting is handled by OnMsgStartGame() which gets called
        // when the NetPlay server sends the start signal. We just need to indicate
        // that the session is ready.
        
        // For now, return true to indicate the session is ready
        // The real game start will happen when OnMsgStartGame() is called
        return JNI_TRUE;
        
    } catch (const std::exception& e) {
        LOGE("Exception in netPlayCheckAndStartGame: %s", e.what());
        return JNI_FALSE;
    } catch (...) {
        LOGE("Unknown exception in netPlayCheckAndStartGame");
        return JNI_FALSE;
    }
}

// JNI method to send GameStatus confirmation from Java side
extern "C" JNIEXPORT void JNICALL
Java_org_dolphinemu_dolphinemu_features_netplay_NetPlayManager_sendGameStatusConfirmation(
    JNIEnv* env, jobject thiz, jboolean sameGame) {
    
    LOGI("NetPlay: Java side requesting GameStatus confirmation - sameGame: %s", sameGame ? "true" : "false");
    
    if (g_netplay_client && g_netplay_client->IsConnected()) {
        try {
            sf::Packet game_status_packet;
            game_status_packet << static_cast<u8>(NetPlay::MessageID::GameStatus);
            
            if (sameGame) {
                game_status_packet << static_cast<u32>(NetPlay::SyncIdentifierComparison::SameGame);
                LOGI("NetPlay: Sending SameGame status to host - Android client is ready!");
            } else {
                game_status_packet << static_cast<u32>(NetPlay::SyncIdentifierComparison::DifferentGame);
                LOGI("NetPlay: Sending DifferentGame status to host");
            }
            
            g_netplay_client->SendAsync(std::move(game_status_packet));
            LOGI("NetPlay: GameStatus confirmation sent to host successfully");
            
        } catch (const std::exception& e) {
            LOGE("NetPlay: Failed to send GameStatus confirmation: %s", e.what());
        }
    } else {
        LOGE("NetPlay: Cannot send GameStatus confirmation - NetPlay client not connected");
    }
}
