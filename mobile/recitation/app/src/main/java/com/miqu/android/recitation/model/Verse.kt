package com.miqu.android.recitation.model

data class Verse(
    val id: Int,
    val surahNumber: Int,
    val verseNumber: Int,
    val arabic: String,
    val english: String,
    val englishY: String,
    val indonesian: String,
    val urdu: String,
    val assamese: String,
    val bengali: String
) {
    fun getTranslation(languageKey: String): String {
        return when (languageKey.lowercase()) {
            "bengali", "bn" -> bengali.ifEmpty { english }
            "urdu", "ur" -> urdu.ifEmpty { english }
            "indonesian", "id" -> indonesian.ifEmpty { english }
            "assamese", "as" -> assamese.ifEmpty { english }
            "english_y", "yusufali" -> englishY.ifEmpty { english }
            else -> english
        }
    }
}
