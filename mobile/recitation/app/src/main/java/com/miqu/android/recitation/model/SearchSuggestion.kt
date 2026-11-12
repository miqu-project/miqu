package com.miqu.android.recitation.model

sealed class SearchSuggestion {
    data class SurahItem(
        val surah: Surah
    ) : SearchSuggestion()

    data class AyahJumpItem(
        val surahId: Int,
        val surahName: String,
        val ayahNumber: Int,
        val totalVerses: Int
    ) : SearchSuggestion()

    data class VerseTextItem(
        val verse: Verse,
        val surahName: String,
        val matchedSnippet: String = "",
        val translationMarker: String = "",
        val translationKey: String? = null
    ) : SearchSuggestion()

    data class RootItem(
        val rootEntry: RootEntry
    ) : SearchSuggestion()
}
