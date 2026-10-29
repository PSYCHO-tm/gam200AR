#pragma once
// LOGI / LOGE: printf-style logging that works on both platforms.
// Android -> Logcat. Desktop -> the console + file logger in log.hpp.
#include "log.hpp"

#if defined(__ANDROID__)
  #include <android/log.h>
  #define LOG_TAG "kopitwin"
  #define LOGI(...) __android_log_print(ANDROID_LOG_INFO,  LOG_TAG, __VA_ARGS__)
  #define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)
#else
  #include <cstdio>
  #define LOGI(...) do { char logBuf_[512]; std::snprintf(logBuf_, sizeof(logBuf_), __VA_ARGS__); LOG_INFO(logBuf_); } while (0)
  #define LOGE(...) do { char logBuf_[512]; std::snprintf(logBuf_, sizeof(logBuf_), __VA_ARGS__); LOG_ERROR(logBuf_); } while (0)
#endif
