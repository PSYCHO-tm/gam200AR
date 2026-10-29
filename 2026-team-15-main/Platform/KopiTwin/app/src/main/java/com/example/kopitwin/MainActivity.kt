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
            setBackgroundColor(Color.RED)
            setPadding(8, 8, 8, 8)
            visibility = View.GONE
        }
        val infoParams = FrameLayout.LayoutParams(
            FrameLayout.LayoutParams.WRAP_CONTENT,
            FrameLayout.LayoutParams.WRAP_CONTENT
        )
        //infoParams.leftMargin = 300
        infoParams.topMargin = 450

        statsText = TextView(this).apply {
            textSize = 16f
            typeface = android.graphics.Typeface.MONOSPACE   // keeps the columns aligned
            setTextColor(Color.WHITE)
            setBackgroundColor(Color.BLUE)           // semi-transparent black
            setPadding(8, 8, 8, 8)
        }
        val statsParams = FrameLayout.LayoutParams(
            FrameLayout.LayoutParams.WRAP_CONTENT,
            FrameLayout.LayoutParams.WRAP_CONTENT,
        )
        statsParams.topMargin = 100

        pauseButton = Button(this).apply {
            text = "PauseTime"
            setOnClickListener {
                arPaused = !arPaused
                text = if (arPaused) "Resume" else "Pause"
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

        val buttonParams = FrameLayout.LayoutParams(
            FrameLayout.LayoutParams.WRAP_CONTENT,
            FrameLayout.LayoutParams.WRAP_CONTENT
        ).apply {
            gravity = Gravity.BOTTOM or Gravity.END
            rightMargin = 32
            bottomMargin = 412
        }

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
        layout.addView(actionBar, actionParams)
        layout.addView(statsText, statsParams)
        layout.addView(infoText, infoParams)
        layout.addView(pauseButton, buttonParams)
        layout.addView(buildAudioPanel(), audioSet)

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
        pauseButton.text = "Pause"
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
