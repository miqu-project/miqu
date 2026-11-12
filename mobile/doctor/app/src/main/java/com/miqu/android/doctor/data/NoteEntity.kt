package com.miqu.android.doctor.data

data class NoteEntity(
    val id: Long = 0L,
    val title: String,
    val content: String,
    val category: String = "General",
    val notebookId: Long = 2L,
    val isPinned: Boolean = false,
    val isBuiltIn: Boolean = false,
    val createdAt: Long = System.currentTimeMillis(),
    val updatedAt: Long = System.currentTimeMillis()
)
