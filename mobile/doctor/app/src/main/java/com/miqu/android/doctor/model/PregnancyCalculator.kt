package com.miqu.android.doctor.model

import java.time.LocalDate
import java.time.temporal.ChronoUnit

data class Milestone(
    val title: String,
    val windowStart: LocalDate,
    val windowEnd: LocalDate,
    val description: String
)

data class PregnancyResult(
    val edd: LocalDate,
    val gestationalWeeks: Int,
    val gestationalDays: Int,
    val totalDays: Long,
    val trimester: Int, // 1, 2, or 3
    val progressPercent: Int,
    val milestones: List<Milestone>
)

object PregnancyCalculator {

    fun calculateFromLmp(
        lmpDate: LocalDate,
        cycleLengthDays: Int = 28,
        today: LocalDate = LocalDate.now()
    ): PregnancyResult {
        // Adjusted Naegele's rule: LMP + 280 days + (cycle length - 28 days)
        val cycleAdjustment = (cycleLengthDays - 28).toLong()
        val edd = lmpDate.plusDays(280L + cycleAdjustment)

        val totalDays = ChronoUnit.DAYS.between(lmpDate, today).coerceAtLeast(0)
        val weeks = (totalDays / 7).toInt()
        val days = (totalDays % 7).toInt()

        val trimester = when {
            totalDays < 98 -> 1   // < 14w0d
            totalDays < 196 -> 2  // 14w0d - 27w6d
            else -> 3             // 28w0d+
        }

        val progressPercent = ((totalDays.toDouble() / 280.0) * 100).toInt().coerceIn(0, 100)

        val milestones = listOf(
            Milestone(
                title = "NT Scan & First Trimester Screen",
                windowStart = lmpDate.plusWeeks(11),
                windowEnd = lmpDate.plusWeeks(13).plusDays(6),
                description = "Nuchal translucency ultrasound & biochemical screening (11w0d - 13w6d)."
            ),
            Milestone(
                title = "Fetal Anatomy Scan",
                windowStart = lmpDate.plusWeeks(18),
                windowEnd = lmpDate.plusWeeks(22),
                description = "Level II detailed structural and anomaly ultrasound (18w0d - 22w0d)."
            ),
            Milestone(
                title = "Glucose Challenge Test (GDM)",
                windowStart = lmpDate.plusWeeks(24),
                windowEnd = lmpDate.plusWeeks(28),
                description = "Screening for gestational diabetes mellitus (24w0d - 28w0d)."
            ),
            Milestone(
                title = "Group B Streptococcus (GBS)",
                windowStart = lmpDate.plusWeeks(36),
                windowEnd = lmpDate.plusWeeks(37).plusDays(6),
                description = "Vaginal-rectal swab screening for intrapartum prophylaxis (36w0d - 37w6d)."
            )
        )

        return PregnancyResult(
            edd = edd,
            gestationalWeeks = weeks,
            gestationalDays = days,
            totalDays = totalDays,
            trimester = trimester,
            progressPercent = progressPercent,
            milestones = milestones
        )
    }

    fun calculateFromUltrasound(
        scanDate: LocalDate,
        scanWeeks: Int,
        scanDays: Int,
        today: LocalDate = LocalDate.now()
    ): PregnancyResult {
        val totalScanDays = (scanWeeks * 7 + scanDays).toLong()
        val impliedLmp = scanDate.minusDays(totalScanDays)
        return calculateFromLmp(impliedLmp, cycleLengthDays = 28, today = today)
    }
}
