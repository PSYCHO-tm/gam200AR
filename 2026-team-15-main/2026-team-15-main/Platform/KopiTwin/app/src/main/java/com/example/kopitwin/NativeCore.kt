package com.example.kopitwin

object NativeCore {
    init {
        System.loadLibrary("kopitwin")   // must match add_library(...) name
    }

    external fun frame()

    external fun onResume()
    external fun nativeOverlayText(): String
    external fun nativeTouch(action: Int, x: Float, y: Float): String
}