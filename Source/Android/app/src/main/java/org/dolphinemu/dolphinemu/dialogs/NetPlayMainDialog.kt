package org.dolphinemu.dolphinemu.dialogs

import android.app.Dialog
import android.app.AlertDialog
import android.content.Context
import android.os.Bundle
import android.view.LayoutInflater
import android.view.View
import android.view.ViewGroup
import android.widget.*
import androidx.fragment.app.DialogFragment
import androidx.recyclerview.widget.LinearLayoutManager
import androidx.recyclerview.widget.RecyclerView
import org.dolphinemu.dolphinemu.R
import org.dolphinemu.dolphinemu.adapters.NetPlayPlayerAdapter
import org.dolphinemu.dolphinemu.features.netplay.*

/**
 * Main NetPlay Dialog for active sessions
 * Handles chat, player management, and game controls
 */
class NetPlayMainDialog : DialogFragment(), ConnectionCallback, ChatCallback, PlayerCallback {
    
    private lateinit var netPlayManager: NetPlayManager
    private lateinit var context: Context
    
    // UI Components
    private lateinit var roomStatusText: TextView
    private lateinit var playerCountText: TextView
    private lateinit var roomBox: Spinner
    private lateinit var hostCodeLabel: TextView
    private lateinit var hostCodeActionButton: Button
    
    // Chat Section
    private lateinit var chatRecyclerView: RecyclerView
    private lateinit var chatInput: EditText
    private lateinit var sendButton: Button
    
    // Players Section
    private lateinit var playersRecyclerView: RecyclerView
    private lateinit var kickButton: Button
    private lateinit var banButton: Button
    private lateinit var assignPortsButton: Button
    
    // Game Controls
    private lateinit var gameButton: Button
    private lateinit var startButton: Button
    private lateinit var quitButton: Button
    
    // Settings
    private lateinit var bufferSizeBox: Spinner
    private lateinit var bufferLabel: TextView
    
    // Adapters
    private lateinit var playerAdapter: NetPlayPlayerAdapter
    private lateinit var chatAdapter: ChatAdapter
    
    override fun onCreateDialog(savedInstanceState: Bundle?): Dialog {
        return Dialog(requireContext(), R.style.Theme_Dolphin_Dialog)
    }
    
    override fun onCreateView(
        inflater: LayoutInflater,
        container: ViewGroup?,
        savedInstanceState: Bundle?
    ): View? {
        return inflater.inflate(R.layout.dialog_netplay_main, container, false)
    }
    
    override fun onViewCreated(view: View, savedInstanceState: Bundle?) {
        super.onViewCreated(view, savedInstanceState)
        
        context = requireContext()
        netPlayManager = NetPlayManager.getInstance()
        netPlayManager.setContext(context)
        
        initializeViews()
        setupAdapters()
        setupListeners()
        updateUI()
        
        // Set callbacks
        netPlayManager.setConnectionCallback(this)
        netPlayManager.setChatCallback(this)
        netPlayManager.setPlayerCallback(this)
    }
    
    private fun initializeViews() {
        view?.let { rootView ->
            // Status and room info
            roomStatusText = rootView.findViewById(R.id.text_room_status)
            playerCountText = rootView.findViewById(R.id.text_player_count)
            roomBox = rootView.findViewById(R.id.spinner_room)
            hostCodeLabel = rootView.findViewById(R.id.label_host_code)
            hostCodeActionButton = rootView.findViewById(R.id.button_host_code_action)
            
            // Chat section
            chatRecyclerView = rootView.findViewById(R.id.recycler_chat)
            chatInput = rootView.findViewById(R.id.edit_chat_input)
            sendButton = rootView.findViewById(R.id.button_send)
            
            // Players section - hidden on mobile
            playersRecyclerView = rootView.findViewById(R.id.recycler_players)
            kickButton = rootView.findViewById(R.id.button_kick)
            banButton = rootView.findViewById(R.id.button_ban)
            assignPortsButton = rootView.findViewById(R.id.button_assign_ports)
            
            // Game controls - hidden on mobile
            gameButton = rootView.findViewById(R.id.button_game)
            startButton = rootView.findViewById(R.id.button_start)
            quitButton = rootView.findViewById(R.id.button_quit)
            
            // Settings - hidden on mobile
            bufferSizeBox = rootView.findViewById(R.id.spinner_buffer_size)
            bufferLabel = rootView.findViewById(R.id.label_buffer)
            
            // Hide mobile-unfriendly elements
            hideMobileUnfriendlyElements()
        }
    }
    
