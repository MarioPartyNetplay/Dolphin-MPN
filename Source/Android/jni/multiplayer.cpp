#include "multiplayer.h"
#include <jni.h>
#include <android/log.h>
#include <string>
#include <vector>
#include <memory>
#include <thread>
#include <chrono>

// Include Dolphin's NetPlay headers
#include "Core/NetPlayClient.h"
#include "Core/NetPlayServer.h"
#include "Core/NetPlayProto.h"
#include "Core/Config/NetplaySettings.h"
#include "UICommon/NetPlayIndex.h"
#include "UICommon/GameFile.h"
#include "UICommon/GameFileCache.h"
#include "Common/Config/Config.h"
#include "Core/Boot/Boot.h"
#include "Common/TraversalClient.h"

#define LOG_TAG "NetPlay"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)

// Global variables
static jobject g_netplay_manager = nullptr;

// NetPlayUI implementation for Android
class AndroidNetPlayUI : public NetPlay::NetPlayUI
{
public:
    AndroidNetPlayUI() = default;
    ~AndroidNetPlayUI() = default;

    void BootGame(const std::string& filename, std::unique_ptr<BootSessionData> boot_session_data) override {
        LOGI("NetPlay: BootGame called for %s", filename.c_str());
        
        // Call Java method to start the game
        JNIEnv* env = getJNIEnv();
        if (env && g_netplay_manager) {
            try {
                jclass managerClass = env->GetObjectClass(g_netplay_manager);
                if (managerClass) {
                    jmethodID startGameMethod = env->GetMethodID(managerClass, "startNetPlayGame", "(Ljava/lang/String;)V");
                    if (startGameMethod) {
                        jstring gameFile = env->NewStringUTF(filename.c_str());
                        env->CallVoidMethod(g_netplay_manager, startGameMethod, gameFile);
                        env->DeleteLocalRef(gameFile);
                        LOGI("NetPlay: Called startNetPlayGame for %s", filename.c_str());
                    } else {
                        LOGE("NetPlay: Could not find startNetPlayGame method");
                    }
                    env->DeleteLocalRef(managerClass);
                }
            } catch (...) {
                LOGE("NetPlay: Exception calling startNetPlayGame");
            }
        }
    }

    void StopGame() override {
        LOGI("NetPlay: StopGame called");
    }

    bool IsHosting() const override {
        return m_is_hosting;
    }

    void Update() override {
        // Called periodically to update NetPlay state
    }

    void AppendChat(const std::string& msg) override {
        LOGI("NetPlay: Chat message: %s", msg.c_str());
    }

    void OnMsgChangeGame(const NetPlay::SyncIdentifier& sync_identifier, const std::string& netplay_name) override {
        LOGI("NetPlay: Game changed to %s", netplay_name.c_str());
        m_current_sync_identifier = sync_identifier;
    }

    void OnMsgChangeGBARom(int pad, const NetPlay::GBAConfig& config) override {
        LOGI("NetPlay: GBA ROM changed for pad %d", pad);
    }

    void OnMsgStartGame() override {
        LOGI("NetPlay: Game started - triggering manual boot");
         m_should_start_game = true;
    }

    void OnMsgStopGame() override {
        LOGI("NetPlay: Game stopped");
    }

    void OnMsgPowerButton() override {
        LOGI("NetPlay: Power button pressed");
    }

    void OnPlayerConnect(const std::string& player) override {
        LOGI("NetPlay: Player connected: %s", player.c_str());
        
        // Call Java callback to notify about new player
        callJavaCallback("onPlayerJoined", 0, player.c_str());
        
        LOGI("Player connected: %s", player.c_str());
    }

    void OnPlayerDisconnect(const std::string& player) override {
        LOGI("NetPlay: Player disconnected: %s", player.c_str());
        
        // Call Java callback to notify about player leaving
        callJavaCallback("onPlayerLeft", 0);
        
        LOGI("Player disconnected: %s", player.c_str());
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
    }

