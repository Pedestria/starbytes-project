#include "Macros.h"

#ifndef BASE_ANSI_COLORS_H
#define BASE_ANSI_COLORS_H

STARBYTES_FOUNDATION_NAMESPACE

    #define ANSI_RED "31m"
    #define ANSI_GREEN "32m"
    #define ANSI_YELLOW "33m"
    #define ANSI_BLUE "34m"
    #define ANSI_PURPLE "35m"
    #define ANSI_CYAN "36m"

    #define ERROR_ANSI_ESC "\x1b[31m"
    #define ANSI_ESC_RESET "\x1b[0m"

NAMESPACE_END

#endif