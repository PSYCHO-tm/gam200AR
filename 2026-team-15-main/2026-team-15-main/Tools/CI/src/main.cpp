#include "log.h"

#include <cstring>

int main(int argc, char** argv)
{
    LOG_INFO("KopiTwin starting");
    LOG_DEBUG("debug message");
    LOG_WARN("warning message");
    LOG_ERROR("error message");

    // Run with --leak to prove the leak detector works.
    if (argc > 1 && std::strcmp(argv[1], "--leak") == 0) {
        LOG_WARN("leaking 64 bytes on purpose");
        int* leak = new int[16];
        leak[0] = 1;
        leak = nullptr;          // last pointer dropped, memory never freed
        LOG_INFO("KopiTwin exits with 64 bytes leaked");
        return 0;
    }

    LOG_INFO("KopiTwin exits cleanly");
    return 0;
}
