
/*
    Language Server Protocol Defintions
    (Does Not Include Language Features!)
*/
#include <string>
#include <vector>
#include <map>
#include <optional>
#include "JSONOutput.h"

#ifndef STARBYTES_LSP_LSPPROTOCOL_H
#define STARBYTES_LSP_LSPPROTOCOL_H

namespace Starbytes {
    namespace LSP {

        typedef std::string LSPDocumentURI;
        // std::vector<std::string> LSPEOLChars = {"\r\n","\n","\r"};
        #define INITIALIZE "initalize"
        #define DOC_COMPLETION "textDocument/completion"
        #define DOC_HOVER "textDocument/hover"
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

        struct LSPWorkspaceFoldersServerCapabilities : LSPServerObject {
            bool supported;
            std::string string_change_notfications;
            bool bool_change_notifications;
        };
        struct LSPWorkspaceFolder : LSPServerObject {
            LSPDocumentURI uri;
            std::string name;
        };

        struct LSPWorkspaceFoldersChangeEvent : LSPServerObject {
            std::vector<LSPWorkspaceFolder> added;
            std::vector<LSPWorkspaceFolder> removed;
        };

        struct LSPDidChangeWorkspaceFoldersParams : LSPServerObject {
            LSPWorkspaceFoldersChangeEvent event;
        };

        struct LSPDidChangeConfigurationClientCapabilities : LSPServerObject {
            bool dynamic_registration;
        };

        struct LSPDidChangeConfigurationParams : LSPServerObject {
            LSPServerObject settings;
        };
        struct LSPConfigurationItem : LSPServerObject {
            bool hasScopeUri;
            LSPDocumentURI scope_uri;
            bool hasSection;
            std::string section;
        };
        struct LSPConfigurationParams : LSPServerObject {
            std::vector<LSPConfigurationItem> items;
        };

        struct LSPDidChangeWatchedFilesClientCapabilities : LSPServerObject {
            bool dynamic_registration;
        };
        enum class LSPWatchKind:int {
            Create = 1,Change = 2,Delete = 4
        };
        struct LSPFileSystemWatcher : LSPServerObject {
            std::string globPattern;
            bool hasKind;
            LSPWatchKind kind;
        };

        struct LSPDidChangeWatchedFilesRegistrationOptions : LSPServerObject {
            std::vector<LSPFileSystemWatcher> watchers;
        };

        enum class LSPFileChangeType:int {
            Created = 1,Changed =2,Deleted = 3
        };

        struct LSPFileEvent : LSPServerObject {
            LSPDocumentURI uri;
            LSPFileChangeType type;
        };

        struct LSPDidChangeWatchedFilesParams : LSPServerObject {
            std::vector<LSPFileEvent> changes;
        };

        enum class LSPSymbolKind:int {
            File = 1,Module = 2,Namespace = 3,Package = 4,Class = 5,Method = 6,Property = 7,Field = 8,
            Constructor = 9,Enum = 10,Interface = 11,Function = 12,Variable = 13,Constant = 14,String = 15,
            Number = 16,Boolean = 17,Array = 18,Object = 19,Key = 20,Null = 21,EnumMember = 22,Struct = 23,Event = 24,Operator = 25,TypeParameter = 26
        };

        struct LSPWorkspaceSymbolClientCapabilitiesSymbolKind : LSPServerObject {
            bool hasValueSet;
            std::vector<LSPSymbolKind> value_set;
        };

        struct LSPWorkspaceSymbolClientCapabilities : LSPServerObject{
            bool dynamic_registration;
            bool hasSymbolKind;
            LSPWorkspaceSymbolClientCapabilitiesSymbolKind symbol_kind;
        };

        struct LSPWorkspaceSymbolOptions : LSPWorkDoneProgressParams {};

        struct LSPWorkspaceSymbolRegistrationOptions : LSPWorkspaceSymbolOptions {};

        struct LSPWorkspaceSymbolParams : LSPWorkDoneProgressParams, LSPPartialResultParams {
            std::string query;
        };

