package com.gelb2.heavymach2

import org.junit.Assert.assertEquals
import org.junit.Test

class FrameClockTest {
    @Test fun tenSecondsProduce600UpdatesAtEachRefreshRate() {
        for (hz in listOf(60, 90, 120)) {
            val clock = FrameClock()
            var ticks = 0
            for (frame in 0..10 * hz) {
                val timestamp = 7_000_000_000L + frame * 1_000_000_000L / hz
                ticks += clock.ticksAt(timestamp)
                assertEquals("tick at frame $frame of $hz Hz", frame * 60 / hz, ticks)
            }
            assertEquals(600, ticks)
        }
    }

    @Test fun refreshRateCanChangeDuringPlay() {
        val clock = FrameClock()
        var elapsed = 0L
        var ticks = clock.ticksAt(elapsed)
        for (hz in listOf(90, 120, 60)) {
            for (frame in 1..hz) ticks += clock.ticksAt(elapsed + frame * 1_000_000_000L / hz)
            elapsed += 1_000_000_000L
            assertEquals((elapsed / 1_000_000_000L * 60).toInt(), ticks)
        }
    }

    @Test fun pauseDropsElapsedTimeAndFractionalTicks() {
        val clock = FrameClock()
        assertEquals(0, clock.ticksAt(0))
        assertEquals(0, clock.ticksAt(16_000_000L))
        clock.reset()
        assertEquals(0, clock.ticksAt(FrameClock.NO_FRAME))
        assertEquals(0, clock.ticksAt(30_000_000_000L))
        assertEquals(0, clock.ticksAt(30_001_000_000L))
        assertEquals(1, clock.ticksAt(30_016_666_666L))
    }

    @Test fun repeatedOrOldCallbacksDoNotAdvanceOrRewindTime() {
        val clock = FrameClock()
        assertEquals(0, clock.ticksAt(1_000_000_000L))
        assertEquals(3, clock.ticksAt(1_050_000_000L))
        assertEquals(0, clock.ticksAt(1_050_000_000L))
        assertEquals(0, clock.ticksAt(1_000_000_000L))
        assertEquals(3, clock.ticksAt(1_100_000_000L))
    }

    @Test fun slowActiveFrameBoundsCatchupAndResumesNormalCadence() {
        val clock = FrameClock()
        assertEquals(0, clock.ticksAt(0))
        assertEquals(6, clock.ticksAt(1_000_000_000L))
        assertEquals(1, clock.ticksAt(1_016_666_666L))
    }
}
