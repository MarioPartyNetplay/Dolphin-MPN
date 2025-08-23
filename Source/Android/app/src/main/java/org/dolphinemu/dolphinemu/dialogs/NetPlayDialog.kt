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
import androidx.viewpager2.widget.ViewPager2
import com.google.android.material.tabs.TabLayout
import com.google.android.material.tabs.TabLayoutMediator
import org.dolphinemu.dolphinemu.R
import org.dolphinemu.dolphinemu.adapters.NetPlayPlayerAdapter
import org.dolphinemu.dolphinemu.adapters.NetPlaySessionAdapter
import org.dolphinemu.dolphinemu.features.netplay.*
import org.dolphinemu.dolphinemu.model.GameFile
import androidx.fragment.app.Fragment
import androidx.fragment.app.FragmentStateAdapter

/**
 * NetPlay Setup Dialog for Android - matches Dolphin's architecture
 * Inspired by Mandarine3DS implementation
 */
class NetPlayDialog : DialogFragment(), ConnectionCallback, ChatCallback, PlayerCallback {
    
    private lateinit var netPlayManager: NetPlayManager
    private lateinit var context: Context
    
    // UI Components
    private lateinit var tabLayout: TabLayout
    private lateinit var viewPager: ViewPager2
    private lateinit var nicknameEdit: EditText
    private lateinit var connectionTypeSpinner: Spinner
    
    // Tab fragments
    private lateinit var connectTab: ConnectTabFragment
    private lateinit var hostTab: HostTabFragment
    private lateinit var browserTab: BrowserTabFragment
    
    override fun onCreateDialog(savedInstanceState: Bundle?): Dialog {
        return Dialog(requireContext(), R.style.Theme_Dolphin_Dialog)
    }
    
    override fun onCreateView(
        inflater: LayoutInflater,
        container: ViewGroup?,
        savedInstanceState: Bundle?
    ): View? {
        return inflater.inflate(R.layout.dialog_netplay_setup, container, false)
    }
    
    override fun onViewCreated(view: View, savedInstanceState: Bundle?) {
        super.onViewCreated(view, savedInstanceState)
        
        context = requireContext()
        netPlayManager = NetPlayManager.getInstance()
        netPlayManager.setContext(context)
        
        initializeViews()
        setupTabs()
        loadSettings()
        
        // Set callbacks
        netPlayManager.setConnectionCallback(this)
        netPlayManager.setChatCallback(this)
        netPlayManager.setPlayerCallback(this)
    }
    
    private fun initializeViews() {
        view?.let { rootView ->
            tabLayout = rootView.findViewById(R.id.tab_layout)
            viewPager = rootView.findViewById(R.id.view_pager)
            nicknameEdit = rootView.findViewById(R.id.edit_nickname)
            connectionTypeSpinner = rootView.findViewById(R.id.spinner_connection_type)
        }
    }
    
    private fun setupTabs() {
        // Create tab fragments
        connectTab = ConnectTabFragment()
        hostTab = HostTabFragment()
        browserTab = BrowserTabFragment()
        
        // Create adapter for ViewPager
        val pagerAdapter = NetPlayPagerAdapter(this)
        pagerAdapter.addFragment(connectTab, "Connect")
        pagerAdapter.addFragment(hostTab, "Host")
        pagerAdapter.addFragment(browserTab, "Browser")
        
        viewPager.adapter = pagerAdapter
        
        // Connect tabs to ViewPager
        TabLayoutMediator(tabLayout, viewPager) { tab, position ->
            tab.text = when (position) {
                0 -> "Connect"
                1 -> "Host"
                2 -> "Browser"
                else -> "Unknown"
            }
        }.attach()
        
        // Setup connection type spinner
        val connectionTypes = arrayOf("Direct Connection", "Traversal Server")
        val adapter = ArrayAdapter(context, android.R.layout.simple_spinner_item, connectionTypes)
        adapter.setDropDownViewResource(android.R.layout.simple_spinner_dropdown_item)
        connectionTypeSpinner.adapter = adapter
        
        connectionTypeSpinner.onItemSelectedListener = object : AdapterView.OnItemSelectedListener {
            override fun onItemSelected(parent: AdapterView<*>?, view: View?, position: Int, id: Long) {
                val isTraversal = position == 1
                val connectionType = if (isTraversal) "traversal" else "direct"
                netPlayManager.setConnectionType(context, connectionType)
                connectTab.updateConnectionType(isTraversal)
                hostTab.updateConnectionType(isTraversal)
            }
            
            override fun onNothingSelected(parent: AdapterView<*>?) {}
        }
    }
    