    private fun hideMobileUnfriendlyElements() {
        // Hide game controls on mobile (but keep quit button for exiting)
        gameButton.visibility = View.GONE
        startButton.visibility = View.GONE
        // quitButton.visibility = View.VISIBLE (keep visible for exiting)
        
        // Hide player management controls on mobile (but keep player list visible)
        kickButton.visibility = View.GONE
        banButton.visibility = View.GONE
        assignPortsButton.visibility = View.GONE
        
        // Hide buffer controls on mobile
        bufferSizeBox.visibility = View.GONE
        bufferLabel.visibility = View.GONE
        
        // Hide room selection on mobile
        roomBox.visibility = View.GONE
        
        // Keep player list visible but remove management functionality
        // playersRecyclerView.visibility = View.VISIBLE (default)
    }
    
    private fun setupAdapters() {
        // Mobile: Only setup chat adapter since player list is hidden
        // Chat adapter
        chatAdapter = ChatAdapter(netPlayManager.getChatMessages().toMutableList())
        chatRecyclerView.layoutManager = LinearLayoutManager(context).apply {
            stackFromEnd = true
        }
        chatRecyclerView.adapter = chatAdapter

        // Player adapter
        playerAdapter = NetPlayPlayerAdapter { player ->
            // Mobile: No player management needed, just log selection for debugging
            android.util.Log.d("NetPlayPlayer", "Player selected: ${player.nickname}")
        }
        playersRecyclerView.layoutManager = LinearLayoutManager(context)
        playersRecyclerView.adapter = playerAdapter
    }
    
    private fun setupListeners() {
        sendButton.setOnClickListener {
            val message = chatInput.text.toString()
            if (message.isNotBlank()) {
                // Debug: Log what we're sending
                android.util.Log.d("NetPlayChat", "Sending message: '$message'")
                
                // Clean the message before sending
                val cleanMessage = message.trim()
                netPlayManager.sendMessage(cleanMessage)
                chatInput.text?.clear()
            }
        }
        
        // Quit button for exiting NetPlay
        quitButton.setOnClickListener {
            netPlayManager.disconnect()
            dismiss()
        }
        
        // Mobile: Only keep essential listeners
        hostCodeActionButton.setOnClickListener {
            copyHostCode()
        }
        
        // Note: Game controls and player management controls are hidden on mobile
        // but quit button is kept for exiting NetPlay sessions
    }
    
    private fun updateUI() {
        val isConnected = netPlayManager.isConnected()
        val isHost = netPlayManager.isHost()
        
        // Update status
        val status = when {
            !isConnected -> "Disconnected"
            isHost -> "Hosting"
            else -> "Connected"
        }
        roomStatusText.text = "Status: $status"
        
        // Update player count
        val playerCount = netPlayManager.getPlayerCount()
        playerCountText.text = "Players: $playerCount/4"
        
        // Refresh player list to show current players immediately
        if (::playerAdapter.isInitialized) {
            val currentPlayers = netPlayManager.getPlayers()
            playerAdapter.updatePlayers(currentPlayers)
        }
        
        // Mobile: Only show host code if hosting
        hostCodeLabel.visibility = if (isHost) View.VISIBLE else View.GONE
        hostCodeActionButton.visibility = if (isHost) View.VISIBLE else View.GONE
        
        // Update host code if hosting
        if (isHost) {
            val hostCode = netPlayManager.getHostCode(context)
            hostCodeLabel.text = "Code: $hostCode"
        }
        
        // Check if we need to start a game (for mobile clients)
        if (isConnected && !isHost) {
            netPlayManager.checkAndStartGame()
        }
    }
    
    private fun updateRoomOptions() {
        // TODO: Populate room options based on available interfaces
        val roomOptions = arrayOf("Local", "External")
        val adapter = ArrayAdapter(context, android.R.layout.simple_spinner_item, roomOptions)
        adapter.setDropDownViewResource(android.R.layout.simple_spinner_dropdown_item)
        roomBox.adapter = adapter
    }
    
    
    private fun startGame() {
        // TODO: Implement game start
        Toast.makeText(context, "Starting game...", Toast.LENGTH_SHORT).show()
    }
    
    private fun copyHostCode() {
        val hostCode = netPlayManager.getHostCode(context)
        val clipboard = context.getSystemService(Context.CLIPBOARD_SERVICE) as android.content.ClipboardManager
        val clip = android.content.ClipData.newPlainText("Host Code", hostCode)
        clipboard.setPrimaryClip(clip)
        Toast.makeText(context, "Host code copied to clipboard", Toast.LENGTH_SHORT).show()
    }
    
