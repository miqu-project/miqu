package com.miqu.android.recitation.data

import android.content.Context
import com.miqu.android.recitation.model.SearchSuggestion
import com.miqu.android.recitation.model.Surah

class UnifiedSearchRepository(private val context: Context) {

    private val surahRepo = SurahRepository(context)
    private val quranDbHelper = QuranDatabaseHelper.getInstance(context)
    private val lexiconRepo = LexiconRepository(context)

    private val allSurahs: List<Surah> by lazy { surahRepo.getSurahs() }
    private val surahNameMap: Map<Int, String> by lazy {
        allSurahs.associate { it.id to it.transliteration }
    }

    private val versePattern1 = Regex("""^(\d{1,3})\s*[:\.]\s*(\d{1,3})$""")
    private val versePattern2 = Regex("""^(?:surah|s)?\s*(\d{1,3})\s*(?:ayah|a|v|verse)?\s*(\d{1,3})$""", RegexOption.IGNORE_CASE)

    fun search(rawQuery: String): List<SearchSuggestion> {
        val query = rawQuery.trim()
        val results = ArrayList<SearchSuggestion>()

        if (query.isEmpty()) {
            val popularIds = intArrayOf(1, 2, 18, 36, 55, 67, 112, 114)
            for (id in popularIds) {
                val surah = surahRepo.getSurahById(id)
                if (surah != null) {
                    results.add(SearchSuggestion.SurahItem(surah))
                }
            }
            return results
        }

        // 1. Check for Direct Ayah Jump (e.g. "2:255" or "18:10")
        val match1 = versePattern1.matchEntire(query) ?: versePattern2.matchEntire(query)
        if (match1 != null) {
            val sId = match1.groupValues[1].toIntOrNull()
            val aNum = match1.groupValues[2].toIntOrNull()
            if (sId != null && aNum != null && sId in 1..114) {
                val surah = surahRepo.getSurahById(sId)
                if (surah != null && aNum in 1..surah.totalVerses) {
                    results.add(
                        SearchSuggestion.AyahJumpItem(
                            surahId = sId,
                            surahName = surah.transliteration,
                            ayahNumber = aNum,
                            totalVerses = surah.totalVerses
                        )
                    )
                }
            }
        }

        // 2. Search Surahs (by number, transliteration, english, arabic, urdu, bengali)
        val matchedSurahs = allSurahs.filter { s ->
            s.id.toString() == query ||
                    s.transliteration.contains(query, ignoreCase = true) ||
                    s.english.contains(query, ignoreCase = true) ||
                    s.name.contains(query) ||
                    s.urdu.contains(query) ||
                    s.bengali.contains(query)
        }.take(6)

        for (s in matchedSurahs) {
            results.add(SearchSuggestion.SurahItem(s))
        }

        // 4. Search Arabic Roots
        if (query.any { it in '\u0600'..'\u06FF' } || query.length in 2..5) {
            val matchedRoots = lexiconRepo.searchRoots(query).take(4)
            for (r in matchedRoots) {
                results.add(SearchSuggestion.RootItem(r))
            }
        }

        // 5. Search Verses by text across all translations
        if (query.length >= 2 && !query.all { it.isDigit() }) {
            val verses = quranDbHelper.searchVerses(query, limit = 12)
            for (v in verses) {
                val sName = surahNameMap[v.surahNumber] ?: "Surah ${v.surahNumber}"

                val (marker, snippet, transKey) = when {
                    v.bengali.contains(query, ignoreCase = true) ->
                        Triple("Bengali", v.bengali, UserSettings.TRANS_BENGALI)
                    v.urdu.contains(query, ignoreCase = true) ->
                        Triple("Urdu", v.urdu, UserSettings.TRANS_URDU)
                    v.indonesian.contains(query, ignoreCase = true) ->
                        Triple("Indonesian", v.indonesian, UserSettings.TRANS_INDONESIAN)
                    v.assamese.contains(query, ignoreCase = true) ->
                        Triple("Assamese", v.assamese, UserSettings.TRANS_ASSAMESE)
                    v.arabic.contains(query, ignoreCase = true) ->
                        Triple("Arabic", v.arabic, null)
                    v.englishY.contains(query, ignoreCase = true) && !v.english.contains(query, ignoreCase = true) ->
                        Triple("Yusuf Ali", v.englishY, UserSettings.TRANS_YUSUF_ALI)
                    v.english.contains(query, ignoreCase = true) && v.englishY.contains(query, ignoreCase = true) ->
                        Triple("Sahih Intl", v.english, UserSettings.TRANS_ENGLISH)
                    v.english.contains(query, ignoreCase = true) ->
                        Triple("Sahih Intl", v.english, UserSettings.TRANS_ENGLISH)
                    v.englishY.isNotEmpty() ->
                        Triple("Yusuf Ali", v.englishY, UserSettings.TRANS_YUSUF_ALI)
                    else ->
                        Triple("Sahih Intl", v.english, UserSettings.TRANS_ENGLISH)
                }

                results.add(SearchSuggestion.VerseTextItem(v, sName, snippet, marker, transKey))
            }
        }

        return results
    }
}
