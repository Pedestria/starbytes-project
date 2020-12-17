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

#ifdef HAS_SYS_WAIT_H
#include <sys/wait.h>
#endif

void execute_child_process(std::string name,std::string & args,bool inPlace,char * const * argv_unix){
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
    if(inPlace){
      int exec_id = execvp(name.c_str(),argv_unix);
      if(wait(&exec_id)){
        return;
      }
    }
    // pid_t child_process_id = fork();
    // switch(child_process_id){
    //   case -1:
    //     std::cerr << ERROR_ANSI_ESC << "Failed to Create Unix Fork";
    //   case 0: {
    //     int exec_id = execv(name.c_str(),argv_unix);
    //     if(!exec_id){
    //       std::cerr << ERROR_ANSI_ESC << "Failed to Invoke Process:" << name << ANSI_ESC_RESET;
    //     }

    //     if(wait(&exec_id) == child_process_id){
    //       std::cerr << "Child Process:" << name << "Timed out!";
    //     };

    //   };
    // };

  #endif
};
using namespace CommandLine;

void formatHelp(std::initializer_list<CommandOption *> & opts_ref,std::initializer_list<CommandInput *> & inputs_ref,std::string & sum_ref){
  std::cout << sum_ref << "\n" << "\x1b[2mFlags:\x1b[0m\n\n--help, -h\tDisplay help info.\n";
  if(opts_ref.size() > 0) {
    std::cout << "Options:\n";
    for(auto & opt : opts_ref){
      std::cout << "--" << opt->first_flag_match << ", -" << opt->second_flag_match << "\t" << opt->desc.val << "\n";
    };
  }
  if(inputs_ref.size() > 0){
    std::cout << "Inputs:\n";
    for(auto & ipt : inputs_ref){
      std::cout << "--" << ipt->first_flag_match << ", -" << ipt->second_flag_match << "\t" << ipt->desc.val << "\n";
    };
  };
};

void CommandLine::parseCmdArgs(int &arg_count, char **&argv,
                  std::initializer_list<CommandOption *> options,
                  std::initializer_list<CommandInput *> inputs,
                  std::string summary) {
  std::vector<std::string> flags;
  for (int i = 1; i < arg_count; ++i) {
    flags.push_back(argv[i]);
  }

  bool is_flag;

  for (int i = 0; i < flags.size(); ++i) {
    is_flag = false;
    if (flags[i] == "--help" || flags[i] == "-h") {
      is_flag = true;
      formatHelp(options,inputs,summary);
      exit(1);
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
          << "\x1b[31m\x1b[2mCommand Line Parse Error:\x1b[0m\nUnknown flag: "
          << flags[i] << "\n";
    }
  }
};

NAMESPACE_END
