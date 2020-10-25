#include "starbytes/Base/CmdLine.h"
#include "starbytes/Base/Macros.h"
#include "starbytes/Base/ANSIColors.h"
#include <iostream>
#include <vector>

STARBYTES_FOUNDATION_NAMESPACE

#ifdef _WIN32
#include <Windows.h> 
#include <tchar.h>
#endif

#ifdef HAS_UNISTD_H
#include <unistd.h>
#endif

void execute_child_process(std::string name,std::string & args){
  #ifdef _WIN32
    STARTUPINFO si;
    PROCESS_INFORMATION pi;

    ZeroMemory( &si, sizeof(si) );
    si.cb = sizeof(si);
    ZeroMemory( &pi, sizeof(pi) );


    name.append(args);

    TCHAR * cmd = (TCHAR *)name.c_str();

    if(!CreateProcess(NULL,cmd,NULL,NULL,FALSE,0,NULL,NULL,&si,&pi)){
      std::cerr << ERROR_ANSI_ESC << "Failed to Invoke Process: " << name << "\nWindows Internal Error:" << GetLastError() << ANSI_ESC_RESET << std::endl;
      return;
    }
    WaitForSingleObject(pi.hProcess,INFINITE);
    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);
  #endif

  #ifdef HAS_UNISTD_H

  #endif
};

void parseCmdArgs(int &arg_count, char **&argv,
                  std::initializer_list<CommandOption *> options,
                  std::initializer_list<CommandInput *> inputs,
                  void (*help_opt_callback)()) {
  std::vector<std::string> flags;
  for (int i = 1; i < arg_count; ++i) {
    flags.push_back(std::string(argv[i]));
  }

  bool is_flag;

  for (int i = 0; i < flags.size(); ++i) {
    is_flag = false;
    if (flags[i] == "--help" || flags[i] == "-h") {
      is_flag = true;
      help_opt_callback();
    }

    for (auto &in : inputs) {
      std::string &flag = flags[i];
      if (flag == "--" + in->first_flag_match ||
          flag == "-" + in->second_flag_match) {
        is_flag = true;
        in->func_ptr(flags[++i]);
      }
    }
    for (auto &opt : options) {
      std::string &flag = flags[i];
      if (flag == "--" + opt->first_flag_match ||
          flag == "-" + opt->second_flag_match) {
        is_flag = true;
        opt->func_ptr();
      }
    }

    if (!is_flag) {
      std::cerr
          << "\x1b[31m\x1b[2mCommand Line Parse Error:\x1b[0m \n Unknown flag: "
          << flags[i] << "\n";
    }
  }
};

NAMESPACE_END