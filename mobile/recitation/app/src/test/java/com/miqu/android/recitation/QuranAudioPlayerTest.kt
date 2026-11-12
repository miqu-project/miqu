package com.miqu.android.recitation

import com.miqu.android.recitation.util.QuranAudioPlayer
import org.junit.Assert.assertEquals
import org.junit.Test

class QuranAudioPlayerTest {

    @Test
    fun testSurahTotalVersesCountAndSum() {
        assertEquals(114, QuranAudioPlayer.SURAH_TOTAL_VERSES.size)
        assertEquals(6236, QuranAudioPlayer.SURAH_TOTAL_VERSES.sum())
    }

    @Test
    fun testGlobalVerseIndexCalculation() {
        // Surah 1: Verse 1 -> 1
        assertEquals(1, QuranAudioPlayer.getGlobalVerseIndex(1, 1))
        // Surah 1: Verse 7 -> 7
        assertEquals(7, QuranAudioPlayer.getGlobalVerseIndex(1, 7))
        // Surah 2: Verse 1 -> 8 (7 + 1)
        assertEquals(8, QuranAudioPlayer.getGlobalVerseIndex(2, 1))
        // Surah 2: Verse 286 -> 293 (7 + 286)
        assertEquals(293, QuranAudioPlayer.getGlobalVerseIndex(2, 286))
        // Surah 114: Verse 6 -> 6236
        assertEquals(6236, QuranAudioPlayer.getGlobalVerseIndex(114, 6))
    }
}
