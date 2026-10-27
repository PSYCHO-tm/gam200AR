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
#include <android/input.h>
#include <math.h>
#include <time.h>
#include "test.hpp"
#include "ARcore.hpp"
#include "Engine.hpp"
#include "Time.hpp"

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
    Core::Init();
    return JNI_TRUE;
}

extern "C" JNIEXPORT void JNICALL
Java_com_example_kopitwin_MainActivity_nativeResize(
        JNIEnv* /*env*/, jobject /*thiz*/, jint width, jint height)
{
    Core::Resize(width, height);
}

extern "C" JNIEXPORT void JNICALL
Java_com_example_kopitwin_NativeCore_update(JNIEnv*, jobject)
{
    Core::Update(Timer::Now());
    Core::Render();   // camera picture, then the equipment

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
Java_com_example_kopitwin_NativeCore_nativeGetTouch(JNIEnv* env, jobject)
{
    const std::string reply = TestCore::OnTouch();
    return env->NewStringUTF(reply.c_str());
}

extern "C" JNIEXPORT void JNICALL
Java_com_example_kopitwin_NativeCore_nativeSetInput(JNIEnv*, jobject, jfloat x, jfloat y, jint action)
{
    bool down;
    switch (action)
    {
        case AMOTION_EVENT_ACTION_DOWN:
        case AMOTION_EVENT_ACTION_MOVE:
            down = true;
            break;
        case AMOTION_EVENT_ACTION_UP:
            down = false;
            break;
        case AMOTION_EVENT_ACTION_CANCEL:
            Core::GetInput().CancelPointer();
            return;
        default:
            return; // ignore other actions, same as the old `when` did
    }

    Core::GetInput().SetPointer(x, y, down);
}

extern "C" JNIEXPORT void JNICALL
Java_com_example_kopitwin_NativeCore_nativeResetInput(JNIEnv*, jobject)
{
    Core::GetInput().Reset();
}

extern "C" JNIEXPORT void JNICALL
Java_com_example_kopitwin_NativeCore_nativeTogglePause(JNIEnv*, jobject, jboolean p)
{
    Core::PauseTime(static_cast<bool>(p));
}