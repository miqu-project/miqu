package com.miqu.android.recitation

import com.miqu.android.recitation.model.CorpusSegment
import org.junit.Assert.assertTrue
import org.junit.Test

class CorpusSegmentTest {

    @Test
    fun testCorpusSegmentFormatting() {
        val segment = CorpusSegment(
            word = 1,
            segment = 2,
            form = "somi",
            tag = "N",
            tagDesc = "اسم (Noun)",
            features = "STEM|POS:N|LEM:{som|ROOT:smw|M|GEN"
        )

        val formatted = segment.formatDisplay()
        assertTrue(formatted.contains("Noun"))
        assertTrue(formatted.contains("Genitive"))
        assertTrue(formatted.contains("Lemma: {som"))
    }
}
