#pragma once
#include <iostream>

/*//////////////////////////////////////////////
Brief:
Variadic log macros, message prints at run time
*///////////////////////////////////////////////

// LOG prints one line, e.g. [INFO ] main.cpp:11  message
// '...' is pasted into a << chain, so you can write: LOG_INFO("n = " << n);
// Variadic, so commas inside the message (e.g. template arguments) are fine.
#define LOG(tag, ...) \
    std::cout << tag << " "<< __FILE_NAME__ << ":" << __LINE__ << " " << __VA_ARGS__ << "\n"

#define LOG_INFO(...)  LOG("INFO ", __VA_ARGS__)
#define LOG_DEBUG(...) LOG("DEBUG", __VA_ARGS__)
#define LOG_WARN(...)  LOG("WARN ", __VA_ARGS__)
#define LOG_ERROR(...) LOG("ERROR", __VA_ARGS__)