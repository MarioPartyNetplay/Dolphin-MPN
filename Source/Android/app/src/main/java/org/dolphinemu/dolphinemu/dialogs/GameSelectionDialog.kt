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
        // For now, we'll create empty games since we can't instantiate the native GameFile class
        // In a real implementation, this would load from the actual game list
        Log.d("GameSelectionDialog", "Loading sample games (placeholder)")
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
            game.getTitle().contains(query, ignoreCase = true) ||
            game.getGameId().contains(query, ignoreCase = true)
        }
        gameAdapter.updateGames(filteredGames)
    }
}