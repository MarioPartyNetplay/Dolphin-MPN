package org.dolphinemu.dolphinemu.features.netplay

import android.content.Context
import android.content.SharedPreferences
import android.util.Log
import java.text.SimpleDateFormat
import java.util.*

/**
 * Manages NetPlay functionality for Dolphin MPN Android
 * Inspired by Mandarine3DS implementation
 */
class NetPlayManager private constructor() {
    companion object {
        private const val TAG = "NetPlayManager"
        private const val PREFS_NAME = "netplay_prefs"
        private const val KEY_USERNAME = "username"
        private const val KEY_SERVER_ADDRESS = "server_address"
        private const val KEY_SERVER_PORT = "server_port"
        private const val KEY_ROOM_VISIBILITY = "room_visibility"
        
        @Volatile
        private var INSTANCE: NetPlayManager? = null
        
        fun getInstance(): NetPlayManager {
            return INSTANCE ?: synchronized(this) {
                INSTANCE ?: NetPlayManager().also { INSTANCE = it }
            }
        }
    }
    
    // NetPlay state
    private var isConnected = false
    private var isHost = false
    private var roomId: String? = null
    private var playerCount = 0
    private var maxPlayers = 4
    private var roomVisibility = RoomVisibility.PUBLIC
    private var chatMessages = mutableListOf<ChatMessage>()
    private var players = mutableListOf<NetPlayPlayer>()
    private var isChatOpen = false
    
    // Callbacks
    private var connectionCallback: ConnectionCallback? = null
    private var chatCallback: ChatCallback? = null
    private var playerCallback: PlayerCallback? = null
    
    // Native methods
    external fun netPlayConnect(address: String, port: Int): Boolean
    external fun netPlayHost(port: Int): Boolean
    external fun netPlayDisconnect()
    external fun netPlaySendMessage(message: String)
    external fun netPlayKickPlayer(playerId: Int)
    external fun netPlayBanPlayer(playerId: Int)
    external fun netPlaySetRoomVisibility(visibility: Int)
    external fun netPlayGetPlayerCount(): Int
    external fun netPlayGetPlayerList(): Array<NetPlayPlayer>
    external fun netPlayIsHost(): Boolean
    external fun netPlayIsConnected(): Boolean
    
    init {
        System.loadLibrary("dolphin-mpn")
    }
    
    // Public API methods
    fun connectToServer(address: String, port: Int, callback: ConnectionCallback) {
        this.connectionCallback = callback
        Log.d(TAG, "Connecting to server: $address:$port")
        
        if (netPlayConnect(address, port)) {
            isConnected = true
            isHost = false
            connectionCallback?.onConnected()
            startPlayerUpdateLoop()
        } else {
            connectionCallback?.onConnectionFailed("Failed to connect to server")
        }
    }
    
    fun hostServer(port: Int, callback: ConnectionCallback) {
        this.connectionCallback = callback
        Log.d(TAG, "Hosting server on port: $port")
        
        if (netPlayHost(port)) {
            isConnected = true
            isHost = true
            connectionCallback?.onConnected()
            startPlayerUpdateLoop()
        } else {
            connectionCallback?.onConnectionFailed("Failed to host server")
        }
    }
    
    fun disconnect() {
        if (isConnected) {
            netPlayDisconnect()
            isConnected = false
            isHost = false
            roomId = null
            playerCount = 0
            players.clear()
            chatMessages.clear()
            connectionCallback?.onDisconnected()
            stopPlayerUpdateLoop()
        }
    }
    
    fun sendMessage(message: String) {
        if (isConnected && message.isNotBlank()) {
            netPlaySendMessage(message)
            addChatMessage(ChatMessage(
                nickname = getUsername(getContext()),
                username = "",
                message = message,
                timestamp = SimpleDateFormat("HH:mm", Locale.getDefault()).format(Date())
            ))
        }
    }
    
