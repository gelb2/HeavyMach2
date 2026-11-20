package com.gelb2.heavymach2

/** Converts display callbacks to the original 60 game updates per second. Used on the GL thread. */
internal class FrameClock {
    companion object {
        const val NO_FRAME = Long.MIN_VALUE
        private const val NANOS_PER_SECOND = 1_000_000_000L
        private const val TICKS_PER_SECOND = 60L
        private const val MAX_ELAPSED_NANOS = 100_000_000L

        // The first ideal deadline is 16,666,666⅔ ns. A frame timestamp rounded
        // down to 16,666,666 ns represents that same vsync, not a missed tick.
        private const val NANOS_ROUNDING = TICKS_PER_SECOND - 1
    }

    private var lastVsync = NO_FRAME
    private var remainder = NANOS_ROUNDING

    fun reset() {
        lastVsync = NO_FRAME
        remainder = NANOS_ROUNDING
    }

    fun ticksAt(vsyncNanos: Long): Int {
        if (vsyncNanos <= lastVsync) return 0
        val previous = lastVsync
        lastVsync = vsyncNanos
        if (previous == NO_FRAME) return 0

        // A single slow frame can catch up at most six ticks. Suspension is
        // handled by reset(), so background time never becomes simulation time.
        val elapsed = (vsyncNanos - previous).coerceAtMost(MAX_ELAPSED_NANOS)
        val scaled = remainder + elapsed * TICKS_PER_SECOND
        remainder = scaled % NANOS_PER_SECOND
        return (scaled / NANOS_PER_SECOND).toInt()
    }
}
