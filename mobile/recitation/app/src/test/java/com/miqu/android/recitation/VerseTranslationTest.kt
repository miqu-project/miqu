package com.miqu.android.recitation

import com.miqu.android.recitation.model.Verse
import org.junit.Assert.assertEquals
import org.junit.Test

class VerseTranslationTest {

    private val sampleVerse = Verse(
        id = 1,
        surahNumber = 1,
        verseNumber = 1,
        arabic = "بِسْمِ اللَّهِ الرَّحْمَٰنِ الرَّحِيمِ",
        english = "In the name of Allah, the Entirely Merciful, the Especially Merciful.",
        englishY = "In the name of God, Most Gracious, Most Merciful",
        indonesian = "Dengan nama Allah Yang Maha Pengasih, Maha Penyayang.",
        urdu = "شروع اللہ کا نام لے کر جو بڑا مہربان نہایت رحم والا ہے",
        assamese = "পৰম কৰুণাময় অতি দয়ালু আল্লাহৰ নামত",
        bengali = "শুরু করছি আল্লাহর নামে যিনি পরম করুণাময়, অতি দয়ালু।"
    )

    @Test
    fun testEnglishTranslation() {
        assertEquals(
            "In the name of Allah, the Entirely Merciful, the Especially Merciful.",
            sampleVerse.getTranslation("english")
        )
    }

    @Test
    fun testBengaliTranslation() {
        assertEquals(
            "শুরু করছি আল্লাহর নামে যিনি পরম করুণাময়, অতি দয়ালু।",
            sampleVerse.getTranslation("bengali")
        )
    }

    @Test
    fun testUrduTranslation() {
        assertEquals(
            "شروع اللہ کا نام لے کر جو بڑا مہربان نہایت رحم والا ہے",
            sampleVerse.getTranslation("urdu")
        )
    }

    @Test
    fun testIndonesianTranslation() {
        assertEquals(
            "Dengan nama Allah Yang Maha Pengasih, Maha Penyayang.",
            sampleVerse.getTranslation("indonesian")
        )
    }

    @Test
    fun testAssameseTranslation() {
        assertEquals(
            "পৰম কৰুণাময় অতি দয়ালু আল্লাহৰ নামত",
            sampleVerse.getTranslation("assamese")
        )
    }

    @Test
    fun testFallbackToEnglish() {
        val verseWithEmptyUrdu = sampleVerse.copy(urdu = "")
        assertEquals(
            "In the name of Allah, the Entirely Merciful, the Especially Merciful.",
            verseWithEmptyUrdu.getTranslation("urdu")
        )
    }
}
