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
import android.view.MotionEvent

//import com.example.kopitwin.databinding.ActivityMainBinding

class MainActivity : AppCompatActivity() {

    private var arPaused = false
    private lateinit var pauseButton: Button
    private var overlayTick = 0
    private lateinit var glView: GLSurfaceView
    private lateinit var infoText: TextView
    private lateinit var statsText: TextView

    private external fun nativeInit(): Boolean
    private external fun nativeResize(width: Int, height: Int)
    private external fun nativeArDisplay(width: Int, height: Int, rotation: Int)

    private external fun nativeArStart(): Boolean
    private external fun nativeArStop()
    private external fun nativeArDestroy()

    @Suppress("DEPRECATION")
    private fun displayRotation(): Int = windowManager.defaultDisplay.rotation

    private fun startArcore() {
        if (ContextCompat.checkSelfPermission(this, Manifest.permission.CAMERA) != PackageManager.PERMISSION_GRANTED) {
            ActivityCompat.requestPermissions(this, arrayOf(Manifest.permission.CAMERA), 1)
            return
        }
        nativeArStart()
    }

    @SuppressLint("ClickableViewAccessibility")
    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)

        infoText = TextView(this).apply {
            textSize = 20f
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
        infoParams.topMargin = 300

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
            text = "Pause"
            setOnClickListener {
                arPaused = !arPaused
                text = if (arPaused) "Resume" else "Pause"
                glView.queueEvent {
                    if (arPaused) nativeArStop() else nativeArStart()
                }
                NativeCore.nativeTogglePause(arPaused)
            }
        }

        val buttonParams = FrameLayout.LayoutParams(
            FrameLayout.LayoutParams.WRAP_CONTENT,
            FrameLayout.LayoutParams.WRAP_CONTENT
        ).apply {
            gravity = Gravity.BOTTOM or Gravity.END
            rightMargin = 48
            bottomMargin = 200
        }

        glView = GLSurfaceView(this).apply {
            setEGLContextClientVersion(3)
            setRenderer(object : GLSurfaceView.Renderer {
                override fun onSurfaceCreated(gl: GL10?, config: EGLConfig?) {
                    nativeInit()
                }
                override fun onSurfaceChanged(gl: GL10?, width: Int, height: Int) {
                    nativeResize(width, height)
                    nativeArDisplay(width, height, displayRotation())
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
        layout.addView(statsText, statsParams)
        layout.addView(infoText, infoParams)
        layout.addView(pauseButton, buttonParams)


        setContentView(layout)
    }

    override fun onPause() {
        nativeArStop()
        glView.onPause()
        glView.queueEvent {
            NativeCore.nativeTogglePause(arPaused)
            NativeCore.nativeResetInput()
        }
        super.onPause()
    }

    override fun onResume() {
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
    companion object {
        init {
            System.loadLibrary("kopitwin")   // must match add_library(...) name
        }
    }
}