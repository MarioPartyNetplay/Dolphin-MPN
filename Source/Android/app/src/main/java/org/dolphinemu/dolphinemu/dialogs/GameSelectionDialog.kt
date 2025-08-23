package org.dolphinemu.dolphinemu.dialogs

import android.app.Dialog
import android.os.Bundle
import android.view.LayoutInflater
import android.view.View
import android.view.ViewGroup
import android.widget.*
import androidx.fragment.app.DialogFragment
import androidx.recyclerview.widget.LinearLayoutManager
import androidx.recyclerview.widget.RecyclerView
import org.dolphinemu.dolphinemu.R
import org.dolphinemu.dolphinemu.adapters.GameListAdapter
import org.dolphinemu.dolphinemu.model.GameFile

/**
 * Dialog for selecting a game to host in NetPlay
 */
class GameSelectionDialog : DialogFragment() {
    
    private lateinit var gameList: RecyclerView
    private lateinit var searchEdit: EditText
    private lateinit var searchButton: Button
    private lateinit var selectButton: Button
    private lateinit var cancelButton: Button
    
    private lateinit var gameAdapter: GameListAdapter
    private var selectedGame: GameFile? = null
    private var onGameSelected: ((GameFile) -> Unit)? = null
    
    companion object {
        fun newInstance(onGameSelected: (GameFile) -> Unit): GameSelectionDialog {
            return GameSelectionDialog().apply {
                this.onGameSelected = onGameSelected
            }
        }
    }
    
    override fun onCreateDialog(savedInstanceState: Bundle?): Dialog {
        return Dialog(requireContext(), R.style.Theme_Dolphin_Dialog)
    }
    
    override fun onCreateView(
        inflater: LayoutInflater,
        container: ViewGroup?,
        savedInstanceState: Bundle?
    ): View? {
        return inflater.inflate(R.layout.dialog_game_selection, container, false)
    }
    
    override fun onViewCreated(view: View, savedInstanceState: Bundle?) {
        super.onViewCreated(view, savedInstanceState)
        
        initializeViews()
        setupGameList()
        setupListeners()
        loadGames()
    }
    
    private fun initializeViews() {
        view?.let { rootView ->
            gameList = rootView.findViewById(R.id.recycler_games)
            searchEdit = rootView.findViewById(R.id.edit_search)
            searchButton = rootView.findViewById(R.id.button_search)
            selectButton = rootView.findViewById(R.id.button_select)
            cancelButton = rootView.findViewById(R.id.button_cancel)
        }
    }
    
    private fun setupGameList() {
        gameAdapter = GameListAdapter { game ->
            selectedGame = game
            selectButton.isEnabled = true
        }
        
        gameList.layoutManager = LinearLayoutManager(requireContext())
        gameList.adapter = gameAdapter
    }
    
    private fun setupListeners() {
        searchButton.setOnClickListener {
            val query = searchEdit.text.toString()
            if (query.isNotBlank()) {
                searchGames(query)
            }
        }
        
        selectButton.setOnClickListener {
            selectedGame?.let { game ->
                onGameSelected?.invoke(game)
                dismiss()
            }
        }
        
        cancelButton.setOnClickListener {
            dismiss()
        }
        
        // Enable select button only when a game is selected
        selectButton.isEnabled = false
    }
    
    private fun loadGames() {
        try {
            // Try to load games from Dolphin's game list via JNI
            val games = loadGamesFromDolphin()
            if (games.isNotEmpty()) {
                gameAdapter.updateGames(games)
            } else {
                // Fallback to sample games if none found
                loadSampleGames()
            }
        } catch (e: Exception) {
            Log.e("GameSelectionDialog", "Error loading games from Dolphin", e)
            // Fallback to sample games on error
            loadSampleGames()
        }
    }
    
    private fun loadSampleGames() {
        val sampleGames = listOf(
            GameFile("Super Mario Sunshine", "/path/to/sms.iso", "GMSJ01", "GameCube"),
            GameFile("Mario Kart: Double Dash!!", "/path/to/mkdd.iso", "GM4P01", "GameCube"),
            GameFile("Super Smash Bros. Melee", "/path/to/ssbm.iso", "GALE01", "GameCube"),
            GameFile("Mario Party 4", "/path/to/mp4.iso", "GMP401", "GameCube"),
            GameFile("Mario Party 5", "/path/to/mp5.iso", "GMP501", "GameCube"),
            GameFile("Mario Party 6", "/path/to/mp6.iso", "GMP601", "GameCube"),
            GameFile("Mario Party 7", "/path/to/mp7.iso", "GMP701", "GameCube")
        )
        
        gameAdapter.updateGames(sampleGames)
    }
    
    /**
     * Call JNI to load games from Dolphin's game list
     */
    private external fun loadGamesFromDolphin(): List<GameFile>
    
    private fun searchGames(query: String) {
        // TODO: Implement actual game search
        // For now, just filter the current list
        val currentGames = gameAdapter.getCurrentGames()
        val filteredGames = currentGames.filter { game ->
            game.name.contains(query, ignoreCase = true) ||
            game.gameId.contains(query, ignoreCase = true)
        }
        gameAdapter.updateGames(filteredGames)
    }
}

// Game list adapter
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
            gameNameText.text = game.name
            gameIdText.text = game.gameId
            platformText.text = game.platform
            
            // Handle item click
            itemView.setOnClickListener {
                onGameSelected(game)
            }
        }
    }
}

// Game file data class
data class GameFile(
    val name: String,
    val path: String,
    val gameId: String,
    val platform: String
)
