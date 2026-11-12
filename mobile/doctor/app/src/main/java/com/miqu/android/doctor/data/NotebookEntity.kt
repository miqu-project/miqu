package com.miqu.android.doctor.data

data class NotebookEntity(
    val id: Long = 0L,
    val name: String,
    val icon: String = "folder",
    val isBuiltIn: Boolean = false,
    val noteCount: Int = 0,
    val createdAt: Long = System.currentTimeMillis(),
    val updatedAt: Long = System.currentTimeMillis()
)
