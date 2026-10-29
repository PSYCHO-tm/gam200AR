package com.example.kopitwin

/**
 * JNI entry points for the interaction screen. The C++ side is cpp/JNI-interaction.cpp.
 * Kotlin only passes plain values across: it never decides what a touch hit or what a button does.
 * (Touches do not come through here: MainActivity already sends them to NativeCore.nativeSetInput, and the core
 * ray-casts them against the equipment on its own.)
 */
object InteractionNative {
    init {
        System.loadLibrary("kopitwin")   // same library as NativeCore, loaded once
    }

    // Screen -> core
    external fun nativeInteractionButton(action: String): String                          // "" or the reason it was rejected
    external fun nativeInteractionButtonQuantity(action: String, quantity: Float): String // a held pour sets the quantity

    // Core -> screen
    external fun nativeInteractionSelected(): String    // id of the selected equipment, or ""
    external fun nativeInteractionButtons(): String     // lines of "action|label|quantity|units"
    external fun nativeInteractionOrderText(): String   // "Order: Kopi | Step 2/3: ... | 38 s"
    external fun nativeInteractionMessage(): String     // the assessment core's last verdict
    external fun nativeInteractionEvents(): String      // lines of "time | equipment | action object | quantity"
    external fun nativeInteractionErrors(): String      // lines of "equipment|message"
}
