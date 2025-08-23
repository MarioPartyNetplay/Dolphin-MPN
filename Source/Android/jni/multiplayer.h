#pragma once

#include <jni.h>
#include <string>

// Data structures for NetPlay
struct NetPlayPlayer {
    int id;
    std::string nickname;
    bool isConnected;
    
    NetPlayPlayer(int player_id, const std::string& name, bool connected)
        : id(player_id), nickname(name), isConnected(connected) {}
};

struct ChatMessage {
    std::string nickname;
    std::string username;
    std::string message;
    std::string timestamp;
    
    ChatMessage(const std::string& nick, const std::string& user, 
                const std::string& msg, const std::string& time)
        : nickname(nick), username(user), message(msg), timestamp(time) {}
};

// JNI helper functions
JNIEnv* getJNIEnv();
void callJavaCallback(const char* method_name, ...);

// Multiplayer JNI lifecycle functions
void InitializeMultiplayerJNI(JavaVM* vm);
void CleanupMultiplayerJNI();

// NetPlay JNI functions
extern "C" {
    JNIEXPORT jboolean JNICALL
    Java_org_dolphinemu_dolphinemu_features_netplay_NetPlayManager_netPlayConnect(
        JNIEnv* env, jobject thiz, jstring address, jint port);
    
    JNIEXPORT jboolean JNICALL
    Java_org_dolphinemu_dolphinemu_features_netplay_NetPlayManager_netPlayHost(
        JNIEnv* env, jobject thiz, jint port);
    
    JNIEXPORT void JNICALL
    Java_org_dolphinemu_dolphinemu_features_netplay_NetPlayManager_netPlayDisconnect(
        JNIEnv* env, jobject thiz);
    
    JNIEXPORT void JNICALL
    Java_org_dolphinemu_dolphinemu_features_netplay_NetPlayManager_netPlaySendMessage(
        JNIEnv* env, jobject thiz, jstring message);
    
    JNIEXPORT void JNICALL
    Java_org_dolphinemu_dolphinemu_features_netplay_NetPlayManager_netPlayKickPlayer(
        JNIEnv* env, jobject thiz, jint player_id);
    
    JNIEXPORT void JNICALL
    Java_org_dolphinemu_dolphinemu_features_netplay_NetPlayManager_netPlaySetRoomVisibility(
        JNIEnv* env, jobject thiz, jint visibility);
    
    JNIEXPORT jint JNICALL
    Java_org_dolphinemu_dolphinemu_features_netplay_NetPlayManager_netPlayGetPlayerCount(
        JNIEnv* env, jobject thiz);
    
    JNIEXPORT jobjectArray JNICALL
    Java_org_dolphinemu_dolphinemu_features_netplay_NetPlayManager_netPlayGetPlayerList(
        JNIEnv* env, jobject thiz);
    
    JNIEXPORT jboolean JNICALL
    Java_org_dolphinemu_dolphinemu_features_netplay_NetPlayManager_netPlayIsHost(
        JNIEnv* env, jobject thiz);
    
    JNIEXPORT jboolean JNICALL
    Java_org_dolphinemu_dolphinemu_features_netplay_NetPlayManager_netPlayIsConnected(
        JNIEnv* env, jobject thiz);
}
