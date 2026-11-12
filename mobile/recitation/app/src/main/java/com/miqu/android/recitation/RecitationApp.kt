package com.miqu.android.recitation

import android.app.Application
import com.google.android.material.color.DynamicColors
import com.miqu.android.recitation.data.DatabaseAssetManager
import kotlin.concurrent.thread

class RecitationApp : Application() {
    override fun onCreate() {
        super.onCreate()
        // Enable Material You dynamic colors on supported devices (Android 12+ / API 31+)
        DynamicColors.applyToActivitiesIfAvailable(this)

        // Initialize databases in background thread on cold launch
        thread(start = true, name = "db-asset-init") {
            DatabaseAssetManager.ensureDatabases(this)
        }
    }
}
