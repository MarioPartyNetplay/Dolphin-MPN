package org.dolphinemu.dolphinemu.dialogs

import android.app.Dialog
import android.os.Bundle
import android.util.Log
import android.view.LayoutInflater
import android.view.View
import android.view.ViewGroup
import android.widget.*
import androidx.fragment.app.DialogFragment
import androidx.recyclerview.widget.LinearLayoutManager
import androidx.recyclerview.widget.RecyclerView
import org.dolphinemu.dolphinemu.R
import org.dolphinemu.dolphinemu.features.netplay.*

/**
 * Simple MPN NetPlay Browser Dialog
 * Focuses on joining sessions with MPN version games
 */
class MPNNetPlayBrowserDialog : DialogFragment(), LobbyCallback, ConnectionCallback {
    
    private lateinit var recyclerView: RecyclerView
    private lateinit var adapter: MPNNetPlayAdapter
    private lateinit var refreshButton: Button
    private lateinit var statusText: TextView
    private lateinit var netPlayManager: NetPlayManager
    
    override fun onCreateDialog(savedInstanceState: Bundle?): Dialog {
        return Dialog(requireContext(), R.style.Theme_Dolphin_Dialog)
    }
    
    override fun onCreateView(
        inflater: LayoutInflater,
        container: ViewGroup?,
        savedInstanceState: Bundle?
    ): View? {
        return inflater.inflate(R.layout.dialog_mpn_netplay_browser, container, false)
    }
    
    override fun onViewCreated(view: View, savedInstanceState: Bundle?) {
        super.onViewCreated(view, savedInstanceState)
        
        try {
            initializeViews()
            setupRecyclerView()
            initializeNetPlay()
            discoverServers()
        } catch (e: Exception) {
            Log.e("MPNNetPlayBrowserDialog", "Error in onViewCreated: ${e.message}")
            statusText.text = "Error initializing: ${e.message}"
        }
    }
    
    private fun initializeViews() {
        view?.let { rootView ->
            recyclerView = rootView.findViewById(R.id.recycler_mpn_games)
            refreshButton = rootView.findViewById(R.id.button_refresh)
            statusText = rootView.findViewById(R.id.text_status)
            
            refreshButton.setOnClickListener {
                discoverServers()
            }
        }
    }
    
    private fun setupRecyclerView() {
        adapter = MPNNetPlayAdapter { server ->
            onServerSelected(server)
        }
        
        recyclerView.layoutManager = LinearLayoutManager(context)
        recyclerView.adapter = adapter
    }

    private fun initializeNetPlay() {
        netPlayManager = NetPlayManager.getInstance()
        netPlayManager.setContext(requireContext())
        netPlayManager.setLobbyCallback(this)
        netPlayManager.setConnectionCallback(this)
    }

    private fun discoverServers() {
        statusText.text = "Discovering NetPlay servers..."
        netPlayManager.discoverServers()
    }

    private fun onServerSelected(server: LobbyServer) {
        try {
            // Show a confirmation dialog to join the server
            val builder = androidx.appcompat.app.AlertDialog.Builder(requireContext())
            builder.setTitle("Join NetPlay Session")
                .setMessage("Join session: ${server.name}\nGame: ${server.gameName}\nPlayers: ${server.playerCount}/${server.maxPlayers}\nServer: ${server.address}:${server.port}")
                .setPositiveButton("Join") { dialog, _ -> 
                    dialog.dismiss()
                    statusText.text = "Connecting to ${server.name}..."
                    try {
                        netPlayManager.connectToLobbyServer(server, this)
                    } catch (e: Exception) {
                        Log.e("MPNNetPlayBrowserDialog", "Error connecting to server: ${e.message}")
                        statusText.text = "Connection error: ${e.message}"
                        Toast.makeText(context, "Connection failed: ${e.message}", Toast.LENGTH_SHORT).show()
                    }
                }
                .setNegativeButton("Cancel") { dialog, _ -> dialog.dismiss() }
                .show()
        } catch (e: Exception) {
            Log.e("MPNNetPlayBrowserDialog", "Error in onServerSelected: ${e.message}")
            Toast.makeText(context, "Error: ${e.message}", Toast.LENGTH_SHORT).show()
        }
    }

    // LobbyCallback implementation
    override fun onServersDiscovered(servers: List<LobbyServer>) {
        requireActivity().runOnUiThread {
            if (servers.isEmpty()) {
                statusText.text = "No available Netplay servers found. All sessions may be in-game or try refreshing."
            } else {
                statusText.text = "Found ${servers.size} available Netplay server(s)"
            }
            adapter.updateServers(servers)
        }
    }

    override fun onDiscoveryFailed(error: String) {
        requireActivity().runOnUiThread {
            statusText.text = "Discovery failed: $error"
        }
    }

    // ConnectionCallback implementation
    override fun onConnected() {
        requireActivity().runOnUiThread {
            Toast.makeText(context, "Connected to server!", Toast.LENGTH_SHORT).show()
            // Close the browser dialog and open the main NetPlay lobby dialog
            dismiss()
            val mainDialog = NetPlayMainDialog()
            mainDialog.show(parentFragmentManager, "NetPlayMain")
        }
    }

    override fun onConnectionFailed(error: String) {
        requireActivity().runOnUiThread {
            Toast.makeText(context, "Connection failed: $error", Toast.LENGTH_SHORT).show()
        }
    }

    override fun onDisconnected() {
        // Not used in browser dialog
    }

    override fun onConnectionLost() {
        requireActivity().runOnUiThread {
            Toast.makeText(context, "Connection lost", Toast.LENGTH_SHORT).show()
        }
    }
}

/**
 * Adapter for displaying MPN servers in the RecyclerView
 */
class MPNNetPlayAdapter(
    private val onServerClick: (LobbyServer) -> Unit
) : RecyclerView.Adapter<MPNNetPlayAdapter.ViewHolder>() {
    
    private var servers: List<LobbyServer> = emptyList()
    
    fun updateServers(newServers: List<LobbyServer>) {
        servers = newServers
        notifyDataSetChanged()
    }
    
    override fun onCreateViewHolder(parent: ViewGroup, viewType: Int): ViewHolder {
        val view = LayoutInflater.from(parent.context)
            .inflate(R.layout.item_mpn_server, parent, false)
        return ViewHolder(view)
    }
    
    override fun onBindViewHolder(holder: ViewHolder, position: Int) {
        val server = servers[position]
        holder.serverNameText.text = server.name
        holder.gameInfoText.text = "${server.gameName} (${server.gameId})"
        holder.playerCountText.text = "${server.playerCount}/${server.maxPlayers} players"
        
        // Hide port if it's 2626 (traversal) or if the address looks like a host code
        val addressText = if (server.address.length <= 8) {
            server.address // Just show the address/host code without port
        } else {
            "${server.address}:${server.port}" // Show full address:port for direct connections
        }
        holder.serverAddressText.text = addressText
        
        holder.itemView.setOnClickListener { onServerClick(server) }
    }
    
    override fun getItemCount(): Int = servers.size
    
    class ViewHolder(itemView: View) : RecyclerView.ViewHolder(itemView) {
        val serverNameText: TextView = itemView.findViewById(R.id.text_server_name)
        val gameInfoText: TextView = itemView.findViewById(R.id.text_game_info)
        val playerCountText: TextView = itemView.findViewById(R.id.text_player_count)
        val serverAddressText: TextView = itemView.findViewById(R.id.text_server_address)
    }
}