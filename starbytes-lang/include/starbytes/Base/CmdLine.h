#include <initializer_list>
#include "Macros.h"
#include <string>

#ifndef BASE_CMDLINE_H
#define BASE_CMDLINE_H

STARBYTES_FOUNDATION_NAMESPACE

#define CMD_INPUT_FUNC void (*)(std::string);

struct CommandInput {
    std::string first_flag_match;
    std::string second_flag_match;
    void (*func_ptr)(std::string);
    CommandInput(std::string first,std::string second,void (*func)(std::string)):first_flag_match(first),second_flag_match(second),func_ptr(func){};
    ~CommandInput(){};
};

struct CommandOption {
    std::string first_flag_match;
    std::string second_flag_match;
    void (*func_ptr)();
    CommandOption(std::string first,std::string second,void (*func)()):func_ptr(func),first_flag_match(first),second_flag_match(second){};
    ~CommandOption(){};
};

void parseCmdArgs(int arg_count,char *argv[],std::initializer_list<CommandOption *> options,std::initializer_list<CommandInput *> inputs,void (*help_opt_callback)());

#ifdef _WIN32
#include <Windows.h>
#define WINDOWS_CONSOLE_INIT /
static HANDLE stdoutHandle; /
static DWORD outModeInit; /
 /
void setupConsole(void){ /
    DWORD outMode = 0; /
    stdoutHandle = GetStdHandle(STD_OUTPUT_HANDLE);
 /
    if(stdoutHandle == INVALID_HANDLE_VALUE) { /
		exit(GetLastError()); /
	} /
 /
    if(!GetConsoleMode(stdoutHandle, &outMode)) { /
	    exit(GetLastError()); /
	} /
 /
    outModeInit = outMode;  /
 /
    outMode |= ENABLE_VIRTUAL_TERMINAL_PROCESSING; /
 / 
    if(!SetConsoleMode(stdoutHandle, outMode)) { /
		exit(GetLastError()); /
	} /
} 
#endif

NAMESPACE_END

#endif