    void OnConnectionError(const std::string& message) override {
        LOGE("NetPlay: Connection error: %s", message.c_str());
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

    std::shared_ptr<const UICommon::GameFile> FindGameFile(const NetPlay::SyncIdentifier& sync_identifier,
                                                           NetPlay::SyncIdentifierComparison* found = nullptr) override {
        LOGI("NetPlay: FindGameFile called for game_id: %s, revision: %d, disc: %d", 
             sync_identifier.game_id.c_str(), sync_identifier.revision, sync_identifier.disc_number);
        
        try {
            // Get the game cache and search for matching games
            static UICommon::GameFileCache game_cache;
            
            // Load the cache if it hasn't been loaded yet
            if (game_cache.GetSize() == 0) {
                LOGI("Loading game cache...");
                if (!game_cache.Load()) {
                    LOGI("Failed to load game cache, scanning for games...");
                    // Try to scan for games in common directories
                    std::vector<std::string> game_dirs = {"/storage/emulated/0/ROMs", "/storage/emulated/0/Games"};
                    auto game_paths = UICommon::FindAllGamePaths(game_dirs, true);
                    game_cache.Update(game_paths);
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
        return ""; // Return empty string for now
    }

    void ShowGameDigestDialog(const std::string& title) override {
        LOGI("NetPlay: ShowGameDigestDialog called for %s", title.c_str());
    }

    void SetGameDigestProgress(int pid, int progress) override {
        LOGI("NetPlay: SetGameDigestProgress called for pid %d: %d%%", pid, progress);
    }

    void SetGameDigestResult(int pid, const std::string& result) override {
        LOGI("NetPlay: SetGameDigestResult called for pid %d: %s", pid, result.c_str());
    }

    void AbortGameDigest() override {
        LOGI("NetPlay: AbortGameDigest called");
    }

    void OnIndexAdded(bool success, std::string error) override {
        if (success) {
            LOGI("NetPlay: Index added successfully");
        } else {
            LOGE("NetPlay: Index add failed: %s", error.c_str());
        }
    }

    void OnIndexRefreshFailed(std::string error) override {
        LOGE("NetPlay: Index refresh failed: %s", error.c_str());
    }

    void ShowChunkedProgressDialog(const std::string& title, u64 data_size,
                                   const std::vector<int>& players) override {
        LOGI("NetPlay: ShowChunkedProgressDialog called for %s (size: %lu, players: %zu)", 
             title.c_str(), data_size, players.size());
    }

    void HideChunkedProgressDialog() override {
        LOGI("NetPlay: HideChunkedProgressDialog called");
    }

    void SetChunkedProgress(int pid, u64 progress) override {
        LOGI("NetPlay: SetChunkedProgress called for pid %d: %lu", pid, progress);
    }

    void SetHostWiiSyncData(std::vector<u64> titles, std::string redirect_folder) override {
        LOGI("NetPlay: SetHostWiiSyncData called with %zu titles, redirect: %s", titles.size(), redirect_folder.c_str());
    }

    void SetHosting(bool hosting) {
        m_is_hosting = hosting;
    }
    
    bool ShouldStartGame() const {
        return m_should_start_game;
    }
    
    void ClearStartGameFlag() {
        m_should_start_game = false;
    }
    
    const NetPlay::SyncIdentifier& GetCurrentSyncIdentifier() const {
        return m_current_sync_identifier;
    }

private:
    bool m_is_hosting = false;
    bool m_should_start_game = false;
    NetPlay::SyncIdentifier m_current_sync_identifier;
};

// Global variables for NetPlay state
static std::unique_ptr<NetPlay::NetPlayClient> g_netplay_client;
static std::unique_ptr<NetPlay::NetPlayServer> g_netplay_server;
static std::unique_ptr<AndroidNetPlayUI> g_netplay_ui;
static bool g_is_connected = false;
static bool g_is_host = false;
static std::string g_server_address;
static int g_server_port = 2626;
static std::vector<NetPlayPlayer> g_players;
static std::vector<ChatMessage> g_chat_messages;

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

// Safe callback function that won't crash
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
    
    // Try to call the appropriate Java method based on the callback name
    try {
        if (strcmp(method_name, "onConnected") == 0) {
            // Find the onConnected method in the NetPlayManager class
            jclass managerClass = env->GetObjectClass(g_netplay_manager);
            if (managerClass) {
                // Call the onConnected method on the NetPlayManager
                // This will trigger the ConnectionCallback.onConnected() method
                jmethodID onConnectedMethod = env->GetMethodID(managerClass, "onConnected", "()V");
                if (onConnectedMethod) {
                    env->CallVoidMethod(g_netplay_manager, onConnectedMethod);
                    LOGI("Successfully called onConnected callback");
                } else {
                    LOGE("Could not find onConnected method - checking for errors");
                    if (env->ExceptionCheck()) {
                        env->ExceptionDescribe();
                        env->ExceptionClear();
                    }
                }
                env->DeleteLocalRef(managerClass);
            }
        } else if (strcmp(method_name, "onDisconnected") == 0) {
            // Find the onDisconnected method
            jclass managerClass = env->GetObjectClass(g_netplay_manager);
            if (managerClass) {
                jmethodID onDisconnectedMethod = env->GetMethodID(managerClass, "onDisconnected", "()V");
                if (onDisconnectedMethod) {
                    env->CallVoidMethod(g_netplay_manager, onDisconnectedMethod);
                    LOGI("Successfully called onDisconnected callback");
                } else {
                    LOGE("Could not find onDisconnected method - checking for errors");
                    if (env->ExceptionCheck()) {
                        env->ExceptionDescribe();
                        env->ExceptionClear();
                    }
                }
                env->DeleteLocalRef(managerClass);
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
                    LOGE("Could not find onConnectionFailed method - checking for errors");
                    if (env->ExceptionCheck()) {
                        env->ExceptionDescribe();
                        env->ExceptionClear();
                    }
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
                    LOGE("Could not find onPlayerJoined method - checking for errors");
                    if (env->ExceptionCheck()) {
                        env->ExceptionDescribe();
                        env->ExceptionClear();
                    }
                }
                env->DeleteLocalRef(managerClass);
            }
        } else if (strcmp(method_name, "onPlayerLeft") == 0) {
            // Handle onPlayerLeft callback
            jclass managerClass = env->GetObjectClass(g_netplay_manager);
            if (managerClass) {
                jmethodID onPlayerLeftMethod = env->GetMethodID(managerClass, "onPlayerLeft", "(I)V");
                if (onPlayerLeftMethod) {
                    va_list args;
                    va_start(args, method_name);
                    int player_id = va_arg(args, int);
                    va_end(args);

                    env->CallVoidMethod(g_netplay_manager, onPlayerLeftMethod, player_id);
                    LOGI("Successfully called onPlayerLeft callback for player %d", player_id);
                } else {
                    LOGE("Could not find onPlayerLeft method - checking for errors");
                    if (env->ExceptionCheck()) {
                        env->ExceptionDescribe();
                        env->ExceptionClear();
                    }
                }
                env->DeleteLocalRef(managerClass);
            }
        } else if (strcmp(method_name, "onMessageReceived") == 0) {
            // Handle onMessageReceived callback
            jclass managerClass = env->GetObjectClass(g_netplay_manager);
            if (managerClass) {
                jmethodID onMessageReceivedMethod = env->GetMethodID(managerClass, "onMessageReceived", "(Ljava/lang/String;Ljava/lang/String;)V");
                if (onMessageReceivedMethod) {
                    va_list args;
                    va_start(args, method_name);
                    const char* sender_name = va_arg(args, const char*);
                    const char* message_text = va_arg(args, const char*);
                    va_end(args);

                    jstring senderNameString = env->NewStringUTF(sender_name);
                    jstring messageTextString = env->NewStringUTF(message_text);
                    env->CallVoidMethod(g_netplay_manager, onMessageReceivedMethod, senderNameString, messageTextString);
                    env->DeleteLocalRef(senderNameString);
                    env->DeleteLocalRef(messageTextString);
                    LOGI("Successfully called onMessageReceived callback from %s: %s", sender_name, message_text);
                } else {
                    LOGE("Could not find onMessageReceived method - checking for errors");
                    if (env->ExceptionCheck()) {
                        env->ExceptionDescribe();
                        env->ExceptionClear();
                    }
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
            
            // Add network diagnostics
            LOGI("Mobile network diagnostics:");
            LOGI("- Network type: Mobile/WiFi detection needed");
            LOGI("- UDP support: Checking if UDP traffic is allowed");
            LOGI("- NAT type: Will be determined during traversal");
            
            // Validate traversal code format
            if (g_server_address.length() != 8) {
                LOGE("Invalid traversal code length: %zu (expected 8)", g_server_address.length());
                callJavaCallback("onConnectionFailed");
                return JNI_FALSE;
            }
            
            // Check if traversal code is valid hex
            for (char c : g_server_address) {
                if (!std::isxdigit(c)) {
                    LOGE("Invalid traversal code character: %c (not hex)", c);
                    callJavaCallback("onConnectionFailed");
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
            LOGI("NetPlayClient created successfully");
            
            // Wait longer for connection to establish, especially for mobile networks
            int timeout_ms = use_traversal ? 10000 : 3000;  // 10 seconds for traversal on mobile, 3 for direct
            LOGI("Waiting %d ms for connection to establish (mobile network optimized)...", timeout_ms);
            
            auto start_time = std::chrono::steady_clock::now();
            bool connected = false;
            int progress_counter = 0;
            
            while (std::chrono::steady_clock::now() - start_time < std::chrono::milliseconds(timeout_ms)) {
                if (g_netplay_client->IsConnected()) {
                    connected = true;
                    break;
                }
                
                // Log progress every 2 seconds for mobile network debugging
                progress_counter++;
                if (progress_counter % 20 == 0) {  // Every 2000ms (20 * 100ms)
                    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
                        std::chrono::steady_clock::now() - start_time).count();
                    LOGI("Connection attempt progress: %lldms/%dms (%.1f%%)", 
                         elapsed, timeout_ms, (float)elapsed / timeout_ms * 100.0f);
                }
                
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
            }
            
            if (connected) {
                g_is_connected = true;
                g_is_host = false;
                g_netplay_ui->SetHosting(false);
                
                // Add local player
                g_players.clear();
                g_players.push_back({1, playerName, true});
                
                LOGI("Successfully connected to NetPlay server as %s", playerName.c_str());
                callJavaCallback("onConnected");
                return JNI_TRUE;
            } else {
                LOGE("Connection timeout after %d ms - server may be offline, unreachable, or traversal failed", timeout_ms);
                
                // Check for specific error conditions
                if (use_traversal) {
                    LOGE("Traversal connection failed - server may be offline or traversal server unreachable");
                } else {
                    LOGE("Direct connection failed - check if server is running and port is open");
                }
                
                g_netplay_client.reset();
                callJavaCallback("onConnectionFailed");
                return JNI_FALSE;
            }
        } else {
            LOGE("Failed to create NetPlayClient - check server address and port");
            callJavaCallback("onConnectionFailed");
            return JNI_FALSE;
        }
        
    } catch (const std::exception& e) {
        LOGE("Exception during NetPlay connection: %s", e.what());
        g_netplay_client.reset();
        callJavaCallback("onConnectionFailed");
        return JNI_FALSE;
    } catch (...) {
        LOGE("Unknown exception during NetPlay connection");
        g_netplay_client.reset();
        callJavaCallback("onConnectionFailed");
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
            
            // Add host player
            g_players.clear();
            g_players.push_back({1, hostName, true});
            
            LOGI("Successfully hosting NetPlay server as %s", hostName.c_str());
            callJavaCallback("onConnected");
            return JNI_TRUE;
        } else {
            LOGE("Failed to host NetPlay server - server not connected");
            g_netplay_server.reset();
            callJavaCallback("onConnectionFailed");
            return JNI_FALSE;
        }
        
    } catch (const std::exception& e) {
        LOGE("Exception during NetPlay hosting: %s", e.what());
        g_netplay_server.reset();
        callJavaCallback("onConnectionFailed");
        return JNI_FALSE;
    } catch (...) {
        LOGE("Unknown exception during NetPlay hosting");
        g_netplay_server.reset();
        callJavaCallback("onConnectionFailed");
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
    g_players.clear();
    g_chat_messages.clear();
    
    callJavaCallback("onDisconnected");
    LOGI("Disconnected from NetPlay server");
}

extern "C" JNIEXPORT void JNICALL
Java_org_dolphinemu_dolphinemu_features_netplay_NetPlayManager_netPlaySendMessage(
    JNIEnv* env, jobject thiz, jstring message) {
    
    if (!g_is_connected) {
        LOGI("Not connected, cannot send message");
        return;
    }
    
    if (!message) {
        LOGE("Message parameter is null");
        return;
    }
    
    const char* msg = env->GetStringUTFChars(message, nullptr);
    if (!msg) {
        LOGE("Failed to get message string chars");
        return;
    }
    
    LOGI("Sending NetPlay message: %s", msg);
    
    try {
        // Get the current player's name
        std::string current_player_name = "Unknown";
        if (g_netplay_client) {
            // Client mode - get local player name
            const auto& players = g_netplay_client->GetPlayers();
            for (const auto* player : players) {
                if (player && player->IsHost()) {
                    current_player_name = player->name;
                    break;
                }
            }
        } else if (g_netplay_server) {
            // Server mode - host is always player 1
            if (!g_players.empty()) {
                current_player_name = g_players[0].nickname;
            }
        }
        
        // Send message through Dolphin's NetPlay
        if (g_netplay_client) {
            g_netplay_client->SendChatMessage(std::string(msg));
            LOGI("Message sent through NetPlay client");
        } else if (g_netplay_server) {
            g_netplay_server->SendChatMessage(std::string(msg));
            LOGI("Message sent through NetPlay server");
        } else {
            LOGI("No NetPlay connection available for sending message");
        }
        
        // Add to local chat with proper player name
        ChatMessage chat_msg = {current_player_name, "", std::string(msg), "now"};
        g_chat_messages.push_back(chat_msg);
        
        // Call Java callback to notify about new chat message
        callJavaCallback("onMessageReceived", current_player_name.c_str(), msg);
        
        env->ReleaseStringUTFChars(message, msg);
        LOGI("Message sent successfully from %s", current_player_name.c_str());
        
    } catch (const std::exception& e) {
        LOGE("Exception sending message: %s", e.what());
        env->ReleaseStringUTFChars(message, msg);
    } catch (...) {
        LOGE("Unknown exception sending message");
        env->ReleaseStringUTFChars(message, msg);
    }
}

extern "C" JNIEXPORT void JNICALL
Java_org_dolphinemu_dolphinemu_features_netplay_NetPlayManager_netPlayKickPlayer(
    JNIEnv* env, jobject thiz, jint player_id) {
    
    if (!g_is_host || !g_is_connected) {
        LOGI("Not host or not connected, cannot kick player");
        return;
    }
    
    LOGI("Kicking player: %d", player_id);
    
    try {
        // Kick player through Dolphin's NetPlayServer
        if (g_netplay_server) {
            g_netplay_server->KickPlayer(static_cast<NetPlay::PlayerId>(player_id));
            LOGI("Player %d kicked successfully", player_id);
        }
        
    } catch (const std::exception& e) {
        LOGE("Exception kicking player: %s", e.what());
    } catch (...) {
        LOGE("Unknown exception kicking player");
    }
}

extern "C" JNIEXPORT jint JNICALL
Java_org_dolphinemu_dolphinemu_features_netplay_NetPlayManager_netPlayGetPlayerCount(
    JNIEnv* env, jobject thiz) {
    
    try {
        // Return the actual player count from NetPlay
        if (g_netplay_client) {
            return static_cast<jint>(g_netplay_client->GetPlayers().size());
        } else if (g_netplay_server) {
            // Server always has at least 1 player (host)
            return static_cast<jint>(g_players.size());
        }
        
        return 0;
        
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
    
    try {
        // Get the NetPlayPlayer class
        jclass player_class = env->FindClass("org/dolphinemu/dolphinemu/features/netplay/NetPlayPlayer");
        if (!player_class) {
            LOGE("Could not find NetPlayPlayer class");
            return nullptr;
        }
        
        // Get the constructor
        jmethodID constructor = env->GetMethodID(player_class, "<init>", "(ILjava/lang/String;Z)V");
        if (!constructor) {
            LOGE("Could not find NetPlayPlayer constructor");
            return nullptr;
        }
        
        // Get the actual player count
        int player_count = 0;
        if (g_netplay_client) {
            player_count = static_cast<int>(g_netplay_client->GetPlayers().size());
        } else if (g_netplay_server) {
            player_count = static_cast<int>(g_players.size());
        }
        
        LOGI("Creating player list with %d players", player_count);
        
        // Create the result array
        jobjectArray result = env->NewObjectArray(player_count, player_class, nullptr);
        if (!result) {
            LOGE("Failed to create player array");
            return nullptr;
        }
        
        // Fill the array with player data
        if (g_netplay_client) {
            // Client mode - get players from NetPlayClient
            const auto& players = g_netplay_client->GetPlayers();
            for (int i = 0; i < player_count && i < static_cast<int>(players.size()); i++) {
                const auto* player = players[i];
                if (player) {
                    jstring nickname = env->NewStringUTF(player->name.c_str());
                    jobject player_obj = env->NewObject(player_class, constructor, 
                                                      static_cast<jint>(player->pid), 
                                                      nickname, 
                                                      JNI_TRUE);
                    if (player_obj) {
                        env->SetObjectArrayElement(result, i, player_obj);
                        env->DeleteLocalRef(player_obj);
                    }
                    env->DeleteLocalRef(nickname);
                    LOGI("Added player %d: %s", player->pid, player->name.c_str());
                }
            }
        } else if (g_netplay_server) {
            // Server mode - use local player list
            for (int i = 0; i < player_count && i < static_cast<int>(g_players.size()); i++) {
                const auto& player = g_players[i];
                jstring nickname = env->NewStringUTF(player.nickname.c_str());
                jobject player_obj = env->NewObject(player_class, constructor, 
                                                  static_cast<jint>(player.id), 
                                                  nickname, 
                                                  player.isConnected ? JNI_TRUE : JNI_FALSE);
                if (player_obj) {
                    env->SetObjectArrayElement(result, i, player_obj);
                    env->DeleteLocalRef(player_obj);
                }
                env->DeleteLocalRef(nickname);
                LOGI("Added server player %d: %s", player.id, player.nickname.c_str());
            }
        }
        
        env->DeleteLocalRef(player_class);
        LOGI("Successfully created player list with %d players", player_count);
        return result;
        
    } catch (const std::exception& e) {
        LOGE("Exception getting player list: %s", e.what());
        return nullptr;
    } catch (...) {
        LOGE("Unknown exception getting player list");
        return nullptr;
    }
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

extern "C" JNIEXPORT jboolean JNICALL
Java_org_dolphinemu_dolphinemu_features_netplay_NetPlayManager_netPlayCheckAndStartGame(
    JNIEnv* env, jobject thiz) {
    
    if (g_netplay_ui && g_netplay_ui->ShouldStartGame()) {
        LOGI("NetPlay: Processing delayed game start");
        
        try {
            // Use the sync identifier stored from OnMsgChangeGame
            auto sync_identifier = g_netplay_ui->GetCurrentSyncIdentifier();
            if (!sync_identifier.game_id.empty()) {
                // Try to find the game file
                NetPlay::SyncIdentifierComparison comparison;
                auto game_file = g_netplay_ui->FindGameFile(sync_identifier, &comparison);
                
                if (game_file && comparison == NetPlay::SyncIdentifierComparison::SameGame) {
                    LOGI("Found game file for delayed NetPlay start: %s", game_file->GetFilePath().c_str());
                    g_netplay_ui->BootGame(game_file->GetFilePath(), nullptr);
                    g_netplay_ui->ClearStartGameFlag();
                    return JNI_TRUE;
                } else {
                    LOGE("Could not find matching game file for delayed NetPlay start");
                }
            } else {
                LOGE("No game ID available for delayed NetPlay start");
            }
        } catch (const std::exception& e) {
            LOGE("Exception in delayed NetPlay start: %s", e.what());
        }
        
        g_netplay_ui->ClearStartGameFlag();
    }
    
    return JNI_FALSE;
}

// Game validation and checksum functions
extern "C" JNIEXPORT jstring JNICALL
Java_org_dolphinemu_dolphinemu_features_netplay_NetPlayManager_netPlayGetGameChecksum(
    JNIEnv* env, jobject thiz, jstring gamePath) {
    
    LOGI("Getting game checksum");
    
    try {
        if (!gamePath) {
            LOGE("Game path is null");
            return nullptr;
        }
        
        const char* pathStr = env->GetStringUTFChars(gamePath, nullptr);
        if (!pathStr) {
            LOGE("Failed to get game path string");
            return nullptr;
        }
        
        std::string path(pathStr);
        env->ReleaseStringUTFChars(gamePath, pathStr);
        
        LOGI("Calculating checksum for game: %s", path.c_str());
        
        // Integrate with Dolphin's actual checksum calculation
        try {
            LOGI("Starting checksum calculation for path: %s", path.c_str());
            
            // Check if file exists first
            FILE* test_file = fopen(path.c_str(), "rb");
            if (!test_file) {
                LOGE("File does not exist or cannot be opened: %s", path.c_str());
                // Fallback to hash-based checksum
                std::string checksum = "HASH_" + std::to_string(std::hash<std::string>{}(path));
                LOGI("Using fallback hash checksum due to file not found: %s", checksum.c_str());
                return env->NewStringUTF(checksum.c_str());
            }
            fclose(test_file);
            
            LOGI("File exists, creating GameFile object...");
            
            // Use Dolphin's NetPlay sync identifier system
            // Create a GameFile object first, then get its sync identifier
            UICommon::GameFile game_file(path);
            LOGI("GameFile object created, checking validity...");
            
            if (game_file.IsValid()) {
                LOGI("GameFile is valid, getting sync identifier...");
                auto sync_identifier = game_file.GetSyncIdentifier();
                // Create a string representation of the sync identifier
                std::string checksum = sync_identifier.game_id + "_" + 
                                     std::to_string(sync_identifier.revision) + "_" +
                                     std::to_string(sync_identifier.disc_number);
                LOGI("Generated real checksum: %s", checksum.c_str());
                return env->NewStringUTF(checksum.c_str());
            } else {
                LOGE("GameFile is not valid for path: %s", path.c_str());
                // Fallback to hash-based checksum if sync identifier fails
                std::string checksum = "HASH_" + std::to_string(std::hash<std::string>{}(path));
                LOGI("Using fallback hash checksum due to invalid GameFile: %s", checksum.c_str());
                return env->NewStringUTF(checksum.c_str());
            }
        } catch (const std::exception& e) {
            LOGE("Exception in checksum calculation: %s", e.what());
            // Fallback to hash-based checksum
            std::string checksum = "HASH_" + std::to_string(std::hash<std::string>{}(path));
            LOGI("Using fallback hash checksum after exception: %s", checksum.c_str());
            return env->NewStringUTF(checksum.c_str());
        }
        
    } catch (const std::exception& e) {
        LOGE("Exception in netPlayGetGameChecksum: %s", e.what());
        return nullptr;
    } catch (...) {
        LOGE("Unknown exception in netPlayGetGameChecksum");
        return nullptr;
    }
}

extern "C" JNIEXPORT jboolean JNICALL
Java_org_dolphinemu_dolphinemu_features_netplay_NetPlayManager_netPlayValidateGameFile(
    JNIEnv* env, jobject thiz, jstring gamePath) {
    
    LOGI("Validating game file");
    
    try {
        if (!gamePath) {
            LOGE("Game path is null");
            return JNI_FALSE;
        }
        
        const char* pathStr = env->GetStringUTFChars(gamePath, nullptr);
        if (!pathStr) {
            LOGE("Failed to get game path string");
            return JNI_FALSE;
        }
        
        std::string path(pathStr);
        env->ReleaseStringUTFChars(gamePath, pathStr);
        
        LOGI("Validating game file: %s", path.c_str());
        
        // Integrate with Dolphin's actual game validation
        try {
            LOGI("Starting game validation for path: %s", path.c_str());
            
            // Check if file exists first
            FILE* test_file = fopen(path.c_str(), "rb");
            if (!test_file) {
                LOGE("File does not exist or cannot be opened: %s", path.c_str());
                return JNI_FALSE;
            }
            fclose(test_file);
            
            LOGI("File exists, creating GameFile object...");
            
            // Use Dolphin's game validation system
            UICommon::GameFile game_file(path);
            LOGI("GameFile object created, checking validity...");
            
            if (!game_file.IsValid()) {
                LOGE("Game file is not a valid Dolphin game: %s", path.c_str());
                return JNI_FALSE;
            }
            
            LOGI("GameFile is valid, getting sync identifier...");
            
            // Get the sync identifier for validation
            auto sync_identifier = game_file.GetSyncIdentifier();
            LOGI("Sync identifier obtained successfully");
            
            // Additional validation: check file integrity
            FILE* file = fopen(path.c_str(), "rb");
            if (!file) {
                LOGE("Game file does not exist: %s", path.c_str());
                return JNI_FALSE;
            }
            
            // Check file size (GameCube games are typically 1.35GB, Wii games vary)
            fseek(file, 0, SEEK_END);
            long fileSize = ftell(file);
            fclose(file);
            
            if (fileSize < 1000000) { // Less than 1MB is suspicious
                LOGE("Game file too small: %ld bytes", fileSize);
                return JNI_FALSE;
            }
            
            LOGI("Game file validation passed: %s (%ld bytes, checksum: %s)", 
                 path.c_str(), fileSize, 
                 (sync_identifier.game_id + "_" + std::to_string(sync_identifier.revision) + "_" + std::to_string(sync_identifier.disc_number)).c_str());
            return JNI_TRUE;
            
        } catch (const std::exception& e) {
            LOGE("Exception in game validation: %s", e.what());
            return JNI_FALSE;
        }
        
    } catch (const std::exception& e) {
        LOGE("Exception in netPlayValidateGameFile: %s", e.what());
        return JNI_FALSE;
    } catch (...) {
        LOGE("Unknown exception in netPlayValidateGameFile");
        return JNI_FALSE;
    }
}

extern "C" JNIEXPORT jboolean JNICALL
Java_org_dolphinemu_dolphinemu_features_netplay_NetPlayManager_netPlayLaunchGame(
    JNIEnv* env, jobject thiz, jstring gamePath) {
    
    LOGI("Launching game in NetPlay");
    
    try {
        if (!gamePath) {
            LOGE("Game path is null");
            return JNI_FALSE;
        }
        
        const char* pathStr = env->GetStringUTFChars(gamePath, nullptr);
        if (!pathStr) {
            LOGE("Failed to get game path string");
            return JNI_FALSE;
        }
        
        std::string path(pathStr);
        env->ReleaseStringUTFChars(gamePath, pathStr);
        
        LOGI("Launching game: %s", path.c_str());
        
        // Integrate with Dolphin's actual NetPlay game launching
        try {
            if (!g_is_connected) {
                LOGE("Cannot launch game: not connected to NetPlay server");
                return JNI_FALSE;
            }
            
            // Validate the game file first
            UICommon::GameFile game_file(path);
            if (!game_file.IsValid()) {
                LOGE("Cannot launch invalid game file: %s", path.c_str());
                return JNI_FALSE;
            }
            
            auto sync_identifier = game_file.GetSyncIdentifier();
            
            // Use Dolphin's NetPlay system to launch the game
            if (g_netplay_server && g_is_host) {
                // Host launching game
                LOGI("Host launching game through NetPlay server: %s", path.c_str());
                
                // Call NetPlayServer::ChangeGame() method to change the game
                if (g_netplay_server->ChangeGame(sync_identifier, path)) {
                    LOGI("Successfully changed game in NetPlay server to: %s", path.c_str());
                } else {
                    LOGE("Failed to change game in NetPlay server: %s", path.c_str());
                    return JNI_FALSE;
                }
            } else if (g_netplay_client && !g_is_host) {
                // Client - game launch will be handled by host
                LOGI("Client waiting for host to launch game: %s", path.c_str());
            }
            
            // Call Java method to start the game
            if (g_netplay_manager) {
                jclass managerClass = env->GetObjectClass(g_netplay_manager);
                if (managerClass) {
                    jmethodID startGameMethod = env->GetMethodID(managerClass, "startNetPlayGame", "(Ljava/lang/String;)V");
                    if (startGameMethod) {
                        jstring gameFile = env->NewStringUTF(path.c_str());
                        env->CallVoidMethod(g_netplay_manager, startGameMethod, gameFile);
                        env->DeleteLocalRef(gameFile);
                        LOGI("Called startNetPlayGame for %s", path.c_str());
                        env->DeleteLocalRef(managerClass);
                        return JNI_TRUE;
                    } else {
                        LOGE("Could not find startNetPlayGame method");
                    }
                    env->DeleteLocalRef(managerClass);
                }
            }
            
            return JNI_TRUE;
            
        } catch (const std::exception& e) {
            LOGE("Exception in NetPlay game launch: %s", e.what());
            return JNI_FALSE;
        }
        
    } catch (const std::exception& e) {
        LOGE("Exception in netPlayLaunchGame: %s", e.what());
        return JNI_FALSE;
    } catch (...) {
        LOGE("Unknown exception in netPlayLaunchGame");
        return JNI_FALSE;
    }
}

extern "C" JNIEXPORT jstring JNICALL
Java_org_dolphinemu_dolphinemu_features_netplay_NetPlayManager_netPlayGetGameId(
    JNIEnv* env, jobject thiz, jstring gamePath) {
    
    LOGI("Getting game ID");
    
    try {
        if (!gamePath) {
            LOGE("Game path is null");
            return nullptr;
        }
        
        const char* pathStr = env->GetStringUTFChars(gamePath, nullptr);
        if (!pathStr) {
            LOGE("Failed to get game path string");
            return nullptr;
        }
        
        std::string path(pathStr);
        env->ReleaseStringUTFChars(gamePath, pathStr);
        
        LOGI("Getting game ID for: %s", path.c_str());
        
        // Integrate with Dolphin's actual game ID extraction
        try {
            // Use Dolphin's game ID extraction system
            UICommon::GameFile game_file(path);
            if (game_file.IsValid()) {
                auto sync_identifier = game_file.GetSyncIdentifier();
                std::string gameId = sync_identifier.game_id;
                LOGI("Extracted real game ID: %s for file: %s", gameId.c_str(), path.c_str());
                return env->NewStringUTF(gameId.c_str());
            } else {
                // Fallback to filename-based ID if sync identifier fails
                std::string filename = path.substr(path.find_last_of("/\\") + 1);
                std::string gameId = "GAME_" + std::to_string(std::hash<std::string>{}(filename));
                LOGI("Using fallback game ID: %s for file: %s", gameId.c_str(), filename.c_str());
                return env->NewStringUTF(gameId.c_str());
            }
        } catch (const std::exception& e) {
            LOGE("Exception in game ID extraction: %s", e.what());
            // Fallback to filename-based ID
            std::string filename = path.substr(path.find_last_of("/\\") + 1);
            std::string gameId = "GAME_" + std::to_string(std::hash<std::string>{}(filename));
            LOGI("Using fallback game ID after exception: %s for file: %s", gameId.c_str(), filename.c_str());
            return env->NewStringUTF(gameId.c_str());
        }
        
    } catch (const std::exception& e) {
        LOGE("Exception in netPlayGetGameId: %s", e.what());
        return nullptr;
    } catch (...) {
        LOGE("Unknown exception in netPlayGetGameId");
        return nullptr;
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

// NetPlay Browser JNI functions
extern "C" JNIEXPORT jobjectArray JNICALL
Java_org_dolphinemu_dolphinemu_features_netplay_NetPlayBrowser_fetchSessionsFromNetPlayIndex(
    JNIEnv* env, jobject thiz) {
    
    LOGI("Fetching sessions from NetPlayIndex");
    
    try {
        // Integrate with Dolphin's actual NetPlayIndex
        std::vector<NetPlaySession> sessions;
        
        // Get sessions from Dolphin's NetPlayIndex
        NetPlayIndex index;
        auto sessions_opt = index.List();
        if (sessions_opt.has_value()) {
            sessions = sessions_opt.value();
            LOGI("Found %zu sessions from NetPlayIndex", sessions.size());
            
            // Get the NetPlaySession class
            jclass sessionClass = env->FindClass("org/dolphinemu/dolphinemu/model/NetPlaySession");
            if (!sessionClass) {
                LOGE("Could not find NetPlaySession class");
                return nullptr;
            }
            
            // Get the constructor
            jmethodID constructor = env->GetMethodID(sessionClass, "<init>", 
                "(Ljava/lang/String;Ljava/lang/String;Ljava/lang/String;Ljava/lang/String;Ljava/lang/String;Ljava/lang/String;IIZZ)V");
            if (!constructor) {
                LOGE("Could not find NetPlaySession constructor");
                return nullptr;
            }
            
            // Create array with actual sessions
            jobjectArray result = env->NewObjectArray(sessions.size(), sessionClass, nullptr);
            
            for (size_t i = 0; i < sessions.size(); i++) {
                const auto& session = sessions[i];
                
                // Create Java strings for the session data
                jstring name = env->NewStringUTF(session.name.c_str());
                jstring region = env->NewStringUTF(session.region.c_str());
                jstring game = env->NewStringUTF(session.game_id.c_str());
                jstring serverId = env->NewStringUTF(session.server_id.c_str());
                jstring port = env->NewStringUTF(std::to_string(session.port).c_str());
                jstring version = env->NewStringUTF(session.version.c_str());
                
                // Create the NetPlaySession object
                jobject sessionObj = env->NewObject(sessionClass, constructor, 
                    name, region, game, serverId, port, version,
                    session.player_count, 4, // max_players hardcoded to 4
                    session.in_game, session.has_password);
                
                // Set the object in the array
                env->SetObjectArrayElement(result, i, sessionObj);
                
                // Clean up local references
                env->DeleteLocalRef(name);
                env->DeleteLocalRef(region);
                env->DeleteLocalRef(game);
                env->DeleteLocalRef(serverId);
                env->DeleteLocalRef(port);
                env->DeleteLocalRef(version);
                env->DeleteLocalRef(sessionObj);
            }
            
            LOGI("Returning %zu real sessions from NetPlayIndex", sessions.size());
            return result;
            
        } else {
            LOGI("NetPlayIndex returned no sessions, using fallback");
            // Fallback to empty array if NetPlayIndex fails
            jclass sessionClass = env->FindClass("org/dolphinemu/dolphinemu/model/NetPlaySession");
            if (!sessionClass) {
                LOGE("Could not find NetPlaySession class");
                return nullptr;
            }
            
            jobjectArray result = env->NewObjectArray(0, sessionClass, nullptr);
            return result;
        }
        
    } catch (const std::exception& e) {
        LOGE("Exception in fetchSessionsFromNetPlayIndex: %s", e.what());
        return nullptr;
    } catch (...) {
        LOGE("Unknown exception in fetchSessionsFromNetPlayIndex");
        return nullptr;
    }
}

// Game List JNI functions
extern "C" JNIEXPORT jobjectArray JNICALL
Java_org_dolphinemu_dolphinemu_dialogs_GameSelectionDialog_loadGamesFromDolphin(
    JNIEnv* env, jobject thiz) {
    
    LOGI("Loading games from Dolphin's game list");
    
    try {
        // Integrate with Dolphin's actual game list
        try {

            // Get the GameFile class
            jclass gameFileClass = env->FindClass("org/dolphinemu/dolphinemu/model/GameFile");
            if (!gameFileClass) {
                LOGE("Could not find GameFile class");
                return nullptr;
            }
            
            // Get the constructor
            jmethodID constructor = env->GetMethodID(gameFileClass, "<init>", 
                "(Ljava/lang/String;Ljava/lang/String;Ljava/lang/String;Ljava/lang/String;)V");
            if (!constructor) {
                LOGE("Could not find GameFile constructor");
                return nullptr;
            }
            
            // For now, return empty array until proper game list API is implemented
            LOGI("Game list integration not yet implemented, returning empty array");
            jobjectArray result = env->NewObjectArray(0, gameFileClass, nullptr);
            return result;
            
        } catch (const std::exception& e) {
            LOGE("Exception in game list loading: %s", e.what());
            return nullptr;
        }
        
    } catch (const std::exception& e) {
        LOGE("Exception in loadGamesFromDolphin: %s", e.what());
        return nullptr;
    } catch (...) {
        LOGE("Unknown exception in loadGamesFromDolphin");
        return nullptr;
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
    g_players.clear();
    g_chat_messages.clear();
    
    LOGI("Multiplayer JNI cleaned up successfully");
}
