package com.example.kopitwin

import android.app.Activity
import android.graphics.Color
import android.graphics.Typeface
import android.view.MotionEvent
import android.widget.Button
import android.widget.LinearLayout
import android.widget.TextView
import org.json.JSONException
import org.json.JSONObject
import java.io.IOException

/**
 * The Goal 5 screen: the current order and step, the buttons for the selected equipment, the timed events
 * and any errors. Everything shown comes from the C++ core through InteractionNative.
 */
class InteractionPanel(private val activity: Activity) {

    val root = LinearLayout(activity)

    private val orderText = makeText(15f, Color.WHITE)
    private val selectedText = makeText(14f, Color.YELLOW)
    private val errorsText = makeText(13f, Color.rgb(255, 120, 120))
    private val eventsText = makeText(11f, Color.LTGRAY)
    private val buttonRow = LinearLayout(activity)
    private val debugRow = LinearLayout(activity)

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
        debugRow.orientation = LinearLayout.HORIZONTAL

        debugRow.addView(Button(activity).apply {
            text = "Demo order"
            setOnClickListener { InteractionNative.nativeInteractionSetOrder("Kopi", "Add water", 2, 4, 20f) }
        })
        debugRow.addView(Button(activity).apply {
            text = "Error on selected"
            setOnClickListener {
                val selected = InteractionNative.nativeInteractionSelected()
                if (selected.isNotEmpty()) {
                    InteractionNative.nativeInteractionShowError(selected, "wrong_quantity", "Wrong quantity", 3f)
                }
            }
        })

        for (view in listOf(orderText, selectedText, buttonRow, errorsText, eventsText, debugRow)) {
            root.addView(view)
        }
    }

    /** Reads Equipment.json and the meshes from the APK's assets and hands them to the core. Returns null on success. */
    fun load(): String? {
        return try {
            val json = readAsset("Json/Equipment.json") ?: return "Equipment.json is not in the app's assets"
            val equipment = JSONObject(json).optJSONObject("EQUIPMENT") ?: return "Equipment.json has no EQUIPMENT object"
            val meshPaths = ArrayList<String>()
            val meshTexts = ArrayList<String>()
            for (key in equipment.keys()) {
                val path = equipment.getJSONObject(key).optString("Mesh")
                // The APK's assets folder is the repo's Assets/ folder, so "Assets/Models/x.obj" is stored as "Models/x.obj"
                val text = readAsset(path.removePrefix("Assets/")) ?: return "Mesh is not in the app's assets: $path"
                meshPaths.add(path)
                meshTexts.add(text)
            }
            val error = InteractionNative.nativeInteractionLoad(json, meshPaths.toTypedArray(), meshTexts.toTypedArray())
            if (error.isEmpty()) null else error
        } catch (e: JSONException) {
            "Equipment.json is not valid JSON: ${e.message}"
        }
    }

    /** Call from the GL view's touch listener. */
    fun onTouch(event: MotionEvent) {
        InteractionNative.nativeInteractionTouch(event.actionMasked, event.x, event.y)
        refresh()
    }

    fun start() = root.post(ticker)

    fun stop() {
        root.removeCallbacks(ticker)
    }

    fun refresh() {
        orderText.text = InteractionNative.nativeInteractionOrderText()

        val selected = InteractionNative.nativeInteractionSelected()
        selectedText.text = if (selected.isEmpty()) "Touch a piece of equipment" else "Selected: $selected"
        if (selected != shownSelection) {
            shownSelection = selected
            rebuildButtons()
        }

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

    private fun readAsset(path: String): String? = try {
        activity.assets.open(path).bufferedReader().use { it.readText() }
    } catch (e: IOException) {
        null
    }

    private fun makeText(size: Float, color: Int) = TextView(activity).apply {
        textSize = size
        typeface = Typeface.MONOSPACE
        setTextColor(color)
    }
}
