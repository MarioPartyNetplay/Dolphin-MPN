#include "multiplayer.h"
#include <jni.h>
#include <android/log.h>
#include <string>
#include <vector>
#include <memory>

// Include Dolphin's NetPlay headers
#include "Core/NetPlayClient.h"
#include "Core/NetPlayServer.h"
#include "Core/NetPlayProto.h"
#include "Core/Config/NetplaySettings.h"
#include "UICommon/NetPlayIndex.h"
#include "Common/Config/Config.h"

#define LOG_TAG "NetPlay"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)

// Global variables for NetPlay state
static std::unique_ptr<NetPlay::NetPlayClient> g_netplay_client;
static std::unique_ptr<NetPlay::NetPlayServer> g_netplay_server;
static bool g_is_connected = false;
static bool g_is_host = false;
static std::string g_server_address;
static int g_server_port = 2626;
static std::vector<NetPlayPlayer> g_players;
static std::vector<ChatMessage> g_chat_messages;

// JNI environment and object references
static JavaVM* g_jvm = nullptr;
static jobject g_netplay_manager = nullptr;
static jmethodID g_on_message_received = nullptr;
static jmethodID g_on_player_joined = nullptr;
static jmethodID g_on_player_left = nullptr;
static jmethodID g_on_connection_lost = nullptr;

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

// JNI helper functions
JNIEnv* getJNIEnv() {
    JNIEnv* env;
    if (g_jvm->GetEnv(reinterpret_cast<void**>(&env), JNI_VERSION_1_6) != JNI_OK) {
        return nullptr;
    }
    return env;
}

void callJavaCallback(const char* method_name, ...) {
    JNIEnv* env = getJNIEnv();
    if (!env || !g_netplay_manager) return;
    
    // Implementation would call the appropriate Java callback method
    LOGI("Calling Java callback: %s", method_name);
}

// NetPlay implementation using Dolphin's classes
extern "C" JNIEXPORT jboolean JNICALL
Java_org_dolphinemu_dolphinemu_features_netplay_NetPlayManager_netPlayConnect(
    JNIEnv* env, jobject thiz, jstring address, jint port) {
    
    const char* addr = env->GetStringUTFChars(address, nullptr);
    g_server_address = std::string(addr);
    g_server_port = port;
    env->ReleaseStringUTFChars(address, addr);
    
    LOGI("Connecting to server: %s:%d", g_server_address.c_str(), g_server_port);
    
    try {
        // Validate input parameters
        if (g_server_address.empty() || g_server_port <= 0 || g_server_port > 65535) {
            LOGE("Invalid server address or port");
            return JNI_FALSE;
        }
        
        // Get device name for player identification
        std::string playerName = getDeviceName();
        
        // Create NetPlayClient using Dolphin's implementation
        g_netplay_client = std::make_unique<NetPlay::NetPlayClient>(
            g_server_address, g_server_port, nullptr, playerName,
            NetPlay::NetTraversalConfig{false, "", 0, 0}
        );
        
        if (g_netplay_client && g_netplay_client->IsConnected()) {
            g_is_connected = true;
            g_is_host = false;
            
            // Add local player
            g_players.clear();
            g_players.push_back({1, playerName, true});
            
            LOGI("Successfully connected to server as %s", playerName.c_str());
            callJavaCallback("onConnected");
            return JNI_TRUE;
        } else {
            LOGE("Failed to connect to server");
            g_netplay_client.reset();
            return JNI_FALSE;
        }
    } catch (const std::exception& e) {
        LOGE("Exception during connection: %s", e.what());
        g_netplay_client.reset();
        return JNI_FALSE;
    } catch (...) {
        LOGE("Unknown exception during connection");
        g_netplay_client.reset();
        return JNI_FALSE;
    }
}

extern "C" JNIEXPORT jboolean JNICALL
Java_org_dolphinemu_dolphinemu_features_netplay_NetPlayManager_netPlayHost(
    JNIEnv* env, jobject thiz, jint port) {
    
    g_server_port = port;
    LOGI("Hosting server on port: %d", g_server_port);
    
    try {
        // Validate port number
        if (port <= 0 || port > 65535) {
            LOGE("Invalid port number: %d", port);
            return JNI_FALSE;
        }
        
        // Get device name for host identification
        std::string hostName = getDeviceName() + " (Host)";
        
        // Create NetPlayServer using Dolphin's implementation
        g_netplay_server = std::make_unique<NetPlay::NetPlayServer>(
            g_server_port, false, nullptr,
            NetPlay::NetTraversalConfig{false, "", 0, 0}
        );
        
        if (g_netplay_server && g_netplay_server->is_connected) {
            g_is_connected = true;
            g_is_host = true;
            
            // Add host player
            g_players.clear();
            g_players.push_back({1, hostName, true});
            
            LOGI("Successfully hosting server as %s", hostName.c_str());
            callJavaCallback("onConnected");
            return JNI_TRUE;
        } else {
            LOGE("Failed to host server");
            g_netplay_server.reset();
            return JNI_FALSE;
        }
    } catch (const std::exception& e) {
        LOGE("Exception during hosting: %s", e.what());
        g_netplay_server.reset();
        return JNI_FALSE;
    } catch (...) {
        LOGE("Unknown exception during hosting");
        g_netplay_server.reset();
        return JNI_FALSE;
    }
}

