#pragma once

#ifdef __linux__
// Color for Terminal
#define BLACK "\033[0;30m"
#define RED "\033[0;31m"
#define GREEN "\033[0;32m"
#define YELLOW "\033[0;33m"
#define BLUE "\033[0;34m"
#define PURPLE "\033[0;35m"
#define CYAN "\033[0;36m"
#define WHITE "\033[0;37m"

#else

#define BLACK ""
#define RED ""
#define GREEN ""
#define YELLOW ""
#define BLUE ""
#define PURPLE ""
#define CYAN ""
#define WHITE ""

#endif

#define P_ERROR "[" RED "ERROR" WHITE "] "
#define P_FATAL RED "[FATAL] "
#define P_DEBUG "[" YELLOW "DEBUG" WHITE "] "
#define P_INFO_INST "[" BLUE "INFO" WHITE "] "
#define P_INFO "[" CYAN "INFO" WHITE "] "
#define P_PPU "[" GREEN "PPU" WHITE "] "
#define P_TIMER "[" GREEN "PPU" WHITE "] "