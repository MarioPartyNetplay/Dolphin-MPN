package org.dolphinemu.dolphinemu.adapters

import android.view.LayoutInflater
import android.view.View
import android.view.ViewGroup
import android.widget.ImageView
import android.widget.TextView
import androidx.recyclerview.widget.RecyclerView
import org.dolphinemu.dolphinemu.R
import org.dolphinemu.dolphinemu.features.netplay.NetPlayPlayer

/**
 * Adapter for NetPlay player list
 */
class NetPlayPlayerAdapter(
    private val onPlayerSelected: (NetPlayPlayer) -> Unit
) : RecyclerView.Adapter<NetPlayPlayerAdapter.PlayerViewHolder>() {
    
    private val players = mutableListOf<NetPlayPlayer>()
    private var selectedPlayerId: Int = -1
    
    fun addPlayer(player: NetPlayPlayer) {
        if (!players.any { it.id == player.id }) {
            players.add(player)
            notifyItemInserted(players.size - 1)
        }
    }
    
    fun removePlayer(playerId: Int) {
        val index = players.indexOfFirst { it.id == playerId }
        if (index != -1) {
            players.removeAt(index)
            notifyItemRemoved(index)
            
            // Clear selection if the selected player was removed
            if (selectedPlayerId == playerId) {
                selectedPlayerId = -1
            }
        }
    }
    
    fun updatePlayers(newPlayers: List<NetPlayPlayer>) {
        players.clear()
        players.addAll(newPlayers)
        notifyDataSetChanged()
    }
    
    fun getSelectedPlayer(): NetPlayPlayer? {
        return players.find { it.id == selectedPlayerId }
    }
    
    override fun onCreateViewHolder(parent: ViewGroup, viewType: Int): PlayerViewHolder {
        val view = LayoutInflater.from(parent.context)
            .inflate(R.layout.item_netplay_player, parent, false)
        return PlayerViewHolder(view)
    }
    
    override fun onBindViewHolder(holder: PlayerViewHolder, position: Int) {
        holder.bind(players[position], selectedPlayerId == players[position].id)
    }
    
    override fun getItemCount(): Int = players.size
    
    inner class PlayerViewHolder(itemView: View) : RecyclerView.ViewHolder(itemView) {
        private val playerIcon: ImageView = itemView.findViewById(R.id.icon_player)
        private val playerName: TextView = itemView.findViewById(R.id.text_player_name)
        private val playerStatus: TextView = itemView.findViewById(R.id.text_player_status)
        private val selectionIndicator: View = itemView.findViewById(R.id.indicator_selection)
        
        fun bind(player: NetPlayPlayer, isSelected: Boolean) {
            playerName.text = player.nickname
            playerStatus.text = if (player.isConnected) "Connected" else "Disconnected"
            
            // Set player icon (host gets crown icon)
            val iconRes = if (player.id == 1) R.drawable.ic_crown else R.drawable.ic_user
            playerIcon.setImageResource(iconRes)
            
            // Update selection state
            selectionIndicator.visibility = if (isSelected) View.VISIBLE else View.GONE
            
            // Handle item click
            itemView.setOnClickListener {
                val previousSelected = selectedPlayerId
                selectedPlayerId = if (isSelected) -1 else player.id
                
                // Update previous selection
                if (previousSelected != -1) {
                    notifyItemChanged(players.indexOfFirst { it.id == previousSelected })
                }
                
                // Update current selection
                notifyItemChanged(adapterPosition)
                
                // Notify callback
                if (selectedPlayerId != -1) {
                    onPlayerSelected(player)
                }
            }
        }
    }
}