extern "C" JNIEXPORT void JNICALL
Java_org_dolphinemu_dolphinemu_features_netplay_NetPlayManager_netPlayDisconnect(
    JNIEnv* env, jobject thiz) {
    
    LOGI("Disconnecting from server");
    
    // Clean up Dolphin's NetPlay objects
    g_netplay_client.reset();
    g_netplay_server.reset();
    
    g_is_connected = false;
    g_is_host = false;
    g_players.clear();
    g_chat_messages.clear();
    
    callJavaCallback("onDisconnected");
    LOGI("Disconnected from server");
}

extern "C" JNIEXPORT void JNICALL
Java_org_dolphinemu_dolphinemu_features_netplay_NetPlayManager_netPlaySendMessage(
    JNIEnv* env, jobject thiz, jstring message) {
    
    if (!g_is_connected) return;
    
    const char* msg = env->GetStringUTFChars(message, nullptr);
    LOGI("Sending message: %s", msg);
    
    try {
        // Send message through Dolphin's NetPlay
        if (g_netplay_client) {
            g_netplay_client->SendChatMessage(std::string(msg));
        } else if (g_netplay_server) {
            g_netplay_server->SendChatMessage(std::string(msg));
        }
        
        // Add to local chat
        ChatMessage chat_msg = {"Local Player", "", std::string(msg), "now"};
        g_chat_messages.push_back(chat_msg);
        
        env->ReleaseStringUTFChars(message, msg);
        LOGI("Message sent successfully");
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
    
    if (!g_is_host || !g_is_connected) return;
    
    LOGI("Kicking player: %d", player_id);
    
    try {
        // Kick player through Dolphin's NetPlayServer
        if (g_netplay_server) {
            g_netplay_server->KickPlayer(static_cast<NetPlay::PlayerId>(player_id));
        }
    } catch (const std::exception& e) {
        LOGE("Exception kicking player: %s", e.what());
    } catch (...) {
        LOGE("Unknown exception kicking player");
    }
}

extern "C" JNIEXPORT void JNICALL
Java_org_dolphinemu_dolphinemu_features_netplay_NetPlayManager_netPlaySetRoomVisibility(
    JNIEnv* env, jobject thiz, jint visibility) {
    
    if (!g_is_host || !g_is_connected) return;
    
    LOGI("Setting room visibility: %d", visibility);
    
    // TODO: Implement room visibility setting through NetPlayServer
}

extern "C" JNIEXPORT jint JNICALL
Java_org_dolphinemu_dolphinemu_features_netplay_NetPlayManager_netPlayGetPlayerCount(
    JNIEnv* env, jobject thiz) {
    
    try {
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
        // TODO: Return actual player list as Java objects
        // For now, return empty array
        jclass player_class = env->FindClass("org/dolphinemu/dolphinemu/features/netplay/NetPlayPlayer");
        if (!player_class) {
            LOGE("Could not find NetPlayPlayer class");
            return nullptr;
        }
        
        jobjectArray result = env->NewObjectArray(0, player_class, nullptr);
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

// NetPlay Browser JNI functions
extern "C" JNIEXPORT jobjectArray JNICALL
Java_org_dolphinemu_dolphinemu_features_netplay_NetPlayBrowser_fetchSessionsFromNetPlayIndex(
    JNIEnv* env, jobject thiz) {
    
    LOGI("Fetching sessions from NetPlayIndex");
    
    try {
        // TODO: Integrate with Dolphin's actual NetPlayIndex
        // For now, return empty array to trigger fallback to sample sessions
        
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
        
        // Create empty array
        jobjectArray result = env->NewObjectArray(0, sessionClass, nullptr);
        
        LOGI("Returning empty session array (will use fallback)");
        return result;
        
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
        // TODO: Integrate with Dolphin's actual game list
        // For now, return empty array to trigger fallback to sample games
        
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
        
        // Create empty array
        jobjectArray result = env->NewObjectArray(0, gameFileClass, nullptr);
        
        LOGI("Returning empty game array (will use fallback)");
        return result;
        
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
    g_jvm = vm;
    LOGI("Multiplayer JNI initialized");
}

// Multiplayer cleanup function - called from IDCache.cpp JNI_Unload
void CleanupMultiplayerJNI() {
    g_jvm = nullptr;
    g_netplay_manager = nullptr;
    LOGI("Multiplayer JNI cleaned up");
}
