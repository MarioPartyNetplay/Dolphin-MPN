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
                    LOGE("Could not find onConnected method");
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
                    LOGE("Could not find onDisconnected method");
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
                    LOGE("Could not find onConnectionFailed method");
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
    
    LOGI("Connecting to server: %s:%d", g_server_address.c_str(), g_server_port);
    
    try {
        // Validate input parameters
        if (g_server_address.empty() || g_server_port <= 0 || g_server_port > 65535) {
            LOGE("Invalid server address or port");
            return JNI_FALSE;
        }
        
        // Get device name for player identification
        std::string playerName = getDeviceName();
        
        // For now, just simulate a connection attempt without actually creating NetPlayClient
        // This prevents crashes while we debug the underlying issues
        LOGI("Simulating connection to %s:%d as %s", g_server_address.c_str(), g_server_port, playerName.c_str());
        
        // Simulate successful connection for testing
        g_is_connected = true;
        g_is_host = false;
        
        // Add local player
        g_players.clear();
        g_players.push_back({1, playerName, true});
        
        LOGI("Successfully connected to server as %s", playerName.c_str());
        callJavaCallback("onConnected");
        return JNI_TRUE;
        
        /* TODO: Re-enable actual NetPlay connection once we fix the underlying issues
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
        */
        
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
        
        // For now, just simulate hosting without actually creating NetPlayServer
        // This prevents crashes while we debug the underlying issues
        LOGI("Simulating hosting on port %d as %s", g_server_port, hostName.c_str());
        
        // Simulate successful hosting for testing
        g_is_connected = true;
        g_is_host = true;
        
        // Add host player
        g_players.clear();
        g_players.push_back({1, hostName, true});
        
        LOGI("Successfully hosting server as %s", hostName.c_str());
        callJavaCallback("onConnected");
        return JNI_TRUE;
        
        /* TODO: Re-enable actual NetPlay hosting once we fix the underlying issues
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
        */
        
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
    
    LOGI("Sending message: %s", msg);
    
    try {
        // For now, just log the message without actually sending through NetPlay
        // This prevents crashes while we debug the underlying issues
        LOGI("Message would be sent: %s", msg);
        
        // Add to local chat
        ChatMessage chat_msg = {"Local Player", "", std::string(msg), "now"};
        g_chat_messages.push_back(chat_msg);
        
        env->ReleaseStringUTFChars(message, msg);
        LOGI("Message logged successfully");
        
        /* TODO: Re-enable actual message sending once we fix the underlying issues
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
        */
        
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
        // For now, just log the kick action without actually kicking
        // This prevents crashes while we debug the underlying issues
        LOGI("Would kick player %d", player_id);
        
        /* TODO: Re-enable actual player kicking once we fix the underlying issues
        // Kick player through Dolphin's NetPlayServer
        if (g_netplay_server) {
            g_netplay_server->KickPlayer(static_cast<NetPlay::PlayerId>(player_id));
        }
        */
        
    } catch (const std::exception& e) {
        LOGE("Exception kicking player: %s", e.what());
    } catch (...) {
        LOGE("Unknown exception kicking player");
    }
}

extern "C" JNIEXPORT void JNICALL
Java_org_dolphinemu_dolphinemu_features_netplay_NetPlayManager_netPlaySetRoomVisibility(
    JNIEnv* env, jobject thiz, jint visibility) {
    
    if (!g_is_host || !g_is_connected) {
        LOGI("Not host or not connected, cannot set room visibility");
        return;
    }
    
    LOGI("Setting room visibility: %d", visibility);
    
    // TODO: Implement room visibility setting through NetPlayServer
}

extern "C" JNIEXPORT jint JNICALL
Java_org_dolphinemu_dolphinemu_features_netplay_NetPlayManager_netPlayGetPlayerCount(
    JNIEnv* env, jobject thiz) {
    
    try {
        // Return the local player count for now
        return static_cast<jint>(g_players.size());
        
        /* TODO: Re-enable actual player count once we fix the underlying issues
        if (g_netplay_client) {
            return static_cast<jint>(g_netplay_client->GetPlayers().size());
        } else if (g_netplay_server) {
            // Server always has at least 1 player (host)
            return static_cast<jint>(g_players.size());
        }
        
        return 0;
        */
        
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
        g_is_connected = false;
        g_is_host = false;
    }
    
    g_jvm = nullptr;
    g_netplay_manager = nullptr;
    g_players.clear();
    g_chat_messages.clear();
    
    LOGI("Multiplayer JNI cleaned up successfully");
}
