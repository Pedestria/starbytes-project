#include "ServerMain.h"

#include <iostream>


int main(int argc,char *argv[]){
  starbytes::lsp::ServerOptions opts {std::cin,std::cout};
  starbytes::lsp::Server server(opts);
  server.run();
  return 0;
}