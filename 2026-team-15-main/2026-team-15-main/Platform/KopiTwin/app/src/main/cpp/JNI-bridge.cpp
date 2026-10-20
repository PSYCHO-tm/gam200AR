/**
 * JNI-bridge.cpp
 *
 * JNI bridge between the Kotlin/Android layer and the C++ core.
 * Self-contained - no Scene/ framework headers are used here.
 *
 * Native method signatures match MainActivity.kt:
 *   package com.example.kopitwin
 *   class   MainActivity
 */

#define LOG_TAG "kopitwin"

#include <jni.h>
#include <android/log.h>
#include "test.hpp"
#include "ARcore.hpp"
#include <GLES3/gl3.h>
#include <math.h>
#include <time.h>
#include "Engine.hpp"
#include "Time/Time.hpp"

#define LOGI(...) __android_log_print(ANDROID_LOG_INFO,  LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)

// ---------------------------------------------------------------------------
// JNI entry points
// ---------------------------------------------------------------------------

extern "C" JNIEXPORT void JNICALL
Java_com_example_kopitwin_MainActivity_nativeArDisplay(
JNIEnv* /*env*/, jobject /*thiz*/, jint width, jint height, jint rotation)
{
    ArCameraResize(width, height, rotation);
}

extern "C" JNIEXPORT jboolean JNICALL
Java_com_example_kopitwin_MainActivity_nativeArStart(JNIEnv* env, jobject activity)
{
    if(!ArInitialise(env, activity)) { return JNI_FALSE; }
    if(!ArSessionCreate(env, activity)) { return JNI_FALSE; }
    return ArSessionResume() ? JNI_TRUE : JNI_FALSE;
}

extern "C" JNIEXPORT void JNICALL
Java_com_example_kopitwin_MainActivity_nativeArStop(JNIEnv* /*env*/, jobject /*thiz*/)
{
    ArSessionPause();
    Core::PauseTime();
}

extern "C" JNIEXPORT void JNICALL
Java_com_example_kopitwin_MainActivity_nativeArDestroy(JNIEnv* /*env*/, jobject /*thiz*/)
{
    ArSessionDestroy();
}

extern "C" JNIEXPORT jboolean JNICALL
Java_com_example_kopitwin_MainActivity_nativeInit(JNIEnv* /*env*/, jobject /*thiz*/)
{
    LOGI("nativeInit called");
    ArCameraInit();
    Core::Init();
    return JNI_TRUE;
}

extern "C" JNIEXPORT void JNICALL
Java_com_example_kopitwin_MainActivity_nativeResize(
        JNIEnv* /*env*/, jobject /*thiz*/, jint width, jint height)
{
    glViewport(0, 0, width, height);
}

extern "C" JNIEXPORT void JNICALL
Java_com_example_kopitwin_MainActivity_nativeRender(JNIEnv* /*env*/, jobject /*thiz*/)
{
    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);
    ArCameraDraw();
}

extern "C" JNIEXPORT void JNICALL
Java_com_example_kopitwin_NativeCore_frame(JNIEnv*, jobject)
{
    Core::Frame(Timer::Now());

    // Test logging lives in the shell so the core stays loop/platform-free.
    if (!(Core::frameCount % 60))
    {
        LOGI("frame %u, dt = %.2f ms, fps = %.1f",
             Core::frameCount, Core::dt * 1000.0f, Core::frameRate);
    }
}

extern "C" JNIEXPORT jstring JNICALL
Java_com_example_kopitwin_NativeCore_nativeOverlayText(JNIEnv* env, jobject)
{
    char buf[512];
    Core::GetOverlayText(buf, sizeof(buf));
    return env->NewStringUTF(buf);
}

extern "C" JNIEXPORT void JNICALL
Java_com_example_kopitwin_NativeCore_onResume(JNIEnv*, jobject)
{
    ArSessionResume();
    Core::OnResume();
}

extern "C" JNIEXPORT jstring JNICALL
Java_com_example_kopitwin_NativeCore_nativeTouch(JNIEnv* env, jobject, jint action, jfloat x, jfloat y)
{
    const std::string reply = TestCore::OnTouch(action, x, y);
    return env->NewStringUTF(reply.c_str());
}