        struct LSPExecuteCommandClientCapabilities : LSPServerObject {
            bool dynamic_registration;
        }; 

        struct LSPExecuteCommandOptions : LSPWorkDoneProgressOptions {
            std::vector<std::string> commands;
        };

        struct LSPExecuteCommandRegistrationOptions : LSPExecuteCommandOptions {};

        struct LSPExecuteCommandParams : LSPWorkDoneProgressParams {
            std::string command;
            bool hasArguments;
            std::vector<LSPServerObject *> arguments;
        };

        struct LSPApplyWorkspaceEditParams : LSPServerObject {
            bool hasLabel;
            std::string label;
            LSPWorkspaceEdit* edit;
        };

        struct LSPApplyWorkspaceEditResponse : LSPServerObject {
            bool applied;
            bool hasFailureReason;
            std::string failure_reason;
        };

        enum class LSPTextDocumentSyncKind:int {
            None,Full,Incremental
        };

        struct LSPTextDocumentSyncOptions : LSPServerObject {
            bool hasOpenClose;
            bool open_close;
            bool hasChange;
            LSPTextDocumentSyncKind change;
        };

        struct LSPDidOpenTextDocumentParams : LSPServerObject {
            LSPTextDocumentItem text_document;
        };

        struct LSPTextDocumentChangeRegistrationOptions : LSPTextDocumentRegistrationOptions {
            LSPTextDocumentSyncKind sync_kind;
        };

        struct LSPTextDocumentContentChangeEvent : LSPServerObject {
            bool hasRange;
            LSPRange range;
            // bool hasRangeLength;
            // //@Deprectated
            // int range_length;
            std::string text;
        };

        struct LSPDidChangeTextDocumentParams : LSPServerObject {
            LSPVersionedTextDocumentIdentfier text_document;
            std::vector<LSPTextDocumentContentChangeEvent *> content_changes;
        };

        enum class LSPTextDocumentSaveReason:int {
            Manual = 1,AfterDelay,FocusOut
        };

        struct LSPWillSaveTextDocumentParams : LSPServerObject {
            LSPTextDocumentIdentifier text_document;
            LSPTextDocumentSaveReason reason;
        };

        struct LSPSaveOptions : LSPServerObject {
            bool hasIncludeText;
            bool include_text;
        };

        struct LSPTextDocumentSaveRegistrationOptions : LSPTextDocumentRegistrationOptions {
            bool hasIncludeText;
            bool include_text;
        };

        struct LSPDidSaveTextDocumentParams : LSPServerObject {
            LSPTextDocumentIdentifier text_document;
            bool hasText;
            std::string text;
        };

        struct LSPDidCloseTextDocumentParams : LSPServerObject {
            LSPTextDocumentIdentifier text_document;
        };

        struct LSPTextDocumentSyncClientCapabilities : LSPServerObject {
            bool dynamic_registration;
            bool will_save;
            bool will_save_wait_until;
            bool did_save;
        };

        struct LSPTagSupportPC : LSPServerObject {
            std::vector<LSPDiagonisticTag> value_set;
        };

        struct LSPPublishDiagnosticsClientCapabilities : LSPServerObject {
            bool related_information;
            bool hasTagSupport;
            LSPTagSupportPC tag_support;
            bool version_support;
        };

        struct LSPPublishDiagnosticsParams : LSPServerObject {
            LSPDocumentURI uri;
            bool hasVersion;
            int version;
            std::vector<LSPDiagnostic *> diagnostics;
        };

        enum class LSPCompletionTriggerKind:int {
            Invoked = 1,TriggerCharacter,TriggerForIncompleteCompletions
        };
        struct LSPCompletetionContext : LSPServerObject {
            LSPCompletionTriggerKind triggerKind;
            bool hasTriggerCharacter;
            std::string trigger_character;
        }; 

        struct LSPCompletionParams : LSPTextDocumentPositionParams,LSPWorkDoneProgressParams,LSPPartialResultParams {
            bool hasContext;
            LSPCompletetionContext context;
        };

        enum class LSPCompletionItemTag:int {
            Deprecated = 1
        };

