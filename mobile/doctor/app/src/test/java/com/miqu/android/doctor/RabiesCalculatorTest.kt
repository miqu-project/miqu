package com.miqu.android.doctor

import com.miqu.android.doctor.model.RabiesCalculator
import org.junit.Assert.assertEquals
import org.junit.Assert.assertNotNull
import org.junit.Assert.assertNull
import org.junit.Test
import java.time.LocalDate

class RabiesCalculatorTest {

    @Test
    fun testRabiesSchedules() {
        val day0 = LocalDate.of(2026, 6, 1)
        val schedule = RabiesCalculator.calculate(day0, weightKg = 60.0)

        // ID Schedule: Days 0, 3, 7, 28
        assertEquals(4, schedule.idVisits.size)
        assertEquals(LocalDate.of(2026, 6, 1), schedule.idVisits[0].date)
        assertEquals(LocalDate.of(2026, 6, 4), schedule.idVisits[1].date)
        assertEquals(LocalDate.of(2026, 6, 8), schedule.idVisits[2].date)
        assertEquals(LocalDate.of(2026, 6, 29), schedule.idVisits[3].date)

        // IM Schedule: Days 0, 3, 7, 14, 28
        assertEquals(5, schedule.imVisits.size)
        assertEquals(LocalDate.of(2026, 6, 1), schedule.imVisits[0].date)
        assertEquals(LocalDate.of(2026, 6, 4), schedule.imVisits[1].date)
        assertEquals(LocalDate.of(2026, 6, 8), schedule.imVisits[2].date)
        assertEquals(LocalDate.of(2026, 6, 15), schedule.imVisits[3].date)
        assertEquals(LocalDate.of(2026, 6, 29), schedule.imVisits[4].date)

        // Re-exposure: Days 0, 3
        assertEquals(2, schedule.reExposureVisits.size)
        assertEquals(LocalDate.of(2026, 6, 1), schedule.reExposureVisits[0].date)
        assertEquals(LocalDate.of(2026, 6, 4), schedule.reExposureVisits[1].date)

        // RIG: 60 kg -> HRIG = 1200 IU (20 IU/kg), ERIG = 2400 IU (40 IU/kg)
        assertNotNull(schedule.hrigDoseIu)
        assertNotNull(schedule.erigDoseIu)
        assertEquals(1200.0, schedule.hrigDoseIu!!, 0.01)
        assertEquals(2400.0, schedule.erigDoseIu!!, 0.01)
    }

    @Test
    fun testNullOrZeroWeight() {
        val day0 = LocalDate.of(2026, 6, 1)
        val schedule = RabiesCalculator.calculate(day0, weightKg = null)
        assertNull(schedule.hrigDoseIu)
        assertNull(schedule.erigDoseIu)

        val scheduleZero = RabiesCalculator.calculate(day0, weightKg = 0.0)
        assertNull(scheduleZero.hrigDoseIu)
        assertNull(scheduleZero.erigDoseIu)
    }
}