    private fun loadSettings() {
        nicknameEdit.setText(netPlayManager.getUsername(context))
        val isTraversal = netPlayManager.getConnectionType(context) == "traversal"
        connectionTypeSpinner.setSelection(if (isTraversal) 1 else 0)
        
        // Save nickname when changed
        nicknameEdit.addTextChangedListener(object : android.text.TextWatcher {
            override fun beforeTextChanged(s: CharSequence?, start: Int, count: Int, after: Int) {}
            override fun onTextChanged(s: CharSequence?, start: Int, before: Int, count: Int) {}
            override fun afterTextChanged(s: android.text.Editable?) {
                s?.toString()?.let { nickname ->
                    if (nickname.isNotBlank()) {
                        netPlayManager.setUsername(context, nickname)
                    }
                }
            }
        })
    }
    
    // ConnectionCallback implementation
    override fun onConnected() {
        requireActivity().runOnUiThread {
            // Close setup dialog and open main netplay dialog
            dismiss()
            val mainDialog = NetPlayMainDialog()
            mainDialog.show(parentFragmentManager, "NetPlayMain")
        }
    }
    
    override fun onConnectionFailed(error: String) {
        requireActivity().runOnUiThread {
            showError("Connection failed: $error")
        }
    }
    
    override fun onDisconnected() {
        // Not used in setup dialog
    }
    
    override fun onConnectionLost() {
        requireActivity().runOnUiThread {
            showError("Connection lost")
        }
    }
    
    // ChatCallback implementation
    override fun onMessageReceived(message: ChatMessage) {
        // Not used in setup dialog
    }
    
    // PlayerCallback implementation
    override fun onPlayerJoined(player: NetPlayPlayer) {
        // Not used in setup dialog
    }
    
    override fun onPlayerLeft(playerId: Int) {
        // Not used in setup dialog
    }
    
    override fun onPlayerListUpdated(players: List<NetPlayPlayer>) {
        // Not used in setup dialog
    }
    
    private fun showError(message: String) {
        Toast.makeText(context, message, Toast.LENGTH_SHORT).show()
    }
    
    override fun onDestroy() {
        super.onDestroy()
        netPlayManager.setConnectionCallback(null)
        netPlayManager.setChatCallback(null)
        netPlayManager.setPlayerCallback(null)
    }
}

// Connect Tab Fragment
class ConnectTabFragment : Fragment() {
    private lateinit var ipEdit: EditText
    private lateinit var portEdit: EditText
    private lateinit var connectButton: Button
    private lateinit var ipLabel: TextView
    private lateinit var portLabel: TextView
    
    override fun onCreateView(
        inflater: LayoutInflater,
        container: ViewGroup?,
        savedInstanceState: Bundle?
    ): View? {
        return inflater.inflate(R.layout.fragment_netplay_connect, container, false)
    }
    
    override fun onViewCreated(view: View, savedInstanceState: Bundle?) {
        super.onViewCreated(view, savedInstanceState)
        
        ipEdit = view.findViewById(R.id.edit_ip)
        portEdit = view.findViewById(R.id.edit_port)
        connectButton = view.findViewById(R.id.button_connect)
        ipLabel = view.findViewById(R.id.label_ip)
        portLabel = view.findViewById(R.id.label_port)
        
        setupListeners()
        loadSettings()
    }
    
    private fun setupListeners() {
        connectButton.setOnClickListener {
            val ip = ipEdit.text.toString()
            val port = portEdit.text.toString().toIntOrNull() ?: 2626
            
            if (ip.isBlank()) {
                showError("Please enter server address")
                return@setOnClickListener
            }
            
            // Get parent dialog and connect
            val dialog = parentFragment as? NetPlayDialog
            dialog?.let {
                val netPlayManager = NetPlayManager.getInstance()
                netPlayManager.setServerAddress(requireContext(), ip)
                netPlayManager.setServerPort(requireContext(), port)
                netPlayManager.connectToServer(ip, port, it)
            }
        }
    }
    
    private fun loadSettings() {
        val netPlayManager = NetPlayManager.getInstance()
        ipEdit.setText(netPlayManager.getServerAddress(requireContext()))
        portEdit.setText(netPlayManager.getServerPort(requireContext()).toString())
    }
    
    fun updateConnectionType(isTraversal: Boolean) {
        if (isTraversal) {
            ipLabel.text = "Host Code:"
            ipEdit.hint = "Enter host code"
        } else {
            ipLabel.text = "IP Address:"
            ipEdit.hint = "Enter IP address"
        }
    }
    
    private fun showError(message: String) {
        Toast.makeText(requireContext(), message, Toast.LENGTH_SHORT).show()
    }
}

// Host Tab Fragment
class HostTabFragment : Fragment() {
    private lateinit var portEdit: EditText
    private lateinit var gameList: RecyclerView
    private lateinit var hostButton: Button
    private lateinit var selectGameButton: Button
    private lateinit var serverNameEdit: EditText
    private lateinit var upnpCheckBox: CheckBox
    private lateinit var chunkedUploadCheckBox: CheckBox
    private lateinit var chunkedUploadLimitEdit: EditText
    
    override fun onCreateView(
        inflater: LayoutInflater,
        container: ViewGroup?,
        savedInstanceState: Bundle?
    ): View? {
        return inflater.inflate(R.layout.fragment_netplay_host, container, false)
    }
    
