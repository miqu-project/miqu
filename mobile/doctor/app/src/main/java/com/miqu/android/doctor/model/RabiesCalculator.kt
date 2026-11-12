package com.miqu.android.doctor.model

import java.time.LocalDate

data class DoseVisit(
    val dayNumber: Int,
    val date: LocalDate,
    val label: String,
    val details: String
)

data class RabiesSchedule(
    val idVisits: List<DoseVisit>,
    val imVisits: List<DoseVisit>,
    val reExposureVisits: List<DoseVisit>,
    val hrigDoseIu: Double?,
    val erigDoseIu: Double?
)

object RabiesCalculator {

    fun calculate(
        dayZeroDate: LocalDate,
        weightKg: Double? = null
    ): RabiesSchedule {
        // Intradermal (ID) Regimen (Updated Thai Red Cross 2-site: Days 0, 3, 7, 28)
        val idVisits = listOf(
            DoseVisit(
                dayNumber = 0,
                date = dayZeroDate,
                label = "Day 0 (Start)",
                details = "2 sites (0.1 mL each) in left & right deltoid"
            ),
            DoseVisit(
                dayNumber = 3,
                date = dayZeroDate.plusDays(3),
                label = "Day 3",
                details = "2 sites (0.1 mL each) in left & right deltoid"
            ),
            DoseVisit(
                dayNumber = 7,
                date = dayZeroDate.plusDays(7),
                label = "Day 7",
                details = "2 sites (0.1 mL each) in left & right deltoid"
            ),
            DoseVisit(
                dayNumber = 28,
                date = dayZeroDate.plusDays(28),
                label = "Day 28",
                details = "2 sites (0.1 mL each) in left & right deltoid"
            )
        )

        // Intramuscular (IM) Regimen (Essen 5-dose: Days 0, 3, 7, 14, 28)
        val imVisits = listOf(
            DoseVisit(
                dayNumber = 0,
                date = dayZeroDate,
                label = "Day 0 (Start)",
                details = "1 standard dose (0.5/1.0 mL) in deltoid (never gluteal)"
            ),
            DoseVisit(
                dayNumber = 3,
                date = dayZeroDate.plusDays(3),
                label = "Day 3",
                details = "1 standard dose in deltoid"
            ),
            DoseVisit(
                dayNumber = 7,
                date = dayZeroDate.plusDays(7),
                label = "Day 7",
                details = "1 standard dose in deltoid"
            ),
            DoseVisit(
                dayNumber = 14,
                date = dayZeroDate.plusDays(14),
                label = "Day 14",
                details = "1 standard dose in deltoid"
            ),
            DoseVisit(
                dayNumber = 28,
                date = dayZeroDate.plusDays(28),
                label = "Day 28",
                details = "1 standard dose in deltoid"
            )
        )

        // Re-exposure (previously vaccinated with documented complete course)
        val reExposureVisits = listOf(
            DoseVisit(
                dayNumber = 0,
                date = dayZeroDate,
                label = "Day 0",
                details = "1 dose IM (deltoid) or 1 site ID (0.1 mL). No RIG needed."
            ),
            DoseVisit(
                dayNumber = 3,
                date = dayZeroDate.plusDays(3),
                label = "Day 3",
                details = "1 dose IM (deltoid) or 1 site ID (0.1 mL). No RIG needed."
            )
        )

        // RIG Calculations
        val hrig = weightKg?.let { if (it > 0) it * 20.0 else null }
        val erig = weightKg?.let { if (it > 0) it * 40.0 else null }

        return RabiesSchedule(
            idVisits = idVisits,
            imVisits = imVisits,
            reExposureVisits = reExposureVisits,
            hrigDoseIu = hrig,
            erigDoseIu = erig
        )
    }
}
