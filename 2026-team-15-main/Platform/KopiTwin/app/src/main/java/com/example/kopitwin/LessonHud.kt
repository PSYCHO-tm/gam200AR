package com.example.kopitwin

import android.app.Activity
import android.graphics.Color
import android.graphics.Typeface
import android.graphics.drawable.GradientDrawable
import android.view.Gravity
import android.view.View
import android.widget.FrameLayout
import android.widget.LinearLayout
import android.widget.TextView

/**
 * Sandbox: the guided-lesson screen from Mabel's Figma prototype, drawn with plain Android views over the AR view.
 *
 *  - top:    step card (red number circle, section, lesson name, progress segments) + the pause button
 *  - bottom: hint card (with an accent bar) + step card ("STEP 1 OF 8 · GUIDED", step title)
 *  - while the station is not shown: a cream "Scan the card" sheet (the C++ loader spins above it)
 *  - after the last step: a cream "done" sheet
 *
 * Everything shown comes from the C++ core (NativeCore.nativeLessonState); this class only lays it out.
 */
class LessonHud(private val activity: Activity) {

    // Mabel's palette (sampled from the prototype).
    private val maroon = Color.rgb(139, 34, 50)
    private val blueGrey = Color.rgb(62, 102, 128)
    private val cream = Color.rgb(241, 236, 228)
    private val darkGlass = Color.argb(215, 46, 43, 41)
    private val arrowBlue = Color.rgb(143, 188, 230)
    private val mutedText = Color.rgb(190, 184, 176)

    val root = FrameLayout(activity)

    private val numberText = text(15f, Color.WHITE, bold = true).apply { gravity = Gravity.CENTER }
    private val sectionText = text(10f, mutedText, bold = false).apply { letterSpacing = 0.08f }
    private val lessonText = text(17f, Color.WHITE, bold = true)
    private val progressRow = LinearLayout(activity).apply { orientation = LinearLayout.HORIZONTAL }
    private val pauseSlot = FrameLayout(activity)

    private val hintAccent = View(activity)
    private val hintText = text(14f, Color.WHITE, bold = false)
    private val stepLabel = text(10f, mutedText, bold = false).apply { letterSpacing = 0.06f }
    private val stepTitle = text(18f, Color.WHITE, bold = true)

    private val sheetTitle = text(20f, Color.rgb(40, 36, 34), bold = true)
    private val sheetBody = text(14f, Color.rgb(95, 90, 86), bold = false)

    private val topBar = LinearLayout(activity)
    private val bottomCards = LinearLayout(activity)
    private val sheet = LinearLayout(activity)

    private var lastState = ""
    private var builtSegments = -1
    private var paused = false

    /** The pause button was pressed (true) or released (false): show the "Paused" sheet instead of "Scan the card". */
    fun setPaused(isPaused: Boolean) {
        paused = isPaused
        val state = lastState
        lastState = ""          // force a re-layout with the new pause state
        update(state)
    }

    init {
        // ---- top bar: step card + pause slot -------------------------------------------------------------------
        val circle = FrameLayout(activity).apply {
            background = GradientDrawable().apply { shape = GradientDrawable.OVAL; setColor(maroon) }
            addView(numberText, FrameLayout.LayoutParams(dp(30), dp(30)))
        }
        val titles = LinearLayout(activity).apply {
            orientation = LinearLayout.VERTICAL
            addView(sectionText)
            addView(lessonText)
            addView(progressRow, LinearLayout.LayoutParams(LinearLayout.LayoutParams.MATCH_PARENT, dp(4)).apply {
                topMargin = dp(6)
            })
        }
        val stepCard = LinearLayout(activity).apply {
            orientation = LinearLayout.HORIZONTAL
            gravity = Gravity.CENTER_VERTICAL
            background = rounded(darkGlass, 14f)
            setPadding(dp(12), dp(10), dp(16), dp(10))
            addView(circle, LinearLayout.LayoutParams(dp(30), dp(30)).apply { rightMargin = dp(12) })
            addView(titles, LinearLayout.LayoutParams(0, LinearLayout.LayoutParams.WRAP_CONTENT, 1f))
        }
        topBar.orientation = LinearLayout.HORIZONTAL
        topBar.gravity = Gravity.CENTER_VERTICAL
        topBar.addView(stepCard, LinearLayout.LayoutParams(0, LinearLayout.LayoutParams.WRAP_CONTENT, 1f))
        root.addView(topBar, FrameLayout.LayoutParams(FrameLayout.LayoutParams.MATCH_PARENT,
            FrameLayout.LayoutParams.WRAP_CONTENT, Gravity.TOP).apply {
            leftMargin = dp(18); rightMargin = dp(18 + 56 + 10); topMargin = dp(44)
        })
        // The pause button sits on its own, outside the top bar, so it is ALWAYS on screen: pausing stops the camera,
        // which hides the station and the top bar, and the player must still be able to un-pause.
        root.addView(pauseSlot, FrameLayout.LayoutParams(dp(56), dp(56), Gravity.TOP or Gravity.END).apply {
            rightMargin = dp(18); topMargin = dp(44)
        })

        // ---- bottom: hint card + step card -----------------------------------------------------------------------
        val hintCard = LinearLayout(activity).apply {
            orientation = LinearLayout.HORIZONTAL
            background = rounded(darkGlass, 12f)
            setPadding(dp(12), dp(10), dp(14), dp(10))
            addView(hintAccent, LinearLayout.LayoutParams(dp(3), LinearLayout.LayoutParams.MATCH_PARENT).apply {
                rightMargin = dp(10)
            })
            addView(hintText, LinearLayout.LayoutParams(0, LinearLayout.LayoutParams.WRAP_CONTENT, 1f))
        }
        val stepCardBottom = LinearLayout(activity).apply {
            orientation = LinearLayout.VERTICAL
            background = rounded(darkGlass, 12f)
            setPadding(dp(16), dp(10), dp(16), dp(12))
            addView(stepLabel)
            addView(stepTitle)
        }
        bottomCards.orientation = LinearLayout.VERTICAL
        bottomCards.addView(hintCard, matchWrap().apply { bottomMargin = dp(8) })
        bottomCards.addView(stepCardBottom, matchWrap())
        root.addView(bottomCards, FrameLayout.LayoutParams(FrameLayout.LayoutParams.MATCH_PARENT,
            FrameLayout.LayoutParams.WRAP_CONTENT, Gravity.BOTTOM).apply {
            leftMargin = dp(18); rightMargin = dp(18); bottomMargin = dp(28)
        })

        // ---- cream sheet: scanning / done ------------------------------------------------------------------------
        sheet.orientation = LinearLayout.VERTICAL
        sheet.background = rounded(cream, 22f)
        sheet.setPadding(dp(20), dp(18), dp(20), dp(22))
        sheet.addView(sheetTitle, matchWrap().apply { bottomMargin = dp(6) })
        sheet.addView(sheetBody, matchWrap())
        root.addView(sheet, FrameLayout.LayoutParams(FrameLayout.LayoutParams.MATCH_PARENT,
            FrameLayout.LayoutParams.WRAP_CONTENT, Gravity.BOTTOM).apply {
            leftMargin = dp(18); rightMargin = dp(18); bottomMargin = dp(28)
        })

        update("scan")
    }