    fun kickPlayer(playerId: Int) {
        if (isHost && isConnected) {
            netPlayKickPlayer(playerId)
        }
    }
    
    fun banPlayer(playerId: Int) {
        if (isHost && isConnected) {
            netPlayBanPlayer(playerId)
        }
    }
    
    fun setRoomVisibility(visibility: RoomVisibility) {
        if (isHost && isConnected) {
            roomVisibility = visibility
            netPlaySetRoomVisibility(visibility.ordinal)
        }
    }
    
    fun getUsername(context: Context): String {
        val prefs = context.getSharedPreferences(PREFS_NAME, Context.MODE_PRIVATE)
        return prefs.getString(KEY_USERNAME, "Player") ?: "Player"
    }
    
    fun setUsername(context: Context, username: String) {
        val prefs = context.getSharedPreferences(PREFS_NAME, Context.MODE_PRIVATE)
        prefs.edit().putString(KEY_USERNAME, username).apply()
    }
    
    fun getServerAddress(context: Context): String {
        val prefs = context.getSharedPreferences(PREFS_NAME, Context.MODE_PRIVATE)
        return prefs.getString(KEY_SERVER_ADDRESS, "127.0.0.1") ?: "127.0.0.1"
    }
    
    fun setServerAddress(context: Context, address: String) {
        val prefs = context.getSharedPreferences(PREFS_NAME, Context.MODE_PRIVATE)
        prefs.edit().putString(KEY_SERVER_ADDRESS, address).apply()
    }
    
    fun getServerPort(context: Context): Int {
        val prefs = context.getSharedPreferences(PREFS_NAME, Context.MODE_PRIVATE)
        return prefs.getInt(KEY_SERVER_PORT, 2626)
    }
    
    fun setServerPort(context: Context, port: Int) {
        val prefs = context.getSharedPreferences(PREFS_NAME, Context.MODE_PRIVATE)
        prefs.edit().putInt(KEY_SERVER_PORT, port).apply()
    }
    
    fun getRoomVisibility(context: Context): RoomVisibility {
        val prefs = context.getSharedPreferences(PREFS_NAME, Context.MODE_PRIVATE)
        val visibility = prefs.getInt(KEY_ROOM_VISIBILITY, RoomVisibility.PUBLIC.ordinal)
        return RoomVisibility.values()[visibility]
    }
    
    fun setRoomVisibility(context: Context, visibility: RoomVisibility) {
        val prefs = context.getSharedPreferences(PREFS_NAME, Context.MODE_PRIVATE)
        prefs.edit().putInt(KEY_ROOM_VISIBILITY, visibility.ordinal).apply()
    }
    
    fun getConnectionType(context: Context): String {
        val prefs = context.getSharedPreferences(PREFS_NAME, Context.MODE_PRIVATE)
        return prefs.getString("connection_type", "direct") ?: "direct"
    }
    
    fun setConnectionType(context: Context, connectionType: String) {
        val prefs = context.getSharedPreferences(PREFS_NAME, Context.MODE_PRIVATE)
        prefs.edit().putString("connection_type", connectionType).apply()
    }
    
    fun getServerName(context: Context): String {
        val prefs = context.getSharedPreferences(PREFS_NAME, Context.MODE_PRIVATE)
        return prefs.getString("server_name", "") ?: ""
    }
    
    fun setServerName(context: Context, serverName: String) {
        val prefs = context.getSharedPreferences(PREFS_NAME, Context.MODE_PRIVATE)
        prefs.edit().putString("server_name", serverName).apply()
    }
    
    // Chat management
    fun addChatMessage(message: ChatMessage) {
        chatMessages.add(message)
        if (chatMessages.size > MAX_CHAT_MESSAGES) {
            chatMessages.removeAt(0)
        }
        chatCallback?.onMessageReceived(message)
    }
    
    fun getChatMessages(): List<ChatMessage> = chatMessages.toList()
    
