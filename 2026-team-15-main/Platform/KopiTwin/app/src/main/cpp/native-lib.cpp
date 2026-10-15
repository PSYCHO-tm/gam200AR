/*#include <jni.h>
#include "test.hpp"

// File for testing JNI functions.
extern "C" JNIEXPORT jstring JNICALL
Java_com_example_kopitwin_MainActivity_stringFromJNI(
        JNIEnv* env,
        jobject) {
    return env->NewStringUTF(TestCore::HelloWorld().c_str());
}

extern "C" JNIEXPORT jstring JNICALL
Java_com_example_kopitwin_MainActivity_nativeTouch(
        JNIEnv* env, jobject, jint action, jfloat x, jfloat y)
{
    const std::string reply = TestCore::OnTouch(action, x, y);
    return env->NewStringUTF(reply.c_str());
}*/