    /** Puts the app's pause button into the top bar, styled as the prototype's square dark button. */
    fun attachPause(button: View) {
        (button.parent as? android.view.ViewGroup)?.removeView(button)
        button.background = rounded(darkGlass, 14f)
        (button as? TextView)?.apply { setTextColor(Color.WHITE); textSize = 12f; isAllCaps = false }
        pauseSlot.addView(button, FrameLayout.LayoutParams(FrameLayout.LayoutParams.MATCH_PARENT,
            FrameLayout.LayoutParams.MATCH_PARENT))
    }

    /** Lays out the latest state from C++. Cheap to call often: it does nothing when nothing changed. */
    fun update(state: String) {
        if (state == lastState) return
        lastState = state
        val f = state.split('\t')
        val mode = f.getOrElse(0) { "scan" }
        val step = f.getOrElse(1) { "1" }.toIntOrNull() ?: 1
        val count = f.getOrElse(2) { "0" }.toIntOrNull() ?: 0
        val section = f.getOrElse(3) { "" }
        val title = f.getOrElse(4) { "" }
        val hint = f.getOrElse(5) { "" }
        val lesson = f.getOrElse(6) { "Kopi" }
        val lessonMode = f.getOrElse(7) { "GUIDED" }
        val wrong = f.getOrElse(8) { "0" } == "1"

        val scanning = mode == "scan"
        val done = mode == "done"
        sheet.visibility = if (scanning || done || paused) View.VISIBLE else View.GONE
        bottomCards.visibility = if (scanning || done || paused) View.GONE else View.VISIBLE
        topBar.visibility = if (scanning) View.GONE else View.VISIBLE

        if (paused) {
            sheetTitle.text = "Paused"
            sheetBody.text = "The timer is stopped and your progress is kept. Tap ▶ at the top right to carry on."
        } else if (scanning) {
            sheetTitle.text = "Scan the card"
            sheetBody.text = "Point your camera at the KopiTwin card and hold steady. The station appears on it."
        } else if (done) {
            sheetTitle.text = "$lesson served!"
            sheetBody.text = "You finished all $count steps. Nice work."
        }

        numberText.text = (if (done) count else step).toString()
        sectionText.text = if (done) "DONE" else section
        lessonText.text = lesson
        buildProgress(count, if (done) count else step - 1)

        hintText.text = if (wrong) "Not that one. $hint" else hint
        hintAccent.setBackgroundColor(if (wrong) maroon else arrowBlue)
        stepLabel.text = "STEP $step OF $count · $lessonMode"
        stepTitle.text = title
    }

    /** One thin segment per step: finished steps white, the rest dim (the prototype's segmented progress bar). */
    private fun buildProgress(count: Int, finished: Int) {
        if (builtSegments != count) {
            progressRow.removeAllViews()
            repeat(count) { i ->
                progressRow.addView(View(activity), LinearLayout.LayoutParams(0, LinearLayout.LayoutParams.MATCH_PARENT, 1f)
                    .apply { if (i > 0) leftMargin = dp(4) })
            }
            builtSegments = count
        }
        for (i in 0 until progressRow.childCount) {
            progressRow.getChildAt(i).background =
                rounded(if (i < finished) Color.WHITE else Color.argb(90, 255, 255, 255), 2f)
        }
    }

    private fun text(sizeSp: Float, color: Int, bold: Boolean) = TextView(activity).apply {
        textSize = sizeSp
        setTextColor(color)
        typeface = if (bold) Typeface.create(Typeface.SANS_SERIF, Typeface.BOLD) else Typeface.SANS_SERIF
    }

    private fun rounded(color: Int, radiusDp: Float) = GradientDrawable().apply {
        setColor(color)
        cornerRadius = radiusDp * activity.resources.displayMetrics.density
    }

    private fun matchWrap() = LinearLayout.LayoutParams(LinearLayout.LayoutParams.MATCH_PARENT,
        LinearLayout.LayoutParams.WRAP_CONTENT)

    private fun dp(v: Int) = (v * activity.resources.displayMetrics.density).toInt()
}
