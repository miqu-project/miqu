package com.miqu.android.recitation

import org.junit.Assert.assertEquals
import org.junit.Assert.assertNotNull
import org.junit.Assert.assertTrue
import org.junit.Test
import java.io.File

class QuranLinesPageTest {

    private fun getQuranLinesDir(): File {
        val candidates = listOf(
            File("src/main/assets/quran_lines"),
            File("app/src/main/assets/quran_lines"),
            File("/home/anisur/projects/miqu-android-apps/recitation/app/src/main/assets/quran_lines"),
            File("/home/anisur/projects/miqu-android-apps/quran_lines")
        )
        return candidates.firstOrNull { it.exists() && it.isDirectory }
            ?: File("src/main/assets/quran_lines")
    }

    private fun findLineFile(baseDir: File, pageStr: String, lineNum: Int): File? {
        val lineStr = String.format("line_%02d", lineNum)
        val webpFile = File(baseDir, "$pageStr/$lineStr.webp")
        if (webpFile.exists()) return webpFile
        val pngFile = File(baseDir, "$pageStr/$lineStr.png")
        if (pngFile.exists()) return pngFile
        return null
    }

    @Test
    fun testPage004Line04FileExists() {
        val baseDir = getQuranLinesDir()
        val file = findLineFile(baseDir, "page_004", 4)
        assertNotNull("line_04 (.webp or .png) must exist", file)
        assertTrue("line_04 file must not be empty", (file?.length() ?: 0) > 0)
    }

    @Test
    fun testPage004All15LinesExist() {
        val baseDir = getQuranLinesDir()
        for (i in 1..15) {
            val lineFile = findLineFile(baseDir, "page_004", i)
            assertNotNull("Line $i (.webp or .png) must exist", lineFile)
            assertTrue("Line $i file must not be empty", (lineFile?.length() ?: 0) > 0)
        }
    }

    @Test
    fun testPage001Has7Lines() {
        val baseDir = getQuranLinesDir()
        for (i in 1..7) {
            val lineFile = findLineFile(baseDir, "page_001", i)
            assertNotNull("Page 1 Line $i (.webp or .png) must exist", lineFile)
            assertTrue("Page 1 Line $i file must not be empty", (lineFile?.length() ?: 0) > 0)
        }
    }

    @Test
    fun testTotalPagesCount() {
        val baseDir = getQuranLinesDir()
        assertTrue("quran_lines directory must exist", baseDir.exists())
        val pages = baseDir.listFiles { f -> f.isDirectory && f.name.startsWith("page_") }
        assertEquals("Total pages in quran_lines should be 615", 615, pages?.size ?: 0)
    }
}