    // ConnectionCallback implementation
    override fun onConnected() {
        requireActivity().runOnUiThread {
            updateUI()
            showSuccess("Successfully connected!")
        }
    }
    
    override fun onConnectionFailed(error: String) {
        requireActivity().runOnUiThread {
            showError("Connection failed: $error")
        }
    }
    
    override fun onDisconnected() {
        requireActivity().runOnUiThread {
            updateUI()
            showSuccess("Disconnected from server")
        }
    }
    
    override fun onConnectionLost() {
        requireActivity().runOnUiThread {
            updateUI()
            showError("Connection lost")
        }
    }
    
    // ChatCallback implementation
    override fun onMessageReceived(message: ChatMessage) {
        android.util.Log.d("NetPlayChat", "Message received: nickname='${message.nickname}', message='${message.message}', timestamp='${message.timestamp}'")
        
        requireActivity().runOnUiThread {
            chatAdapter.addMessage(message)
            chatRecyclerView.scrollToPosition(chatAdapter.itemCount - 1)
        }
    }
    
    // PlayerCallback implementation
    override fun onPlayerJoined(player: NetPlayPlayer) {
        requireActivity().runOnUiThread {
            playerAdapter.addPlayer(player)
            updateUI()
        }
    }
    
    override fun onPlayerLeft(playerId: Int) {
        requireActivity().runOnUiThread {
            playerAdapter.removePlayer(playerId)
            updateUI()
        }
    }
    
    override fun onPlayerListUpdated(players: List<NetPlayPlayer>) {
        requireActivity().runOnUiThread {
            playerAdapter.updatePlayers(players)
            updateUI()
        }
    }
    
    private fun showError(message: String) {
        Toast.makeText(context, message, Toast.LENGTH_SHORT).show()
    }
    
    private fun showSuccess(message: String) {
        Toast.makeText(context, message, Toast.LENGTH_SHORT).show()
    }
    
    override fun onDestroy() {
        super.onDestroy()
        netPlayManager.setConnectionCallback(null)
        netPlayManager.setChatCallback(null)
        netPlayManager.setPlayerCallback(null)
    }
}

// Chat adapter for RecyclerView
class ChatAdapter(private val messages: MutableList<ChatMessage>) : 
    RecyclerView.Adapter<ChatAdapter.ChatViewHolder>() {
    
    fun addMessage(message: ChatMessage) {
        // Check for duplicate messages to prevent showing the same message twice
        val isDuplicate = messages.any { existing ->
            existing.nickname == message.nickname && 
            existing.message == message.message && 
            existing.timestamp == message.timestamp
        }
        
        if (!isDuplicate) {
            messages.add(message)
            if (messages.size > 100) {
                messages.removeAt(0)
            }
            notifyDataSetChanged()
        } else {
            android.util.Log.d("NetPlayChat", "Duplicate message detected, ignoring: ${message.message}")
        }
    }
    
    override fun onCreateViewHolder(parent: ViewGroup, viewType: Int): ChatViewHolder {
        val view = LayoutInflater.from(parent.context)
            .inflate(R.layout.item_chat_message, parent, false)
        return ChatViewHolder(view)
    }
    
    override fun onBindViewHolder(holder: ChatViewHolder, position: Int) {
        holder.bind(messages[position])
    }
    
    override fun getItemCount(): Int = messages.size
    
    class ChatViewHolder(itemView: View) : RecyclerView.ViewHolder(itemView) {
        private val usernameText: TextView = itemView.findViewById(R.id.text_username)
        private val timestampText: TextView = itemView.findViewById(R.id.text_timestamp)
        private val messageText: TextView = itemView.findViewById(R.id.text_message)
        private val userIcon: ImageView = itemView.findViewById(R.id.icon_user)
        
        fun bind(message: ChatMessage) {
            // Debug: Log the message content to see what's happening
            android.util.Log.d("NetPlayChat", "Message: nickname='${message.nickname}', message='${message.message}'")
            
            // Clean up nickname if it contains duplicate text
            val cleanNickname = message.nickname.trim()
            usernameText.text = cleanNickname
            timestampText.text = message.timestamp
            messageText.text = message.message
            
            // Set user icon based on nickname
            val iconRes = when {
                cleanNickname.equals("System", ignoreCase = true) -> R.drawable.ic_system
                else -> R.drawable.ic_user
            }
            userIcon.setImageResource(iconRes)
        }
    }
}
