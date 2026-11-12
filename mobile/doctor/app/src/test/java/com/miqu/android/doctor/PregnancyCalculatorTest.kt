package com.miqu.android.doctor

import com.miqu.android.doctor.model.PregnancyCalculator
import org.junit.Assert.assertEquals
import org.junit.Test
import java.time.LocalDate

class PregnancyCalculatorTest {

    @Test
    fun testStandardLmpCalculation() {
        val lmp = LocalDate.of(2026, 1, 1)
        val today = LocalDate.of(2026, 3, 12) // 70 days after Jan 1 (10 weeks 0 days)
        val result = PregnancyCalculator.calculateFromLmp(lmp, cycleLengthDays = 28, today = today)

        // Jan 1, 2026 + 280 days = Oct 8, 2026
        assertEquals(LocalDate.of(2026, 10, 8), result.edd)
        assertEquals(10, result.gestationalWeeks)
        assertEquals(0, result.gestationalDays)
        assertEquals(1, result.trimester)
    }

    @Test
    fun testAdjustedCycleCalculation() {
        val lmp = LocalDate.of(2026, 1, 1)
        // 32-day cycle: +4 days to EDD
        val result = PregnancyCalculator.calculateFromLmp(lmp, cycleLengthDays = 32, today = lmp)
        assertEquals(LocalDate.of(2026, 10, 12), result.edd)

        // 26-day cycle: -2 days to EDD
        val resultShort = PregnancyCalculator.calculateFromLmp(lmp, cycleLengthDays = 26, today = lmp)
        assertEquals(LocalDate.of(2026, 10, 6), resultShort.edd)
    }

    @Test
    fun testUltrasoundDatingCalculation() {
        val scanDate = LocalDate.of(2026, 4, 1)
        // Scan shows 12 weeks 0 days (84 days)
        val result = PregnancyCalculator.calculateFromUltrasound(scanDate, scanWeeks = 12, scanDays = 0, today = scanDate)

        // Implied LMP = April 1 - 84 days = Jan 7, 2026. EDD = Jan 7 + 280 days = Oct 14, 2026
        assertEquals(LocalDate.of(2026, 10, 14), result.edd)
        assertEquals(12, result.gestationalWeeks)
        assertEquals(0, result.gestationalDays)
    }
}
