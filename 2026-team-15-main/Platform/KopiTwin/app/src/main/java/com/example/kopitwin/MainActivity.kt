package com.example.kopitwin

import android.annotation.SuppressLint
import android.graphics.Color
import android.opengl.GLSurfaceView
import android.os.Bundle
//import android.util.Log
import android.view.View
import android.widget.FrameLayout
import android.widget.TextView
import android.Manifest
import android.content.pm.PackageManager
import androidx.core.app.ActivityCompat
import androidx.core.content.ContextCompat

import androidx.appcompat.app.AppCompatActivity
import javax.microedition.khronos.egl.EGLConfig
import javax.microedition.khronos.opengles.GL10
import android.widget.Button // For Buttons (temporary)
import android.view.Gravity  // For Object alignment and order (temporary)
import android.view.KeyEvent
import android.widget.LinearLayout
import android.view.MotionEvent

//import com.example.kopitwin.databinding.ActivityMainBinding

class MainActivity : AppCompatActivity() {

    private var arPaused = false
    private lateinit var pauseButton: Button
    private lateinit var lessonHud: LessonHud   // sandbox: Mabel's guided-lesson screen
    private var overlayTick = 0
    private lateinit var glView: GLSurfaceView
    private lateinit var infoText: TextView
    private lateinit var statsText: TextView
    private lateinit var actionBar: LinearLayout
    private var lastActions = ""
    private external fun nativeInit(): Boolean
    private external fun nativeResize(width: Int, height: Int, rotation: Int)
    private external fun nativeArCreate(): Boolean
    private external fun nativeArResume()
    private external fun nativeArStop()

    private external fun nativeArDestroy()
    private external fun nativeLoadMarkerConfig(): String
    private external fun nativeSetMarkerImage(grayscale: ByteArray, width: Int, height: Int)

    private var markerSent = false   // C++ keeps its copy until the session is configured: decode once, not on every resume

    // Goal 5: the printed card that places the station. Which picture, its printed width and tilt mode are in
    // Assets/Json/AR.json (read by C++). This only decodes the picture: ARCore (C++) does all the tracking.
    // No card = place on a table as before.
    private fun loadMarkerImage() {
        if (markerSent) return
        markerSent = true
        val imagePath = nativeLoadMarkerConfig()
        if (imagePath.isEmpty()) return
        val markerAsset = imagePath.removePrefix("Assets/")   // the repo's Assets folder is the APK's assets root
        val options = android.graphics.BitmapFactory.Options().apply { inScaled = false }
        val bitmap = try {
            assets.open(markerAsset).use { android.graphics.BitmapFactory.decodeStream(it, null, options) }
        } catch (e: java.io.IOException) {
            android.util.Log.e("kopitwin", "Marker card: cannot open $markerAsset ($e)")
            null
        }
        if (bitmap == null) {
            android.util.Log.e("kopitwin", "Marker card: $markerAsset did not decode, placing on tables only")
            return
        }
        android.util.Log.i("kopitwin", "Marker card: decoded ${bitmap.width}x${bitmap.height}, sending to ARCore")
        val pixels = IntArray(bitmap.width * bitmap.height)
        bitmap.getPixels(pixels, 0, bitmap.width, 0, 0, bitmap.width, bitmap.height)
        val gray = ByteArray(pixels.size) { i ->
            val p = pixels[i]
            ((Color.red(p) * 299 + Color.green(p) * 587 + Color.blue(p) * 114) / 1000).toByte()
        }
        nativeSetMarkerImage(gray, bitmap.width, bitmap.height)
        bitmap.recycle()
    }

    private fun audioKey(id: Int) = glView.queueEvent { NativeCore.nativeAudioKey(id) }

    private fun buildAudioPanel(): LinearLayout {
        val labels = (1..8).map { "S$it" } + listOf("Music", "Pause Music", "StopFX")
        val panel = LinearLayout(this).apply { orientation = LinearLayout.VERTICAL }
        labels.chunked(4).forEachIndexed { row, names ->
            val line = LinearLayout(this).apply { orientation = LinearLayout.HORIZONTAL }
            names.forEachIndexed { col, name ->
                line.addView(Button(this).apply {
                    text = name
                    setOnClickListener { audioKey(row * 4 + col) } // ids 0-10 match the JNI map
                })
            }
            panel.addView(line)
        }
        return panel
    }

