package com.gelb2.heavymach2

import android.content.res.AssetManager

internal object NativeGame {
    init { System.loadLibrary("heavymach2") }
    external fun create(owner: GameActivity, assets: AssetManager, files: String): Long
    external fun restore(handle: Long)
    external fun display(handle: Long, width: Int, height: Int, density: Float,
                         left: Int, bottom: Int, right: Int, top: Int, tablet: Boolean)
    external fun frame(handle: Long, ticks: Int)
    external fun active(handle: Long, active: Boolean)
    external fun focus(handle: Long, focused: Boolean)
    external fun touch(handle: Long, phase: Int, id: Int, x: Float, y: Float)
    external fun cancel(handle: Long)
    external fun confirm(handle: Long, kind: Int, argument: Long, name: String, country: String)
    external fun destroy(handle: Long)
}
