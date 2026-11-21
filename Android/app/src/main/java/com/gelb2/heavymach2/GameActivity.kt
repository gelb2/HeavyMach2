package com.gelb2.heavymach2

import android.app.Activity
import android.app.AlertDialog
import android.content.ClipData
import android.content.ClipDescription
import android.content.ClipboardManager
import android.content.Intent
import android.content.pm.ActivityInfo
import android.media.AudioAttributes
import android.media.AudioFocusRequest
import android.media.AudioManager
import android.net.Uri
import android.os.Build
import android.os.Bundle
import android.os.PersistableBundle
import android.text.InputFilter
import android.text.InputType
import android.view.View
import android.view.WindowInsets
import android.view.WindowManager
import android.view.inputmethod.EditorInfo
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
    // Match the appended recovery kinds in the native DialogKind enum.
    private companion object {
        const val RECOVERY_MENU = 5
        const val RECOVERY_INPUT = 6
        const val RECOVERY_CONFIRM = 7
        const val RECOVERY_COPY = 8
    }

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
        if (request.kind in RECOVERY_MENU..RECOVERY_COPY) {
            presentRecovery(request)
            return
        }
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
        showNativeDialog(builder)
    }

    private fun presentRecovery(request: Request) {
        fun respond(kind: Int, value: String = "") =
            surface.confirm(kind, request.arg, value, "")

        if (request.kind == RECOVERY_COPY) {
            val clip = ClipData.newPlainText("HeavyMach 2 복구코드", request.message)
            if (Build.VERSION.SDK_INT >= 33) {
                clip.description.extras = PersistableBundle().apply {
                    putBoolean(ClipDescription.EXTRA_IS_SENSITIVE, true)
                }
            }
            getSystemService(ClipboardManager::class.java).setPrimaryClip(clip)
            presentNext()
            return
        }

        val builder = AlertDialog.Builder(this).setTitle(request.title)
        if (request.message.isNotEmpty()) builder.setMessage(request.message)
        when (request.kind) {
            RECOVERY_MENU -> {
                builder.setNegativeButton(android.R.string.cancel, null)
                    .setNeutralButton("코드 복사") { _, _ -> respond(RECOVERY_COPY) }
                    .setPositiveButton("코드 입력") { _, _ -> showDialog(RECOVERY_INPUT, request.arg, "복구코드 입력", "") }
            }
            RECOVERY_INPUT -> {
                val input = EditText(this).apply {
                    isSingleLine = true
                    inputType = InputType.TYPE_CLASS_TEXT or InputType.TYPE_TEXT_FLAG_NO_SUGGESTIONS
                    imeOptions = EditorInfo.IME_ACTION_DONE or EditorInfo.IME_FLAG_NO_EXTRACT_UI or
                        EditorInfo.IME_FLAG_NO_FULLSCREEN
                    filters = arrayOf(InputFilter.LengthFilter(65536))
                    hint = "복구코드를 붙여넣으세요"
                }
                val submit = {
                    val code = input.text.toString()
                    respond(RECOVERY_INPUT, if (code.all { it.code < 128 }) code else "")
                }
                input.setOnEditorActionListener { _, action, _ ->
                    if (action != EditorInfo.IME_ACTION_DONE) false
                    else {
                        submit()
                        dialog?.dismiss()
                        true
                    }
                }
                builder.setView(input)
                    .setNegativeButton(android.R.string.cancel, null)
                    .setPositiveButton("코드 확인") { _, _ -> submit() }
            }
            RECOVERY_CONFIRM -> {
                builder.setNegativeButton(android.R.string.cancel) { _, _ -> respond(RECOVERY_CONFIRM) }
                    .setOnCancelListener { respond(RECOVERY_CONFIRM) }
                    .setPositiveButton("슬롯 교체") { _, _ -> respond(RECOVERY_CONFIRM, "restore") }
            }
        }
        showNativeDialog(builder)
    }

    private fun showNativeDialog(builder: AlertDialog.Builder) {
        dialog = builder.create().also {
            it.setOnDismissListener { dialog = null; presentNext() }
            it.show()
        }
    }
}
