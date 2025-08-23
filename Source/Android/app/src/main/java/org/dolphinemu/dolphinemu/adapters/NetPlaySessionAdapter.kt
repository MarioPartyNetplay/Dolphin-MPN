package org.dolphinemu.dolphinemu.adapters

import android.view.LayoutInflater
import android.view.View
import android.view.ViewGroup
import android.widget.ImageView
import android.widget.TextView
import androidx.recyclerview.widget.RecyclerView
import org.dolphinemu.dolphinemu.R
import org.dolphinemu.dolphinemu.model.NetPlaySession

/**
 * Adapter for NetPlay session list in the browser
 */
class NetPlaySessionAdapter(
    private val onSessionSelected: (NetPlaySession) -> Unit
) : RecyclerView.Adapter<NetPlaySessionAdapter.SessionViewHolder>() {
    
    private val sessions = mutableListOf<NetPlaySession>()
    
    fun updateSessions(newSessions: List<NetPlaySession>) {
        sessions.clear()
        sessions.addAll(newSessions)
        notifyDataSetChanged()
    }
    
    override fun onCreateViewHolder(parent: ViewGroup, viewType: Int): SessionViewHolder {
        val view = LayoutInflater.from(parent.context)
            .inflate(R.layout.item_netplay_session, parent, false)
        return SessionViewHolder(view)
    }
    
    override fun onBindViewHolder(holder: SessionViewHolder, position: Int) {
        holder.bind(sessions[position])
    }
    
    override fun getItemCount(): Int = sessions.size
    
    inner class SessionViewHolder(itemView: View) : RecyclerView.ViewHolder(itemView) {
        private val sessionNameText: TextView = itemView.findViewById(R.id.text_session_name)
        private val gameIdText: TextView = itemView.findViewById(R.id.text_game_id)
        private val regionText: TextView = itemView.findViewById(R.id.text_region)
        private val methodText: TextView = itemView.findViewById(R.id.text_method)
        private val playerCountText: TextView = itemView.findViewById(R.id.text_player_count)
        private val passwordIcon: ImageView = itemView.findViewById(R.id.icon_password)
        private val inGameIcon: ImageView = itemView.findViewById(R.id.icon_ingame)
        
        fun bind(session: NetPlaySession) {
            sessionNameText.text = session.getDisplayName()
            gameIdText.text = session.gameId
            regionText.text = session.getRegionDisplayName()
            methodText.text = session.getMethodDisplayName()
            playerCountText.text = "${session.playerCount}/4"
            
            // Show password icon if session has password
            passwordIcon.visibility = if (session.hasPassword) View.VISIBLE else View.GONE
            
            // Show in-game icon if session is in game
            inGameIcon.visibility = if (session.inGame) View.VISIBLE else View.GONE
            
            // Handle item click
            itemView.setOnClickListener {
                onSessionSelected(session)
            }
        }
    }
}
