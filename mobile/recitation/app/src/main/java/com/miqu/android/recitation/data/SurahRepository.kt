package com.miqu.android.recitation.data

import android.content.Context
import com.miqu.android.recitation.model.Surah
import org.json.JSONArray

class SurahRepository(private val context: Context) {
    private var cachedSurahs: List<Surah>? = null

    fun getSurahs(): List<Surah> {
        cachedSurahs?.let { return it }

        val list = mutableListOf<Surah>()
        try {
            val jsonString = context.assets.open("metadata/surahs.json").bufferedReader().use { it.readText() }
            val array = JSONArray(jsonString)
            for (i in 0 until array.length()) {
                val obj = array.getJSONObject(i)
                list.add(
                    Surah(
                        id = obj.getInt("id"),
                        name = obj.getString("name"),
                        transliteration = obj.getString("transliteration"),
                        type = obj.optString("type", "meccan"),
                        totalVerses = obj.getInt("total_verses"),
                        english = obj.optString("english", ""),
                        bengali = obj.optString("bengali", ""),
                        urdu = obj.optString("urdu", ""),
                        indonesian = obj.optString("indonesian", "")
                    )
                )
            }
        } catch (e: Exception) {
            e.printStackTrace()
        }
        cachedSurahs = list
        return list
    }

    fun getSurahById(id: Int): Surah? {
        return getSurahs().find { it.id == id }
    }
}
