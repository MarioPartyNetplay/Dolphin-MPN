package org.dolphinemu.dolphinemu.adapters

import android.view.LayoutInflater
import android.view.View
import android.view.ViewGroup
import android.widget.TextView
import androidx.recyclerview.widget.RecyclerView
import org.dolphinemu.dolphinemu.R
import org.dolphinemu.dolphinemu.model.GameFile

/**
 * Game list adapter for game selection dialogs
 */
class GameListAdapter(
    private val onGameSelected: (GameFile) -> Unit
) : RecyclerView.Adapter<GameListAdapter.GameViewHolder>() {
    
    private val games = mutableListOf<GameFile>()
    
    fun updateGames(newGames: List<GameFile>) {
        games.clear()
        games.addAll(newGames)
        notifyDataSetChanged()
    }
    
    fun getCurrentGames(): List<GameFile> = games.toList()
    
    override fun onCreateViewHolder(parent: ViewGroup, viewType: Int): GameViewHolder {
        val view = LayoutInflater.from(parent.context)
            .inflate(R.layout.item_game_selection, parent, false)
        return GameViewHolder(view)
    }
    
    override fun onBindViewHolder(holder: GameViewHolder, position: Int) {
        holder.bind(games[position])
    }
    
    override fun getItemCount(): Int = games.size
    
    inner class GameViewHolder(itemView: View) : RecyclerView.ViewHolder(itemView) {
        private val gameNameText: TextView = itemView.findViewById(R.id.text_game_name)
        private val gameIdText: TextView = itemView.findViewById(R.id.text_game_id)
        private val platformText: TextView = itemView.findViewById(R.id.text_platform)
        private val selectionIndicator: View = itemView.findViewById(R.id.indicator_selection)
        
        fun bind(game: GameFile) {
            gameNameText.text = game.getTitle()
            gameIdText.text = game.getGameId()
            platformText.text = when (game.getPlatform()) {
                0 -> "GameCube"
                1 -> "Wii"
                else -> "Unknown"
            }
            
            // Handle item click
            itemView.setOnClickListener {
                onGameSelected(game)
            }
        }
    }
}
