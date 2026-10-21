package com.example.kopitwin

/**
 * JNI entry points for the Goal 5 interaction engine. The C++ side is cpp/JNI-interaction.cpp.
 * Kotlin only passes plain values across: it never decides what a touch hit or what a button does.
 */
object InteractionNative {
    init {
        System.loadLibrary("kopitwin")   // same library as NativeCore, loaded once
    }

    /** Equipment.json plus the OBJ text of each mesh it names. Returns "" on success, otherwise what is wrong. */
    external fun nativeInteractionLoad(equipmentJson: String, meshPaths: Array<String>, meshTexts: Array<String>): String

    // GL thread
    external fun nativeInteractionInitGl()
    external fun nativeInteractionResize(width: Int, height: Int)
    external fun nativeInteractionDraw()

    // Camera and anchor. Optional until ARCore's matrices are connected (16 floats each, column-major).
    external fun nativeInteractionSetCamera(view: FloatArray, projection: FloatArray)
    external fun nativeInteractionSetAnchor(pose: FloatArray)

    // Input -> core. Touch returns the selected equipment id ("" if none); Button returns "" or the reason it was rejected.
    external fun nativeInteractionTouch(action: Int, x: Float, y: Float): String
    external fun nativeInteractionButton(action: String): String

    // Core -> screen
    external fun nativeInteractionSelected(): String
    external fun nativeInteractionButtons(): String     // lines of "action|label|quantity|units"
    external fun nativeInteractionOrderText(): String
    external fun nativeInteractionEvents(): String      // lines of "time | object | action | quantity"
    external fun nativeInteractionErrors(): String      // lines of "object|message"

    // Stand-ins for the assessment core (Goal 6) until it is connected
    external fun nativeInteractionSetOrder(orderName: String, stepName: String, stepIndex: Int, stepCount: Int, secondsLeft: Float)
    external fun nativeInteractionShowError(objectId: String, kind: String, message: String, seconds: Float)
}
