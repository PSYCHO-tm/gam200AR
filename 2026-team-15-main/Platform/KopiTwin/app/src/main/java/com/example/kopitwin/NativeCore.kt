package com.example.kopitwin

object NativeCore {
    init {
        System.loadLibrary("kopitwin")   // must match add_library(...) name
    }

    external fun update()

    external fun onResume()
    external fun nativeOverlayText(): String
    external fun nativeGetTouch(): String
    external fun nativeSetInput(x: Float, y: Float, action: Int)
    external fun nativeResetInput()

    external fun nativeTogglePause(paused: Boolean)
}