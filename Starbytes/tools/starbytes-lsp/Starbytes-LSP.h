#include <iostream>
#include "JSONOutput.h"
#include "starbytes/Base/Base.h"

#ifndef STARBYTES_LSP_STARBYTES_LSP_H
#define STARBYTES_LSP_STARBYTES_LSP_H

STARBYTES_STD_NAMESPACE
   namespace LSP {

        class LSPQueue {
            private:
                std::vector<LSPServerMessage *> messages;
            public:
                LSPQueue(){};
                ~LSPQueue(){};
                void queueMessage(LSPServerMessage * msg);
                LSPServerMessage * getLatest();
        };
        class StarbytesLSPServer {
            public:
                StarbytesLSPServer();
                ~StarbytesLSPServer();
                LSPQueue queue;
                void init();
                void getMessageFromStdin();
            private:
                Messenger json_transit;
        };
    }

NAMESPACE_END

#endif
