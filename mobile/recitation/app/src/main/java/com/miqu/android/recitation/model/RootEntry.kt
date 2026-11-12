package com.miqu.android.recitation.model

data class RootEntry(
    val root: String,
    val definition: String,
    var occurrencesCount: Int = 0
)