    override fun onViewCreated(view: View, savedInstanceState: Bundle?) {
        super.onViewCreated(view, savedInstanceState)
        
        portEdit = view.findViewById(R.id.edit_host_port)
        gameList = view.findViewById(R.id.recycler_games)
        hostButton = view.findViewById(R.id.button_host)
        selectGameButton = view.findViewById(R.id.button_select_game)
        serverNameEdit = view.findViewById(R.id.edit_server_name)
        upnpCheckBox = view.findViewById(R.id.checkbox_upnp)
        chunkedUploadCheckBox = view.findViewById(R.id.checkbox_chunked_upload)
        chunkedUploadLimitEdit = view.findViewById(R.id.edit_chunked_upload_limit)
        
        setupGameList()
        setupListeners()
        loadSettings()
    }
    
    private fun setupGameList() {
        // TODO: Implement game list adapter
        gameList.layoutManager = LinearLayoutManager(requireContext())
    }
    
    private fun setupListeners() {
        selectGameButton.setOnClickListener {
            showGameSelectionDialog()
        }
        
        hostButton.setOnClickListener {
            val port = portEdit.text.toString().toIntOrNull() ?: 2626
            val serverName = serverNameEdit.text.toString()
            
            if (serverName.isBlank()) {
                showError("Please enter a server name")
                return@setOnClickListener
            }
            
            // TODO: Get selected game and host
            val dialog = parentFragment as? NetPlayDialog
            dialog?.let {
                val netPlayManager = NetPlayManager.getInstance()
                netPlayManager.setServerPort(requireContext(), port)
                netPlayManager.setServerName(requireContext(), serverName)
                netPlayManager.hostServer(port, it)
            }
        }
    }
    
    private fun loadSettings() {
        val netPlayManager = NetPlayManager.getInstance()
        portEdit.setText(netPlayManager.getServerPort(requireContext()).toString())
        serverNameEdit.setText(netPlayManager.getServerName(requireContext()))
    }
    
    fun updateConnectionType(isTraversal: Boolean) {
        // Update UI based on connection type
        upnpCheckBox.isVisible = !isTraversal
    }
    
    private fun showError(message: String) {
        Toast.makeText(requireContext(), message, Toast.LENGTH_SHORT).show()
    }
    
    private fun showGameSelectionDialog() {
        val gameDialog = GameSelectionDialog.newInstance { game ->
            // Game selected, update UI and enable host button
            Toast.makeText(requireContext(), "Selected: ${game.name}", Toast.LENGTH_SHORT).show()
            // TODO: Store selected game and enable host button
        }
        gameDialog.show(parentFragmentManager, "GameSelection")
    }
}

// Browser Tab Fragment
class BrowserTabFragment : Fragment() {
    private lateinit var sessionList: RecyclerView
    private lateinit var refreshButton: Button
    private lateinit var regionSpinner: Spinner
    private lateinit var hideInGameCheckBox: CheckBox
    private lateinit var visibilityRadioGroup: RadioGroup
    private lateinit var statusLabel: TextView
    
    override fun onCreateView(
        inflater: LayoutInflater,
        container: ViewGroup?,
        savedInstanceState: Bundle?
    ): View? {
        return inflater.inflate(R.layout.fragment_netplay_browser, container, false)
    }
    
    override fun onViewCreated(view: View, savedInstanceState: Bundle?) {
        super.onViewCreated(view, savedInstanceState)
        
        sessionList = view.findViewById(R.id.recycler_sessions)
        refreshButton = view.findViewById(R.id.button_refresh)
        regionSpinner = view.findViewById(R.id.spinner_region)
        hideInGameCheckBox = view.findViewById(R.id.checkbox_hide_ingame)
        visibilityRadioGroup = view.findViewById(R.id.radio_group_visibility)
        statusLabel = view.findViewById(R.id.label_status)
        
        setupSessionList()
        setupListeners()
        loadSettings()
        refreshSessions()
    }
    
    private fun setupSessionList() {
        // TODO: Implement session list adapter
        sessionList.layoutManager = LinearLayoutManager(requireContext())
    }
    
    private fun setupListeners() {
        refreshButton.setOnClickListener {
            refreshSessions()
        }
        
        // TODO: Handle session selection
    }
    
    private fun loadSettings() {
        // Load browser settings
    }
    
    private fun refreshSessions() {
        // TODO: Implement session refresh
        statusLabel.text = "Refreshing..."
        // Simulate refresh
        statusLabel.text = "Ready"
    }
}

// ViewPager Adapter
class NetPlayPagerAdapter(fragment: Fragment) : FragmentStateAdapter(fragment) {
    private val fragments = mutableListOf<Fragment>()
    private val titles = mutableListOf<String>()
    
    fun addFragment(fragment: Fragment, title: String) {
        fragments.add(fragment)
        titles.add(title)
    }
    
    override fun getItemCount(): Int = fragments.size
    
    override fun createFragment(position: Int): Fragment = fragments[position]
}
