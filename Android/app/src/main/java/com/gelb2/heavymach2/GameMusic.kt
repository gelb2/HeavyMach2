package com.gelb2.heavymach2

import android.content.res.AssetManager
import android.media.AudioAttributes
import android.media.MediaPlayer
import android.os.Handler
import android.os.HandlerThread
import android.util.Log

internal class GameMusic(private val assets: AssetManager) {
    private val thread = HandlerThread("HM2 music").apply { start() }
    private val queue = Handler(thread.looper)
    private var player: MediaPlayer? = null
    private var prepared = false
    private var active = false
    @Volatile private var closed = false

    // All MediaPlayer operations, including asynchronous callbacks, are serialized on queue.
    private fun releasePlayer() {
        val old = player
        player = null
        prepared = false
        old?.release()
    }

    private fun applyPlayback() {
        val current = player ?: return
        if (!prepared || closed) return
        try {
            if (active && !current.isPlaying) current.start()
            if (!active && current.isPlaying) current.pause()
        } catch (error: IllegalStateException) {
            releasePlayer()
            Log.e("HeavyMach2", "BGM output lost its prepared state", error)
        }
    }

    fun play(track: String) {
        queue.post {
            if (closed) return@post
            releasePlayer()
            try {
                val next = MediaPlayer()
                player = next
                val attributes = AudioAttributes.Builder().setUsage(AudioAttributes.USAGE_GAME)
                    .setContentType(AudioAttributes.CONTENT_TYPE_MUSIC).build()
                next.setAudioAttributes(attributes)
                assets.openFd(track).use { next.setDataSource(it.fileDescriptor, it.startOffset, it.length) }
                next.isLooping = true
                next.setOnPreparedListener {
                    if (player === it) {
                        prepared = true
                        applyPlayback()
                        Log.i("HeavyMach2", "BGM ready $track")
                    }
                }
                next.setOnErrorListener { failed, what, extra ->
                    if (player === failed) {
                        releasePlayer()
                        Log.e("HeavyMach2", "BGM playback failed for $track ($what, $extra)")
                    }
                    true
                }
                next.prepareAsync()
            } catch (error: Exception) {
                releasePlayer()
                Log.e("HeavyMach2", "Cannot open $track", error)
            }
        }
    }

    fun active(value: Boolean) {
        queue.post {
            active = value
            applyPlayback()
        }
    }

    fun close() {
        closed = true
        queue.post {
            releasePlayer()
            thread.quitSafely()
        }
    }
}
