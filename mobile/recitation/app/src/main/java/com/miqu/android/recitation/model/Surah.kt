package com.miqu.android.recitation.model

data class Surah(
    val id: Int,
    val name: String,
    val transliteration: String,
    val type: String,
    val totalVerses: Int,
    val english: String,
    val bengali: String,
    val urdu: String,
    val indonesian: String
)
