package com.miqu.android.doctor

import android.app.Application
import com.google.android.material.color.DynamicColors

class DoctorApp : Application() {
    override fun onCreate() {
        super.onCreate()
        // Enable Material You dynamic colors on supported devices (Android 12+ / API 31+)
        DynamicColors.applyToActivitiesIfAvailable(this)
    }
}
