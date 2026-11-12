package com.miqu.android.recitation.data

import android.content.Context
import com.miqu.android.recitation.model.RootEntry

class LexiconRepository(private val context: Context) {
    private var cachedEntries: List<RootEntry>? = null
    private var rootMap: Map<String, RootEntry>? = null

    private fun loadLexicon() {
        if (cachedEntries != null) return

        val entries = mutableListOf<RootEntry>()
        val map = HashMap<String, RootEntry>()

        try {
            val reader = context.assets.open("metadata/dictionary.md").bufferedReader()
            var currentRoot: String? = null
            val currentDef = StringBuilder()

            reader.forEachLine { line ->
                val trimmed = line.trim()
                if (trimmed.startsWith("# ")) {
                    if (currentRoot != null && currentDef.isNotEmpty()) {
                        val entry = RootEntry(currentRoot!!, currentDef.toString().trim())
                        entries.add(entry)
                        map[currentRoot!!] = entry
                    }
                    currentRoot = trimmed.removePrefix("# ").trim()
                    currentDef.clear()
                } else if (currentRoot != null) {
                    currentDef.append(line).append("\n")
                }
            }

            if (currentRoot != null && currentDef.isNotEmpty()) {
                val entry = RootEntry(currentRoot!!, currentDef.toString().trim())
                entries.add(entry)
                map[currentRoot!!] = entry
            }
        } catch (e: Exception) {
            e.printStackTrace()
        }

        cachedEntries = entries
        rootMap = map
    }

    fun getAllRoots(): List<RootEntry> {
        loadLexicon()
        return cachedEntries ?: emptyList()
    }

    fun getRootEntry(root: String): RootEntry? {
        loadLexicon()
        return rootMap?.get(root.trim())
    }

    fun searchRoots(query: String): List<RootEntry> {
        val q = query.trim()
        val all = getAllRoots()
        if (q.isEmpty()) return all

        return all.filter {
            it.root.contains(q) || it.definition.contains(q, ignoreCase = true)
        }
    }
}
