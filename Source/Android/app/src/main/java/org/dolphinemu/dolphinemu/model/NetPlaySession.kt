package org.dolphinemu.dolphinemu.model

/**
 * NetPlay Session data class matching Dolphin's structure
 */
data class NetPlaySession(
    val name: String,
    val region: String,
    val method: String,
    val serverId: String,
    val gameId: String,
    val version: String,
    val playerCount: Int,
    val port: Int,
    val hasPassword: Boolean,
    val inGame: Boolean
) {
    /**
     * Get display name with player count
     */
    fun getDisplayName(): String {
        return "$name ($playerCount/4)"
    }
    
    /**
     * Get region display name
     */
    fun getRegionDisplayName(): String {
        return when (region) {
            "US" -> "United States"
            "EU" -> "Europe"
            "JP" -> "Japan"
            "AU" -> "Australia"
            "KR" -> "Korea"
            else -> region
        }
    }
    
    /**
     * Get method display name
     */
    fun getMethodDisplayName(): String {
        return when (method) {
            "direct" -> "Direct Connection"
            "traversal" -> "Traversal Server"
            else -> method
        }
    }
}
