package com.miqu.android.recitation.data

import android.content.Context
import android.util.Log
import java.io.File
import java.io.FileOutputStream

object DatabaseAssetManager {
    private const val TAG = "DatabaseAssetManager"
    private const val PREF_DB_VERSION = "installed_db_version"
    private const val CURRENT_DB_VERSION = 5

    private val DATABASES = listOf("quran.db", "roots.db")

    @Synchronized
    fun ensureDatabases(context: Context) {
        val prefs = context.getSharedPreferences("db_asset_mgr", Context.MODE_PRIVATE)
        val installedVersion = prefs.getInt(PREF_DB_VERSION, 0)

        val dbDir = context.getDatabasePath("quran.db").parentFile ?: File(context.filesDir, "../databases")
        if (!dbDir.exists()) {
            dbDir.mkdirs()
        }

        // Clean up legacy Mushaf database if present to save storage
        val legacyDb = File(dbDir, "indopak-nastaleeq.db")
        if (legacyDb.exists()) {
            try {
                legacyDb.delete()
                Log.d(TAG, "Purged legacy indopak-nastaleeq.db")
            } catch (_: Exception) {}
        }

        val needsCopy = installedVersion < CURRENT_DB_VERSION || DATABASES.any { !File(dbDir, it).exists() }

        if (needsCopy) {
            for (dbName in DATABASES) {
                val destFile = File(dbDir, dbName)
                try {
                    copyAssetDatabase(context, "databases/$dbName", destFile)
                    Log.d(TAG, "Copied asset database: $dbName -> ${destFile.absolutePath} (${destFile.length()} bytes)")
                } catch (e: Exception) {
                    Log.e(TAG, "Failed to copy database $dbName", e)
                }
            }
            prefs.edit().putInt(PREF_DB_VERSION, CURRENT_DB_VERSION).apply()
        }
    }

    private fun copyAssetDatabase(context: Context, assetPath: String, destFile: File) {
        val tempFile = File(destFile.parentFile, "${destFile.name}.tmp")
        context.assets.open(assetPath).use { input ->
            FileOutputStream(tempFile).use { output ->
                val buffer = ByteArray(64 * 1024)
                var bytesRead: Int
                while (input.read(buffer).also { bytesRead = it } != -1) {
                    output.write(buffer, 0, bytesRead)
                }
                output.flush()
            }
        }
        if (destFile.exists()) {
            destFile.delete()
        }
        if (!tempFile.renameTo(destFile)) {
            tempFile.copyTo(destFile, overwrite = true)
            tempFile.delete()
        }
    }
}
