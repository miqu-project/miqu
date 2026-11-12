package com.miqu.android.recitation.data

import android.content.Context
import android.database.Cursor
import android.database.sqlite.SQLiteDatabase
import android.database.sqlite.SQLiteOpenHelper
import com.miqu.android.recitation.model.WordRoot

class RootsDatabaseHelper(context: Context) :
    SQLiteOpenHelper(context, DATABASE_NAME, null, DATABASE_VERSION) {

    companion object {
        const val DATABASE_NAME = "roots.db"
        const val DATABASE_VERSION = 1

        const val TABLE_ROOTS = "roots"
        const val COL_SURAH = "surah"
        const val COL_VERSE = "verse"
        const val COL_SURAH_VERSE = "surah:verse"
        const val COL_ARABIC = "arabic"
        const val COL_ENGLISH = "english"
        const val COL_ROOT = "root"
        const val COL_POSITION = "position"

        @Volatile
        private var instance: RootsDatabaseHelper? = null

        fun getInstance(context: Context): RootsDatabaseHelper {
            return instance ?: synchronized(this) {
                DatabaseAssetManager.ensureDatabases(context)
                instance ?: RootsDatabaseHelper(context.applicationContext).also { instance = it }
            }
        }
    }

    override fun onCreate(db: SQLiteDatabase) {
        ensureIndices(db)
    }

    override fun onUpgrade(db: SQLiteDatabase, oldVersion: Int, newVersion: Int) {
        ensureIndices(db)
    }

    override fun onOpen(db: SQLiteDatabase) {
        super.onOpen(db)
        ensureIndices(db)
    }

    private fun ensureIndices(db: SQLiteDatabase) {
        try {
            db.execSQL("CREATE INDEX IF NOT EXISTS idx_roots_surah_verse ON $TABLE_ROOTS ($COL_SURAH, $COL_VERSE, $COL_POSITION);")
            db.execSQL("CREATE INDEX IF NOT EXISTS idx_roots_root ON $TABLE_ROOTS ($COL_ROOT);")
            db.execSQL("CREATE INDEX IF NOT EXISTS idx_roots_arabic ON $TABLE_ROOTS ($COL_ARABIC);")
            db.execSQL("CREATE INDEX IF NOT EXISTS idx_roots_arabic_loc ON $TABLE_ROOTS ($COL_ARABIC, $COL_SURAH, $COL_VERSE, $COL_POSITION);")
            db.execSQL("CREATE INDEX IF NOT EXISTS idx_roots_root_loc ON $TABLE_ROOTS ($COL_ROOT, $COL_SURAH, $COL_VERSE, $COL_POSITION);")
            db.execSQL("CREATE INDEX IF NOT EXISTS idx_corpus_loc ON corpus_morphology (surah, verse, word);")
        } catch (_: Exception) {}
    }

    fun getWordsForVerse(surah: Int, verse: Int): List<WordRoot> {
        val list = mutableListOf<WordRoot>()
        val db = readableDatabase
        val cursor = db.query(
            TABLE_ROOTS,
            null,
            "$COL_SURAH = ? AND $COL_VERSE = ?",
            arrayOf(surah.toString(), verse.toString()),
            null,
            null,
            "$COL_POSITION ASC"
        )
        cursor.use {
            while (it.moveToNext()) {
                list.add(cursorToWordRoot(it))
            }
        }
        return list
    }

    fun getCorpusSegmentsForVerse(surah: Int, verse: Int): Map<Int, List<com.miqu.android.recitation.model.CorpusSegment>> {
        val map = mutableMapOf<Int, MutableList<com.miqu.android.recitation.model.CorpusSegment>>()
        val db = readableDatabase
        try {
            val cursor = db.query(
                "corpus_morphology",
                null,
                "surah = ? AND verse = ?",
                arrayOf(surah.toString(), verse.toString()),
                null,
                null,
                "word ASC, segment ASC"
            )
            cursor.use {
                while (it.moveToNext()) {
                    val seg = com.miqu.android.recitation.model.CorpusSegment(
                        word = it.getInt(it.getColumnIndexOrThrow("word")),
                        segment = it.getInt(it.getColumnIndexOrThrow("segment")),
                        form = it.getString(it.getColumnIndexOrThrow("form")),
                        tag = it.getString(it.getColumnIndexOrThrow("tag")),
                        tagDesc = it.getString(it.getColumnIndexOrThrow("tag_desc")),
                        features = it.getString(it.getColumnIndexOrThrow("features"))
                    )
                    map.getOrPut(seg.word) { mutableListOf() }.add(seg)
                }
            }
        } catch (_: Exception) {}
        return map
    }

    fun getOccurrencesForRoot(root: String, limit: Int = 50, offset: Int = 0): List<WordRoot> {
        val list = mutableListOf<WordRoot>()
        val db = readableDatabase
        val cursor = db.query(
            TABLE_ROOTS,
            null,
            "$COL_ROOT = ?",
            arrayOf(root.trim()),
            null,
            null,
            "$COL_SURAH ASC, $COL_VERSE ASC, $COL_POSITION ASC",
            "$offset, $limit"
        )
        cursor.use {
            while (it.moveToNext()) {
                list.add(cursorToWordRoot(it))
            }
        }
        return list
    }

    fun getExactWordOccurrences(arabicWord: String, limit: Int = 50, offset: Int = 0): List<WordRoot> {
        val list = mutableListOf<WordRoot>()
        val db = readableDatabase
        val cursor = db.query(
            TABLE_ROOTS,
            null,
            "$COL_ARABIC = ?",
            arrayOf(arabicWord.trim()),
            null,
            null,
            "$COL_SURAH ASC, $COL_VERSE ASC, $COL_POSITION ASC",
            "$offset, $limit"
        )
        cursor.use {
            while (it.moveToNext()) {
                list.add(cursorToWordRoot(it))
            }
        }
        return list
    }

    fun getExactWordOccurrenceCount(arabicWord: String): Int {
        val db = readableDatabase
        val cursor = db.rawQuery(
            "SELECT count(*) FROM $TABLE_ROOTS WHERE $COL_ARABIC = ?",
            arrayOf(arabicWord.trim())
        )
        return cursor.use {
            if (it.moveToFirst()) it.getInt(0) else 0
        }
    }

    fun getRootOccurrenceCount(root: String): Int {
        val db = readableDatabase
        val cursor = db.rawQuery(
            "SELECT count(*) FROM $TABLE_ROOTS WHERE $COL_ROOT = ?",
            arrayOf(root.trim())
        )
        return cursor.use {
            if (it.moveToFirst()) it.getInt(0) else 0
        }
    }

    fun getRootsByFrequency(query: String? = null, limit: Int = 2000): List<com.miqu.android.recitation.model.RootEntry> {
        val list = mutableListOf<com.miqu.android.recitation.model.RootEntry>()
        val db = readableDatabase
        val sql = if (query.isNullOrBlank()) {
            "SELECT root, count(root) as cnt FROM $TABLE_ROOTS WHERE root IS NOT NULL AND root != '' GROUP BY root ORDER BY cnt DESC LIMIT ?"
        } else {
            "SELECT root, count(root) as cnt FROM $TABLE_ROOTS WHERE root IS NOT NULL AND root != '' AND root LIKE ? GROUP BY root ORDER BY cnt DESC LIMIT ?"
        }
        val args = if (query.isNullOrBlank()) {
            arrayOf(limit.toString())
        } else {
            arrayOf("%${query.trim()}%", limit.toString())
        }
        val cursor = db.rawQuery(sql, args)
        cursor.use {
            while (it.moveToNext()) {
                val root = it.getString(0) ?: ""
                val count = it.getInt(1)
                list.add(com.miqu.android.recitation.model.RootEntry(root = root, definition = "", occurrencesCount = count))
            }
        }
        return list
    }

    fun getUniqueWords(query: String? = null, limit: Int = 300): List<com.miqu.android.recitation.model.QuranWord> {
        val list = mutableListOf<com.miqu.android.recitation.model.QuranWord>()
        val db = readableDatabase
        val sql = if (query.isNullOrBlank()) {
            "SELECT arabic, root, count(*) as cnt FROM $TABLE_ROOTS GROUP BY arabic ORDER BY cnt DESC LIMIT ?"
        } else {
            "SELECT arabic, root, count(*) as cnt FROM $TABLE_ROOTS WHERE arabic LIKE ? OR root LIKE ? GROUP BY arabic ORDER BY cnt DESC LIMIT ?"
        }
        val args = if (query.isNullOrBlank()) {
            arrayOf(limit.toString())
        } else {
            val q = "%${query.trim()}%"
            arrayOf(q, q, limit.toString())
        }
        val cursor = db.rawQuery(sql, args)
        cursor.use {
            while (it.moveToNext()) {
                val root = it.getString(1)?.trim() ?: ""
                list.add(
                    com.miqu.android.recitation.model.QuranWord(
                        arabic = it.getString(0) ?: "",
                        english = "",
                        root = root,
                        count = it.getInt(2)
                    )
                )
            }
        }
        return list
    }

    private fun cursorToWordRoot(cursor: Cursor): WordRoot {
        return WordRoot(
            surah = cursor.getInt(cursor.getColumnIndexOrThrow(COL_SURAH)),
            verse = cursor.getInt(cursor.getColumnIndexOrThrow(COL_VERSE)),
            surahVerse = cursor.getString(cursor.getColumnIndexOrThrow(COL_SURAH_VERSE)),
            arabic = cursor.getString(cursor.getColumnIndexOrThrow(COL_ARABIC)),
            english = cursor.getString(cursor.getColumnIndexOrThrow(COL_ENGLISH)),
            root = cursor.getString(cursor.getColumnIndexOrThrow(COL_ROOT)),
            position = cursor.getInt(cursor.getColumnIndexOrThrow(COL_POSITION))
        )
    }
}
