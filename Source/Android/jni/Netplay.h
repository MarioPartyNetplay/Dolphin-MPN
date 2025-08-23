#pragma once

#include <jni.h>
#include <string>

// JNI helper functions
JNIEnv* getJNIEnv();
void callJavaCallback(const char* method_name, ...);

// Wrapper functions for JNI lifecycle
void InitializeMultiplayerJNI(JavaVM* vm);
void CleanupMultiplayerJNI();

// NetPlay JNI functions
extern "C" {
    // Multiplayer JNI lifecycle functions
    JNIEXPORT void JNICALL
    Java_org_dolphinemu_dolphinemu_features_netplay_NetPlayManager_InitializeMultiplayerJNI(
        JNIEnv* env, jobject thiz, jobject manager);
    
    JNIEXPORT void JNICALL
    Java_org_dolphinemu_dolphinemu_features_netplay_NetPlayManager_CleanupMultiplayerJNI(
        JNIEnv* env, jobject thiz);
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
    
    // Game validation and checksum functions
    JNIEXPORT jstring JNICALL
    Java_org_dolphinemu_dolphinemu_features_netplay_NetPlayManager_netPlayGetGameChecksum(
        JNIEnv* env, jobject thiz, jstring gamePath);
    
    JNIEXPORT jboolean JNICALL
    Java_org_dolphinemu_dolphinemu_features_netplay_NetPlayManager_netPlayValidateGameFile(
        JNIEnv* env, jobject thiz, jstring gamePath);
    
    JNIEXPORT jboolean JNICALL
    Java_org_dolphinemu_dolphinemu_features_netplay_NetPlayManager_netPlayLaunchGame(
        JNIEnv* env, jobject thiz, jstring gamePath);
    
    JNIEXPORT jstring JNICALL
    Java_org_dolphinemu_dolphinemu_features_netplay_NetPlayManager_netPlayGetGameId(
        JNIEnv* env, jobject thiz, jstring gamePath);
    
    JNIEXPORT jboolean JNICALL
    Java_org_dolphinemu_dolphinemu_features_netplay_NetPlayManager_netPlayCheckAndStartGame(
        JNIEnv* env, jobject thiz);
        
    // Additional methods for full NetPlay replication
    JNIEXPORT void JNICALL
    Java_org_dolphinemu_dolphinemu_features_netplay_NetPlayManager_sendGameStatusConfirmation(
        JNIEnv* env, jobject thiz, jboolean sameGame);
        
    JNIEXPORT void JNICALL
    Java_org_dolphinemu_dolphinemu_features_netplay_NetPlayManager_setNetPlayManagerReference(
        JNIEnv* env, jobject thiz);
        
    JNIEXPORT jstring JNICALL
    Java_org_dolphinemu_dolphinemu_features_netplay_NetPlayManager_netPlayGetPlayerName(
        JNIEnv* env, jobject thiz, jint player_id);
        
    JNIEXPORT jboolean JNICALL
    Java_org_dolphinemu_dolphinemu_features_netplay_NetPlayManager_netPlayIsHosting(
        JNIEnv* env, jobject thiz);
        
    JNIEXPORT jstring JNICALL
    Java_org_dolphinemu_dolphinemu_features_netplay_NetPlayManager_netPlayGetGameName(
        JNIEnv* env, jobject thiz);
        
    JNIEXPORT jint JNICALL
    Java_org_dolphinemu_dolphinemu_features_netplay_NetPlayManager_netPlayGetPort(
        JNIEnv* env, jobject thiz);
        
    JNIEXPORT void JNICALL
    Java_org_dolphinemu_dolphinemu_features_netplay_NetPlayManager_netPlayBanPlayer(
        JNIEnv* env, jobject thiz, jint player_id);

    // NetPlay message processing
    JNIEXPORT void JNICALL
    Java_org_dolphinemu_dolphinemu_features_netplay_NetPlayManager_netPlayProcessMessages(
        JNIEnv* env, jobject thiz);
        
    // Player name management
    JNIEXPORT void JNICALL
    Java_org_dolphinemu_dolphinemu_features_netplay_NetPlayManager_setPlayerName(
        JNIEnv* env, jobject thiz, jstring playerName);
        
    JNIEXPORT jstring JNICALL
    Java_org_dolphinemu_dolphinemu_features_netplay_NetPlayManager_getPlayerName(
        JNIEnv* env, jobject thiz);
        
    // ROM folder management
    JNIEXPORT void JNICALL
    Java_org_dolphinemu_dolphinemu_features_netplay_NetPlayManager_setRomFolder(
        JNIEnv* env, jobject thiz, jstring folderPath);
        
    JNIEXPORT jstring JNICALL
    Java_org_dolphinemu_dolphinemu_features_netplay_NetPlayManager_getRomFolder(
        JNIEnv* env, jobject thiz);
        
    // Android device name
    JNIEXPORT jstring JNICALL
    Java_org_dolphinemu_dolphinemu_features_netplay_NetPlayManager_getAndroidDeviceName(
        JNIEnv* env, jobject thiz);
}
