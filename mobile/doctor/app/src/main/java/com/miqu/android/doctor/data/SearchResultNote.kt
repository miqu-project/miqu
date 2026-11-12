package com.miqu.android.doctor.data

data class SearchResultNote(
    val note: NoteEntity,
    val notebookName: String,
    val matchSnippet: String = "",
    val matchedInTitle: Boolean = false
)
