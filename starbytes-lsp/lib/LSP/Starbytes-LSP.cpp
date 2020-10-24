#include "LSP/Starbytes-LSP.h"
#include "LSPProtocol.h"
#include "starbytes/Base/Base.h"
// #include "Parser/Lexer.h"
// #include "Parser/Parser.h"
#include <array>
#include <cctype>
#include <cstdlib>
#include <sstream>
#include <string>
#include <vector>

// #ifdef HAS_IO_H
// #include <io.h>
// #endif

// #ifdef HAS_UNISTD_H
// #include <unistd.h>
// #endif



using namespace Starbytes;
using namespace std;

using namespace LSP;

void LSPQueue::queueMessage(LSPServerMessage * msg){
    messages.push_back(msg);
};

LSPServerMessage * LSPQueue::getLatest(){
    LSPServerMessage * rc =  messages.back();
    messages.pop_back();
    return rc;
};

void StarbytesLSPServer::init(){
    getMessageFromStdin();
    LSPServerMessage *INITAL = queue.getLatest();
    if(INITAL->type == Request){
        LSPServerRequest *RQ = (LSPServerRequest *)INITAL;
        if(RQ->method == INITIALIZE){
            json_transit.sendIntializeMessage(RQ->id);
        }
    }
};

void StarbytesLSPServer::getMessageFromStdin(){
    LSP::LSPServerMessage *cntr = json_transit.read();
    queue.queueMessage(cntr);
};


