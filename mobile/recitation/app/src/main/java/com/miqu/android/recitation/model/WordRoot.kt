package com.miqu.android.recitation.model

data class WordRoot(
    val surah: Int,
    val verse: Int,
    val surahVerse: String,
    val arabic: String,
    val english: String,
    val root: String,
    val position: Int
)
