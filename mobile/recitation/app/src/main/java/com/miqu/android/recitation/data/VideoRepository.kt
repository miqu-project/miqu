package com.miqu.android.recitation.data

import android.content.Context
import com.miqu.android.recitation.model.VideoEntry
import org.json.JSONArray

class VideoRepository(private val context: Context) {
    private var cachedVideos: List<VideoEntry>? = null

    fun getVideos(): List<VideoEntry> {
        cachedVideos?.let { return it }

        val list = mutableListOf<VideoEntry>()
        try {
            val jsonString = context.assets.open("metadata/videos.json").bufferedReader().use { it.readText() }
            val array = JSONArray(jsonString)
            for (i in 0 until array.length()) {
                val obj = array.getJSONObject(i)
                list.add(
                    VideoEntry(
                        id = obj.getString("id"),
                        title = obj.getString("title"),
                        desc = obj.optString("desc", "")
                    )
                )
            }
        } catch (e: Exception) {
            e.printStackTrace()
        }
        cachedVideos = list
        return list
    }
}
