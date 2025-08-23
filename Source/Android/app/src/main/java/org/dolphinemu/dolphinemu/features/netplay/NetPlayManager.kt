package org.dolphinemu.dolphinemu.features.netplay

import android.content.Context
import android.content.Intent
import android.content.SharedPreferences
import android.util.Log
import org.dolphinemu.dolphinemu.DolphinApplication
import org.dolphinemu.dolphinemu.activities.EmulationActivity
import java.text.SimpleDateFormat
import java.util.*
import java.io.File

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
        private const val MAX_CHAT_MESSAGES = 100
        
        @Volatile
        private var INSTANCE: NetPlayManager? = null
        
        fun getInstance(): NetPlayManager {
            return INSTANCE ?: synchronized(this) {
                INSTANCE ?: NetPlayManager().also { INSTANCE = it }
            }
        }
    }
    
    // Context management for preferences
    private var appContext: Context? = null
    
    fun setContext(context: Context) {
        appContext = context.applicationContext
    }
    
    private fun getContext(): Context? {
        return appContext
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
    private var lobbyCallback: LobbyCallback? = null

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
    external fun setNetPlayManagerReference()
    
    // Game validation and checksum methods
    external fun netPlayGetGameChecksum(gamePath: String): String?
    external fun netPlayValidateGameFile(gamePath: String): Boolean
    external fun netPlayLaunchGame(gamePath: String): Boolean
    external fun netPlayGetGameId(gamePath: String): String?
    external fun netPlayCheckAndStartGame(): Boolean
    
    // NetPlay game status confirmation
    external fun sendGameStatusConfirmation(sameGame: Boolean)
    
    // NetPlay message processing
    external fun netPlayProcessMessages()
    
    // ROM path getter for native code
    fun getRomPath(): String {
        // Return the default ROM path that Android Dolphin uses
        // This should match the path where the user's games are stored
        return try {
            // Try to get the ROM path from Android storage
            val context = DolphinApplication.getAppContext()
            val externalDir = context.getExternalFilesDir(null)
            val romsDir = File(externalDir, "ROMs")
            
            if (romsDir.exists() && romsDir.isDirectory) {
                romsDir.absolutePath
            } else {
                // Fallback to common Android ROM directories
                "/storage/emulated/0/ROMs"
            }
        } catch (e: Exception) {
            Log.e(TAG, "Error getting ROM path: ${e.message}")
            "/storage/emulated/0/ROMs"
        }
    }
    
    // Callback methods called from native code
    fun onHostGameStarted() {
        Log.d(TAG, "Host started the game - Android client should sync")
        
        // Send confirmation to host that we're ready to sync
        // This tells the desktop host that the Android client has the game and is ready
        try {
            sendGameStatusConfirmation(true) // true = SameGame
            Log.d(TAG, "Sent SameGame status confirmation to host")
        } catch (e: Exception) {
            Log.e(TAG, "Failed to send game status confirmation: ${e.message}")
        }
        
        // TODO: Implement additional game synchronization logic
        // This could trigger UI updates, show sync status, etc.
    }
    
    init {
        try {
            System.loadLibrary("main")
            Log.d(TAG, "Native library loaded successfully")
            
            // Set the NetPlay manager reference for JNI callbacks
            setNetPlayManagerReference()
            
        } catch (e: UnsatisfiedLinkError) {
            Log.e(TAG, "Failed to load native library: ${e.message}")
            // Don't crash, just log the error
        } catch (e: Exception) {
            Log.e(TAG, "Unexpected error loading native library: ${e.message}")
        }
    }
    
    // Public API methods
    fun connectToServer(address: String, port: Int, callback: ConnectionCallback) {
        this.connectionCallback = callback
        Log.d(TAG, "Connecting to server: $address:$port")
        
        try {
            if (netPlayConnect(address, port)) {
                isConnected = true
                isHost = false
                connectionCallback?.onConnected()
                startPlayerUpdateLoop()
            } else {
                connectionCallback?.onConnectionFailed("Failed to connect to server")
            }
        } catch (e: UnsatisfiedLinkError) {
            Log.e(TAG, "Native method not available: ${e.message}")
            connectionCallback?.onConnectionFailed("NetPlay functionality not available")
        } catch (e: Exception) {
            Log.e(TAG, "Error connecting to server: ${e.message}")
            connectionCallback?.onConnectionFailed("Connection error: ${e.message}")
        }
    }
    
    fun hostServer(port: Int, callback: ConnectionCallback) {
        this.connectionCallback = callback
        Log.d(TAG, "Hosting server on port: $port")
        
        // Generate new host code for this session
        val context = getContext()
        if (context != null) {
            clearHostCode(context)
            val newHostCode = generateHostCode()
            // Store the new host code
            val prefs = context.getSharedPreferences(PREFS_NAME, Context.MODE_PRIVATE)
            prefs.edit().putString("host_code", newHostCode).apply()
        }
        
        try {
            if (netPlayHost(port)) {
                isConnected = true
                isHost = true
                connectionCallback?.onConnected()
                startPlayerUpdateLoop()
            } else {
                connectionCallback?.onConnectionFailed("Failed to host server")
            }
        } catch (e: UnsatisfiedLinkError) {
            Log.e(TAG, "Native method not available: ${e.message}")
            connectionCallback?.onConnectionFailed("NetPlay functionality not available")
        } catch (e: Exception) {
            Log.e(TAG, "Error hosting server: ${e.message}")
            connectionCallback?.onConnectionFailed("Hosting error: ${e.message}")
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
            val context = getContext()
            if (context != null) {
                addChatMessage(ChatMessage(
                    nickname = getUsername(context),
                    username = "",
                    message = message,
                    timestamp = SimpleDateFormat("HH:mm", Locale.getDefault()).format(Date())
                ))
            }
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
    
    /**
     * Generate a unique host code for traversal server connections
     */
    fun generateHostCode(): String {
        // Generate a random 8-character alphanumeric code
        val chars = "ABCDEF0123456789"
        return (1..8).map { chars.random() }.joinToString("")
    }
    
    /**
     * Get the current host code (generates new one if none exists)
     */
    fun getHostCode(context: Context): String {
        val prefs = context.getSharedPreferences(PREFS_NAME, Context.MODE_PRIVATE)
        var hostCode = prefs.getString("host_code", "") ?: ""
        
        if (hostCode.isBlank()) {
            hostCode = generateHostCode()
            prefs.edit().putString("host_code", hostCode).apply()
        }
        
        return hostCode
    }
    
    /**
     * Clear the current host code (for new sessions)
     */
    fun clearHostCode(context: Context) {
        val prefs = context.getSharedPreferences(PREFS_NAME, Context.MODE_PRIVATE)
        prefs.edit().remove("host_code").apply()
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
    fun setConnectionCallback(callback: ConnectionCallback?) {
        this.connectionCallback = callback
    }
    
    fun setChatCallback(callback: ChatCallback?) {
        this.chatCallback = callback
    }
    
    fun setPlayerCallback(callback: PlayerCallback?) {
        this.playerCallback = callback
    }

    fun setLobbyCallback(callback: LobbyCallback?) {
        this.lobbyCallback = callback
    }

    /**
     * Discover available netplay servers from the lobby
     */
    fun discoverServers() {
        Thread {
            try {
                // Query the real Dolphin lobby server for MPN sessions
                val lobbyUrl = "https://lobby.dolphin-emu.org/v0/list"
                val filters = mapOf("version" to "MPN")
                
                val urlBuilder = StringBuilder(lobbyUrl)
                if (filters.isNotEmpty()) {
                    urlBuilder.append('?')
                    filters.forEach { (key, value) ->
                        urlBuilder.append("$key=$value&")
                    }
                    urlBuilder.setLength(urlBuilder.length - 1) // Remove trailing &
                }
                
                val url = java.net.URL(urlBuilder.toString())
                val connection = url.openConnection() as java.net.HttpURLConnection
                connection.requestMethod = "GET"
                connection.setRequestProperty("X-Is-Dolphin", "1")
                connection.connectTimeout = 10000
                connection.readTimeout = 10000
                
                val responseCode = connection.responseCode
                if (responseCode == 200) {
                    val inputStream = connection.inputStream
                    val response = inputStream.bufferedReader().use { it.readText() }
                    
                    // Parse JSON response
                    val servers = parseLobbyResponse(response)
                    lobbyCallback?.onServersDiscovered(servers)
                } else {
                    lobbyCallback?.onDiscoveryFailed("HTTP Error: $responseCode")
                }
                
                connection.disconnect()
            } catch (e: Exception) {
                lobbyCallback?.onDiscoveryFailed("Failed to discover servers: ${e.message}")
            }
        }.start()
    }

    /**
     * Parse the lobby server response JSON
     */
    private fun parseLobbyResponse(jsonResponse: String): List<LobbyServer> {
        val servers = mutableListOf<LobbyServer>()
        
        try {
            Log.d(TAG, "Raw lobby response: $jsonResponse")
            val jsonObject = org.json.JSONObject(jsonResponse)
            if (jsonObject.getString("status") == "OK") {
                val sessions = jsonObject.getJSONArray("sessions")
                Log.d(TAG, "Found ${sessions.length()} sessions")
                
                for (i in 0 until sessions.length()) {
                    val session = sessions.getJSONObject(i)
                    Log.d(TAG, "Session $i: ${session.toString()}")
                    
                    // Only include MPN version sessions that are NOT in-game
                    if (session.optString("version", "") == "MPN" && !session.getBoolean("in_game")) {
                        val serverId = session.getString("server_id")
                        val serverName = session.getString("name")
                        val serverPort = session.getInt("port")
                        
                        Log.d(TAG, "Creating server: name='$serverName', server_id='$serverId', port=$serverPort")
                        
                        val server = LobbyServer(
                            name = serverName,
                            address = serverId, // server_id contains the IP/address
                            port = serverPort,
                            gameId = session.getString("game"),
                            gameName = getGameNameFromId(session.getString("game")),
                            playerCount = session.getInt("player_count"),
                            maxPlayers = 4, // Default max players
                            version = "MPN"
                        )
                        servers.add(server)
                        Log.d(TAG, "Added server: ${server.name} at ${server.address}:${server.port}")
                    }
                }
            }
        } catch (e: Exception) {
            Log.e(TAG, "Failed to parse lobby response: ${e.message}")
        }
        
        return servers
    }

    /**
     * Connect to a specific server from the lobby
     */
    fun connectToLobbyServer(server: LobbyServer, callback: ConnectionCallback) {
        this.connectionCallback = callback
        Log.d(TAG, "Connecting to lobby server: ${server.name} at ${server.address}:${server.port}")
        
        try {
            if (netPlayConnect(server.address, server.port)) {
                isConnected = true
                isHost = false
                connectionCallback?.onConnected()
                startPlayerUpdateLoop()
            } else {
                connectionCallback?.onConnectionFailed("Failed to connect to ${server.name}")
            }
        } catch (e: UnsatisfiedLinkError) {
            Log.e(TAG, "Native method not available: ${e.message}")
            connectionCallback?.onConnectionFailed("NetPlay functionality not available")
        } catch (e: Exception) {
            Log.e(TAG, "Error connecting to lobby server: ${e.message}")
            connectionCallback?.onConnectionFailed("Connection error: ${e.message}")
        }
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
        
        // CRITICAL: Start the NetPlay message processing loop
        // This ensures incoming NetPlay messages are processed and callbacks are triggered
        startMessageProcessingLoop()
    }
    
    private fun startMessageProcessingLoop() {
        Thread {
            Log.d(TAG, "Starting NetPlay message processing loop")
            while (isConnected) {
                try {
                    // Poll for NetPlay messages every 500ms to match C++ side
                    // This reduces CPU usage while still ensuring timely message processing
                    processNetPlayMessages()
                    Thread.sleep(500) // Process messages 2 times per second
                } catch (e: InterruptedException) {
                    Log.d(TAG, "Message processing loop interrupted")
                    break
                } catch (e: Exception) {
                    Log.e(TAG, "Error in message processing loop: ${e.message}")
                    // Don't break the loop on errors, just log and continue
                }
            }
            Log.d(TAG, "NetPlay message processing loop stopped")
        }.start()
    }
    
    private fun processNetPlayMessages() {
        try {
            // Call native method to process any pending NetPlay messages
            // This will trigger the appropriate callbacks (onMsgStartGame, etc.)
            netPlayProcessMessages()
        } catch (e: Exception) {
            Log.e(TAG, "Error processing NetPlay messages: ${e.message}")
        }
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
    fun onConnected() {
        Log.d(TAG, "Native onConnected callback received")
        connectionCallback?.onConnected()
    }
    
    @Suppress("unused")
    fun startNetPlayGame(filename: String) {
        Log.d(TAG, "Starting NetPlay game: $filename")
        
        try {
            // Launch the game using EmulationActivity
            val context = DolphinApplication.getAppContext()
            val intent = Intent(context, EmulationActivity::class.java).apply {
                putExtra(EmulationActivity.EXTRA_SELECTED_GAMES, arrayOf(filename))
                addFlags(Intent.FLAG_ACTIVITY_NEW_TASK)
            }
            context.startActivity(intent)
            Log.d(TAG, "Successfully launched game: $filename")
        } catch (e: Exception) {
            Log.e(TAG, "Failed to start NetPlay game: ${e.message}")
        }
    }
    
    @Suppress("unused")
    fun onConnectionFailed(error: String) {
        Log.d(TAG, "Native onConnectionFailed callback received: $error")
        connectionCallback?.onConnectionFailed(error)
    }
    
    @Suppress("unused")
    fun onDisconnected() {
        Log.d(TAG, "Native onDisconnected callback received")
        connectionCallback?.onDisconnected()
    }
    
    @Suppress("unused")
    fun onMessageReceived(nickname: String, message: String) {
        val chatMessage = ChatMessage(
            nickname = nickname,
            username = "",
            message = message,
            timestamp = SimpleDateFormat("HH:mm", Locale.getDefault()).format(Date())
        )
        addChatMessage(chatMessage)
    }
    
    @Suppress("unused")
    fun onPlayerJoined(playerId: Int, nickname: String) {
        val player = NetPlayPlayer(playerId, nickname, true)
        players.add(player)
        playerCount = players.size
        playerCallback?.onPlayerJoined(player)
        playerCallback?.onPlayerListUpdated(players.toList())
    }
    
    @Suppress("unused")
    fun onPlayerLeft(playerId: Int) {
        players.removeAll { it.id == playerId }
        playerCount = players.size
        playerCallback?.onPlayerLeft(playerId)
        playerCallback?.onPlayerListUpdated(players.toList())
    }
    
    @Suppress("unused")
    fun onConnectionLost() {
        isConnected = false
        isHost = false
        connectionCallback?.onConnectionLost()
    }

    /**
     * Get a friendly game name from the game ID
     */
    private fun getGameNameFromId(gameId: String): String {
        // Try to get the real game title from the user's game library first
        try {
            val gameFile = org.dolphinemu.dolphinemu.services.GameFileCacheManager.getGameFileByGameId(gameId)
            if (gameFile != null) {
                return gameFile.getTitle()
            }
        } catch (e: Exception) {
            Log.d(TAG, "Could not get game title from cache for $gameId: ${e.message}")
        }
        
        // Fallback to common game ID mappings for MPN games
        val gameNames = mapOf(
            // Mario Kart series
            "RMCE01" to "Mario Kart: Double Dash!!",
            "RMCP01" to "Mario Kart Wii",
            "RMCE08" to "Mario Kart 8 Deluxe",
            
            // Super Smash Bros series
            "GALE01" to "Super Smash Bros. Melee",
            "GALE02" to "Super Smash Bros. Brawl",
            "GALE08" to "Super Smash Bros. Ultimate",
            
            // Mario Party series
            "RZDP01" to "Mario Party 8",
            "RZDP02" to "Mario Party 9",
            "RZDP03" to "Mario Party 10",
            "RZDP04" to "Mario Party Superstars",
            
            // Other popular MPN games
            "RMCE02" to "Mario Golf: Toadstool Tour",
            "RMCE03" to "Mario Tennis: Power Tour",
            "RMCE04" to "Mario Baseball",
            "RMCE05" to "Mario Strikers",
            "RMCE06" to "Mario Hoops 3-on-3",
            "RMCE07" to "Mario Tennis Open",
            
            // Wii Sports and related
            "RSPE01" to "Wii Sports",
            "RSPE02" to "Wii Sports Resort",
            "RSPE03" to "Wii Play",
            "RSPE04" to "Wii Play Motion",
            
            // Other Wii games
            "RZDE01" to "Wii Party",
            "RZDE02" to "Wii Party U",
            "RZDE03" to "Wii Party Deluxe"
        )
        
        return gameNames[gameId] ?: "Game ($gameId)"
    }
    
    // Game validation and checksum helper methods
    fun validateGameForNetPlay(gamePath: String): NetPlayGameValidation {
        return try {
            val isValid = netPlayValidateGameFile(gamePath)
            val checksum = if (isValid) netPlayGetGameChecksum(gamePath) else null
            val gameId = if (isValid) netPlayGetGameId(gamePath) else null
            
            NetPlayGameValidation(
                isValid = isValid,
                checksum = checksum,
                gameId = gameId,
                errorMessage = if (isValid) null else "Game file validation failed"
            )
        } catch (e: Exception) {
            Log.e(TAG, "Error validating game: ${e.message}")
            NetPlayGameValidation(
                isValid = false,
                checksum = null,
                gameId = null,
                errorMessage = "Validation error: ${e.message}"
            )
        }
    }
    
    fun launchGameInNetPlay(gamePath: String): Boolean {
        return try {
            val isValid = netPlayValidateGameFile(gamePath)
            if (!isValid) {
                Log.e(TAG, "Cannot launch invalid game file: $gamePath")
                return false
            }
            
            if (!isConnected) {
                Log.e(TAG, "Cannot launch game: not connected to NetPlay server")
                return false
            }
            
            val success = netPlayLaunchGame(gamePath)
            if (success) {
                Log.d(TAG, "Game launch initiated: $gamePath")
            } else {
                Log.e(TAG, "Failed to launch game: $gamePath")
            }
            success
            
        } catch (e: Exception) {
            Log.e(TAG, "Error launching game: ${e.message}")
            false
        }
    }
    
    fun getGameChecksum(gamePath: String): String? {
        return try {
            netPlayGetGameChecksum(gamePath)
        } catch (e: Exception) {
            Log.e(TAG, "Error getting game checksum: ${e.message}")
            null
        }
    }
    
    fun getGameId(gamePath: String): String? {
        return try {
            netPlayGetGameId(gamePath)
        } catch (e: Exception) {
            Log.e(TAG, "Error getting game ID: ${e.message}")
            null
        }
    }
    
    fun checkAndStartGame(): Boolean {
        return try {
            netPlayCheckAndStartGame()
        } catch (e: Exception) {
            Log.e(TAG, "Error checking for game start: ${e.message}")
            false
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

data class NetPlayGameValidation(
    val isValid: Boolean,
    val checksum: String?,
    val gameId: String?,
    val errorMessage: String?
)

enum class RoomVisibility {
    PUBLIC,
    PRIVATE,
    FRIENDS_ONLY
}

// Lobby server data class
data class LobbyServer(
    val name: String,
    val address: String,
    val port: Int,
    val gameId: String,
    val gameName: String,
    val playerCount: Int,
    val maxPlayers: Int,
    val version: String
)

// Lobby callback interface
interface LobbyCallback {
    fun onServersDiscovered(servers: List<LobbyServer>)
    fun onDiscoveryFailed(error: String)
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
