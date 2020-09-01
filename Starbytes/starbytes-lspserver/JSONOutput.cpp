#include "JSONOutput.h"
#include <iostream>
using namespace LSP;

using namespace std;

void Messager::reply(string methodToInvoke){
    cout << "Content-Length: ...\r\n\r\n{ \"jsonrpc\":\"2.0\",\n \"id\":\"2.0\"";
}