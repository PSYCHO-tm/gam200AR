package com.example.kopitwin

import android.content.res.AssetManager;
import android.opengl.GLSurfaceView;
import android.os.Bundle;
import android.util.Log;
import android.view.WindowManager;

import androidx.appcompat.app.AppCompatActivity
import javax.microedition.khronos.egl.EGLConfig
import javax.microedition.khronos.opengles.GL10
import com.example.kopitwin.databinding.ActivityMainBinding

class MainActivity : AppCompatActivity() {

    private lateinit var glView: GLSurfaceView

    private external fun nativeInit(): Boolean
    private external fun nativeResize(width: Int, height: Int)
    private external fun nativeRender()

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)

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
        setContentView(glView)
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