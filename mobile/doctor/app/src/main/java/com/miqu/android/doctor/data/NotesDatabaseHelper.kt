package com.miqu.android.doctor.data

import android.content.ContentValues
import android.content.Context
import android.database.Cursor
import android.database.sqlite.SQLiteDatabase
import android.database.sqlite.SQLiteOpenHelper

class NotesDatabaseHelper(private val context: Context) :
    SQLiteOpenHelper(context, DATABASE_NAME, null, DATABASE_VERSION) {

    companion object {
        private const val DATABASE_NAME = "doctor_notes.db"
        private const val DATABASE_VERSION = 2

        const val TABLE_NOTEBOOKS = "notebooks"
        const val COL_NB_ID = "id"
        const val COL_NB_NAME = "name"
        const val COL_NB_ICON = "icon"
        const val COL_NB_IS_BUILT_IN = "is_built_in"
        const val COL_NB_CREATED_AT = "created_at"
        const val COL_NB_UPDATED_AT = "updated_at"

        const val TABLE_NOTES = "notes"
        const val COL_ID = "id"
        const val COL_NOTEBOOK_ID = "notebook_id"
        const val COL_TITLE = "title"
        const val COL_CONTENT = "content"
        const val COL_CATEGORY = "category"
        const val COL_IS_PINNED = "is_pinned"
        const val COL_IS_BUILT_IN = "is_built_in"
        const val COL_CREATED_AT = "created_at"
        const val COL_UPDATED_AT = "updated_at"

        const val NOTEBOOK_ID_CLINICAL_GUIDES = 1L
        const val NOTEBOOK_ID_GENERAL = 2L

        @Volatile
        private var instance: NotesDatabaseHelper? = null

        fun getInstance(context: Context): NotesDatabaseHelper {
            return instance ?: synchronized(this) {
                instance ?: NotesDatabaseHelper(context.applicationContext).also { instance = it }
            }
        }
    }

    override fun onCreate(db: SQLiteDatabase) {
        val createNotebooksSql = """
            CREATE TABLE $TABLE_NOTEBOOKS (
                $COL_NB_ID INTEGER PRIMARY KEY AUTOINCREMENT,
                $COL_NB_NAME TEXT NOT NULL,
                $COL_NB_ICON TEXT NOT NULL DEFAULT 'folder',
                $COL_NB_IS_BUILT_IN INTEGER NOT NULL DEFAULT 0,
                $COL_NB_CREATED_AT INTEGER NOT NULL,
                $COL_NB_UPDATED_AT INTEGER NOT NULL
            );
        """.trimIndent()
        db.execSQL(createNotebooksSql)

        val createNotesSql = """
            CREATE TABLE $TABLE_NOTES (
                $COL_ID INTEGER PRIMARY KEY AUTOINCREMENT,
                $COL_NOTEBOOK_ID INTEGER NOT NULL DEFAULT $NOTEBOOK_ID_GENERAL,
                $COL_TITLE TEXT NOT NULL,
                $COL_CONTENT TEXT NOT NULL,
                $COL_CATEGORY TEXT NOT NULL DEFAULT 'General',
                $COL_IS_PINNED INTEGER NOT NULL DEFAULT 0,
                $COL_IS_BUILT_IN INTEGER NOT NULL DEFAULT 0,
                $COL_CREATED_AT INTEGER NOT NULL,
                $COL_UPDATED_AT INTEGER NOT NULL,
                FOREIGN KEY ($COL_NOTEBOOK_ID) REFERENCES $TABLE_NOTEBOOKS ($COL_NB_ID)
            );
        """.trimIndent()
        db.execSQL(createNotesSql)

        db.execSQL("CREATE INDEX idx_notes_notebook ON $TABLE_NOTES ($COL_NOTEBOOK_ID);")
        db.execSQL("CREATE INDEX idx_notes_pinned_updated ON $TABLE_NOTES ($COL_IS_PINNED, $COL_UPDATED_AT);")
        db.execSQL("CREATE INDEX idx_notes_title ON $TABLE_NOTES ($COL_TITLE);")

        val now = System.currentTimeMillis()
        val cvGuides = ContentValues().apply {
            put(COL_NB_ID, NOTEBOOK_ID_CLINICAL_GUIDES)
            put(COL_NB_NAME, "Clinical Guides")
            put(COL_NB_ICON, "book")
            put(COL_NB_IS_BUILT_IN, 1)
            put(COL_NB_CREATED_AT, now)
            put(COL_NB_UPDATED_AT, now)
        }
        db.insert(TABLE_NOTEBOOKS, null, cvGuides)

        val cvGeneral = ContentValues().apply {
            put(COL_NB_ID, NOTEBOOK_ID_GENERAL)
            put(COL_NB_NAME, "General")
            put(COL_NB_ICON, "folder")
            put(COL_NB_IS_BUILT_IN, 0)
            put(COL_NB_CREATED_AT, now)
            put(COL_NB_UPDATED_AT, now)
        }
        db.insert(TABLE_NOTEBOOKS, null, cvGeneral)

        seedBuiltInNotes(db)
    }

    override fun onUpgrade(db: SQLiteDatabase, oldVersion: Int, newVersion: Int) {
        if (oldVersion < 2) {
            val createNotebooksSql = """
                CREATE TABLE IF NOT EXISTS $TABLE_NOTEBOOKS (
                    $COL_NB_ID INTEGER PRIMARY KEY AUTOINCREMENT,
                    $COL_NB_NAME TEXT NOT NULL,
                    $COL_NB_ICON TEXT NOT NULL DEFAULT 'folder',
                    $COL_NB_IS_BUILT_IN INTEGER NOT NULL DEFAULT 0,
                    $COL_NB_CREATED_AT INTEGER NOT NULL,
                    $COL_NB_UPDATED_AT INTEGER NOT NULL
                );
            """.trimIndent()
            db.execSQL(createNotebooksSql)

            val now = System.currentTimeMillis()
            db.execSQL("INSERT OR IGNORE INTO $TABLE_NOTEBOOKS ($COL_NB_ID, $COL_NB_NAME, $COL_NB_ICON, $COL_NB_IS_BUILT_IN, $COL_NB_CREATED_AT, $COL_NB_UPDATED_AT) VALUES ($NOTEBOOK_ID_CLINICAL_GUIDES, 'Clinical Guides', 'book', 1, $now, $now);")
            db.execSQL("INSERT OR IGNORE INTO $TABLE_NOTEBOOKS ($COL_NB_ID, $COL_NB_NAME, $COL_NB_ICON, $COL_NB_IS_BUILT_IN, $COL_NB_CREATED_AT, $COL_NB_UPDATED_AT) VALUES ($NOTEBOOK_ID_GENERAL, 'General', 'folder', 0, $now, $now);")

            db.execSQL("ALTER TABLE $TABLE_NOTES ADD COLUMN $COL_NOTEBOOK_ID INTEGER NOT NULL DEFAULT $NOTEBOOK_ID_GENERAL;")
            db.execSQL("UPDATE $TABLE_NOTES SET $COL_NOTEBOOK_ID = $NOTEBOOK_ID_CLINICAL_GUIDES WHERE $COL_IS_BUILT_IN = 1;")
            db.execSQL("CREATE INDEX IF NOT EXISTS idx_notes_notebook ON $TABLE_NOTES ($COL_NOTEBOOK_ID);")
        }
    }

    private fun seedBuiltInNotes(db: SQLiteDatabase) {
        val assetNotes = listOf(
            "rabies_pep_protocol.md" to "Emergency / Vaccine",
            "pregnancy_reference.md" to "Obstetrics",
            "emergency_drugs.md" to "Emergency / Resuscitation",
            "normal_lab_values.md" to "Laboratory"
        )

        for ((filename, category) in assetNotes) {
            try {
                val content = context.assets.open("notes/$filename").bufferedReader().use { it.readText() }
                val title = extractTitle(content, filename.removeSuffix(".md").replace('_', ' ').capitalizeWords())
                val now = System.currentTimeMillis()

                val values = ContentValues().apply {
                    put(COL_NOTEBOOK_ID, NOTEBOOK_ID_CLINICAL_GUIDES)
                    put(COL_TITLE, title)
                    put(COL_CONTENT, content)
                    put(COL_CATEGORY, category)
                    put(COL_IS_PINNED, 0)
                    put(COL_IS_BUILT_IN, 1)
                    put(COL_CREATED_AT, now)
                    put(COL_UPDATED_AT, now)
                }
                db.insert(TABLE_NOTES, null, values)
            } catch (_: Exception) {
            }
        }
    }

    fun getAllNotebooksWithCounts(): List<NotebookEntity> {
        val list = mutableListOf<NotebookEntity>()
        val db = readableDatabase
        val query = """
            SELECT nb.$COL_NB_ID, nb.$COL_NB_NAME, nb.$COL_NB_ICON, nb.$COL_NB_IS_BUILT_IN,
                   nb.$COL_NB_CREATED_AT, nb.$COL_NB_UPDATED_AT,
                   COUNT(n.$COL_ID) as note_count
            FROM $TABLE_NOTEBOOKS nb
            LEFT JOIN $TABLE_NOTES n ON nb.$COL_NB_ID = n.$COL_NOTEBOOK_ID
            GROUP BY nb.$COL_NB_ID
            ORDER BY nb.$COL_NB_IS_BUILT_IN DESC, nb.$COL_NB_ID ASC
        """.trimIndent()

        val cursor = db.rawQuery(query, null)
        cursor.use {
            while (it.moveToNext()) {
                list.add(
                    NotebookEntity(
                        id = it.getLong(0),
                        name = it.getString(1),
                        icon = it.getString(2),
                        isBuiltIn = it.getInt(3) == 1,
                        createdAt = it.getLong(4),
                        updatedAt = it.getLong(5),
                        noteCount = it.getInt(6)
                    )
                )
            }
        }
        return list
    }

    fun getNotebookById(id: Long): NotebookEntity? {
        val db = readableDatabase
        val cursor = db.query(
            TABLE_NOTEBOOKS,
            null,
            "$COL_NB_ID = ?",
            arrayOf(id.toString()),
            null,
            null,
            null
        )
        return cursor.use {
            if (it.moveToFirst()) {
                NotebookEntity(
                    id = it.getLong(it.getColumnIndexOrThrow(COL_NB_ID)),
                    name = it.getString(it.getColumnIndexOrThrow(COL_NB_NAME)),
                    icon = it.getString(it.getColumnIndexOrThrow(COL_NB_ICON)),
                    isBuiltIn = it.getInt(it.getColumnIndexOrThrow(COL_NB_IS_BUILT_IN)) == 1,
                    createdAt = it.getLong(it.getColumnIndexOrThrow(COL_NB_CREATED_AT)),
                    updatedAt = it.getLong(it.getColumnIndexOrThrow(COL_NB_UPDATED_AT))
                )
            } else null
        }
    }

    fun insertNotebook(name: String, icon: String = "folder"): Long {
        val db = writableDatabase
        val now = System.currentTimeMillis()
        val values = ContentValues().apply {
            put(COL_NB_NAME, name.trim())
            put(COL_NB_ICON, icon)
            put(COL_NB_IS_BUILT_IN, 0)
            put(COL_NB_CREATED_AT, now)
            put(COL_NB_UPDATED_AT, now)
        }
        return db.insert(TABLE_NOTEBOOKS, null, values)
    }

    fun renameNotebook(id: Long, newName: String): Boolean {
        val db = writableDatabase
        val values = ContentValues().apply {
            put(COL_NB_NAME, newName.trim())
            put(COL_NB_UPDATED_AT, System.currentTimeMillis())
        }
        val rows = db.update(TABLE_NOTEBOOKS, values, "$COL_NB_ID = ?", arrayOf(id.toString()))
        return rows > 0
    }

    fun deleteNotebook(id: Long, moveToGeneral: Boolean = true): Boolean {
        if (id == NOTEBOOK_ID_CLINICAL_GUIDES) return false
        val db = writableDatabase
        db.beginTransaction()
        try {
            if (moveToGeneral) {
                val cv = ContentValues().apply { put(COL_NOTEBOOK_ID, NOTEBOOK_ID_GENERAL) }
                db.update(TABLE_NOTES, cv, "$COL_NOTEBOOK_ID = ?", arrayOf(id.toString()))
            } else {
                db.delete(TABLE_NOTES, "$COL_NOTEBOOK_ID = ?", arrayOf(id.toString()))
            }
            db.delete(TABLE_NOTEBOOKS, "$COL_NB_ID = ?", arrayOf(id.toString()))
            db.setTransactionSuccessful()
            return true
        } finally {
            db.endTransaction()
        }
    }

    fun getNotesInNotebook(notebookId: Long, query: String = ""): List<NoteEntity> {
        val list = mutableListOf<NoteEntity>()
        val db = readableDatabase
        val cursor = if (query.trim().isEmpty()) {
            db.query(
                TABLE_NOTES,
                null,
                "$COL_NOTEBOOK_ID = ?",
                arrayOf(notebookId.toString()),
                null,
                null,
                "$COL_IS_PINNED DESC, $COL_IS_BUILT_IN ASC, $COL_UPDATED_AT DESC"
            )
        } else {
            val wildcard = "%${query.trim()}%"
            db.query(
                TABLE_NOTES,
                null,
                "$COL_NOTEBOOK_ID = ? AND ($COL_TITLE LIKE ? OR $COL_CONTENT LIKE ? OR $COL_CATEGORY LIKE ?)",
                arrayOf(notebookId.toString(), wildcard, wildcard, wildcard),
                null,
                null,
                "$COL_IS_PINNED DESC, $COL_IS_BUILT_IN ASC, $COL_UPDATED_AT DESC"
            )
        }

        cursor.use {
            while (it.moveToNext()) {
                list.add(cursorToNote(it))
            }
        }
        return list
    }

    fun smartSearch(rawQuery: String): List<SearchResultNote> {
        val query = rawQuery.trim()
        if (query.isEmpty()) return emptyList()

        val list = mutableListOf<SearchResultNote>()
        val db = readableDatabase
        val wildcard = "%$query%"

        val sql = """
            SELECT n.$COL_ID, n.$COL_NOTEBOOK_ID, n.$COL_TITLE, n.$COL_CONTENT, n.$COL_CATEGORY,
                   n.$COL_IS_PINNED, n.$COL_IS_BUILT_IN, n.$COL_CREATED_AT, n.$COL_UPDATED_AT,
                   COALESCE(nb.$COL_NB_NAME, 'General') as notebook_name
            FROM $TABLE_NOTES n
            LEFT JOIN $TABLE_NOTEBOOKS nb ON n.$COL_NOTEBOOK_ID = nb.$COL_NB_ID
            WHERE n.$COL_TITLE LIKE ? OR n.$COL_CONTENT LIKE ? OR n.$COL_CATEGORY LIKE ? OR nb.$COL_NB_NAME LIKE ?
            ORDER BY 
                CASE WHEN n.$COL_TITLE LIKE ? THEN 0 ELSE 1 END,
                n.$COL_IS_PINNED DESC,
                n.$COL_UPDATED_AT DESC
        """.trimIndent()

        val cursor = db.rawQuery(sql, arrayOf(wildcard, wildcard, wildcard, wildcard, wildcard))
        cursor.use {
            while (it.moveToNext()) {
                val note = NoteEntity(
                    id = it.getLong(0),
                    notebookId = it.getLong(1),
                    title = it.getString(2),
                    content = it.getString(3),
                    category = it.getString(4),
                    isPinned = it.getInt(5) == 1,
                    isBuiltIn = it.getInt(6) == 1,
                    createdAt = it.getLong(7),
                    updatedAt = it.getLong(8)
                )
                val notebookName = it.getString(9)
                val matchedInTitle = note.title.contains(query, ignoreCase = true)
                val snippet = buildSnippet(note.content, query)

                list.add(SearchResultNote(note, notebookName, snippet, matchedInTitle))
            }
        }
        return list
    }

    private fun buildSnippet(content: String, query: String): String {
        val index = content.indexOf(query, ignoreCase = true)
        if (index == -1) {
            val firstLine = content.lineSequence().firstOrNull { it.isNotBlank() && !it.startsWith("#") } ?: ""
            return if (firstLine.length > 80) firstLine.take(80) + "..." else firstLine
        }
        val start = (index - 30).coerceAtLeast(0)
        val end = (index + query.length + 40).coerceAtMost(content.length)
        val prefix = if (start > 0) "..." else ""
        val suffix = if (end < content.length) "..." else ""
        return prefix + content.substring(start, end).replace('\n', ' ').trim() + suffix
    }

    fun moveNoteToNotebook(noteId: Long, targetNotebookId: Long): Boolean {
        val db = writableDatabase
        val values = ContentValues().apply {
            put(COL_NOTEBOOK_ID, targetNotebookId)
            put(COL_UPDATED_AT, System.currentTimeMillis())
        }
        val rows = db.update(TABLE_NOTES, values, "$COL_ID = ?", arrayOf(noteId.toString()))
        return rows > 0
    }

    fun getAllNotes(): List<NoteEntity> {
        val list = mutableListOf<NoteEntity>()
        val db = readableDatabase
        val cursor = db.query(
            TABLE_NOTES,
            null,
            null,
            null,
            null,
            null,
            "$COL_IS_PINNED DESC, $COL_IS_BUILT_IN ASC, $COL_UPDATED_AT DESC"
        )
        cursor.use {
            while (it.moveToNext()) {
                list.add(cursorToNote(it))
            }
        }
        return list
    }

    fun getNoteById(id: Long): NoteEntity? {
        val db = readableDatabase
        val cursor = db.query(
            TABLE_NOTES,
            null,
            "$COL_ID = ?",
            arrayOf(id.toString()),
            null,
            null,
            null
        )
        return cursor.use {
            if (it.moveToFirst()) cursorToNote(it) else null
        }
    }

    fun insertNote(note: NoteEntity): Long {
        val db = writableDatabase
        val now = System.currentTimeMillis()
        val values = ContentValues().apply {
            put(COL_NOTEBOOK_ID, note.notebookId)
            put(COL_TITLE, note.title)
            put(COL_CONTENT, note.content)
            put(COL_CATEGORY, note.category)
            put(COL_IS_PINNED, if (note.isPinned) 1 else 0)
            put(COL_IS_BUILT_IN, if (note.isBuiltIn) 1 else 0)
            put(COL_CREATED_AT, now)
            put(COL_UPDATED_AT, now)
        }
        return db.insert(TABLE_NOTES, null, values)
    }

    fun updateNote(note: NoteEntity): Boolean {
        val db = writableDatabase
        val values = ContentValues().apply {
            put(COL_NOTEBOOK_ID, note.notebookId)
            put(COL_TITLE, note.title)
            put(COL_CONTENT, note.content)
            put(COL_CATEGORY, note.category)
            put(COL_IS_PINNED, if (note.isPinned) 1 else 0)
            put(COL_UPDATED_AT, System.currentTimeMillis())
        }
        val rows = db.update(TABLE_NOTES, values, "$COL_ID = ?", arrayOf(note.id.toString()))
        return rows > 0
    }

    fun togglePin(id: Long, isPinned: Boolean): Boolean {
        val db = writableDatabase
        val values = ContentValues().apply {
            put(COL_IS_PINNED, if (isPinned) 1 else 0)
        }
        val rows = db.update(TABLE_NOTES, values, "$COL_ID = ?", arrayOf(id.toString()))
        return rows > 0
    }

    fun deleteNote(id: Long): Boolean {
        val db = writableDatabase
        val rows = db.delete(TABLE_NOTES, "$COL_ID = ? AND $COL_IS_BUILT_IN = 0", arrayOf(id.toString()))
        return rows > 0
    }

    fun resetBuiltInNote(id: Long): Boolean {
        val existing = getNoteById(id) ?: return false
        if (!existing.isBuiltIn) return false

        val assetNotes = listOf(
            "rabies_pep_protocol.md",
            "pregnancy_reference.md",
            "emergency_drugs.md",
            "normal_lab_values.md"
        )
        for (filename in assetNotes) {
            try {
                val content = context.assets.open("notes/$filename").bufferedReader().use { it.readText() }
                val title = extractTitle(content, filename.removeSuffix(".md").replace('_', ' ').capitalizeWords())
                if (title.equals(existing.title, ignoreCase = true) || 
                    existing.title.contains(filename.removeSuffix(".md").replace('_', ' '), ignoreCase = true)) {
                    val updated = existing.copy(
                        title = title,
                        content = content,
                        updatedAt = System.currentTimeMillis()
                    )
                    return updateNote(updated)
                }
            } catch (_: Exception) {}
        }
        return false
    }

    private fun cursorToNote(cursor: Cursor): NoteEntity {
        val nbIdIndex = cursor.getColumnIndex(COL_NOTEBOOK_ID)
        val notebookId = if (nbIdIndex != -1 && !cursor.isNull(nbIdIndex)) cursor.getLong(nbIdIndex) else NOTEBOOK_ID_GENERAL

        return NoteEntity(
            id = cursor.getLong(cursor.getColumnIndexOrThrow(COL_ID)),
            notebookId = notebookId,
            title = cursor.getString(cursor.getColumnIndexOrThrow(COL_TITLE)),
            content = cursor.getString(cursor.getColumnIndexOrThrow(COL_CONTENT)),
            category = cursor.getString(cursor.getColumnIndexOrThrow(COL_CATEGORY)),
            isPinned = cursor.getInt(cursor.getColumnIndexOrThrow(COL_IS_PINNED)) == 1,
            isBuiltIn = cursor.getInt(cursor.getColumnIndexOrThrow(COL_IS_BUILT_IN)) == 1,
            createdAt = cursor.getLong(cursor.getColumnIndexOrThrow(COL_CREATED_AT)),
            updatedAt = cursor.getLong(cursor.getColumnIndexOrThrow(COL_UPDATED_AT))
        )
    }

    private fun extractTitle(content: String, fallback: String): String {
        for (line in content.lineSequence()) {
            val trimmed = line.trim()
            if (trimmed.startsWith("# ")) {
                return trimmed.removePrefix("# ").trim()
            }
        }
        return fallback
    }

    private fun String.capitalizeWords(): String =
        split(" ").joinToString(" ") { it.replaceFirstChar { char -> char.uppercase() } }
}
