package com.gelb2.heavymach2

import android.app.Activity
import android.app.AlertDialog
import android.content.Intent
import android.content.pm.ActivityInfo
import android.media.AudioAttributes
import android.media.AudioFocusRequest
import android.media.AudioManager
import android.net.Uri
import android.os.Build
import android.os.Bundle
import android.text.InputFilter
import android.view.View
import android.view.WindowInsets
import android.view.WindowManager
import android.widget.EditText
import java.util.ArrayDeque
import java.util.Locale

class GameActivity : Activity() {
    internal lateinit var music: GameMusic
    private lateinit var surface: GameSurface
    private lateinit var manager: AudioManager
    private lateinit var focusRequest: AudioFocusRequest
    internal var tablet = false
        private set
    private var foreground = false
    private val dialogs = ArrayDeque<Request>()
    private var dialog: AlertDialog? = null
    private data class Request(val kind: Int, val arg: Long, val title: String, val message: String)

    @Suppress("DEPRECATION")
    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        tablet = resources.configuration.smallestScreenWidthDp >= 600
        requestedOrientation = if (tablet) ActivityInfo.SCREEN_ORIENTATION_SENSOR_PORTRAIT
                               else ActivityInfo.SCREEN_ORIENTATION_SENSOR_LANDSCAPE
        window.addFlags(WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON)
        if (Build.VERSION.SDK_INT >= 28) {
            val params = window.attributes
            params.layoutInDisplayCutoutMode = WindowManager.LayoutParams.LAYOUT_IN_DISPLAY_CUTOUT_MODE_SHORT_EDGES
            window.attributes = params
        }
        window.decorView.systemUiVisibility = View.SYSTEM_UI_FLAG_FULLSCREEN or View.SYSTEM_UI_FLAG_HIDE_NAVIGATION or
            View.SYSTEM_UI_FLAG_IMMERSIVE_STICKY or View.SYSTEM_UI_FLAG_LAYOUT_HIDE_NAVIGATION or View.SYSTEM_UI_FLAG_LAYOUT_FULLSCREEN or View.SYSTEM_UI_FLAG_LAYOUT_STABLE
        music = GameMusic(assets)
        surface = GameSurface(this)
        surface.setOnApplyWindowInsetsListener { _, insets ->
            val cutout = if (Build.VERSION.SDK_INT >= 28) insets.displayCutout else null
            if (Build.VERSION.SDK_INT >= 30) {
                // A swipe can reveal hidden bars, but the underlying game still receives taps.
                // Reserve space only for cutouts and bars that actually cover the surface.
                val edges = insets.getInsets(WindowInsets.Type.displayCutout() or WindowInsets.Type.systemBars())
                surface.insets(edges.left, edges.bottom, edges.right, edges.top)
            } else surface.insets(maxOf(insets.systemWindowInsetLeft, cutout?.safeInsetLeft ?: 0),
                maxOf(insets.systemWindowInsetBottom, cutout?.safeInsetBottom ?: 0),
                maxOf(insets.systemWindowInsetRight, cutout?.safeInsetRight ?: 0),
                maxOf(insets.systemWindowInsetTop, cutout?.safeInsetTop ?: 0))
            insets
        }
        manager = getSystemService(AudioManager::class.java)
        val attributes = AudioAttributes.Builder().setUsage(AudioAttributes.USAGE_GAME)
            .setContentType(AudioAttributes.CONTENT_TYPE_MUSIC).build()
        focusRequest = AudioFocusRequest.Builder(AudioManager.AUDIOFOCUS_GAIN).setAudioAttributes(attributes)
            .setOnAudioFocusChangeListener { change ->
                if (foreground) surface.audioFocus(change == AudioManager.AUDIOFOCUS_GAIN)
            }.build()
        setContentView(surface)
        acceptCommand(intent)
    }

    override fun onNewIntent(intent: Intent) {
        super.onNewIntent(intent)
        acceptCommand(intent)
    }

    private fun acceptCommand(intent: Intent) {
        if (BuildConfig.CHEAT) intent.getStringExtra("hm2.validation")?.let { surface.command(it, intent.getStringExtra("hm2.request").orEmpty()) }
    }

    override fun onResume() {
        super.onResume()
        foreground = true
        surface.start()
        surface.audioFocus(manager.requestAudioFocus(focusRequest) == AudioManager.AUDIOFOCUS_REQUEST_GRANTED)
        presentNext()
    }

    override fun onPause() {
        foreground = false
        surface.stop()
        music.active(false)
        manager.abandonAudioFocusRequest(focusRequest)
        super.onPause()
    }

    override fun onDestroy() {
        dialog?.dismiss()
        dialogs.clear()
        surface.dispose()
        music.close()
        super.onDestroy()
    }

    // These methods are invoked from the native game thread.
    fun raster(value: String, font: String, size: Float, width: Int, height: Int,
               contentWidth: Float, contentHeight: Float, align: Int) =
        GameText.rasterize(value, font, size, width, height, contentWidth, contentHeight, align)
    fun playMusic(name: String) = music.play(name)
    fun audioActive(active: Boolean) = music.active(active)
    fun showDialog(kind: Int, arg: Long, title: String, message: String) = runOnUiThread {
        if (!isFinishing && !isDestroyed) {
            dialogs.addLast(Request(kind, arg, title, message))
            presentNext()
        }
    }

    private fun presentNext() {
        if (!foreground || isFinishing || dialog != null || dialogs.isEmpty()) return
        val request = dialogs.removeFirst()
        val builder = AlertDialog.Builder(this).setTitle(request.title)
        if (request.message.isNotEmpty()) builder.setMessage(request.message)
        val input = if (request.kind == 1) EditText(this).apply {
            isSingleLine = true
            filters = arrayOf(InputFilter.LengthFilter(10))
            hint = "이름 (1~10자)"
            builder.setView(this)
        } else null
        builder.setPositiveButton(android.R.string.ok) { _, _ ->
            when (request.kind) {
                1, 2, 3 -> surface.confirm(request.kind, request.arg, input?.text?.toString().orEmpty(),
                    Locale.getDefault().country.lowercase(Locale.ROOT).ifEmpty { "us" })
                4 -> {
                    val link = Intent(Intent.ACTION_VIEW, Uri.parse("http://www.indieapps.kr"))
                    if (link.resolveActivity(packageManager) != null) startActivity(link)
                }
            }
        }
        if (request.kind == 1 || request.kind == 2 || request.kind == 4)
            builder.setNegativeButton(android.R.string.cancel, null)
        if (request.kind == 3) builder.setCancelable(false)
        dialog = builder.create().also {
            it.setOnDismissListener { dialog = null; presentNext() }
            it.show()
        }
    }
}
