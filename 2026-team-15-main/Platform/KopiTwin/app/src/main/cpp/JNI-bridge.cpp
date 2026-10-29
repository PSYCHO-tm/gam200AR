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

#define SDL_MAIN_HANDLED
#include <SDL3/SDL_main.h>
#include <SDL3/SDL_scancode.h>
#include <jni.h>
#include <android/log.h>
#include <android/input.h>
#include <math.h>
#include <time.h>
#include "test.hpp"
#include "ARcore.hpp"
#include "Engine.hpp"
#include "Time.hpp"
#include "EmbeddedAssets.hpp"
#include "DefinitionLoader.hpp"

#define LOGI(...) __android_log_print(ANDROID_LOG_INFO,  LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)

// Gives the ECS the text of an embedded file, looked up by its repo-relative path.
std::optional<std::string> FindEmbeddedText(std::string const& path)
{
    for (int i = 0; i < gEmbeddedAssetCount; ++i)
    {
        if (path == gEmbeddedAssets[i].path)
        {
            return std::string(gEmbeddedAssets[i].text);
        }
    }
    LOGE("Embedded asset not found: %s", path.c_str());
    return std::nullopt;
}


// ---------------------------------------------------------------------------
// JNI entry points
// ---------------------------------------------------------------------------

extern "C" JNIEXPORT jboolean JNICALL
Java_com_example_kopitwin_MainActivity_nativeArCreate(JNIEnv* env, jobject activity)
{
    if(!ArInitialise(env, activity)) { return JNI_FALSE; }
    return ArSessionCreate(env, activity) ? JNI_TRUE : JNI_FALSE;   // created, but not started
}

extern "C" JNIEXPORT void JNICALL
Java_com_example_kopitwin_MainActivity_nativeArResume(JNIEnv* /*env*/, jobject /*thiz*/)
{
    ArSessionResume();
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
    SDL_SetMainReady();
    Ecs::SetTextReader(FindEmbeddedText);
    Core::Init();
    LOGI("audio ready=%d err=%s", Core::audio.IsReady(), Core::audio.GetLastError().c_str());
    return JNI_TRUE;
}

extern "C" JNIEXPORT jboolean JNICALL
Java_com_example_kopitwin_MainActivity_nativeLateInit(JNIEnv* /*env*/, jobject /*thiz*/)
{
    static bool isLateInit = false;
    if (isLateInit) return JNI_FALSE;

    Core::Start();
    isLateInit = true;
    return JNI_TRUE;
}

extern "C" JNIEXPORT void JNICALL
Java_com_example_kopitwin_MainActivity_nativeResize(
        JNIEnv* /*env*/, jobject /*thiz*/, jint width, jint height, jint rotation)
{
    Core::Resize(width, height, rotation);
}

extern "C" JNIEXPORT void JNICALL
Java_com_example_kopitwin_NativeCore_update(JNIEnv*, jobject)
{
    Core::Update();

    static bool logged = false;
    if (!logged && Core::frameCount > 5)
    {
        logged = true;
        LOGI("manifest sfx=%zu music=%zu err=%s",
             Core::audio.GetManifestSfx().size(), Core::audio.GetManifestMusic().size(),
             Core::audio.GetManifestError().c_str());
    }
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
    char buf[768];
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

extern "C" JNIEXPORT jstring JNICALL
Java_com_example_kopitwin_NativeCore_nativeGetActions(JNIEnv* env, jobject)
{
    char buf[512];
    buf[0] = '\0';
    Core::GetActionsText(buf, sizeof(buf));
    return env->NewStringUTF(buf);
}

extern "C" JNIEXPORT void JNICALL
Java_com_example_kopitwin_NativeCore_nativeDoAction(JNIEnv* env, jobject, jstring actionId)
{
char const* id = env->GetStringUTFChars(actionId, nullptr);
Core::DoAction(id);
env->ReleaseStringUTFChars(actionId, id);
}

extern "C" JNIEXPORT void JNICALL
Java_com_example_kopitwin_NativeCore_nativeTogglePause(JNIEnv*, jobject, jboolean p)
{
    Core::PauseTime(static_cast<bool>(p));
}

extern "C" JNIEXPORT void JNICALL
Java_com_example_kopitwin_NativeCore_nativeAudioKey(JNIEnv*, jobject, jint id)
{
    static constexpr SDL_Scancode kMap[] = {
            SDL_SCANCODE_1, SDL_SCANCODE_2, SDL_SCANCODE_3, SDL_SCANCODE_4,
            SDL_SCANCODE_5, SDL_SCANCODE_6, SDL_SCANCODE_7, SDL_SCANCODE_8, // 0-7  sfx
            SDL_SCANCODE_9,      // 8  music on/off
            SDL_SCANCODE_P,      // 9  pause/resume music
            SDL_SCANCODE_0,      // 10 stop all sfx
            SDL_SCANCODE_EQUALS, // 11 volume up
            SDL_SCANCODE_MINUS   // 12 volume down
    };
    if (id < 0 || id >= static_cast<jint>(sizeof(kMap) / sizeof(kMap[0]))) return;
    const KeyId k = static_cast<KeyId>(kMap[id]);
    Core::GetInput().SetKey(k, true);
    Core::GetInput().SetKey(k, false); // Pressed stays latched until the next Poll
}

extern "C" JNIEXPORT void JNICALL
Java_com_example_kopitwin_NativeCore_nativeAudioPause(JNIEnv*, jobject)  { Core::audio.PauseAll(); }

extern "C" JNIEXPORT void JNICALL
Java_com_example_kopitwin_NativeCore_nativeAudioResume(JNIEnv*, jobject) { Core::audio.ResumeAll(); }