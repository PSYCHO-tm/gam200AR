package com.example.kopitwin

import android.annotation.SuppressLint
import android.graphics.Color
import android.opengl.GLSurfaceView
import android.os.Bundle
import android.util.Log
import android.view.View
import android.widget.FrameLayout
import android.widget.TextView
//import android.view.WindowManager
//import android.content.res.AssetManager
//import android.view.MotionEvent

import androidx.appcompat.app.AppCompatActivity
import javax.microedition.khronos.egl.EGLConfig
import javax.microedition.khronos.opengles.GL10
//import com.example.kopitwin.databinding.ActivityMainBinding

class MainActivity : AppCompatActivity() {

    private lateinit var glView: GLSurfaceView
    private lateinit var infoText: TextView

    private external fun nativeInit(): Boolean
    private external fun nativeResize(width: Int, height: Int)
    private external fun nativeRender()

    private external fun stringFromJNI() : String
    private external fun nativeTouch(action: Int, x: Float, y: Float): String

    @SuppressLint("ClickableViewAccessibility")
    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)

        infoText = TextView(this).apply {
            textSize = 32f
            setTextColor(Color.WHITE)
            setBackgroundColor(Color.RED)
            setPadding(8, 8, 8, 8)
            visibility = View.GONE
        }

        val textParams = FrameLayout.LayoutParams(
            FrameLayout.LayoutParams.WRAP_CONTENT,
            FrameLayout.LayoutParams.WRAP_CONTENT
        )
        textParams.leftMargin = 300
        textParams.topMargin = 1200

        glView = GLSurfaceView(this).apply {
            setEGLContextClientVersion(3)
            setRenderer(object : GLSurfaceView.Renderer {
                override fun onSurfaceCreated(gl: GL10?, config: EGLConfig?) {
                    nativeInit()
                }
                override fun onSurfaceChanged(gl: GL10?, width: Int, height: Int) {
                    nativeResize(width, height)
                }
                override fun onDrawFrame(gl: GL10?) {
                    nativeRender()
                }
            })
        }
        glView.setOnTouchListener { _, e ->
            infoText.visibility = View.VISIBLE
            infoText.x = e.x
            infoText.y = e.y
            infoText.text = nativeTouch(e.actionMasked, e.x, e.y)//reply
            Log.d("MyTag", stringFromJNI())
            true
        }

        /** Put OpenGL + TextView in the same layout */
        val layout = FrameLayout(this)
        layout.addView(glView)
        layout.addView(infoText, textParams)

        setContentView(layout)
    }

    override fun onPause() {
        super.onPause()
        glView.onPause()
    }

    override fun onResume() {
        super.onResume()
        glView.onResume()
    }

    companion object {
        init {
            System.loadLibrary("kopitwin")   // must match add_library(...) name
        }
    }
}