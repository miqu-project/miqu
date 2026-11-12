package com.miqu.android.recitation

import com.miqu.android.recitation.model.WordRoot
import org.junit.Assert.assertEquals
import org.junit.Assert.assertTrue
import org.junit.Test

class WordOccurrenceTest {

    @Test
    fun testExactWordMatchFiltering() {
        val allWords = listOf(
            WordRoot(1, 7, "1:7", "أَنْعَمْتَ", "You have bestowed (Your) Favors", "نعم", 3),
            WordRoot(2, 40, "2:40", "أَنْعَمْتُ", "I bestowed", "نعم", 7),
            WordRoot(2, 47, "2:47", "أَنْعَمْتُ", "I bestowed", "نعم", 7),
            WordRoot(4, 69, "4:69", "أَنْعَمَ", "bestowed", "نعم", 6),
            WordRoot(5, 110, "5:110", "أَنْعَمْتُ", "I bestowed", "نعم", 7),
            WordRoot(19, 58, "19:58", "أَنْعَمَ", "bestowed", "نعم", 4),
            WordRoot(48, 12, "48:12", "أَنْعَمْتَ", "You have bestowed", "نعم", 5)
        )

        val targetWord = "أَنْعَمْتَ"
        val exactMatches = allWords.filter { it.arabic == targetWord }

        assertEquals(2, exactMatches.size)
        assertTrue(exactMatches.all { it.arabic == targetWord })
        assertEquals("You have bestowed (Your) Favors", exactMatches[0].english)
        assertEquals("You have bestowed", exactMatches[1].english)
    }
}
