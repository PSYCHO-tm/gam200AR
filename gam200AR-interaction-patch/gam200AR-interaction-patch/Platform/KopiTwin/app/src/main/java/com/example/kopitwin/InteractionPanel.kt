package com.example.kopitwin

import android.app.Activity
import android.graphics.Color
import android.graphics.Typeface
import android.widget.Button
import android.widget.LinearLayout
import android.widget.TextView

/**
 * The interaction screen: the current order and step, the buttons for the selected equipment, the timed events and
 * any errors. Everything shown comes from the C++ core through InteractionNative. Selecting equipment is done by
 * tapping it in the AR view (the core ray-casts the tap), not here.
 */
class InteractionPanel(private val activity: Activity) {

    val root = LinearLayout(activity)

    private val orderText = makeText(15f, Color.WHITE)
    private val selectedText = makeText(14f, Color.YELLOW)
    private val messageText = makeText(12f, Color.rgb(160, 255, 160))
    private val errorsText = makeText(13f, Color.rgb(255, 120, 120))
    private val eventsText = makeText(11f, Color.LTGRAY)
    private val buttonRow = LinearLayout(activity)

    private var shownSelection = "?"   // forces the first button build
    private val ticker = object : Runnable {
        override fun run() {
            refresh()
            root.postDelayed(this, 200)
        }
    }

    init {
        root.orientation = LinearLayout.VERTICAL
        root.setBackgroundColor(Color.argb(170, 0, 0, 0))
        root.setPadding(12, 8, 12, 8)
        buttonRow.orientation = LinearLayout.HORIZONTAL
        for (view in listOf(orderText, selectedText, buttonRow, messageText, errorsText, eventsText)) {
            root.addView(view)
        }
    }

    fun start() = root.post(ticker)

    fun stop() {
        root.removeCallbacks(ticker)
    }

    fun refresh() {
        orderText.text = InteractionNative.nativeInteractionOrderText()

        val selected = InteractionNative.nativeInteractionSelected()
        selectedText.text = if (selected.isEmpty()) "Tap a piece of equipment" else "Selected: $selected"
        if (selected != shownSelection) {
            shownSelection = selected
            rebuildButtons()
        }

        messageText.text = InteractionNative.nativeInteractionMessage()
        val errors = InteractionNative.nativeInteractionErrors().trim()
        errorsText.text = errors.lines().filter { it.isNotEmpty() }.joinToString("\n") {
            "ERROR at " + it.replace("|", ": ")
        }
        eventsText.text = InteractionNative.nativeInteractionEvents().trim()
    }

    private fun rebuildButtons() {
        buttonRow.removeAllViews()
        for (line in InteractionNative.nativeInteractionButtons().lines()) {
            val parts = line.split("|")
            if (parts.size < 4) continue
            val action = parts[0]
            buttonRow.addView(Button(activity).apply {
                text = "${parts[1]} (${parts[2]} ${parts[3]})"
                setOnClickListener {
                    val rejected = InteractionNative.nativeInteractionButton(action)
                    if (rejected.isNotEmpty()) errorsText.text = rejected
                    refresh()
                }
            })
        }
    }

    private fun makeText(size: Float, color: Int) = TextView(activity).apply {
        textSize = size
        typeface = Typeface.MONOSPACE
        setTextColor(color)
    }
}
