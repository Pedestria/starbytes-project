#pragma once
/*
    Language Server Protocol Defintions
    (Does Not Include Language Features!)
*/
#include <string>
#include <vector>
#include <map>
#include "JSONOutput.h"

namespace Starbytes {
    namespace LSP {
        typedef std::string LSPDocumentURI;
        std::vector<std::string> LSPEOLChars = {"\r\n","\n","\r"};

        struct LSPPosition : LSPServerObject {
            int line;
            int character;
        };

        struct LSPRange : LSPServerObject {
            LSPPosition start;
            LSPPosition end;
        };

        struct LSPLocation : LSPServerObject {
            LSPDocumentURI uri;
            LSPRange range;
        };

        struct LSPLocationLink : LSPServerObject {
            LSPRange origin_selection_range;
            LSPDocumentURI target_uri;
            LSPRange target_range;
            LSPRange target_selection_range;
        };
        enum class LSPDiagonisticLevel:int {
            Error = 1,Warning = 2,Information = 3,Hint = 4
        };

        enum class LSPDiagonisticTag:int {
            Unnecessary = 1,Deprecated = 2
        };
        struct LSPDiagonisticRelatedInfo : LSPServerObject {
            LSPLocation location;
            std::string message;
        };

        struct LSPDiagnostic : LSPServerObject {
            LSPRange range;
            LSPDiagonisticLevel serverity;
            int code;
            std::string source;
            std::string message;
            std::vector<LSPDiagonisticTag> tags;
            LSPDiagonisticRelatedInfo related_information;
        };

        struct LSPCommand : LSPServerObject {
            std::string title;
            std::string command;
            std::vector<std::string> arguments;
        };

        struct LSPTextEdit : LSPServerObject {
            LSPRange range;
            std::string new_text;
        };

        enum class LSPDocumentChangeType:int {
            CreateFile,RenameFile,DeleteFile,TextDocumentEdit
        };

        struct LSPDocumentChange : LSPServerObject{
            LSPDocumentChangeType change_type;
        };

        enum class LSPTextDocumentIDType:int {
            Regular,Versioned
        };

        struct LSPTextDocumentIdentifier : LSPServerObject{
            LSPTextDocumentIDType id_type;
            LSPDocumentURI uri;
        };

        struct LSPTextDocumentItem : LSPServerObject {
            LSPDocumentURI uri;
            std::string language_id;
            int version;
            std::string text;
        };

        struct LSPVersionedTextDocumentIdentfier : LSPTextDocumentIdentifier{
            bool hasVersion;
            int version;
        };

        struct LSPTextDocumentPositionParams : LSPServerObject{
            LSPTextDocumentIdentifier text_document;
            LSPPosition position;
        };

        struct LSPDocumentFilter : LSPServerObject {
            std::string language;
            std::string scheme;
            std::string pattern;
        };

        typedef std::vector<LSPDocumentFilter> LSPDocumentSelector;

        struct LSPStaticRegistrationOptions : LSPServerObject {
            std::string id;
        };
        struct LSPTextDocumentRegistrationOptions : LSPServerObject {
            bool hasDocumentSelector;
            LSPDocumentSelector document_selector;
        };

        struct LSPTextDocumentEdit : LSPDocumentChange {
            LSPVersionedTextDocumentIdentfier text_document;
            std::vector<LSPTextEdit> edits;
        };

        struct LSPCreateFileOptions : LSPServerObject {
            bool overwrite;
            bool ignore_if_exists;
        };

        struct LSPCreateFile : LSPDocumentChange {
            std::string kind = "create";
            LSPDocumentURI uri;
            bool hasOptions;
            LSPCreateFileOptions options;
        };

        struct LSPRenameFileOptions : LSPServerObject {
            bool overwrite;
            bool ignore_if_exists;
        };

        struct LSPRenameFile : LSPDocumentChange {
            std::string kind = "rename";
            LSPDocumentURI old_uri;
            LSPDocumentURI new_uri;
            bool hasOptions;
            LSPRenameFileOptions options;
        };

        struct LSPDeleteFileOptions : LSPServerObject {
            bool recursive;
            bool ignore_if_not_exists;
        };
        struct LSPDeleteFile : LSPDocumentChange {
            std::string kind = "delete";
            LSPDocumentURI uri;
            bool hasOptions;
            LSPDeleteFileOptions options;
        };

        struct LSPWorkspaceEdit : LSPServerObject {
            bool hasChanges;
            std::map<LSPDocumentURI,std::vector<LSPTextEdit>> changes;
            bool hasDocumentChanges;
            std::vector<LSPDocumentChange> document_changes;
        };

