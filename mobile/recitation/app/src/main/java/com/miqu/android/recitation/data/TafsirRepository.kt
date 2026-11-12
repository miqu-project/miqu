package com.miqu.android.recitation.data

import android.content.Context
import java.io.BufferedReader
import java.io.InputStreamReader

class TafsirRepository(private val context: Context) {

    data class TafsirBook(
        val fileName: String,
        val displayName: String,
        val author: String,
        val language: String
    )

    companion object {
        val AVAILABLE_TAFSIRS = listOf(
            TafsirBook("tafsir_en_ibne_katheer.md", "Ibn Kathir (English)", "Hafiz Ibn Kathir", "English"),
            TafsirBook("tafsir_ben_ibne_katheer.md", "তাফসীর ইবনে কাসীর (বাংলা)", "হাফিজ ইবনে কাসীর", "Bengali"),
            TafsirBook("tafsir_ur_ibne_katheer.md", "تفسیر ابن کثیر (اردو)", "حافظ ابن کثیر", "Urdu"),
            TafsirBook("tafsir_en_maarif_ul_quran.md", "Ma'arif-ul-Quran (English)", "Mufti Muhammad Shafi", "English"),
            TafsirBook("tafsir_indo_jalalayn_tanzil.md", "Tafsir Jalalayn (Indonesian)", "Jalal ad-Din al-Mahalli & as-Suyuti", "Indonesian"),
            TafsirBook("tafsir_as_mokhtasar_islamhouse.md", "Al-Mukhtasar (অসমীয়া)", "Markaz Tafsir", "Assamese")
        )
    }

    private val cache = mutableMapOf<String, String>()

    fun getTafsirForVerse(fileName: String, globalVerseId: Int): String {
        val cacheKey = "$fileName:$globalVerseId"
        cache[cacheKey]?.let { return it }

        val targetHeader = "# $globalVerseId"
        val tafsirContent = StringBuilder()
        var foundTarget = false

        try {
            context.assets.open("tafsir/$fileName").use { inputStream ->
                BufferedReader(InputStreamReader(inputStream, Charsets.UTF_8)).use { reader ->
                    var line: String?
                    while (reader.readLine().also { line = it } != null) {
                        val currentLine = line!!
                        val trimmed = currentLine.trim()

                        if (trimmed == targetHeader) {
                            foundTarget = true
                            continue
                        }

                        if (foundTarget) {
                            if (trimmed.startsWith("# ") && trimmed != targetHeader) {
                                break // reached next verse
                            }
                            tafsirContent.append(currentLine).append("\n")
                        }
                    }
                }
            }
        } catch (e: Exception) {
            e.printStackTrace()
            return "Error loading Tafsir: ${e.localizedMessage}"
        }

        val result = if (foundTarget) {
            tafsirContent.toString().trim()
        } else {
            "No tafsir commentary found for this Ayah in the selected book."
        }

        if (cache.size > 50) cache.clear()
        cache[cacheKey] = result
        return result
    }
}