        enum class LSPInsertTextFormat:int {
            PlainText = 1,Snippet = 2
        };

        enum class LSPCompletionItemKind:int {
            Text = 1,Method,Function,Constructor,Field,Variable,Class,
            Interface,Module,Property,Unit,Value,Enum,Keyword,Snippet,
            Color,File,Reference,Folder,EnumMember,Constant,Struct,Event,Operator,TypeParameter
        };

        struct LSPCompletionItem : LSPServerObject {
            std::string label;
            bool hasKind;
            LSPCompletionItemKind kind;
            bool hasTags;
            std::vector<LSPCompletionItemTag> tags;
            bool hasDetail;
            std::string detail;
            bool hasDocumentation;
            LSPMarkupContent documentation;
            bool deprecated;
            bool preselect;
            bool sort_text;
            bool hasFilterText;
            std::string filter_text;
            bool hasInsertText;
            std::string insert_text;
            bool hasInsertTextFormat;
            LSPInsertTextFormat insert_text_format;
            bool hasTextEdit;
            LSPTextEdit * text_edit;
            bool hasAdditionalTextEdits;
            std::vector<LSPTextEdit *> additional_text_edits;
            bool hasCommitCharacters;
            std::vector<std::string> commit_characters;
            bool hasCommand;
            LSPCommand command;
        };

        struct LSPCompletionList : LSPServerObject {
            bool is_incomplete;
            std::vector<LSPCompletionItem *> items;
        };
        struct LSPTagSupportCC : LSPServerObject {
            std::vector<LSPCompletionItemTag> value_set;
        };

        struct LSPCompletionItemKindCC : LSPServerObject {
            bool hasValueSet;
            std::vector<LSPCompletionItemKind> value_set;
        };
        struct LSPCompletionClientCapabilitiesCompletion : LSPServerObject {
            bool snippet_support;
            bool commit_characters_support;
            bool hasDocumentationFormat;
            std::vector<LSPMarkupKind *> documentation_format;
            bool deprecated_support;
            bool preselect_support;
            bool hasTagSupport;
            LSPTagSupportCC tag_support;
            bool hasCompletionItemKind;
            LSPCompletionItemKindCC completion_item_kind;
            bool context_support;

        };

        struct LSPCompletionOptions : LSPWorkDoneProgressOptions {
            bool hasTriggerCharacters;
            std::vector<std::string> trigger_characters;
            bool hasAllCommitCharacters;
            std::vector<std::string> all_commit_characters;
            bool resolve_provider;
        };

        struct LSPCompletionRegistrationOptions : LSPTextDocumentRegistrationOptions, LSPCompletionOptions {};

       struct LSPCompletionClientCapabilities : LSPServerObject {
           bool dynamic_registration;

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
            std::string process_id;
            bool hasClientInfo;
            LSPClientInfo* client_info;
            bool hasRootPath;
            std::string root_path;
            bool hasRootUri;
            LSPDocumentURI root_uri;
            LSPStarbytesIntializationOptions* intialization_options;
            //TODO CAPABILITIES!
            LSPClientCapabilities* capabilites;
            bool hasTrace;
            LSPTraceSetting trace;
            //TODO WORKSPACE_FOLDERS!
            bool hasWorkspacefolders;
            std::vector<LSPWorkspaceFolder *> workspace_folders;
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

        struct LSPWorkDoneProgressCreateParams : LSPServerObject {
            ProgressToken token;
        };

        struct WorkDoneProgressCancelParams : LSPServerObject {
            ProgressToken token;
        };

        struct LSPRegistration : LSPServerObject {
            std::string id;
            std::string method;
            LSPServerObject register_options;
        };

        struct LSPRegistrationParams : LSPServerObject{
            std::vector<LSPRegistration> registrations;
        };

        struct LSPUnregistration : LSPServerObject {
            std::string id;
            std::string method;
        };

        struct LSPUnregistrationParams : LSPServerObject {
            std::vector<LSPUnregistration> unregistrations;
        };





    }
}

#endif