        enum class LSPResourceOperationKind:int {
            Create,Rename,Delete
        };

        enum class LSPFailureHandlingKind:int {
            Abort,Transactional,Undo,TextOnlyTransactional
        };

        struct LSPWorkspaceEditClientCapabilities : LSPServerObject {
            bool hasDocumentChanges;
            bool hasResourceOperations;
            std::vector<LSPResourceOperationKind> resource_operations;
            bool hasFailureHandling;
            LSPFailureHandlingKind failure_handling;

        };

        enum class LSPMarkupKind:int {
            PlainText,Markdown
        };

        struct LSPMarkupContent : LSPServerObject {
            LSPMarkupKind kind;
            std::string value;
        };

        struct LSPWorkDoneProgressBegin : LSPServerObject {
            std::string kind = "begin";
            std::string title;
            bool cancellable;
            bool hasMessage;
            std::string message;
            bool hasPercentage;
            int percentage;
        };

        struct LSPWorkDoneProgressReport : LSPServerObject {
            std::string kind = "report";
            bool cancellable;
            bool hasMessage;
            std::string message;
            bool hasPercentage;
            int percentage;
        };
        struct LSPWorkDoneProgressEnd : LSPServerObject {
            std::string kind = "end";
            bool hasMessage;
            std::string message;
        };

        enum class LSPWorkDoneProgressParamsType:int {
            IntializeParams
        };

        struct LSPWorkDoneProgressParams : LSPServerObject {
            LSPWorkDoneProgressParamsType param_type;
            ProgressToken work_done_token;
        };
        struct LSPWorkDoneProgressOptions : LSPServerObject {
            bool workDoneProgress;
        };

        struct LSPPartialResultParams : LSPServerObject {
            ProgressToken partial_result_token;
        };
        struct LSPClientInfo : LSPServerObject {
            std::string name;
            bool hasVersion;
            std::string version;
        };
        //
        //Client Capabilites and Intializie Params!
        //
        struct LSPStarbytesIntializationOptions : LSPServerObject {

        };

        enum class LSPTraceSetting:int {
            Off,Messages,Verbose
        };
        struct LSPWorkspaceFolder : LSPServerObject {
            LSPDocumentURI uri;
            std::string name;
        };
        struct LSPTextDocumentClientCapabilities: LSPServerObject {};
        struct LSPClientCapabilitiesWorkspace : LSPServerObject {
            bool apply_edit;
            bool hasWorkspaceEdit;
            LSPWorkspaceEditClientCapabilities workspace_edit;
            bool workspace_folders;
            bool configuration;
        };
        struct LSPClientCapabilitiesWindow : LSPServerObject {
            bool work_done_progress;
        };
        struct LSPClientCapabilities : LSPServerObject {
            bool hasWorkspace;
            LSPClientCapabilitiesWorkspace workspace;
            bool hasTextDocument;
            LSPTextDocumentClientCapabilities text_document;
            bool hasWindow;
            LSPClientCapabilitiesWindow window;
        };
        struct LSPIntializeParams : LSPWorkDoneProgressParams {
            bool hasProcessId;
            int process_id;
            bool hasClientInfo;
            LSPClientInfo client_info;
            bool hasRootPath;
            std::string root_path;
            bool hasRootUri;
            LSPDocumentURI root_uri;
            LSPStarbytesIntializationOptions intialization_options;
            //TODO CAPABILITIES!
            bool hasTrace;
            LSPTraceSetting trace;
            //TODO WORKSPACE_FOLDERS!
        };

        struct LSPServerCapabilities : LSPServerObject {

        };

        struct LSPServerInfo : LSPServerObject {
            std::string name;
            float version;
        };
        struct LSPIntializeResult : LSPServerObject {
            LSPServerCapabilities capabilites;
            LSPServerInfo server_info;
        };

        enum class LSPIntializeErrorCode:int {
            UnknownProtocolVersion = 1
        };

        struct LSPIntializeError : LSPServerObject {
            bool retry;
        };

        struct LSPIntializedParams : LSPServerObject {

        };
        enum class LSPShowMessageType:int {
            Error = 1,Warning = 2,Info = 3,Log = 4
        };
        struct LSPShowMessageParams : LSPServerObject {
            LSPShowMessageType message_type;
            std::string message;
        };
        struct LSPMessageActionItem : LSPServerObject {
            std::string title;
        };
        struct LSPShowMessageRequestParams : LSPServerObject {
            LSPShowMessageType message_type;
            std::string message;
            bool hasActions;
            std::vector<LSPMessageActionItem> actions;
        };




    }
}