#ifndef LOG_H
#define LOG_H

#include <stdio.h>
#include <stdlib.h>

// Toggle debug and verbose logging
#define DEBUG 1
#define VERBOSE 0

// Always-on INFO log (Green)
#define log_info(fmt, ...) \
    fprintf(stderr, "\033[1;32m[INFO] \033[0m" fmt "\n", ##__VA_ARGS__)

// Always-on ERROR log (Red)
#define log_err(fmt, ...) \
    fprintf(stderr, "\033[1;31m[ERROR] \033[0m" fmt "\n", ##__VA_ARGS__)

// DEBUG log (Blue), controlled by DEBUG flag
#if DEBUG
    #define log_msg(fmt, ...) \
        fprintf(stderr, "\033[1;34m[DEBUG] \033[0m" fmt "\n", ##__VA_ARGS__)

    #if VERBOSE
        #define log_msg_v(fmt, ...) \
            fprintf(stderr, "\033[1;36m[DEBUG-V] \033[0m" fmt "\n", ##__VA_ARGS__)
    #else
        #define log_msg_v(fmt, ...) \
            do {} while (0)
    #endif

#else
    #define log_msg(fmt, ...) \
        do {} while (0)

    #define log_msg_v(fmt, ...) \
        do {} while (0)
#endif

#endif // LOG_H