    @Suppress("DEPRECATION")
    private fun displayRotation(): Int = windowManager.defaultDisplay.rotation

    private fun startArcore(){
        if (ContextCompat.checkSelfPermission(this, Manifest.permission.CAMERA) != PackageManager.PERMISSION_GRANTED) {
            ActivityCompat.requestPermissions(this, arrayOf(Manifest.permission.CAMERA), 1)
            return
        }
        loadMarkerImage()   // before nativeArCreate: the card is added when the session is configured
        nativeArCreate()
    }

    private fun showActions(actions: String) {
        actionBar.removeAllViews()
        for (line in actions.lines()) {
            val parts = line.split('\t')
            if (parts.size < 2) continue
            val id = parts[0]
            actionBar.addView(Button(this).apply {
                text = parts[1]
                setOnClickListener { glView.queueEvent { NativeCore.nativeDoAction(id) } }   // runs on the GL thread
            })
        }
    }

    @SuppressLint("ClickableViewAccessibility")
    override fun onCreate(savedInstanceState: Bundle?) {
        System.loadLibrary("SDL3")
        org.libsdl.app.SDL.setupJNI()
        org.libsdl.app.SDL.initialize()
        org.libsdl.app.SDL.setContext(this)

        super.onCreate(savedInstanceState)

        infoText = TextView(this).apply {
            textSize = 16f
            typeface = android.graphics.Typeface.MONOSPACE   // keeps the columns aligned
            setTextColor(Color.WHITE)
            setBackgroundColor(Color.argb(90, 255, 0, 0))   // see-through red, like the stats box
            setPadding(8, 8, 8, 8)
            visibility = View.GONE
        }

        statsText = TextView(this).apply {
            textSize = 16f
            typeface = android.graphics.Typeface.MONOSPACE   // keeps the columns aligned
            setTextColor(Color.WHITE)
            setBackgroundColor(Color.argb(90, 0, 0, 255))   // see-through blue (alpha 90 of 255): the camera shows behind it
            setPadding(8, 8, 8, 8)
        }
        val statsParams = FrameLayout.LayoutParams(
            FrameLayout.LayoutParams.WRAP_CONTENT,
            FrameLayout.LayoutParams.WRAP_CONTENT,
        )
        statsParams.topMargin = 100

        pauseButton = Button(this).apply {
            text = "II"
            setOnClickListener {
                arPaused = !arPaused
                text = if (arPaused) "▶" else "II"
                lessonHud.setPaused(arPaused)
                glView.queueEvent {
                    if (arPaused) nativeArStop() else nativeArResume()
                }
                NativeCore.nativeTogglePause(arPaused)
            }
        }

        actionBar = LinearLayout(this).apply { orientation = LinearLayout.HORIZONTAL; gravity = Gravity.CENTER }
        val actionParams = FrameLayout.LayoutParams(
            FrameLayout.LayoutParams.WRAP_CONTENT, FrameLayout.LayoutParams.WRAP_CONTENT
        ).apply { gravity = Gravity.BOTTOM or Gravity.CENTER_HORIZONTAL; bottomMargin = 150 }


        val audioSet = FrameLayout.LayoutParams(
            FrameLayout.LayoutParams.WRAP_CONTENT,
            FrameLayout.LayoutParams.WRAP_CONTENT,
            Gravity.TOP or Gravity.END
        ).apply {
            topMargin = 1500
            rightMargin = 100
        }

        glView = GLSurfaceView(this).apply {
            setEGLContextClientVersion(3)
            setRenderer(object : GLSurfaceView.Renderer {
                override fun onSurfaceCreated(gl: GL10?, config: EGLConfig?) {
                    nativeInit()
                }
                override fun onSurfaceChanged(gl: GL10?, width: Int, height: Int) {
                    nativeResize(width, height, displayRotation())
                }
                override fun onDrawFrame(gl: GL10?) {
                    NativeCore.update()                                // update + draw, all in C++

                    if (++overlayTick % 15 == 0) {                    // about 4 times per second at 60 fps
                        val text = NativeCore.nativeOverlayText()     // on the GL thread
                        this@MainActivity.runOnUiThread { statsText.text = text }
                    }

                    if (overlayTick % 4 == 0) {                       // ~15 Hz touch readout
                        val touch = NativeCore.nativeGetTouch()       // GL thread, same as the input code
                        this@MainActivity.runOnUiThread { infoText.text = touch }
                    }

                    if (overlayTick % 6 == 0) {                       // sandbox: Mabel's lesson HUD, ~5 Hz
                        val lesson = NativeCore.nativeLessonState()   // GL thread
                        this@MainActivity.runOnUiThread { lessonHud.update(lesson) }
                    }

                    if (overlayTick % 6 == 0) {
                        val actions = NativeCore.nativeGetActions()          // GL thread
                        if (actions != lastActions) {                        // rebuild buttons only when the list changes
                            lastActions = actions
                            this@MainActivity.runOnUiThread { showActions(actions) }
                        }
                    }
                }
            })
        }

        glView.setOnTouchListener { _, e ->
            infoText.visibility = View.VISIBLE
            val x = e.x
            val y = e.y
            val action = e.actionMasked   // copied now: Android reuses the event after this returns
            glView.queueEvent { NativeCore.nativeSetInput(x, y, action) }
            true
        }

        val layout = FrameLayout(this)
        layout.addView(glView)

        // Sandbox: the developer views (stats, touch readout, audio test buttons, action buttons) live in one layer,
        // hidden by default so the screen looks like Mabel's design. The small DBG button shows them again.
        val debugLayer = FrameLayout(this).apply { visibility = View.GONE }
        debugLayer.addView(actionBar, actionParams)
        // The blue stats box with the red touch box right under it: stacked, so they never overlap whatever the text length.
        val debugColumn = LinearLayout(this).apply { orientation = LinearLayout.VERTICAL }
        debugColumn.addView(statsText)
        debugColumn.addView(infoText)
        debugLayer.addView(debugColumn, statsParams)
        debugLayer.addView(buildAudioPanel(), audioSet)
        layout.addView(debugLayer, FrameLayout.LayoutParams(FrameLayout.LayoutParams.MATCH_PARENT,
            FrameLayout.LayoutParams.MATCH_PARENT))

        lessonHud = LessonHud(this)
        layout.addView(lessonHud.root, FrameLayout.LayoutParams(FrameLayout.LayoutParams.MATCH_PARENT,
            FrameLayout.LayoutParams.MATCH_PARENT))
        lessonHud.attachPause(pauseButton)   // the prototype's square pause button, top right

        val debugToggle = Button(this).apply {
            text = "DBG"
            textSize = 10f
            alpha = 0.5f
            setOnClickListener { debugLayer.visibility = if (debugLayer.visibility == View.VISIBLE) View.GONE else View.VISIBLE }
        }
        layout.addView(debugToggle, FrameLayout.LayoutParams(FrameLayout.LayoutParams.WRAP_CONTENT,
            FrameLayout.LayoutParams.WRAP_CONTENT, Gravity.START or Gravity.CENTER_VERTICAL))

        setContentView(layout)
    }

    override fun onPause() {
        NativeCore.nativeAudioPause()
        glView.queueEvent { nativeArStop() }   // runs on the GL thread before it pauses
        glView.onPause()
        glView.queueEvent {
            NativeCore.nativeTogglePause(arPaused)
            NativeCore.nativeResetInput()
        }
        super.onPause()
    }

    override fun onResume() {
        NativeCore.nativeAudioResume()
        startArcore()
        arPaused = false
        pauseButton.text = "II"
        if (::lessonHud.isInitialized) lessonHud.setPaused(false)
        glView.onResume()
        glView.queueEvent {
            NativeCore.nativeTogglePause(arPaused)
            NativeCore.onResume()
        }
        super.onResume()
    }

    override fun onDestroy(){
        nativeArDestroy()
        super.onDestroy()
    }

    override fun onKeyDown(keyCode: Int, event: KeyEvent): Boolean = when (keyCode) {
        KeyEvent.KEYCODE_VOLUME_UP   -> { audioKey(11); true }
        KeyEvent.KEYCODE_VOLUME_DOWN -> { audioKey(12); true }
        else -> super.onKeyDown(keyCode, event)
    }

    companion object {
        init {
            System.loadLibrary("kopitwin")   // must match add_library(...) name
        }
    }
}
