package org.dolphinemu.dolphinemu.dialogs

import android.app.Dialog
import android.os.Bundle
import android.view.LayoutInflater
import android.view.View
import android.view.ViewGroup
import android.widget.*
import androidx.fragment.app.DialogFragment
import org.dolphinemu.dolphinemu.R

/**
 * Dialog for assigning controller ports to players in NetPlay
 */
class PortAssignmentDialog : DialogFragment() {
    
    private lateinit var port0Spinner: Spinner
    private lateinit var port1Spinner: Spinner
    private lateinit var port2Spinner: Spinner
    private lateinit var port3Spinner: Spinner
    private lateinit var assignButton: Button
    private lateinit var cancelButton: Button
    
    private var onPortsAssigned: ((Map<Int, String>) -> Unit)? = null
    
    companion object {
        fun newInstance(onPortsAssigned: (Map<Int, String>) -> Unit): PortAssignmentDialog {
            return PortAssignmentDialog().apply {
                this.onPortsAssigned = onPortsAssigned
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
        return inflater.inflate(R.layout.dialog_port_assignment, container, false)
    }
    
    override fun onViewCreated(view: View, savedInstanceState: Bundle?) {
        super.onViewCreated(view, savedInstanceState)
        
        initializeViews()
        setupPortSpinners()
        setupListeners()
    }
    
    private fun initializeViews() {
        view?.let { rootView ->
            port0Spinner = rootView.findViewById(R.id.spinner_port_0)
            port1Spinner = rootView.findViewById(R.id.spinner_port_1)
            port2Spinner = rootView.findViewById(R.id.spinner_port_2)
            port3Spinner = rootView.findViewById(R.id.spinner_port_3)
            assignButton = rootView.findViewById(R.id.button_assign)
            cancelButton = rootView.findViewById(R.id.button_cancel)
        }
    }
    
    private fun setupPortSpinners() {
        val playerOptions = listOf(
            "None" to "No player",
            "Player 1" to "Player 1",
            "Player 2" to "Player 2", 
            "Player 3" to "Player 3",
            "Player 4" to "Player 4"
        )
        
        val adapter = ArrayAdapter(
            requireContext(),
            android.R.layout.simple_spinner_item,
            playerOptions.map { it.second }
        )
        adapter.setDropDownViewResource(android.R.layout.simple_spinner_dropdown_item)
        
        port0Spinner.adapter = adapter
        port1Spinner.adapter = adapter
        port2Spinner.adapter = adapter
        port3Spinner.adapter = adapter
        
        // Set default assignments
        port0Spinner.setSelection(1) // Player 1 on port 0
        port1Spinner.setSelection(2) // Player 2 on port 1
        port2Spinner.setSelection(0) // None on port 2
        port3Spinner.setSelection(0) // None on port 3
    }
    
    private fun setupListeners() {
        assignButton.setOnClickListener {
            val assignments = getPortAssignments()
            if (validateAssignments(assignments)) {
                onPortsAssigned?.invoke(assignments)
                dismiss()
            } else {
                showError("Invalid port assignments. Each player can only be assigned to one port.")
            }
        }
        
        cancelButton.setOnClickListener {
            dismiss()
        }
    }
    
    private fun getPortAssignments(): Map<Int, String> {
        val assignments = mutableMapOf<Int, String>()
        
        val port0Player = getSelectedPlayer(port0Spinner)
        val port1Player = getSelectedPlayer(port1Spinner)
        val port2Player = getSelectedPlayer(port2Spinner)
        val port3Player = getSelectedPlayer(port3Spinner)
        
        if (port0Player != "No player") assignments[0] = port0Player
        if (port1Player != "No player") assignments[1] = port1Player
        if (port2Player != "No player") assignments[2] = port2Player
        if (port3Player != "No player") assignments[3] = port3Player
        
        return assignments
    }
    
    private fun getSelectedPlayer(spinner: Spinner): String {
        val position = spinner.selectedItemPosition
        return when (position) {
            0 -> "No player"
            1 -> "Player 1"
            2 -> "Player 2"
            3 -> "Player 3"
            4 -> "Player 4"
            else -> "No player"
        }
    }
    
    private fun validateAssignments(assignments: Map<Int, String>): Boolean {
        val players = assignments.values.toSet()
        val uniquePlayers = players.filter { it != "No player" }
        
        // Check for duplicate player assignments
        if (uniquePlayers.size != uniquePlayers.toSet().size) {
            return false
        }
        
        // Check that at least one player is assigned
        if (uniquePlayers.isEmpty()) {
            return false
        }
        
        return true
    }
    
    private fun showError(message: String) {
        Toast.makeText(requireContext(), message, Toast.LENGTH_SHORT).show()
    }
}
