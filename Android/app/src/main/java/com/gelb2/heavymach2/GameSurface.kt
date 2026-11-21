package com.gelb2.heavymach2

import android.opengl.GLSurfaceView
import android.util.Log
import android.view.Choreographer
import android.view.MotionEvent
import javax.microedition.khronos.egl.EGLConfig
import javax.microedition.khronos.opengles.GL10
import java.util.ArrayDeque

/** The GL thread owns all game, texture and pointer state. */
internal class GameSurface(private val owner: GameActivity) : GLSurfaceView(owner),
        GLSurfaceView.Renderer, Choreographer.FrameCallback {
    private var handle = 0L
    private var failed = false
    private var widthPx = 0
    private var heightPx = 0
    @Volatile private var edges = intArrayOf(0, 0, 0, 0)
    private var appliedEdges: IntArray? = null
    private var appliedDensity = 0f
    private var appliedTablet: Boolean? = null
    private val touches = ArrayDeque<() -> Unit>()
    private val confirmations = ArrayDeque<() -> Unit>()
    // Activity and View lifecycle callbacks both run on the UI thread.
    private var disposeRequested = false
    private var threadJoined = false
    private val clock = FrameClock()
    private var active = false
    private var focused = false
    private var pumping = false
    private val commands = ArrayDeque<Pair<String,String>>()
    @Volatile private var vsync = FrameClock.NO_FRAME

    init {
        setEGLContextClientVersion(1)
        setEGLConfigChooser(8, 8, 8, 8, 0, 0)
        preserveEGLContextOnPause = true
        setRenderer(this)
        renderMode = RENDERMODE_WHEN_DIRTY
    }

    private inline fun game(action: () -> Unit) {
        if (failed) return
        try { action() } catch (error: RuntimeException) {
            failed = true
            owner.music.active(false)
            Log.e("HeavyMach2", "Game stopped", error)
            owner.showDialog(0, 0, "HeavyMach 2", error.message ?: "실행 오류")
        }
    }

    override fun onSurfaceCreated(gl: GL10?, config: EGLConfig?) = game {
        if (handle == 0L) {
            handle = NativeGame.create(owner, owner.assets, owner.filesDir.absolutePath)
            NativeGame.active(handle, active)
            NativeGame.focus(handle, focused)
        } else NativeGame.restore(handle)
        touches.clear()
        appliedEdges = null
        clock.reset()
    }

    private fun applyDisplay() {
        val next = edges
        val density = resources.displayMetrics.density
        val tablet = owner.tablet
        if (next.contentEquals(appliedEdges) && density == appliedDensity && tablet == appliedTablet) return
        if (next[0] + next[2] >= widthPx || next[1] + next[3] >= heightPx) return
        NativeGame.display(handle, widthPx, heightPx, density, next[0], next[1], next[2], next[3], tablet)
        appliedEdges = next
        appliedDensity = density
        appliedTablet = tablet
        touches.clear()
    }

    override fun onSurfaceChanged(gl: GL10?, width: Int, height: Int) = game {
        widthPx = width
        heightPx = height
        appliedEdges = null
        clock.reset()
        if (width > 0 && height > 0) applyDisplay()
    }

    fun insets(left: Int, bottom: Int, right: Int, top: Int) {
        edges = intArrayOf(left, bottom, right, top)
    }

    override fun onDrawFrame(gl: GL10?) = game {
        if (!active || handle == 0L || widthPx == 0 || heightPx == 0) return@game
        // queueEvent alone does not guarantee the GL thread has a drawable.
        // Events that can prepare textures run only within a renderer callback.
        applyDisplay()
        if (appliedEdges == null) return@game
        while (confirmations.isNotEmpty()) confirmations.removeFirst().invoke()
        while (touches.isNotEmpty()) touches.removeFirst().invoke()
        NativeGame.frame(handle, clock.ticksAt(vsync))
        if (BuildConfig.CHEAT && commands.isNotEmpty()) {
            val (command, request) = commands.removeFirst()
            TestControl.run(handle, command, request, owner.filesDir)
        }
    }

    override fun doFrame(frameTimeNanos: Long) {
        if (!pumping) return
        vsync = frameTimeNanos
        requestRender()
        Choreographer.getInstance().postFrameCallback(this)
    }

    fun start() {
        vsync = FrameClock.NO_FRAME
        onResume()
        queueEvent { game { active = true; clock.reset(); if (handle != 0L) NativeGame.active(handle, true) } }
        pumping = true
        Choreographer.getInstance().postFrameCallback(this)
    }

    fun stop() {
        pumping = false
        Choreographer.getInstance().removeFrameCallback(this)
        queueEvent { game {
            touches.clear()
            active = false
            focused = false
            if (handle != 0L) {
                NativeGame.active(handle, false)
                NativeGame.focus(handle, false)
            }
        } }
        onPause()
    }

    fun audioFocus(value: Boolean) = queueEvent {
        game { focused = value; if (handle != 0L) NativeGame.focus(handle, value) }
    }

    fun confirm(kind: Int, arg: Long, name: String, country: String) = queueEvent {
        confirmations.addLast { NativeGame.confirm(handle, kind, arg, name, country) }
    }

    fun command(value: String, request: String) = queueEvent {
        if (BuildConfig.CHEAT) commands.addLast(value to request)
    }

    override fun onAttachedToWindow() {
        threadJoined = false
        super.onAttachedToWindow()
    }

    override fun onDetachedFromWindow() {
        super.onDetachedFromWindow() // Joins the renderer thread and releases its EGL context.
        threadJoined = true
        if (disposeRequested) releaseSession()
    }

    fun dispose() {
        disposeRequested = true
        if (threadJoined) releaseSession()
    }

    private fun releaseSession() {
        if (handle != 0L) {
            NativeGame.destroy(handle)
            handle = 0L
        }
    }

    override fun onTouchEvent(event: MotionEvent): Boolean {
        val phase = when (event.actionMasked) {
            MotionEvent.ACTION_DOWN, MotionEvent.ACTION_POINTER_DOWN -> 0
            MotionEvent.ACTION_MOVE -> 1
            MotionEvent.ACTION_UP, MotionEvent.ACTION_POINTER_UP -> 2
            MotionEvent.ACTION_CANCEL -> {
                queueEvent { if (active) touches.addLast { NativeGame.cancel(handle) } }
                return true
            }
            else -> return false
        }
        val range = if (phase == 1) 0 until event.pointerCount else event.actionIndex..event.actionIndex
        val density = resources.displayMetrics.density
        for (index in range) {
            val pointer = event.getPointerId(index) + 1
            val x = event.getX(index) / density
            val y = (height - event.getY(index)) / density
            queueEvent { if (active) touches.addLast { NativeGame.touch(handle, phase, pointer, x, y) } }
        }
        return true
    }
}
