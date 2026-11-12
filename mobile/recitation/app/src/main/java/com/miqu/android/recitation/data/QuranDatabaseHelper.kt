package com.miqu.android.recitation.data

import android.content.Context
import android.database.Cursor
import android.database.sqlite.SQLiteDatabase
import android.database.sqlite.SQLiteOpenHelper
import com.miqu.android.recitation.model.Verse

class QuranDatabaseHelper(context: Context) :
    SQLiteOpenHelper(context, DATABASE_NAME, null, DATABASE_VERSION) {

    companion object {
        const val DATABASE_NAME = "quran.db"
        const val DATABASE_VERSION = 1

        const val TABLE_VERSES = "Verses"
        const val COL_ID = "id"
        const val COL_SURAH_NUMBER = "surah_number"
        const val COL_VERSE_NUMBER = "verse_number"
        const val COL_ARABIC = "arabic"
        const val COL_ENGLISH = "english"
        const val COL_ENGLISH_Y = "english_y"
        const val COL_INDONESIAN = "indonesian"
        const val COL_URDU = "urdu"
        const val COL_ASSAMESE = "assamese"
        const val COL_BENGALI = "bengali"

        @Volatile
        private var instance: QuranDatabaseHelper? = null

        fun getInstance(context: Context): QuranDatabaseHelper {
            return instance ?: synchronized(this) {
                DatabaseAssetManager.ensureDatabases(context)
                instance ?: QuranDatabaseHelper(context.applicationContext).also { instance = it }
            }
        }
    }

    override fun onCreate(db: SQLiteDatabase) {
        // Database is copied from pre-built assets
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
            db.execSQL("CREATE INDEX IF NOT EXISTS idx_verses_surah ON $TABLE_VERSES ($COL_SURAH_NUMBER);")
            db.execSQL("CREATE INDEX IF NOT EXISTS idx_verses_surah_verse ON $TABLE_VERSES ($COL_SURAH_NUMBER, $COL_VERSE_NUMBER);")
        } catch (_: Exception) {}
    }

    fun getVersesForSurah(surahNumber: Int): List<Verse> {
        val list = mutableListOf<Verse>()
        val db = readableDatabase
        val cursor = db.query(
            TABLE_VERSES,
            null,
            "$COL_SURAH_NUMBER = ?",
            arrayOf(surahNumber.toString()),
            null,
            null,
            "$COL_VERSE_NUMBER ASC"
        )
        cursor.use {
            while (it.moveToNext()) {
                list.add(cursorToVerse(it))
            }
        }
        return list
    }

    fun getVerseBySurahAndNumber(surahNumber: Int, verseNumber: Int): Verse? {
        val db = readableDatabase
        val cursor = db.query(
            TABLE_VERSES,
            null,
            "$COL_SURAH_NUMBER = ? AND $COL_VERSE_NUMBER = ?",
            arrayOf(surahNumber.toString(), verseNumber.toString()),
            null,
            null,
            null
        )
        return cursor.use {
            if (it.moveToFirst()) cursorToVerse(it) else null
        }
    }

    fun getVerseById(id: Int): Verse? {
        val db = readableDatabase
        val cursor = db.query(
            TABLE_VERSES,
            null,
            "$COL_ID = ?",
            arrayOf(id.toString()),
            null,
            null,
            null
        )
        return cursor.use {
            if (it.moveToFirst()) cursorToVerse(it) else null
        }
    }

    fun searchVerses(query: String, limit: Int = 50): List<Verse> {
        val list = mutableListOf<Verse>()
        val trimmed = query.trim()
        if (trimmed.isEmpty()) return list

        val db = readableDatabase
        val wildcard = "%$trimmed%"
        val cursor = db.query(
            TABLE_VERSES,
            null,
            "$COL_ARABIC LIKE ? OR $COL_ENGLISH LIKE ? OR $COL_ENGLISH_Y LIKE ? OR $COL_BENGALI LIKE ? OR $COL_URDU LIKE ? OR $COL_INDONESIAN LIKE ? OR $COL_ASSAMESE LIKE ?",
            arrayOf(wildcard, wildcard, wildcard, wildcard, wildcard, wildcard, wildcard),
            null,
            null,
            "$COL_SURAH_NUMBER ASC, $COL_VERSE_NUMBER ASC",
            limit.toString()
        )
        cursor.use {
            while (it.moveToNext()) {
                list.add(cursorToVerse(it))
            }
        }
        return list
    }

    private fun cursorToVerse(cursor: Cursor): Verse {
        return Verse(
            id = cursor.getInt(cursor.getColumnIndexOrThrow(COL_ID)),
            surahNumber = cursor.getInt(cursor.getColumnIndexOrThrow(COL_SURAH_NUMBER)),
            verseNumber = cursor.getInt(cursor.getColumnIndexOrThrow(COL_VERSE_NUMBER)),
            arabic = cursor.getString(cursor.getColumnIndexOrThrow(COL_ARABIC)),
            english = cursor.getString(cursor.getColumnIndexOrThrow(COL_ENGLISH)),
            englishY = cursor.getString(cursor.getColumnIndexOrThrow(COL_ENGLISH_Y)),
            indonesian = cursor.getString(cursor.getColumnIndexOrThrow(COL_INDONESIAN)),
            urdu = cursor.getString(cursor.getColumnIndexOrThrow(COL_URDU)),
            assamese = cursor.getString(cursor.getColumnIndexOrThrow(COL_ASSAMESE)),
            bengali = cursor.getString(cursor.getColumnIndexOrThrow(COL_BENGALI))
        )
    }
}
