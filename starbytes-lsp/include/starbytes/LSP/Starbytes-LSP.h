#include <iostream>
#include "JSONOutput.h"
#include "starbytes/Base/Base.h"
#include "LSPProtocol.h"

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
        enum class CompletionItemStyle:int {
            Detailed,Brief
        };
        struct LSPServerSettings {
            CompletionItemStyle completion_item_style;
        };

        struct StarbytesLSPServerTextDocument {
            LSPTextDocumentItem & content;
            StarbytesLSPServerTextDocument(LSPTextDocumentItem & _content):content(_content){};
        };
        class StarbytesLSPServer {
            public:
                LSPServerSettings & settings;
                StarbytesLSPServer(LSPServerSettings & sett):settings(sett){};
                ~StarbytesLSPServer(){};
                LSPQueue queue;
                void init();
                void getMessageFromStdin();
            private:
                void mainLoop();
                std::vector<StarbytesLSPServerTextDocument> files;
                Messenger json_transit;
                void addFile(StarbytesLSPServerTextDocument & file_ref);
                bool fileExists(LSPTextDocumentIdentifier & id_ref);
                LSPTextDocumentItem & getTextDocumentOfFile(LSPTextDocumentIdentifier & id_ref);
                void replaceFile(LSPTextDocumentIdentifier & id_ref,LSPTextDocumentItem & item_ref);
                bool deallocFile(LSPTextDocumentIdentifier & id_ref);

        };
    }

NAMESPACE_END

#endif
