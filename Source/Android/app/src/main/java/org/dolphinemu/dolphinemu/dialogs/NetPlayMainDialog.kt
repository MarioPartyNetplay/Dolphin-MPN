package org.dolphinemu.dolphinemu.dialogs

import android.app.Dialog
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
            
            // Players section
            playersRecyclerView = rootView.findViewById(R.id.recycler_players)
            kickButton = rootView.findViewById(R.id.button_kick)
            banButton = rootView.findViewById(R.id.button_ban)
            assignPortsButton = rootView.findViewById(R.id.button_assign_ports)
            
            // Game controls
            gameButton = rootView.findViewById(R.id.button_game)
            startButton = rootView.findViewById(R.id.button_start)
            quitButton = rootView.findViewById(R.id.button_quit)
            
            // Settings
            bufferSizeBox = rootView.findViewById(R.id.spinner_buffer_size)
            bufferLabel = rootView.findViewById(R.id.label_buffer)
        }
    }
    
    private fun setupAdapters() {
        // Player adapter
        playerAdapter = NetPlayPlayerAdapter { player ->
            showPlayerOptions(player)
        }
        playersRecyclerView.layoutManager = LinearLayoutManager(context)
        playersRecyclerView.adapter = playerAdapter
        
        // Chat adapter
        chatAdapter = ChatAdapter(netPlayManager.getChatMessages())
        chatRecyclerView.layoutManager = LinearLayoutManager(context).apply {
            stackFromEnd = true
        }
        chatRecyclerView.adapter = chatAdapter
        
        // Buffer size spinner
        val bufferSizes = arrayOf("1", "2", "3", "4", "5", "6", "7", "8", "9", "10")
        val bufferAdapter = ArrayAdapter(context, android.R.layout.simple_spinner_item, bufferSizes)
        bufferAdapter.setDropDownViewResource(android.R.layout.simple_spinner_dropdown_item)
        bufferSizeBox.adapter = bufferAdapter
        bufferSizeBox.setSelection(4) // Default to 5
    }
    
    private fun setupListeners() {
        sendButton.setOnClickListener {
            val message = chatInput.text.toString()
            if (message.isNotBlank()) {
                netPlayManager.sendMessage(message)
                chatInput.text?.clear()
            }
        }
        
        kickButton.setOnClickListener {
            val selectedPlayer = playerAdapter.getSelectedPlayer()
            if (selectedPlayer != null) {
                netPlayManager.kickPlayer(selectedPlayer.id)
            }
        }
        
        banButton.setOnClickListener {
            val selectedPlayer = playerAdapter.getSelectedPlayer()
            if (selectedPlayer != null) {
                netPlayManager.banPlayer(selectedPlayer.id)
            }
        }
        
        assignPortsButton.setOnClickListener {
            showAssignPortsDialog()
        }
        
        gameButton.setOnClickListener {
            showGameSelectionDialog()
        }
        
        startButton.setOnClickListener {
            startGame()
        }
        
        quitButton.setOnClickListener {
            netPlayManager.disconnect()
            dismiss()
        }
        
        hostCodeActionButton.setOnClickListener {
            copyHostCode()
        }
        
        bufferSizeBox.onItemSelectedListener = object : AdapterView.OnItemSelectedListener {
            override fun onItemSelected(parent: AdapterView<*>?, view: View?, position: Int, id: Long) {
                val bufferSize = position + 1
                // TODO: Set buffer size in NetPlayManager
            }
            
            override fun onNothingSelected(parent: AdapterView<*>?) {}
        }
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
        
        // Update button states
        kickButton.isEnabled = isHost && isConnected
        banButton.isEnabled = isHost && isConnected
        assignPortsButton.isEnabled = isHost && isConnected
        gameButton.isEnabled = isHost && isConnected
        startButton.isEnabled = isHost && isConnected
        
        // Update room box visibility
        roomBox.visibility = if (isHost) View.VISIBLE else View.GONE
        hostCodeLabel.visibility = if (isHost) View.VISIBLE else View.GONE
        hostCodeActionButton.visibility = if (isHost) View.VISIBLE else View.GONE
        
        // Update host code if hosting
        if (isHost) {
            val hostCode = netPlayManager.getHostCode(context)
            hostCodeLabel.text = "Code: $hostCode"
            updateRoomOptions()
        }
    }
    
    private fun updateRoomOptions() {
        // TODO: Populate room options based on available interfaces
        val roomOptions = arrayOf("Local", "External")
        val adapter = ArrayAdapter(context, android.R.layout.simple_spinner_item, roomOptions)
        adapter.setDropDownViewResource(android.R.layout.simple_spinner_dropdown_item)
        roomBox.adapter = adapter
    }
    
    private fun showPlayerOptions(player: NetPlayPlayer) {
        if (!netPlayManager.isHost()) return
        
        val options = arrayOf("Kick Player", "Ban Player")
        AlertDialog.Builder(context)
            .setTitle("Player Options: ${player.nickname}")
            .setItems(options) { _, which ->
                when (which) {
                    0 -> netPlayManager.kickPlayer(player.id)
                    1 -> netPlayManager.banPlayer(player.id)
                }
            }
            .show()
    }
    
    private fun showAssignPortsDialog() {
        val portDialog = PortAssignmentDialog.newInstance { assignments ->
            // Handle port assignments
            val message = "Ports assigned: ${assignments.entries.joinToString { "P${it.key}=${it.value}" }}"
            Toast.makeText(context, message, Toast.LENGTH_SHORT).show()
            
            // TODO: Send port assignments to NetPlayManager
            // netPlayManager.assignPorts(assignments)
        }
        portDialog.show(parentFragmentManager, "PortAssignment")
    }
    
    private fun showGameSelectionDialog() {
        // TODO: Implement game selection dialog
        Toast.makeText(context, "Game selection not yet implemented", Toast.LENGTH_SHORT).show()
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
        messages.add(message)
        if (messages.size > 100) {
            messages.removeAt(0)
        }
        notifyDataSetChanged()
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
            usernameText.text = message.nickname
            timestampText.text = message.timestamp
            messageText.text = message.message
            
            // Set user icon based on nickname
            val iconRes = when (message.nickname) {
                "System" -> R.drawable.ic_system
                else -> R.drawable.ic_user
            }
            userIcon.setImageResource(iconRes)
        }
    }
}
