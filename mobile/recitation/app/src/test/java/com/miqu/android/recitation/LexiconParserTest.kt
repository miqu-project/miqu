package com.miqu.android.recitation

import com.miqu.android.recitation.model.RootEntry
import org.junit.Assert.assertEquals
import org.junit.Assert.assertNotNull
import org.junit.Test

class LexiconParserTest {

    @Test
    fun testRootEntryCreationAndSearch() {
        val entry = RootEntry(
            root = "سلم",
            definition = "to be safe, sound, secure, unimpaired, free of blemish, surrender, submit, pay in advance, peace.",
            occurrencesCount = 140
        )

        assertEquals("سلم", entry.root)
        assertEquals(140, entry.occurrencesCount)
        assertNotNull(entry.definition)
    }

    @Test
    fun testRootFormatting() {
        val root = "سلم"
        val formatted = root.toCharArray().joinToString(" ")
        assertEquals("س ل م", formatted)
    }
}