    fun setChatOpen(open: Boolean) {
        isChatOpen = open
    }
    
    fun isChatOpen(): Boolean = isChatOpen
    
    // Player management
    fun getPlayers(): List<NetPlayPlayer> = players.toList()
    
    fun getPlayerCount(): Int = playerCount
    
    fun isConnected(): Boolean = isConnected
    
    fun isHost(): Boolean = isHost
    
    // Callback setters
    fun setConnectionCallback(callback: ConnectionCallback) {
        this.connectionCallback = callback
    }
    
    fun setChatCallback(callback: ChatCallback) {
        this.chatCallback = callback
    }
    
    fun setPlayerCallback(callback: PlayerCallback) {
        this.playerCallback = callback
    }
    
    // Private helper methods
    private fun startPlayerUpdateLoop() {
        Thread {
            while (isConnected) {
                try {
                    updatePlayerList()
                    Thread.sleep(1000) // Update every second
                } catch (e: InterruptedException) {
                    break
                }
            }
        }.start()
    }
    
    private fun stopPlayerUpdateLoop() {
        // The loop will naturally stop when isConnected becomes false
    }
    
    private fun updatePlayerList() {
        if (isConnected) {
            val newPlayerCount = netPlayGetPlayerCount()
            val newPlayers = netPlayGetPlayerList()
            
            if (newPlayerCount != playerCount || players != newPlayers.toList()) {
                playerCount = newPlayerCount
                players.clear()
                players.addAll(newPlayers.toList())
                playerCallback?.onPlayerListUpdated(players.toList())
            }
        }
    }
    
    // Native callback methods (called from C++)
    @Suppress("unused")
    private fun onMessageReceived(nickname: String, message: String) {
        val chatMessage = ChatMessage(
            nickname = nickname,
            username = "",
            message = message,
            timestamp = SimpleDateFormat("HH:mm", Locale.getDefault()).format(Date())
        )
        addChatMessage(chatMessage)
    }
    
    @Suppress("unused")
    private fun onPlayerJoined(playerId: Int, nickname: String) {
        val player = NetPlayPlayer(playerId, nickname, true)
        players.add(player)
        playerCount = players.size
        playerCallback?.onPlayerJoined(player)
        playerCallback?.onPlayerListUpdated(players.toList())
    }
    
    @Suppress("unused")
    private fun onPlayerLeft(playerId: Int) {
        players.removeAll { it.id == playerId }
        playerCount = players.size
        playerCallback?.onPlayerLeft(playerId)
        playerCallback?.onPlayerListUpdated(players.toList())
    }
    
    @Suppress("unused")
    private fun onConnectionLost() {
        isConnected = false
        isHost = false
        connectionCallback?.onConnectionLost()
    }
    
    companion object {
        private const val MAX_CHAT_MESSAGES = 100
        
        // This is a temporary solution - in a real implementation, you'd get the context properly
        private fun getContext(): Context {
            // This should be properly implemented with dependency injection
            throw UnsupportedOperationException("Context must be provided through setConnectionCallback")
        }
    }
}

// Data classes
data class ChatMessage(
    val nickname: String,
    val username: String,
    val message: String,
    val timestamp: String
)

data class NetPlayPlayer(
    val id: Int,
    val nickname: String,
    val isConnected: Boolean
)

enum class RoomVisibility {
    PUBLIC,
    PRIVATE,
    FRIENDS_ONLY
}

// Callback interfaces
interface ConnectionCallback {
    fun onConnected()
    fun onConnectionFailed(error: String)
    fun onDisconnected()
    fun onConnectionLost()
}

interface ChatCallback {
    fun onMessageReceived(message: ChatMessage)
}

interface PlayerCallback {
    fun onPlayerJoined(player: NetPlayPlayer)
    fun onPlayerLeft(playerId: Int)
    fun onPlayerListUpdated(players: List<NetPlayPlayer>)
}
