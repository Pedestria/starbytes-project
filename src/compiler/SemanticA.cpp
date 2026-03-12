#include "starbytes/compiler/SemanticA.h"
#include "starbytes/compiler/SymTable.h"
#include "starbytes/compiler/AST.h"
#include "starbytes/compiler/ASTNodes.def"
#include <iostream>
#include <sstream>
#include <algorithm>
#include <set>
#include <memory>
#include <unordered_map>

namespace starbytes {

    static bool shouldSuppressUnusedInvocationWarning(ASTExpr *expr){
        if(!expr || expr->type != IVKE_EXPR || !expr->callee){
            return false;
        }
        if(expr->callee->type == ID_EXPR && expr->callee->id){
            return expr->callee->id->val == "print";
        }
        if(expr->callee->type == MEMBER_EXPR && expr->callee->rightExpr && expr->callee->rightExpr->id){
            return expr->callee->rightExpr->id->val == "print";
        }
        return false;
    }

    static bool attributeArgIsString(const ASTAttributeArg &arg){
        if(!arg.value){
            return false;
        }
        auto *expr = (ASTExpr *)arg.value;
        if(expr->type != STR_LITERAL){
            return false;
        }
        auto *literal = (ASTLiteralExpr *)expr;
        return literal->strValue.has_value();
    }

    static bool hasAttributeNamed(const std::vector<ASTAttribute> &attrs,const std::string &name){
        for(const auto &attr : attrs){
            if(attr.name == name){
                return true;
            }
        }
        return false;
    }

    static std::optional<std::string> deprecatedMessageFromAttributes(const std::vector<ASTAttribute> &attrs){
        for(const auto &attr : attrs){
            if(attr.name != "deprecated"){
                continue;
            }
            for(const auto &arg : attr.args){
                if(arg.key.has_value() && arg.key.value() != "message"){
                    continue;
                }
                if(arg.value && attributeArgIsString(arg)){
                    auto *literal = static_cast<ASTLiteralExpr *>(arg.value);
                    if(literal->strValue.has_value()){
                        return literal->strValue.value();
                    }
                }
            }
            return std::nullopt;
        }
        return std::nullopt;
    }

    static bool validateSystemAttribute(ASTDecl *decl,
                                        const ASTAttribute &attr,
                                        DiagnosticHandler &errStream){
        auto pushAttrError = [&](const std::string &msg){
            errStream.push(SemanticADiagnostic::create(msg,decl,Diagnostic::Error));
        };

        if(attr.name == "readonly"){
            if(decl->type != VAR_DECL || decl->scope->type != ASTScope::Class){
                pushAttrError("@readonly is only valid on class fields.");
                return false;
            }
            if(!attr.args.empty()){
                pushAttrError("@readonly does not accept arguments.");
                return false;
            }
            return true;
        }

        if(attr.name == "private"){
            if(decl->type != VAR_DECL || decl->scope->type != ASTScope::Class){
                pushAttrError("@private is only valid on class fields.");
                return false;
            }
            if(!attr.args.empty()){
                pushAttrError("@private does not accept arguments.");
                return false;
            }
            return true;
        }

        if(attr.name == "deprecated"){
            if(!(decl->type == VAR_DECL || decl->type == FUNC_DECL || decl->type == CLASS_DECL
                 || decl->type == INTERFACE_DECL || decl->type == TYPE_ALIAS_DECL)){
                pushAttrError("@deprecated is only valid on class/interface/typealias/function/field declarations.");
                return false;
            }
            if(attr.args.empty()){
                return true;
            }
            if(attr.args.size() != 1 || !attributeArgIsString(attr.args[0])){
                pushAttrError("@deprecated accepts at most one string argument.");
                return false;
            }
            if(attr.args[0].key.has_value() && attr.args[0].key.value() != "message"){
                pushAttrError("@deprecated only accepts named argument `message`.");
                return false;
            }
            return true;
        }

        if(attr.name == "native"){
            if(decl->type == VAR_DECL){
                auto *varDecl = (ASTVarDecl *)decl;
                if(decl->scope && decl->scope->type == ASTScope::Class){
                    pushAttrError("@native is not valid on class fields.");
                    return false;
                }
                if(!(decl->scope == ASTScopeGlobal || (decl->scope && decl->scope->type == ASTScope::Namespace))){
                    pushAttrError("@native variable declarations are only valid at module or namespace scope.");
                    return false;
                }
                if(varDecl->specs.size() != 1){
                    pushAttrError("@native variable declarations must declare exactly one symbol.");
                    return false;
                }
                auto &spec = varDecl->specs.front();
                if(!varDecl->isConst){
                    pushAttrError("@native variable declarations must be immutable.");
                    return false;
                }
                if(spec.type == nullptr){
                    pushAttrError("@native variable declarations must declare an explicit type.");
                    return false;
                }
                if(spec.expr != nullptr){
                    pushAttrError("@native variable declarations must not declare an initializer.");
                    return false;
                }
            }
            else if(!(decl->type == FUNC_DECL || decl->type == CLASS_DECL)){
                pushAttrError("@native is only valid on class/function declarations and immutable module constants.");
                return false;
            }
            if(attr.args.empty()){
                return true;
            }
            if(attr.args.size() != 1 || !attributeArgIsString(attr.args[0])){
                pushAttrError("@native accepts at most one string argument.");
                return false;
            }
            if(attr.args[0].key.has_value() && attr.args[0].key.value() != "name"){
                pushAttrError("@native only accepts named argument `name`.");
                return false;
            }
            return true;
        }

        return true;
    }

    static bool validateAttributesForDecl(ASTDecl *decl, DiagnosticHandler &errStream){
        for(auto &attr : decl->attributes){
            if(attr.name == "readonly" || attr.name == "private" || attr.name == "deprecated" || attr.name == "native"){
                if(!validateSystemAttribute(decl,attr,errStream)){
                    return false;
                }
            }
        }
        return true;
    }

    static std::vector<std::string> getNamespacePath(std::shared_ptr<ASTScope> scope){
        std::vector<std::string> path;
        for(auto s = scope; s != nullptr; s = s->parentScope){
            if(s->type == ASTScope::Namespace){
                path.push_back(s->name);
            }
        }
        std::reverse(path.begin(),path.end());
        return path;
    }

    static std::string buildEmittedName(std::shared_ptr<ASTScope> scope,string_ref symbolName){
        auto nsPath = getNamespacePath(scope);
        if(nsPath.empty()){
            return symbolName.str();
        }
        Twine out;
        for(size_t i = 0;i < nsPath.size();++i){
            if(i > 0){
                out + "__";
            }
            out + nsPath[i];
        }
        out + "__";
        out + symbolName.str();
        return out.str();
    }

    enum class SemanticFlowBindingKind : uint8_t {
        Local,
        Parameter,
        Catch
    };

    struct SemanticFlowBinding {
        std::string name;
        ASTIdentifier *id = nullptr;
        SemanticFlowBindingKind kind = SemanticFlowBindingKind::Local;
        bool initialized = false;
        bool allowsAbsentRead = false;
    };

    struct SemanticFlowState {
        std::vector<std::vector<SemanticFlowBinding>> frames;
    };

    struct SemanticFlowResult {
        SemanticFlowState state;
        bool continues = true;
        bool errored = false;
    };

    enum class SemanticUsageBindingKind : uint8_t {
        Import,
        Local,
        Parameter,
        PrivateField
    };

    struct SemanticUsageBinding {
        std::string name;
        std::string displayName;
        ASTIdentifier *id = nullptr;
        SemanticUsageBindingKind kind = SemanticUsageBindingKind::Local;
        bool used = false;
        bool warned = false;
    };

    using SemanticUsageBindingPtr = std::shared_ptr<SemanticUsageBinding>;

    struct SemanticUsageState {
        std::vector<std::vector<SemanticUsageBindingPtr>> frames;
    };

    static std::unordered_map<const SemanticA *, SemanticFlowState> gNamespaceFlowStates;
    static std::unordered_map<const SemanticA *, SemanticUsageState> gNamespaceUsageStates;

    static void pushSemanticFlowFrame(SemanticFlowState &state,const std::vector<SemanticFlowBinding> &bindings = {}){
        state.frames.push_back(bindings);
    }

    static void popSemanticFlowFrame(SemanticFlowState &state){
        if(!state.frames.empty()){
            state.frames.pop_back();
        }
    }

    static SemanticFlowBinding *findSemanticBindingInFrame(std::vector<SemanticFlowBinding> &frame,const std::string &name){
        for(auto it = frame.rbegin();it != frame.rend();++it){
            if(it->name == name){
                return &*it;
            }
        }
        return nullptr;
    }

    static const SemanticFlowBinding *findSemanticBindingInFrame(const std::vector<SemanticFlowBinding> &frame,const std::string &name){
        for(auto it = frame.rbegin();it != frame.rend();++it){
            if(it->name == name){
                return &*it;
            }
        }
        return nullptr;
    }

    static SemanticFlowBinding *findNearestSemanticBinding(SemanticFlowState &state,const std::string &name){
        for(auto it = state.frames.rbegin();it != state.frames.rend();++it){
            if(auto *binding = findSemanticBindingInFrame(*it,name)){
                return binding;
            }
        }
        return nullptr;
    }

    static void emitUseBeforeInitializationDiagnostic(DiagnosticHandler &errStream,
                                                      ASTIdentifier *id,
                                                      const SemanticFlowBinding &binding){
        std::ostringstream ss;
        ss << "Binding `" << binding.name << "` may be read before it is definitely initialized.";
        errStream.push(SemanticADiagnostic::create(ss.str(),id ? static_cast<ASTStmt *>(id) : nullptr,Diagnostic::Error));
    }

    static std::vector<SemanticFlowBinding> makeSemanticFlowParamBindings(const std::map<ASTIdentifier *,ASTType *> &params){
        std::vector<SemanticFlowBinding> bindings;
        bindings.reserve(params.size());
        for(const auto &entry : params){
            if(entry.first){
                bindings.push_back({entry.first->val,entry.first,SemanticFlowBindingKind::Parameter,true});
            }
        }
        return bindings;
    }

    static bool shouldSuppressUnusedBindingWarning(const std::string &name){
        return name.empty() || name == "self" || name.front() == '_';
    }

    static void pushSemanticUsageFrame(SemanticUsageState &state,
                                       const std::vector<SemanticUsageBindingPtr> &bindings = {}){
        state.frames.push_back(bindings);
    }

    static void popSemanticUsageFrame(SemanticUsageState &state){
        if(!state.frames.empty()){
            state.frames.pop_back();
        }
    }

    static SemanticUsageBindingPtr findSemanticUsageBindingInFrame(const std::vector<SemanticUsageBindingPtr> &frame,
                                                                  const std::string &name){
        for(auto it = frame.rbegin();it != frame.rend();++it){
            if(*it && (*it)->name == name){
                return *it;
            }
        }
        return nullptr;
    }

    static SemanticUsageBindingPtr findNearestSemanticUsageBinding(SemanticUsageState &state,const std::string &name){
        for(auto it = state.frames.rbegin();it != state.frames.rend();++it){
            auto binding = findSemanticUsageBindingInFrame(*it,name);
            if(binding){
                return binding;
            }
        }
        return nullptr;
    }

    static std::vector<SemanticUsageBindingPtr> makeSemanticUsageParamBindings(const std::map<ASTIdentifier *,ASTType *> &params){
        std::vector<SemanticUsageBindingPtr> bindings;
        bindings.reserve(params.size());
        for(const auto &entry : params){
            if(!entry.first){
                continue;
            }
            auto binding = std::make_shared<SemanticUsageBinding>();
            binding->name = entry.first->val;
            binding->displayName = entry.first->sourceName.empty() ? entry.first->val : entry.first->sourceName;
            binding->id = entry.first;
            binding->kind = SemanticUsageBindingKind::Parameter;
            bindings.push_back(binding);
        }
        return bindings;
    }

    static std::vector<SemanticUsageBindingPtr> makeSemanticUsagePrivateFieldBindings(ASTClassDecl *classDecl){
        std::vector<SemanticUsageBindingPtr> bindings;
        if(!classDecl){
            return bindings;
        }
        for(auto *fieldDecl : classDecl->fields){
            if(!fieldDecl || !hasAttributeNamed(fieldDecl->attributes,"private")){
                continue;
            }
            for(const auto &spec : fieldDecl->specs){
                if(!spec.id){
                    continue;
                }
                auto binding = std::make_shared<SemanticUsageBinding>();
                binding->name = spec.id->val;
                binding->displayName = spec.id->sourceName.empty() ? spec.id->val : spec.id->sourceName;
                binding->id = spec.id;
                binding->kind = SemanticUsageBindingKind::PrivateField;
                bindings.push_back(binding);
            }
        }
        return bindings;
    }

    static SemanticUsageBindingPtr findNearestSemanticUsageBindingOfKind(SemanticUsageState &state,
                                                                         const std::string &name,
                                                                         SemanticUsageBindingKind kind){
        for(auto it = state.frames.rbegin();it != state.frames.rend();++it){
            for(auto bindingIt = it->rbegin();bindingIt != it->rend();++bindingIt){
                if(*bindingIt && (*bindingIt)->kind == kind && (*bindingIt)->name == name){
                    return *bindingIt;
                }
            }
        }
        return nullptr;
    }

    static void emitUnusedUsageBindingDiagnostic(DiagnosticHandler &errStream,
                                                 const SemanticUsageBindingPtr &binding){
        auto displayName = binding && !binding->displayName.empty() ? binding->displayName
                                                                    : (binding ? binding->name : std::string());
        if(!binding || binding->warned || binding->used || shouldSuppressUnusedBindingWarning(displayName)){
            return;
        }
        std::ostringstream ss;
        switch(binding->kind){
            case SemanticUsageBindingKind::Import:
                ss << "Imported module `" << displayName << "` is unused.";
                break;
            case SemanticUsageBindingKind::Local:
                ss << "Local binding `" << displayName << "` is unused.";
                break;
            case SemanticUsageBindingKind::Parameter:
                ss << "Parameter `" << displayName << "` is unused.";
                break;
            case SemanticUsageBindingKind::PrivateField:
                ss << "Private field `" << displayName << "` is unused.";
                break;
        }
        errStream.push(SemanticADiagnostic::create(ss.str(),
                                                   binding->id ? static_cast<ASTStmt *>(binding->id) : nullptr,
                                                   Diagnostic::Warning));
        binding->warned = true;
    }

    static void emitUnusedUsageWarningsForFrame(DiagnosticHandler &errStream,
                                                const std::vector<SemanticUsageBindingPtr> &frame){
        for(const auto &binding : frame){
            emitUnusedUsageBindingDiagnostic(errStream,binding);
        }
    }

    static SemanticFlowState mergeContinuingSemanticFlowStates(const std::vector<SemanticFlowState> &states){
        if(states.empty()){
            return {};
        }
        auto merged = states.front();
        for(size_t frameIndex = 0;frameIndex < merged.frames.size();++frameIndex){
            for(auto &binding : merged.frames[frameIndex]){
                bool initialized = true;
                for(const auto &state : states){
                    if(frameIndex >= state.frames.size()){
                        initialized = false;
                        break;
                    }
                    const auto *other = findSemanticBindingInFrame(state.frames[frameIndex],binding.name);
                    if(!other || !other->initialized){
                        initialized = false;
                        break;
                    }
                }
                binding.initialized = initialized;
            }
        }
        return merged;
    }

    static bool isNonVoidSemanticReturnType(ASTType *returnType){
        return returnType != nullptr && !returnType->nameMatches(VOID_TYPE);
    }

    static SemanticFlowResult analyzeSemanticFlowSequence(const std::vector<ASTStmt *> &statements,
                                                          SemanticFlowState state,
                                                          DiagnosticHandler &errStream);
    static SemanticFlowResult analyzeSemanticFlowBlock(ASTBlockStmt *block,
                                                       SemanticFlowState state,
                                                       DiagnosticHandler &errStream,
                                                       const std::vector<SemanticFlowBinding> &bindings = {});
    static bool validateCallableSemanticFlow(ASTBlockStmt *block,
                                             const std::map<ASTIdentifier *,ASTType *> &params,
                                             ASTType *returnType,
                                             DiagnosticHandler &errStream,
                                             ASTStmt *owner,
                                             const std::string &displayName);

    static void analyzeInlineFunctionSemanticFlow(ASTExpr *expr,
                                                  DiagnosticHandler &errStream,
                                                  bool &errored){
        if(!expr || expr->type != INLINE_FUNC_EXPR || !expr->inlineFuncBlock){
            return;
        }
        if(!validateCallableSemanticFlow(expr->inlineFuncBlock,
                                         expr->inlineFuncParams,
                                         expr->inlineFuncReturnType,
                                         errStream,
                                         expr,
                                         "inline function")){
            errored = true;
        }
    }

    static void analyzeSemanticFlowExpr(ASTExpr *expr,
                                        SemanticFlowState &state,
                                        DiagnosticHandler &errStream,
                                        bool &errored,
                                        bool allowAbsentReads = false){
        if(!expr || errored){
            return;
        }

        if(expr->type == INLINE_FUNC_EXPR){
            analyzeInlineFunctionSemanticFlow(expr,errStream,errored);
            return;
        }

        if(expr->type == ID_EXPR){
            if(expr->id){
                if(auto *binding = findNearestSemanticBinding(state,expr->id->val)){
                    if(!binding->initialized && !(allowAbsentReads && binding->allowsAbsentRead)){
                        emitUseBeforeInitializationDiagnostic(errStream,expr->id,*binding);
                        errored = true;
                    }
                }
            }
            return;
        }

        if(expr->type == MEMBER_EXPR){
            analyzeSemanticFlowExpr(expr->leftExpr,state,errStream,errored,allowAbsentReads);
            return;
        }

        if(expr->type == ASSIGN_EXPR && expr->oprtr_str.has_value() && *expr->oprtr_str == "="
           && expr->leftExpr != nullptr && expr->leftExpr->type == ID_EXPR && expr->leftExpr->id != nullptr){
            analyzeSemanticFlowExpr(expr->rightExpr,state,errStream,errored,allowAbsentReads);
            if(errored){
                return;
            }
            if(auto *binding = findNearestSemanticBinding(state,expr->leftExpr->id->val)){
                binding->initialized = true;
            }
            return;
        }

        analyzeSemanticFlowExpr(expr->callee,state,errStream,errored,allowAbsentReads);
        analyzeSemanticFlowExpr(expr->leftExpr,state,errStream,errored,allowAbsentReads);
        analyzeSemanticFlowExpr(expr->middleExpr,state,errStream,errored,allowAbsentReads);
        analyzeSemanticFlowExpr(expr->rightExpr,state,errStream,errored,allowAbsentReads);
        if(errored){
            return;
        }
        for(auto *child : expr->exprArrayData){
            analyzeSemanticFlowExpr(child,state,errStream,errored,allowAbsentReads);
            if(errored){
                return;
            }
        }
        for(const auto &entry : expr->dictExpr){
            analyzeSemanticFlowExpr(entry.first,state,errStream,errored,allowAbsentReads);
            if(errored){
                return;
            }
            analyzeSemanticFlowExpr(entry.second,state,errStream,errored,allowAbsentReads);
            if(errored){
                return;
            }
        }
    }

    static SemanticFlowResult analyzeSemanticFlowStmt(ASTStmt *stmt,
                                                      SemanticFlowState state,
                                                      DiagnosticHandler &errStream){
        SemanticFlowResult result {std::move(state),true,false};
        if(!stmt){
            return result;
        }

        if(!(stmt->type & DECL)){
            analyzeSemanticFlowExpr((ASTExpr *)stmt,result.state,errStream,result.errored);
            return result;
        }

        auto *decl = (ASTDecl *)stmt;
        switch(decl->type){
            case VAR_DECL: {
                auto *varDecl = (ASTVarDecl *)decl;
                for(const auto &spec : varDecl->specs){
                    bool allowsAbsentRead = spec.type != nullptr && (spec.type->isOptional || spec.type->isThrowable);
                    if(spec.expr){
                        analyzeSemanticFlowExpr(spec.expr,result.state,errStream,result.errored);
                        if(result.errored){
                            return result;
                        }
                    }
                    if(result.state.frames.empty()){
                        pushSemanticFlowFrame(result.state);
                    }
                    result.state.frames.back().push_back({
                        spec.id ? spec.id->val : std::string(),
                        spec.id,
                        SemanticFlowBindingKind::Local,
                        spec.expr != nullptr || allowsAbsentRead,
                        allowsAbsentRead
                    });
                }
                return result;
            }
            case RETURN_DECL: {
                auto *returnDecl = (ASTReturnDecl *)decl;
                analyzeSemanticFlowExpr(returnDecl->expr,result.state,errStream,result.errored);
                result.continues = false;
                return result;
            }
            case COND_DECL: {
                auto *condDecl = (ASTConditionalDecl *)decl;
                std::vector<SemanticFlowState> continuingStates;
                bool hasElse = false;
                for(const auto &spec : condDecl->specs){
                    auto branchState = result.state;
                    if(spec.expr){
                        analyzeSemanticFlowExpr(spec.expr,branchState,errStream,result.errored);
                        if(result.errored){
                            return result;
                        }
                    }
                    auto branchResult = analyzeSemanticFlowBlock(spec.blockStmt,std::move(branchState),errStream);
                    if(branchResult.errored){
                        return branchResult;
                    }
                    if(spec.expr == nullptr){
                        hasElse = true;
                    }
                    if(branchResult.continues){
                        continuingStates.push_back(std::move(branchResult.state));
                    }
                }
                if(!hasElse){
                    continuingStates.push_back(result.state);
                }
                if(continuingStates.empty()){
                    result.continues = false;
                    return result;
                }
                result.state = mergeContinuingSemanticFlowStates(continuingStates);
                return result;
            }
            case FOR_DECL: {
                auto *forDecl = (ASTForDecl *)decl;
                analyzeSemanticFlowExpr(forDecl->expr,result.state,errStream,result.errored);
                if(result.errored){
                    return result;
                }
                auto loopResult = analyzeSemanticFlowBlock(forDecl->blockStmt,result.state,errStream);
                if(loopResult.errored){
                    return loopResult;
                }
                result.continues = true;
                return result;
            }
            case WHILE_DECL: {
                auto *whileDecl = (ASTWhileDecl *)decl;
                analyzeSemanticFlowExpr(whileDecl->expr,result.state,errStream,result.errored);
                if(result.errored){
                    return result;
                }
                auto loopResult = analyzeSemanticFlowBlock(whileDecl->blockStmt,result.state,errStream);
                if(loopResult.errored){
                    return loopResult;
                }
                result.continues = true;
                return result;
            }
            case SECURE_DECL: {
                auto *secureDecl = (ASTSecureDecl *)decl;
                if(!secureDecl->guardedDecl || secureDecl->guardedDecl->specs.empty()){
                    return result;
                }
                const auto &spec = secureDecl->guardedDecl->specs.front();
                if(spec.expr){
                    analyzeSemanticFlowExpr(spec.expr,result.state,errStream,result.errored,true);
                    if(result.errored){
                        return result;
                    }
                }
                if(result.state.frames.empty()){
                    pushSemanticFlowFrame(result.state);
                }
                result.state.frames.back().push_back({
                    spec.id ? spec.id->val : std::string(),
                    spec.id,
                    SemanticFlowBindingKind::Local,
                    true,
                    false
                });

                auto catchState = result.state;
                if(secureDecl->catchErrorId){
                    pushSemanticFlowFrame(catchState,{{secureDecl->catchErrorId->val,
                                                      secureDecl->catchErrorId,
                                                      SemanticFlowBindingKind::Catch,
                                                      true,
                                                      false}});
                }
                auto catchResult = analyzeSemanticFlowBlock(secureDecl->catchBlock,std::move(catchState),errStream);
                if(catchResult.errored){
                    return catchResult;
                }
                return result;
            }
            default:
                return result;
        }
    }

    static SemanticFlowResult analyzeSemanticFlowSequence(const std::vector<ASTStmt *> &statements,
                                                          SemanticFlowState state,
                                                          DiagnosticHandler &errStream){
        SemanticFlowResult result {std::move(state),true,false};
        for(auto *stmt : statements){
            if(result.errored || !result.continues){
                break;
            }
            result = analyzeSemanticFlowStmt(stmt,std::move(result.state),errStream);
        }
        return result;
    }

    static SemanticFlowResult analyzeSemanticFlowBlock(ASTBlockStmt *block,
                                                       SemanticFlowState state,
                                                       DiagnosticHandler &errStream,
                                                       const std::vector<SemanticFlowBinding> &bindings){
        if(!block){
            return {std::move(state),true,false};
        }
        pushSemanticFlowFrame(state,bindings);
        auto result = analyzeSemanticFlowSequence(block->body,std::move(state),errStream);
        popSemanticFlowFrame(result.state);
        return result;
    }

    static bool validateCallableSemanticFlow(ASTBlockStmt *block,
                                             const std::map<ASTIdentifier *,ASTType *> &params,
                                             ASTType *returnType,
                                             DiagnosticHandler &errStream,
                                             ASTStmt *owner,
                                             const std::string &displayName){
        SemanticFlowState state;
        pushSemanticFlowFrame(state,makeSemanticFlowParamBindings(params));
        auto result = analyzeSemanticFlowBlock(block,std::move(state),errStream);
        if(result.errored){
            return false;
        }
        if(isNonVoidSemanticReturnType(returnType) && result.continues){
            std::ostringstream ss;
            ss << "Callable `" << displayName << "` has declared non-Void return type `"
               << returnType->getName() << "` but may fall through without returning a value.";
            errStream.push(SemanticADiagnostic::create(ss.str(),owner,Diagnostic::Error));
            return false;
        }
        return true;
    }

    static void analyzeSemanticUsageStmt(ASTStmt *stmt,
                                         SemanticUsageState &state,
                                         DiagnosticHandler &errStream,
                                         bool trackLocalBindings);
    static void analyzeSemanticUsageBlock(ASTBlockStmt *block,
                                          SemanticUsageState &state,
                                          DiagnosticHandler &errStream,
                                          bool trackLocalBindings);
    static void validateCallableSemanticUsage(ASTBlockStmt *block,
                                              const std::map<ASTIdentifier *,ASTType *> &params,
                                              DiagnosticHandler &errStream,
                                              const SemanticUsageState *outerState);

    static void analyzeSemanticUsageExpr(ASTExpr *expr,
                                         SemanticUsageState &state,
                                         DiagnosticHandler &errStream,
                                         bool readContext = true){
        if(!expr){
            return;
        }

        if(expr->type == INLINE_FUNC_EXPR){
            validateCallableSemanticUsage(expr->inlineFuncBlock,expr->inlineFuncParams,errStream,&state);
            return;
        }

        if(expr->type == ID_EXPR){
            if(readContext && expr->id){
                auto binding = findNearestSemanticUsageBinding(state,expr->id->val);
                if(binding){
                    binding->used = true;
                }
            }
            return;
        }

        if(expr->type == MEMBER_EXPR){
            analyzeSemanticUsageExpr(expr->leftExpr,state,errStream,true);
            if(readContext && expr->leftExpr && expr->leftExpr->type == ID_EXPR &&
               expr->leftExpr->id && expr->leftExpr->id->val == "self" &&
               expr->rightExpr && expr->rightExpr->id){
                auto binding = findNearestSemanticUsageBindingOfKind(state,
                                                                     expr->rightExpr->id->val,
                                                                     SemanticUsageBindingKind::PrivateField);
                if(binding){
                    binding->used = true;
                }
            }
            return;
        }

        if(expr->type == ASSIGN_EXPR){
            auto op = expr->oprtr_str.value_or("=");
            if(op == "=" && expr->leftExpr && expr->leftExpr->type == ID_EXPR){
                analyzeSemanticUsageExpr(expr->rightExpr,state,errStream,true);
                return;
            }
            if(op == "=" && expr->leftExpr && expr->leftExpr->type == MEMBER_EXPR){
                analyzeSemanticUsageExpr(expr->leftExpr->leftExpr,state,errStream,true);
                analyzeSemanticUsageExpr(expr->rightExpr,state,errStream,true);
                return;
            }
            analyzeSemanticUsageExpr(expr->leftExpr,state,errStream,true);
            analyzeSemanticUsageExpr(expr->rightExpr,state,errStream,true);
            return;
        }

        analyzeSemanticUsageExpr(expr->callee,state,errStream,true);
        analyzeSemanticUsageExpr(expr->leftExpr,state,errStream,true);
        analyzeSemanticUsageExpr(expr->middleExpr,state,errStream,true);
        analyzeSemanticUsageExpr(expr->rightExpr,state,errStream,true);
        for(auto *child : expr->exprArrayData){
            analyzeSemanticUsageExpr(child,state,errStream,true);
        }
        for(const auto &entry : expr->dictExpr){
            analyzeSemanticUsageExpr(entry.first,state,errStream,true);
            analyzeSemanticUsageExpr(entry.second,state,errStream,true);
        }
    }

    static void analyzeSemanticUsageStmt(ASTStmt *stmt,
                                         SemanticUsageState &state,
                                         DiagnosticHandler &errStream,
                                         bool trackLocalBindings){
        if(!stmt){
            return;
        }

        if(!(stmt->type & DECL)){
            analyzeSemanticUsageExpr((ASTExpr *)stmt,state,errStream,true);
            return;
        }

        auto *decl = (ASTDecl *)stmt;
        switch(decl->type){
            case IMPORT_DECL: {
                auto *importDecl = (ASTImportDecl *)decl;
                if(!importDecl->moduleName){
                    return;
                }
                if(state.frames.empty()){
                    pushSemanticUsageFrame(state);
                }
                auto binding = std::make_shared<SemanticUsageBinding>();
                binding->name = importDecl->moduleName->val;
                binding->displayName = importDecl->moduleName->sourceName.empty()
                    ? importDecl->moduleName->val
                    : importDecl->moduleName->sourceName;
                binding->id = importDecl->moduleName;
                binding->kind = SemanticUsageBindingKind::Import;
                state.frames.back().push_back(std::move(binding));
                return;
            }
            case VAR_DECL: {
                auto *varDecl = (ASTVarDecl *)decl;
                for(const auto &spec : varDecl->specs){
                    analyzeSemanticUsageExpr(spec.expr,state,errStream,true);
                    if(!trackLocalBindings || !spec.id){
                        continue;
                    }
                    if(state.frames.empty()){
                        pushSemanticUsageFrame(state);
                    }
                    auto binding = std::make_shared<SemanticUsageBinding>();
                    binding->name = spec.id->val;
                    binding->displayName = spec.id->sourceName.empty() ? spec.id->val : spec.id->sourceName;
                    binding->id = spec.id;
                    binding->kind = SemanticUsageBindingKind::Local;
                    state.frames.back().push_back(std::move(binding));
                }
                return;
            }
            case RETURN_DECL: {
                auto *returnDecl = (ASTReturnDecl *)decl;
                analyzeSemanticUsageExpr(returnDecl->expr,state,errStream,true);
                return;
            }
            case COND_DECL: {
                auto *condDecl = (ASTConditionalDecl *)decl;
                for(const auto &spec : condDecl->specs){
                    analyzeSemanticUsageExpr(spec.expr,state,errStream,true);
                    analyzeSemanticUsageBlock(spec.blockStmt,state,errStream,trackLocalBindings);
                }
                return;
            }
            case FOR_DECL: {
                auto *forDecl = (ASTForDecl *)decl;
                analyzeSemanticUsageExpr(forDecl->expr,state,errStream,true);
                analyzeSemanticUsageBlock(forDecl->blockStmt,state,errStream,trackLocalBindings);
                return;
            }
            case WHILE_DECL: {
                auto *whileDecl = (ASTWhileDecl *)decl;
                analyzeSemanticUsageExpr(whileDecl->expr,state,errStream,true);
                analyzeSemanticUsageBlock(whileDecl->blockStmt,state,errStream,trackLocalBindings);
                return;
            }
            case SECURE_DECL: {
                auto *secureDecl = (ASTSecureDecl *)decl;
                if(!secureDecl->guardedDecl || secureDecl->guardedDecl->specs.empty()){
                    return;
                }
                const auto &spec = secureDecl->guardedDecl->specs.front();
                analyzeSemanticUsageExpr(spec.expr,state,errStream,true);
                if(trackLocalBindings && spec.id){
                    if(state.frames.empty()){
                        pushSemanticUsageFrame(state);
                    }
                    auto binding = std::make_shared<SemanticUsageBinding>();
                    binding->name = spec.id->val;
                    binding->displayName = spec.id->sourceName.empty() ? spec.id->val : spec.id->sourceName;
                    binding->id = spec.id;
                    binding->kind = SemanticUsageBindingKind::Local;
                    state.frames.back().push_back(std::move(binding));
                }
                analyzeSemanticUsageBlock(secureDecl->catchBlock,state,errStream,trackLocalBindings);
                return;
            }
            case FUNC_DECL: {
                auto *funcDecl = (ASTFuncDecl *)decl;
                if(!funcDecl->declarationOnly && funcDecl->blockStmt){
                    validateCallableSemanticUsage(funcDecl->blockStmt,funcDecl->params,errStream,&state);
                }
                return;
            }
            case CLASS_DECL: {
                auto *classDecl = (ASTClassDecl *)decl;
                pushSemanticUsageFrame(state,makeSemanticUsagePrivateFieldBindings(classDecl));
                for(auto *fieldDecl : classDecl->fields){
                    analyzeSemanticUsageStmt(fieldDecl,state,errStream,false);
                }
                for(auto *methodDecl : classDecl->methods){
                    if(methodDecl && !methodDecl->declarationOnly && methodDecl->blockStmt){
                        auto methodParams = methodDecl->params;
                        validateCallableSemanticUsage(methodDecl->blockStmt,methodParams,errStream,&state);
                    }
                }
                for(auto *ctorDecl : classDecl->constructors){
                    if(ctorDecl && ctorDecl->blockStmt){
                        validateCallableSemanticUsage(ctorDecl->blockStmt,ctorDecl->params,errStream,&state);
                    }
                }
                emitUnusedUsageWarningsForFrame(errStream,state.frames.back());
                popSemanticUsageFrame(state);
                return;
            }
            case INTERFACE_DECL: {
                auto *interfaceDecl = (ASTInterfaceDecl *)decl;
                for(auto *fieldDecl : interfaceDecl->fields){
                    analyzeSemanticUsageStmt(fieldDecl,state,errStream,false);
                }
                for(auto *methodDecl : interfaceDecl->methods){
                    if(methodDecl && !methodDecl->declarationOnly && methodDecl->blockStmt){
                        auto methodParams = methodDecl->params;
                        validateCallableSemanticUsage(methodDecl->blockStmt,methodParams,errStream,&state);
                    }
                }
                return;
            }
            case SCOPE_DECL: {
                auto *scopeDecl = (ASTScopeDecl *)decl;
                analyzeSemanticUsageBlock(scopeDecl->blockStmt,state,errStream,false);
                return;
            }
            default:
                return;
        }
    }

    static void analyzeSemanticUsageBlock(ASTBlockStmt *block,
                                          SemanticUsageState &state,
                                          DiagnosticHandler &errStream,
                                          bool trackLocalBindings){
        if(!block){
            return;
        }
        pushSemanticUsageFrame(state);
        for(auto *stmt : block->body){
            analyzeSemanticUsageStmt(stmt,state,errStream,trackLocalBindings);
        }
        emitUnusedUsageWarningsForFrame(errStream,state.frames.back());
        popSemanticUsageFrame(state);
    }

    static void validateCallableSemanticUsage(ASTBlockStmt *block,
                                              const std::map<ASTIdentifier *,ASTType *> &params,
                                              DiagnosticHandler &errStream,
                                              const SemanticUsageState *outerState){
        if(!block){
            return;
        }
        SemanticUsageState state;
        if(outerState){
            state = *outerState;
        }
        pushSemanticUsageFrame(state,makeSemanticUsageParamBindings(params));
        analyzeSemanticUsageBlock(block,state,errStream,true);
        emitUnusedUsageWarningsForFrame(errStream,state.frames.back());
        popSemanticUsageFrame(state);
    }

    static bool paramComesBefore(ASTIdentifier *lhs,ASTIdentifier *rhs){
        if(lhs == rhs){
            return false;
        }
        if(!lhs){
            return false;
        }
        if(!rhs){
            return true;
        }
        if(lhs->codeRegion.startLine != rhs->codeRegion.startLine){
            return lhs->codeRegion.startLine < rhs->codeRegion.startLine;
        }
        if(lhs->codeRegion.startCol != rhs->codeRegion.startCol){
            return lhs->codeRegion.startCol < rhs->codeRegion.startCol;
        }
        return lhs->val < rhs->val;
    }

    static std::vector<std::pair<ASTIdentifier *,ASTType *>> orderedParamPairs(const std::map<ASTIdentifier *,ASTType *> &paramMap){
        std::vector<std::pair<ASTIdentifier *,ASTType *>> ordered;
        ordered.reserve(paramMap.size());
        for(auto &entry : paramMap){
            ordered.push_back(entry);
        }
        std::sort(ordered.begin(),ordered.end(),[](const auto &lhs,const auto &rhs){
            return paramComesBefore(lhs.first,rhs.first);
        });
        return ordered;
    }

    static bool regionHasLocation(const Region &region){
        return region.startLine > 0 || region.endLine > 0 || region.startCol > 0 || region.endCol > 0;
    }

    static Region deriveRegionFromStmt(ASTStmt *stmt);
    static Region deriveRegionFromExpr(ASTExpr *expr);
    static Semantics::SymbolTable::Entry *findTypeEntryNoDiag(Semantics::STableContext &symbolTableContext,
                                                               string_ref typeName,
                                                               std::shared_ptr<ASTScope> scope);

    static std::string deprecationKeyForNode(ASTStmt *diagNode,const std::string &symbolKey){
        auto region = deriveRegionFromStmt(diagNode);
        std::ostringstream out;
        out << symbolKey << "|"
            << (diagNode ? diagNode->parentFile : std::string()) << "|"
            << region.startLine << ":" << region.startCol << ":"
            << region.endLine << ":" << region.endCol;
        return out.str();
    }

    static Region deriveRegionFromDecl(ASTDecl *decl){
        if(!decl){
            return {};
        }
        if(regionHasLocation(decl->codeRegion)){
            return decl->codeRegion;
        }
        if(decl->type == VAR_DECL){
            auto *varDecl = (ASTVarDecl *)decl;
            if(!varDecl->specs.empty()){
                auto &spec = varDecl->specs.front();
                if(spec.id && regionHasLocation(spec.id->codeRegion)){
                    return spec.id->codeRegion;
                }
                if(spec.expr){
                    auto region = deriveRegionFromExpr(spec.expr);
                    if(regionHasLocation(region)){
                        return region;
                    }
                }
            }
        }
        else if(decl->type == FUNC_DECL){
            auto *funcDecl = (ASTFuncDecl *)decl;
            if(funcDecl->funcId && regionHasLocation(funcDecl->funcId->codeRegion)){
                return funcDecl->funcId->codeRegion;
            }
        }
        else if(decl->type == CLASS_DECL){
            auto *classDecl = (ASTClassDecl *)decl;
            if(classDecl->id && regionHasLocation(classDecl->id->codeRegion)){
                return classDecl->id->codeRegion;
            }
        }
        else if(decl->type == INTERFACE_DECL){
            auto *interfaceDecl = (ASTInterfaceDecl *)decl;
            if(interfaceDecl->id && regionHasLocation(interfaceDecl->id->codeRegion)){
                return interfaceDecl->id->codeRegion;
            }
        }
        else if(decl->type == TYPE_ALIAS_DECL){
            auto *aliasDecl = (ASTTypeAliasDecl *)decl;
            if(aliasDecl->id && regionHasLocation(aliasDecl->id->codeRegion)){
                return aliasDecl->id->codeRegion;
            }
        }
        else if(decl->type == RETURN_DECL){
            auto *retDecl = (ASTReturnDecl *)decl;
            if(retDecl->expr){
                auto region = deriveRegionFromExpr(retDecl->expr);
                if(regionHasLocation(region)){
                    return region;
                }
            }
        }
        else if(decl->type == COND_DECL){
            auto *condDecl = (ASTConditionalDecl *)decl;
            if(!condDecl->specs.empty() && condDecl->specs.front().expr){
                auto region = deriveRegionFromExpr(condDecl->specs.front().expr);
                if(regionHasLocation(region)){
                    return region;
                }
            }
        }
        else if(decl->type == FOR_DECL){
            auto *forDecl = (ASTForDecl *)decl;
            if(forDecl->expr){
                auto region = deriveRegionFromExpr(forDecl->expr);
                if(regionHasLocation(region)){
                    return region;
                }
            }
        }
        else if(decl->type == WHILE_DECL){
            auto *whileDecl = (ASTWhileDecl *)decl;
            if(whileDecl->expr){
                auto region = deriveRegionFromExpr(whileDecl->expr);
                if(regionHasLocation(region)){
                    return region;
                }
            }
        }
        else if(decl->type == SECURE_DECL){
            auto *secureDecl = (ASTSecureDecl *)decl;
            if(secureDecl->guardedDecl && !secureDecl->guardedDecl->specs.empty() && secureDecl->guardedDecl->specs.front().expr){
                auto region = deriveRegionFromExpr(secureDecl->guardedDecl->specs.front().expr);
                if(regionHasLocation(region)){
                    return region;
                }
            }
        }
        return {};
    }

    static Region deriveRegionFromExpr(ASTExpr *expr){
        if(!expr){
            return {};
        }
        if(regionHasLocation(expr->codeRegion)){
            return expr->codeRegion;
        }
        if(expr->id && regionHasLocation(expr->id->codeRegion)){
            return expr->id->codeRegion;
        }
        if(expr->callee){
            auto region = deriveRegionFromExpr(expr->callee);
            if(regionHasLocation(region)){
                return region;
            }
        }
        if(expr->leftExpr){
            auto region = deriveRegionFromExpr(expr->leftExpr);
            if(regionHasLocation(region)){
                return region;
            }
        }
        if(expr->rightExpr){
            auto region = deriveRegionFromExpr(expr->rightExpr);
            if(regionHasLocation(region)){
                return region;
            }
        }
        if(!expr->exprArrayData.empty()){
            auto region = deriveRegionFromExpr(expr->exprArrayData.front());
            if(regionHasLocation(region)){
                return region;
            }
        }
        return {};
    }

    static Region deriveRegionFromStmt(ASTStmt *stmt){
        if(!stmt){
            return {};
        }
        if(regionHasLocation(stmt->codeRegion)){
            return stmt->codeRegion;
        }
        if(stmt->type & DECL){
            return deriveRegionFromDecl((ASTDecl *)stmt);
        }
        if(stmt->type & EXPR){
            return deriveRegionFromExpr((ASTExpr *)stmt);
        }
        return {};
    }

    static ASTType *findFieldTypeByName(ASTClassDecl *classDecl,const std::string &fieldName){
        for(auto *fieldDecl : classDecl->fields){
            for(auto &spec : fieldDecl->specs){
                if(spec.id && spec.id->val == fieldName){
                    return spec.type;
                }
            }
        }
        return nullptr;
    }

    static ASTFuncDecl *findMethodByName(ASTClassDecl *classDecl,const std::string &methodName){
        for(auto *methodDecl : classDecl->methods){
            if(methodDecl->funcId && methodDecl->funcId->val == methodName){
                return methodDecl;
            }
        }
        return nullptr;
    }

    static Semantics::SymbolTable::Entry *findTypeEntryNoDiag(Semantics::STableContext &symbolTableContext,
                                                               string_ref typeName,
                                                               std::shared_ptr<ASTScope> scope);

    static ASTClassDecl *resolveClassDeclFromType(ASTType *type,
                                                  Semantics::STableContext &symbolTableContext,
                                                  std::shared_ptr<ASTScope> scope){
        if(!type){
            return nullptr;
        }
        auto *entry = findTypeEntryNoDiag(symbolTableContext,type->getName(),scope);
        if(!entry || entry->type != Semantics::SymbolTable::Entry::Class){
            return nullptr;
        }
        auto *classData = (Semantics::SymbolTable::Class *)entry->data;
        if(!classData || !classData->classType){
            return nullptr;
        }
        auto *parent = classData->classType->getParentNode();
        if(!parent || parent->type != CLASS_DECL){
            return nullptr;
        }
        return (ASTClassDecl *)parent;
    }

    static ASTType *findFieldTypeByNameRecursive(ASTClassDecl *classDecl,
                                                 Semantics::STableContext &symbolTableContext,
                                                 std::shared_ptr<ASTScope> scope,
                                                 const std::string &fieldName,
                                                 string_set &visited){
        if(!classDecl){
            return nullptr;
        }
        if(auto *fieldType = findFieldTypeByName(classDecl,fieldName)){
            return fieldType;
        }
        if(!classDecl->superClass){
            return nullptr;
        }
        auto superName = classDecl->superClass->getName();
        if(visited.find(superName.view()) != visited.end()){
            return nullptr;
        }
        visited.insert(superName.str());
        auto *superDecl = resolveClassDeclFromType(classDecl->superClass,symbolTableContext,scope);
        return findFieldTypeByNameRecursive(superDecl,symbolTableContext,scope,fieldName,visited);
    }

    static ASTFuncDecl *findMethodByNameRecursive(ASTClassDecl *classDecl,
                                                  Semantics::STableContext &symbolTableContext,
                                                  std::shared_ptr<ASTScope> scope,
                                                  const std::string &methodName,
                                                  string_set &visited){
        if(!classDecl){
            return nullptr;
        }
        if(auto *method = findMethodByName(classDecl,methodName)){
            return method;
        }
        if(!classDecl->superClass){
            return nullptr;
        }
        auto superName = classDecl->superClass->getName();
        if(visited.find(superName.view()) != visited.end()){
            return nullptr;
        }
        visited.insert(superName.str());
        auto *superDecl = resolveClassDeclFromType(classDecl->superClass,symbolTableContext,scope);
        return findMethodByNameRecursive(superDecl,symbolTableContext,scope,methodName,visited);
    }

    static bool typesStructurallyEqual(ASTType *lhs,ASTType *rhs){
        if(lhs == rhs){
            return true;
        }
        if(!lhs || !rhs){
            return false;
        }
        if(!(lhs->getName() == rhs->getName())){
            return false;
        }
        if(lhs->isGenericParam != rhs->isGenericParam){
            return false;
        }
        if(lhs->isOptional != rhs->isOptional){
            return false;
        }
        if(lhs->isThrowable != rhs->isThrowable){
            return false;
        }
        if(lhs->typeParams.size() != rhs->typeParams.size()){
            return false;
        }
        for(size_t i = 0;i < lhs->typeParams.size();++i){
            if(!typesStructurallyEqual(lhs->typeParams[i],rhs->typeParams[i])){
                return false;
            }
        }
        return true;
    }

    static bool methodParamsStructurallyMatch(const std::map<ASTIdentifier *,ASTType *> &classParams,
                                              const string_map<ASTType *> &requiredParams){
        if(classParams.size() != requiredParams.size()){
            return false;
        }
        for(auto &classParam : classParams){
            if(!classParam.first || !classParam.second){
                return false;
            }
            auto it = requiredParams.find(classParam.first->val);
            if(it == requiredParams.end() || !it->second){
                return false;
            }
            if(!typesStructurallyEqual(classParam.second,it->second)){
                return false;
            }
        }
        return true;
    }

    static Semantics::SymbolTable::GenericParam::Variance toSymbolGenericVariance(ASTGenericVariance variance){
        switch(variance){
            case ASTGenericVariance::In:
                return Semantics::SymbolTable::GenericParam::In;
            case ASTGenericVariance::Out:
                return Semantics::SymbolTable::GenericParam::Out;
            case ASTGenericVariance::Invariant:
            default:
                return Semantics::SymbolTable::GenericParam::Invariant;
        }
    }

    static void appendGenericParams(std::vector<Semantics::SymbolTable::GenericParam> &dest,
                                    const std::vector<ASTGenericParamDecl *> &src){
        for(auto *param : src){
            if(!param || !param->id){
                continue;
            }
            Semantics::SymbolTable::GenericParam meta;
            meta.name = param->id->val;
            meta.variance = toSymbolGenericVariance(param->variance);
            meta.defaultType = param->defaultType;
            meta.bounds = param->bounds;
            dest.push_back(std::move(meta));
        }
    }

    static string_set genericParamSet(const std::vector<ASTGenericParamDecl *> &params){
        string_set names;
        for(auto *param : params){
            if(param && param->id){
                names.insert(param->id->val);
            }
        }
        return names;
    }

    static bool isGenericParamName(const string_set *genericTypeParams,string_ref name){
        return genericTypeParams && genericTypeParams->find(name.view()) != genericTypeParams->end();
    }

    static ASTType *cloneTypeNode(ASTType *type,ASTStmt *parent){
        if(!type){
            return nullptr;
        }
        auto *cloned = ASTType::Create(type->getName(),parent,false,type->isAlias);
        cloned->isOptional = type->isOptional;
        cloned->isThrowable = type->isThrowable;
        cloned->isGenericParam = type->isGenericParam;
        for(auto *param : type->typeParams){
            auto *clonedParam = cloneTypeNode(param,parent);
            if(clonedParam){
                cloned->addTypeParam(clonedParam);
            }
        }
        return cloned;
    }

    static Semantics::SymbolTable::Entry *findFunctionEntryNoDiag(Semantics::STableContext &symbolTableContext,
                                                                   string_ref functionName,
                                                                   std::shared_ptr<ASTScope> scope){
        auto *entry = symbolTableContext.findEntryNoDiag(functionName,scope);
        if(entry && entry->type == Semantics::SymbolTable::Entry::Function){
            return entry;
        }
        entry = symbolTableContext.findEntryByEmittedNoDiag(functionName);
        if(entry && entry->type == Semantics::SymbolTable::Entry::Function){
            return entry;
        }
        return nullptr;
    }

    static ASTType *buildFunctionTypeFromFunctionData(Semantics::SymbolTable::Function *funcData,ASTStmt *parent){
        if(!funcData){
            return nullptr;
        }
        if(funcData->funcType && funcData->funcType->nameMatches(FUNCTION_TYPE)){
            auto *funcType = cloneTypeNode(funcData->funcType,parent);
            if(!funcType){
                return nullptr;
            }
            if(funcData->isLazy &&
               !funcType->typeParams.empty() &&
               funcType->typeParams.front() &&
               !funcType->typeParams.front()->nameMatches(TASK_TYPE)){
                auto *lazyReturn = ASTType::Create(TASK_TYPE->getName(),parent,false,false);
                lazyReturn->addTypeParam(cloneTypeNode(funcType->typeParams.front(),parent));
                funcType->typeParams[0] = lazyReturn;
            }
            return funcType;
        }
        auto *funcType = ASTType::Create(FUNCTION_TYPE->getName(),parent,false,false);
        auto *returnType = cloneTypeNode(funcData->returnType ? funcData->returnType : VOID_TYPE,parent);
        if(!returnType){
            return nullptr;
        }
        if(funcData->isLazy){
            auto *taskReturnType = ASTType::Create(TASK_TYPE->getName(),parent,false,false);
            taskReturnType->addTypeParam(returnType);
            funcType->addTypeParam(taskReturnType);
        }
        else {
            funcType->addTypeParam(returnType);
        }
        if(!funcData->orderedParams.empty()){
            for(auto &param : funcData->orderedParams){
                if(!param.second){
                    continue;
                }
                funcType->addTypeParam(cloneTypeNode(param.second,parent));
            }
        }
        else {
            for(auto &param : funcData->paramMap){
                if(!param.second){
                    continue;
                }
                funcType->addTypeParam(cloneTypeNode(param.second,parent));
            }
        }
        return funcType;
    }

    static void fillFunctionParamsFromDecl(Semantics::SymbolTable::Function *data,const std::map<ASTIdentifier *,ASTType *> &declParams){
        if(!data){
            return;
        }
        auto orderedDeclParams = orderedParamPairs(declParams);
        for(auto & param_pair : orderedDeclParams){
            if(!param_pair.first){
                continue;
            }
            data->paramMap.insert(std::make_pair(param_pair.first->val,param_pair.second));
            data->orderedParams.push_back(std::make_pair(param_pair.first->val,param_pair.second));
        }
    }

    static void applyDeprecationMetadata(Semantics::SymbolTable::Var *data,
                                         const std::vector<ASTAttribute> &attrs){
        if(!data){
            return;
        }
        data->isDeprecated = hasAttributeNamed(attrs,"deprecated");
        data->deprecationMessage = data->isDeprecated
            ? deprecatedMessageFromAttributes(attrs).value_or("")
            : "";
    }

    static void applyDeprecationMetadata(Semantics::SymbolTable::Function *data,
                                         const std::vector<ASTAttribute> &attrs){
        if(!data){
            return;
        }
        data->isDeprecated = hasAttributeNamed(attrs,"deprecated");
        data->deprecationMessage = data->isDeprecated
            ? deprecatedMessageFromAttributes(attrs).value_or("")
            : "";
    }

    static void applyDeprecationMetadata(Semantics::SymbolTable::Class *data,
                                         const std::vector<ASTAttribute> &attrs){
        if(!data){
            return;
        }
        data->isDeprecated = hasAttributeNamed(attrs,"deprecated");
        data->deprecationMessage = data->isDeprecated
            ? deprecatedMessageFromAttributes(attrs).value_or("")
            : "";
    }

    static void applyDeprecationMetadata(Semantics::SymbolTable::Interface *data,
                                         const std::vector<ASTAttribute> &attrs){
        if(!data){
            return;
        }
        data->isDeprecated = hasAttributeNamed(attrs,"deprecated");
        data->deprecationMessage = data->isDeprecated
            ? deprecatedMessageFromAttributes(attrs).value_or("")
            : "";
    }

    static void applyDeprecationMetadata(Semantics::SymbolTable::TypeAlias *data,
                                         const std::vector<ASTAttribute> &attrs){
        if(!data){
            return;
        }
        data->isDeprecated = hasAttributeNamed(attrs,"deprecated");
        data->deprecationMessage = data->isDeprecated
            ? deprecatedMessageFromAttributes(attrs).value_or("")
            : "";
    }

    struct ScopedAdditionalSymbolTable {
        Semantics::STableContext &context;
        bool active = false;

        ScopedAdditionalSymbolTable(Semantics::STableContext &contextRef,
                                    std::shared_ptr<Semantics::SymbolTable> table):
            context(contextRef){
            if(table){
                context.otherTables.push_back(std::move(table));
                active = true;
            }
        }

        ~ScopedAdditionalSymbolTable(){
            if(active && !context.otherTables.empty()){
                context.otherTables.pop_back();
            }
        }
    };

    static void addTempDeclEntryForSelfReference(ASTDecl *decl,Semantics::SymbolTable *tablePtr){
        if(!decl || !tablePtr){
            return;
        }
        switch(decl->type){
            case FUNC_DECL: {
                auto *funcDecl = (ASTFuncDecl *)decl;
                if(!funcDecl->funcId){
                    return;
                }
                auto sourceName = funcDecl->funcId->val;
                auto emittedName = buildEmittedName(decl->scope,sourceName);
                auto *entry = tablePtr->allocate<Semantics::SymbolTable::Entry>();
                auto *data = tablePtr->allocate<Semantics::SymbolTable::Function>();
                data->name = sourceName;
                data->returnType = funcDecl->returnType ? funcDecl->returnType : VOID_TYPE;
                data->funcType = funcDecl->funcType;
                data->isLazy = funcDecl->isLazy;
                applyDeprecationMetadata(data,funcDecl->attributes);
                appendGenericParams(data->genericParams,funcDecl->genericParams);
                fillFunctionParamsFromDecl(data,funcDecl->params);
                data->funcType = buildFunctionTypeFromFunctionData(data,funcDecl);
                entry->name = sourceName;
                entry->emittedName = emittedName;
                entry->type = Semantics::SymbolTable::Entry::Function;
                entry->data = data;
                tablePtr->addSymbolInScope(entry,decl->scope);
                return;
            }
            case CLASS_DECL: {
                auto *classDecl = (ASTClassDecl *)decl;
                if(!classDecl->id){
                    return;
                }
                auto sourceName = classDecl->id->val;
                auto emittedName = buildEmittedName(decl->scope,sourceName);
                auto *entry = tablePtr->allocate<Semantics::SymbolTable::Entry>();
                auto *data = tablePtr->allocate<Semantics::SymbolTable::Class>();
                entry->name = sourceName;
                entry->emittedName = emittedName;
                entry->type = Semantics::SymbolTable::Entry::Class;
                entry->data = data;
                data->classType = classDecl->classType ? classDecl->classType : ASTType::Create(sourceName,classDecl,false,false);
                data->superClassType = classDecl->superClass;
                data->interfaces = classDecl->interfaces;
                applyDeprecationMetadata(data,classDecl->attributes);
                appendGenericParams(data->genericParams,classDecl->genericParams);
                for(auto *fieldDecl : classDecl->fields){
                    if(!fieldDecl){
                        continue;
                    }
                    bool readonlyField = fieldDecl->isConst;
                    for(auto &attr : fieldDecl->attributes){
                        if(attr.name == "readonly"){
                            readonlyField = true;
                            break;
                        }
                    }
                    for(auto &spec : fieldDecl->specs){
                        if(!spec.id){
                            continue;
                        }
                        auto *field = tablePtr->allocate<Semantics::SymbolTable::Var>();
                        field->name = spec.id->val;
                        field->type = spec.type;
                        field->isReadonly = readonlyField;
                        applyDeprecationMetadata(field,fieldDecl->attributes);
                        data->fields.push_back(field);
                    }
                }
                for(auto *methodDecl : classDecl->methods){
                    if(!methodDecl || !methodDecl->funcId){
                        continue;
                    }
                    auto *method = tablePtr->allocate<Semantics::SymbolTable::Function>();
                    method->name = methodDecl->funcId->val;
                    method->returnType = methodDecl->returnType;
                    method->funcType = methodDecl->funcType;
                    method->isLazy = methodDecl->isLazy;
                    applyDeprecationMetadata(method,methodDecl->attributes);
                    appendGenericParams(method->genericParams,methodDecl->genericParams);
                    fillFunctionParamsFromDecl(method,methodDecl->params);
                    method->funcType = buildFunctionTypeFromFunctionData(method,methodDecl);
                    data->instMethods.push_back(method);
                }
                for(auto *ctorDecl : classDecl->constructors){
                    if(!ctorDecl){
                        continue;
                    }
                    auto *ctor = tablePtr->allocate<Semantics::SymbolTable::Function>();
                    ctor->name = "__ctor__" + std::to_string(ctorDecl->params.size());
                    ctor->returnType = VOID_TYPE;
                    ctor->funcType = data->classType;
                    appendGenericParams(ctor->genericParams,ctorDecl->genericParams);
                    fillFunctionParamsFromDecl(ctor,ctorDecl->params);
                    data->constructors.push_back(ctor);
                }
                tablePtr->addSymbolInScope(entry,decl->scope);
                return;
            }
            case INTERFACE_DECL: {
                auto *interfaceDecl = (ASTInterfaceDecl *)decl;
                if(!interfaceDecl->id){
                    return;
                }
                auto sourceName = interfaceDecl->id->val;
                auto emittedName = buildEmittedName(decl->scope,sourceName);
                auto *entry = tablePtr->allocate<Semantics::SymbolTable::Entry>();
                auto *data = tablePtr->allocate<Semantics::SymbolTable::Interface>();
                entry->name = sourceName;
                entry->emittedName = emittedName;
                entry->type = Semantics::SymbolTable::Entry::Interface;
                entry->data = data;
                data->interfaceType = interfaceDecl->interfaceType ? interfaceDecl->interfaceType
                                                                    : ASTType::Create(sourceName,interfaceDecl,false,false);
                applyDeprecationMetadata(data,interfaceDecl->attributes);
                appendGenericParams(data->genericParams,interfaceDecl->genericParams);
                for(auto *fieldDecl : interfaceDecl->fields){
                    if(!fieldDecl){
                        continue;
                    }
                    for(auto &spec : fieldDecl->specs){
                        if(!spec.id){
                            continue;
                        }
                        auto *field = tablePtr->allocate<Semantics::SymbolTable::Var>();
                        field->name = spec.id->val;
                        field->type = spec.type;
                        field->isReadonly = fieldDecl->isConst;
                        applyDeprecationMetadata(field,fieldDecl->attributes);
                        data->fields.push_back(field);
                    }
                }
                for(auto *methodDecl : interfaceDecl->methods){
                    if(!methodDecl || !methodDecl->funcId){
                        continue;
                    }
                    auto *method = tablePtr->allocate<Semantics::SymbolTable::Function>();
                    method->name = methodDecl->funcId->val;
                    method->returnType = methodDecl->returnType;
                    method->funcType = methodDecl->funcType;
                    method->isLazy = methodDecl->isLazy;
                    applyDeprecationMetadata(method,methodDecl->attributes);
                    appendGenericParams(method->genericParams,methodDecl->genericParams);
                    fillFunctionParamsFromDecl(method,methodDecl->params);
                    method->funcType = buildFunctionTypeFromFunctionData(method,methodDecl);
                    data->methods.push_back(method);
                }
                tablePtr->addSymbolInScope(entry,decl->scope);
                return;
            }
            case TYPE_ALIAS_DECL: {
                auto *aliasDecl = (ASTTypeAliasDecl *)decl;
                if(!aliasDecl->id){
                    return;
                }
                auto sourceName = aliasDecl->id->val;
                auto emittedName = buildEmittedName(decl->scope,sourceName);
                auto *entry = tablePtr->allocate<Semantics::SymbolTable::Entry>();
                auto *data = tablePtr->allocate<Semantics::SymbolTable::TypeAlias>();
                entry->name = sourceName;
                entry->emittedName = emittedName;
                entry->type = Semantics::SymbolTable::Entry::TypeAlias;
                entry->data = data;
                data->aliasType = aliasDecl->aliasedType;
                applyDeprecationMetadata(data,aliasDecl->attributes);
                appendGenericParams(data->genericParams,aliasDecl->genericParams);
                tablePtr->addSymbolInScope(entry,decl->scope);
                return;
            }
            default:
                return;
        }
    }

static ASTType *substituteTypeParams(ASTType *type,
                                         const string_map<ASTType *> &bindings,
                                         ASTStmt *parent){
        if(!type){
            return nullptr;
        }
        auto bound = bindings.find(type->getName().view());
        if(type->isGenericParam && bound != bindings.end()){
            auto *replacement = cloneTypeNode(bound->second,parent);
            if(!replacement){
                return nullptr;
            }
            replacement->isOptional = replacement->isOptional || type->isOptional;
            replacement->isThrowable = replacement->isThrowable || type->isThrowable;
            return replacement;
        }
        auto *result = ASTType::Create(type->getName(),parent,false,type->isAlias);
        result->isOptional = type->isOptional;
        result->isThrowable = type->isThrowable;
        result->isGenericParam = type->isGenericParam;
        for(auto *param : type->typeParams){
            auto *subParam = substituteTypeParams(param,bindings,parent);
            if(subParam){
                result->addTypeParam(subParam);
            }
        }
        return result;
    }

    static Semantics::SymbolTable::Entry *findTypeEntryNoDiag(Semantics::STableContext &symbolTableContext,
                                                               string_ref typeName,
                                                               std::shared_ptr<ASTScope> scope){
        auto *entry = symbolTableContext.findEntryNoDiag(typeName,scope);
        if(entry && (entry->type == Semantics::SymbolTable::Entry::Class
                     || entry->type == Semantics::SymbolTable::Entry::Interface
                     || entry->type == Semantics::SymbolTable::Entry::TypeAlias)){
            return entry;
        }
        entry = symbolTableContext.findEntryByEmittedNoDiag(typeName);
        if(entry && (entry->type == Semantics::SymbolTable::Entry::Class
                     || entry->type == Semantics::SymbolTable::Entry::Interface
                     || entry->type == Semantics::SymbolTable::Entry::TypeAlias)){
            return entry;
        }
        return nullptr;
    }

    static ASTType *canonicalizeBuiltinAliasType(ASTType *type){
        return type;
    }

    static bool isNumericTypeNode(ASTType *type){
        return type && (type->nameMatches(INT_TYPE)
                        || type->nameMatches(LONG_TYPE)
                        || type->nameMatches(FLOAT_TYPE)
                        || type->nameMatches(DOUBLE_TYPE));
    }

    static bool isWideningNumericCast(ASTType *expected,ASTType *actual){
        if(!expected || !actual || !isNumericTypeNode(expected) || !isNumericTypeNode(actual)){
            return false;
        }
        if(expected->nameMatches(actual)){
            return false;
        }
        if(expected->nameMatches(LONG_TYPE) && actual->nameMatches(INT_TYPE)){
            return true;
        }
        if(expected->nameMatches(FLOAT_TYPE) && actual->nameMatches(INT_TYPE)){
            return true;
        }
        if(expected->nameMatches(DOUBLE_TYPE) &&
           (actual->nameMatches(INT_TYPE) || actual->nameMatches(LONG_TYPE) || actual->nameMatches(FLOAT_TYPE))){
            return true;
        }
        return false;
    }

    static bool applyImplicitNumericCast(ASTType *expected,ASTType *actual,ASTExpr *expr){
        if(!expr || !isWideningNumericCast(expected,actual)){
            return false;
        }
        expr->runtimeCastTargetName = expected->getName().str();
        return true;
    }

    static ASTType *resolveAliasType(ASTType *type,
                                     Semantics::STableContext &symbolTableContext,
                                     std::shared_ptr<ASTScope> scope,
                                     const string_set *genericTypeParams,
                                     string_set &visiting){
        if(!type){
            return nullptr;
        }

        auto *resolved = ASTType::Create(type->getName(),type->getParentNode(),false,type->isAlias);
        resolved->isOptional = type->isOptional;
        resolved->isThrowable = type->isThrowable;
        resolved->isGenericParam = type->isGenericParam;
        for(auto *param : type->typeParams){
            auto *resolvedParam = resolveAliasType(param,symbolTableContext,scope,genericTypeParams,visiting);
            if(resolvedParam){
                resolved->addTypeParam(resolvedParam);
            }
        }

        if(isGenericParamName(genericTypeParams,resolved->getName()) || resolved->isGenericParam){
            resolved->isGenericParam = true;
            return resolved;
        }

        resolved = canonicalizeBuiltinAliasType(resolved);

        auto *entry = findTypeEntryNoDiag(symbolTableContext,resolved->getName(),scope);
        if(!entry || entry->type != Semantics::SymbolTable::Entry::TypeAlias){
            return resolved;
        }
        auto key = entry->emittedName.empty()? entry->name : entry->emittedName;
        if(visiting.find(key) != visiting.end()){
            return resolved;
        }
        auto *aliasData = (Semantics::SymbolTable::TypeAlias *)entry->data;
        if(!aliasData || !aliasData->aliasType){
            return resolved;
        }
        if(aliasData->genericParams.size() < resolved->typeParams.size()){
            return resolved;
        }

        string_map<ASTType *> bindings;
        for(size_t i = 0;i < resolved->typeParams.size();++i){
            bindings.insert(std::make_pair(aliasData->genericParams[i].name,resolved->typeParams[i]));
        }
        for(size_t i = resolved->typeParams.size();i < aliasData->genericParams.size();++i){
            auto &param = aliasData->genericParams[i];
            if(!param.defaultType){
                return resolved;
            }
            auto *materializedDefault = substituteTypeParams(param.defaultType,bindings,type->getParentNode());
            if(!materializedDefault){
                return resolved;
            }
            materializedDefault = resolveAliasType(materializedDefault,symbolTableContext,scope,genericTypeParams,visiting);
            if(!materializedDefault){
                return resolved;
            }
            resolved->addTypeParam(materializedDefault);
            bindings[param.name] = materializedDefault;
        }
        visiting.insert(key);
        auto *substituted = substituteTypeParams(aliasData->aliasType,bindings,type->getParentNode());
        if(substituted){
            substituted->isOptional = substituted->isOptional || resolved->isOptional;
            substituted->isThrowable = substituted->isThrowable || resolved->isThrowable;
        }
        auto *finalType = resolveAliasType(substituted,symbolTableContext,scope,genericTypeParams,visiting);
        visiting.erase(key);
        return finalType? finalType : resolved;
    }

    static ASTType *resolveAliasType(ASTType *type,
                                     Semantics::STableContext &symbolTableContext,
                                     std::shared_ptr<ASTScope> scope,
                                     const string_set *genericTypeParams){
        string_set visiting;
        return resolveAliasType(type,symbolTableContext,scope,genericTypeParams,visiting);
    }

    DiagnosticPtr SemanticADiagnostic::create(string_ref message, ASTStmt *stmt, Type ty){
        auto region = deriveRegionFromStmt(stmt);
        DiagnosticPtr created = nullptr;
        if(ty == Diagnostic::Warning){
            if(regionHasLocation(region)){
                created = StandardDiagnostic::createWarning(message,region);
            }
            else {
                created = StandardDiagnostic::createWarning(message);
            }
        }
        else if(regionHasLocation(region)){
            created = StandardDiagnostic::createError(message,region);
        }
        else {
            created = StandardDiagnostic::createError(message);
        }
        if(created){
            created->phase = Diagnostic::Phase::Semantic;
            created->sourceName = "starbytes-sema";
            created->code = (ty == Diagnostic::Warning) ? "SB-SEMA-W0001" : "SB-SEMA-E0001";
        }
        return created;
    }

    SemanticA::SemanticA(Syntax::SyntaxA & syntaxARef,DiagnosticHandler & errStream):syntaxARef(syntaxARef),errStream(errStream){
            
    }

    void SemanticA::setPrefer64BitNumberInference(bool enabled){
        prefer64BitNumberInference = enabled;
    }

    bool SemanticA::getPrefer64BitNumberInference() const{
        return prefer64BitNumberInference;
    }

    void SemanticA::warnDeprecatedUse(const std::string &symbolKind,
                                      const std::string &symbolName,
                                      const std::string &symbolKey,
                                      const std::string &message,
                                      ASTStmt *diagNode){
        if(symbolName.empty() || !diagNode){
            return;
        }
        auto key = deprecationKeyForNode(diagNode,symbolKey.empty() ? symbolName : symbolKey);
        if(!emittedDeprecationWarningKeys.insert(key).second){
            return;
        }
        std::ostringstream ss;
        ss << "Use of deprecated " << symbolKind << " `" << symbolName << "`.";
        if(!message.empty()){
            ss << " " << message;
        }
        errStream.push(SemanticADiagnostic::create(ss.str(),diagNode,Diagnostic::Warning));
    }

    void SemanticA::start(){
        gNamespaceFlowStates[this] = {};
        pushSemanticFlowFrame(gNamespaceFlowStates[this]);
        gNamespaceUsageStates[this] = {};
        pushSemanticUsageFrame(gNamespaceUsageStates[this]);
        emittedDeprecationWarningKeys.clear();
    }

    void SemanticA::finish(){
        auto usageIt = gNamespaceUsageStates.find(this);
        if(usageIt != gNamespaceUsageStates.end()){
            for(const auto &frame : usageIt->second.frames){
                emitUnusedUsageWarningsForFrame(errStream,frame);
            }
            gNamespaceUsageStates.erase(usageIt);
        }
        gNamespaceFlowStates.erase(this);
        emittedDeprecationWarningKeys.clear();
    }
     /// Only registers new symbols associated with top level decls!
    void SemanticA::addSTableEntryForDecl(ASTDecl *decl,Semantics::SymbolTable *tablePtr){
       
        auto buildVarEntry = [&](ASTVarDecl::VarSpec &spec,std::shared_ptr<ASTScope> scope,bool isReadonly){
            std::string sourceName = spec.id->val;
            std::string emittedName = buildEmittedName(scope,sourceName);
            auto *e = tablePtr->allocate<Semantics::SymbolTable::Entry>();
            auto data = tablePtr->allocate<Semantics::SymbolTable::Var>();
            data->name = sourceName;
            data->type = spec.type;
            data->isReadonly = isReadonly;
            applyDeprecationMetadata(data,decl->attributes);
            e->name = sourceName;
            e->emittedName = emittedName;
            e->data = data;
            e->type = Semantics::SymbolTable::Entry::Var;
            spec.id->val = emittedName;
            return e;
        };
        
        auto buildFuncEntry = [&](ASTFuncDecl *func,std::shared_ptr<ASTScope> scope){
            std::string sourceName = func->funcId->val;
            std::string emittedName = buildEmittedName(scope,sourceName);
            auto *e = tablePtr->allocate<Semantics::SymbolTable::Entry>();
            auto data = tablePtr->allocate<Semantics::SymbolTable::Function>();
            data->name = sourceName;
            data->returnType = func->returnType;
            data->funcType = func->funcType;
            data->isLazy = func->isLazy;
            applyDeprecationMetadata(data,func->attributes);
            appendGenericParams(data->genericParams,func->genericParams);
            fillFunctionParamsFromDecl(data,func->params);
            data->funcType = buildFunctionTypeFromFunctionData(data,func);
            
            e->name = sourceName;
            e->emittedName = emittedName;
            e->data = data;
            e->type = Semantics::SymbolTable::Entry::Function;
            func->funcId->val = emittedName;
            return e;
        };
        
        switch (decl->type) {
            case VAR_DECL : {
                auto *varDecl = (ASTVarDecl *)decl;
                for(auto & spec : varDecl->specs) {
                    tablePtr->addSymbolInScope(buildVarEntry(spec,varDecl->scope,varDecl->isConst),varDecl->scope);
                }
                break;
            }
            case FUNC_DECL : {
                auto *funcDecl = (ASTFuncDecl *)decl;
                tablePtr->addSymbolInScope(buildFuncEntry(funcDecl,decl->scope),decl->scope);
                break;
            }
             case CLASS_DECL : {
                 auto classDecl = (ASTClassDecl *)decl;
                 std::string sourceName = classDecl->id->val;
                 std::string emittedName = buildEmittedName(decl->scope,sourceName);
                 auto *e = tablePtr->allocate<Semantics::SymbolTable::Entry>();
                 e->name = sourceName;
                 e->emittedName = emittedName;
                 e->type = Semantics::SymbolTable::Entry::Class;
                 auto *data = tablePtr->allocate<Semantics::SymbolTable::Class>();
                 e->data = data;
                 data->classType = ASTType::Create(emittedName.c_str(),classDecl,false,false);
                 applyDeprecationMetadata(data,classDecl->attributes);
                 for(auto *genericParam : classDecl->genericParams){
                     if(!genericParam || !genericParam->id){
                         continue;
                     }
                     Semantics::SymbolTable::GenericParam meta;
                     meta.name = genericParam->id->val;
                     meta.variance = toSymbolGenericVariance(genericParam->variance);
                     meta.defaultType = genericParam->defaultType;
                     meta.bounds = genericParam->bounds;
                     data->genericParams.push_back(std::move(meta));
                     auto *paramType = ASTType::Create(genericParam->id->val,classDecl,true,false);
                     paramType->isGenericParam = true;
                     data->classType->addTypeParam(paramType);
                 }
                 data->superClassType = classDecl->superClass;
                 data->interfaces = classDecl->interfaces;
                 classDecl->classType = data->classType;
                 classDecl->id->val = emittedName;
                 for(auto & f : classDecl->fields){
                     bool readonlyField = f->isConst;
                     for(auto &attr : f->attributes){
                         if(attr.name == "readonly"){
                             readonlyField = true;
                             break;
                         }
                     }
                     for(auto & v_spec : f->specs){
                         auto *field = tablePtr->allocate<Semantics::SymbolTable::Var>();
                         field->name = v_spec.id->val;
                         field->type = v_spec.type;
                         field->isReadonly = readonlyField;
                         applyDeprecationMetadata(field,f->attributes);
                         data->fields.push_back(field);
                     }
                 }
                 for(auto & m : classDecl->methods){
                     auto *method = tablePtr->allocate<Semantics::SymbolTable::Function>();
                     method->name = m->funcId->val;
                     method->returnType = m->returnType;
                     method->funcType = m->funcType;
                     method->isLazy = m->isLazy;
                     applyDeprecationMetadata(method,m->attributes);
                     appendGenericParams(method->genericParams,m->genericParams);
                     fillFunctionParamsFromDecl(method,m->params);
                     method->funcType = buildFunctionTypeFromFunctionData(method,m);
                     data->instMethods.push_back(method);
                 }
                 for(auto & c : classDecl->constructors){
                     auto *ctor = tablePtr->allocate<Semantics::SymbolTable::Function>();
                     ctor->name = "__ctor__" + std::to_string(c->params.size());
                     ctor->returnType = VOID_TYPE;
                     ctor->funcType = data->classType;
                     appendGenericParams(ctor->genericParams,c->genericParams);
                     fillFunctionParamsFromDecl(ctor,c->params);
                     data->constructors.push_back(ctor);
                 }
                 tablePtr->addSymbolInScope(e,decl->scope);
                 break;
             }
            case INTERFACE_DECL : {
                auto *interfaceDecl = (ASTInterfaceDecl *)decl;
                std::string sourceName = interfaceDecl->id->val;
                std::string emittedName = buildEmittedName(decl->scope,sourceName);
                auto *e = tablePtr->allocate<Semantics::SymbolTable::Entry>();
                e->name = sourceName;
                e->emittedName = emittedName;
                e->type = Semantics::SymbolTable::Entry::Interface;
                auto *data = tablePtr->allocate<Semantics::SymbolTable::Interface>();
                e->data = data;
                data->interfaceType = ASTType::Create(emittedName.c_str(),interfaceDecl,false,false);
                applyDeprecationMetadata(data,interfaceDecl->attributes);
                for(auto *genericParam : interfaceDecl->genericParams){
                    if(!genericParam || !genericParam->id){
                        continue;
                    }
                    Semantics::SymbolTable::GenericParam meta;
                    meta.name = genericParam->id->val;
                    meta.variance = toSymbolGenericVariance(genericParam->variance);
                    meta.defaultType = genericParam->defaultType;
                    meta.bounds = genericParam->bounds;
                    data->genericParams.push_back(std::move(meta));
                    auto *paramType = ASTType::Create(genericParam->id->val,interfaceDecl,true,false);
                    paramType->isGenericParam = true;
                    data->interfaceType->addTypeParam(paramType);
                }
                interfaceDecl->interfaceType = data->interfaceType;
                interfaceDecl->id->val = emittedName;

                for(auto *fieldDecl : interfaceDecl->fields){
                    for(auto &spec : fieldDecl->specs){
                        auto *field = tablePtr->allocate<Semantics::SymbolTable::Var>();
                        field->name = spec.id->val;
                        field->type = spec.type;
                        field->isReadonly = fieldDecl->isConst;
                        applyDeprecationMetadata(field,fieldDecl->attributes);
                        data->fields.push_back(field);
                    }
                }

                for(auto *methodDecl : interfaceDecl->methods){
                    auto *method = tablePtr->allocate<Semantics::SymbolTable::Function>();
                    method->name = methodDecl->funcId->val;
                    method->returnType = methodDecl->returnType;
                    method->funcType = methodDecl->funcType;
                    method->isLazy = methodDecl->isLazy;
                    applyDeprecationMetadata(method,methodDecl->attributes);
                    appendGenericParams(method->genericParams,methodDecl->genericParams);
                    fillFunctionParamsFromDecl(method,methodDecl->params);
                    method->funcType = buildFunctionTypeFromFunctionData(method,methodDecl);
                    data->methods.push_back(method);
                }
                tablePtr->addSymbolInScope(e,decl->scope);
                break;
            }
            case TYPE_ALIAS_DECL : {
                auto *aliasDecl = (ASTTypeAliasDecl *)decl;
                auto sourceName = aliasDecl->id->val;
                auto emittedName = buildEmittedName(decl->scope,sourceName);
                auto *entry = tablePtr->allocate<Semantics::SymbolTable::Entry>();
                auto *data = tablePtr->allocate<Semantics::SymbolTable::TypeAlias>();
                entry->name = sourceName;
                entry->emittedName = emittedName;
                entry->type = Semantics::SymbolTable::Entry::TypeAlias;
                entry->data = data;
                data->aliasType = aliasDecl->aliasedType;
                applyDeprecationMetadata(data,aliasDecl->attributes);
                appendGenericParams(data->genericParams,aliasDecl->genericParams);
                aliasDecl->id->val = emittedName;
                tablePtr->addSymbolInScope(entry,decl->scope);
                break;
            }
            case IMPORT_DECL : {
                auto *importDecl = (ASTImportDecl *)decl;
                if(importDecl->moduleName && !importDecl->moduleName->val.empty()){
                    tablePtr->importModule(importDecl->moduleName->val);
                }
                break;
            }
            case SCOPE_DECL : {
                auto *scopeDecl = (ASTScopeDecl *)decl;
                auto *entry = tablePtr->allocate<Semantics::SymbolTable::Entry>();
                entry->name = scopeDecl->scopeId->val;
                entry->emittedName = entry->name;
                entry->type = Semantics::SymbolTable::Entry::Scope;
                entry->data = tablePtr->allocate<std::shared_ptr<ASTScope>>(scopeDecl->blockStmt->parentScope);
                tablePtr->addSymbolInScope(entry,decl->scope);
                break;
            }
            case SECURE_DECL : {
                auto *secureDecl = (ASTSecureDecl *)decl;
                if(secureDecl->guardedDecl){
                    addSTableEntryForDecl(secureDecl->guardedDecl,tablePtr);
                }
                if(secureDecl->catchErrorId && secureDecl->catchBlock && secureDecl->catchBlock->parentScope){
                    auto *entry = tablePtr->allocate<Semantics::SymbolTable::Entry>();
                    auto *data = tablePtr->allocate<Semantics::SymbolTable::Var>();
                    data->name = secureDecl->catchErrorId->val;
                    data->type = secureDecl->catchErrorType ? secureDecl->catchErrorType : STRING_TYPE;
                    data->isReadonly = false;
                    entry->name = data->name;
                    entry->emittedName = buildEmittedName(secureDecl->catchBlock->parentScope,data->name);
                    entry->type = Semantics::SymbolTable::Entry::Var;
                    entry->data = data;
                    tablePtr->addSymbolInScope(entry,secureDecl->catchBlock->parentScope);
                }
                break;
            }
        default : {
            break;
        }
        }
    }


    bool SemanticA::typeExists(ASTType *type,
                               Semantics::STableContext &contextTableContext,
                               std::shared_ptr<ASTScope> scope,
                               const string_set *genericTypeParams,
                               ASTStmt *diagNode){
        if(!type){
            return false;
        }

        for(auto *param : type->typeParams){
            if(!typeExists(param,contextTableContext,scope,genericTypeParams,diagNode ? diagNode : (ASTStmt *)type->getParentNode())){
                return false;
            }
        }

        if(isGenericParamName(genericTypeParams,type->getName()) || type->isGenericParam){
            if(!type->typeParams.empty()){
                errStream.push(SemanticADiagnostic::create("Generic type parameters cannot have nested type arguments.",diagNode ? diagNode : (ASTStmt *)type->getParentNode(),Diagnostic::Error));
                return false;
            }
            return true;
        }

        if(type->nameMatches(TASK_TYPE)){
            if(type->typeParams.size() != 1){
                errStream.push(SemanticADiagnostic::create("Type `Task` expects exactly one type argument.",diagNode ? diagNode : (ASTStmt *)type->getParentNode(),Diagnostic::Error));
                return false;
            }
            return true;
        }

        if(type->nameMatches(MAP_TYPE)){
            if(type->typeParams.size() != 2){
                errStream.push(SemanticADiagnostic::create("Type `Map` expects exactly two type arguments.",diagNode ? diagNode : (ASTStmt *)type->getParentNode(),Diagnostic::Error));
                return false;
            }
            auto *keyType = resolveAliasType(type->typeParams.front(),contextTableContext,scope,genericTypeParams);
            bool keyIsValid = keyType && (keyType->isGenericParam
                                          || keyType->nameMatches(ANY_TYPE)
                                          || keyType->nameMatches(STRING_TYPE)
                                          || keyType->nameMatches(INT_TYPE)
                                          || keyType->nameMatches(LONG_TYPE)
                                          || keyType->nameMatches(FLOAT_TYPE)
                                          || keyType->nameMatches(DOUBLE_TYPE));
            if(!keyIsValid){
                errStream.push(SemanticADiagnostic::create("Type `Map` key must be String/Int/Long/Float/Double (or generic).",diagNode ? diagNode : (ASTStmt *)type->getParentNode(),Diagnostic::Error));
                return false;
            }
            return true;
        }

        if(type->nameMatches(FUNCTION_TYPE)){
            if(type->typeParams.empty()){
                errStream.push(SemanticADiagnostic::create("Function type must include a return type.",diagNode ? diagNode : (ASTStmt *)type->getParentNode(),Diagnostic::Error));
                return false;
            }
            return true;
        }

        if(type->nameMatches(VOID_TYPE) ||
           type->nameMatches(STRING_TYPE) ||
           type->nameMatches(ARRAY_TYPE) ||
           type->nameMatches(DICTIONARY_TYPE) ||
           type->nameMatches(MAP_TYPE) ||
           type->nameMatches(BOOL_TYPE) ||
           type->nameMatches(INT_TYPE) ||
           type->nameMatches(FLOAT_TYPE) ||
           type->nameMatches(LONG_TYPE) ||
           type->nameMatches(DOUBLE_TYPE) ||
           type->nameMatches(ANY_TYPE) ||
           type->nameMatches(REGEX_TYPE)){
            return true;
        }

        auto *entry = findTypeEntryNoDiag(contextTableContext,type->getName(),scope);
        if(!entry){
            std::ostringstream ss;
            ss << "Unknown type `" << type->getName() << "`.";
            errStream.push(SemanticADiagnostic::create(ss.str(),diagNode ? diagNode : (ASTStmt *)type->getParentNode(),Diagnostic::Error));
            return false;
        }
        if(entry->type == Semantics::SymbolTable::Entry::Class){
            auto *classData = (Semantics::SymbolTable::Class *)entry->data;
            if(classData && classData->isDeprecated){
                warnDeprecatedUse("class",
                                  entry->name,
                                  !entry->emittedName.empty() ? entry->emittedName : entry->name,
                                  classData->deprecationMessage,
                                  diagNode ? diagNode : (ASTStmt *)type->getParentNode());
            }
        }
        else if(entry->type == Semantics::SymbolTable::Entry::Interface){
            auto *interfaceData = (Semantics::SymbolTable::Interface *)entry->data;
            if(interfaceData && interfaceData->isDeprecated){
                warnDeprecatedUse("interface",
                                  entry->name,
                                  !entry->emittedName.empty() ? entry->emittedName : entry->name,
                                  interfaceData->deprecationMessage,
                                  diagNode ? diagNode : (ASTStmt *)type->getParentNode());
            }
        }
        else if(entry->type == Semantics::SymbolTable::Entry::TypeAlias){
            auto *aliasData = (Semantics::SymbolTable::TypeAlias *)entry->data;
            if(aliasData && aliasData->isDeprecated){
                warnDeprecatedUse("type alias",
                                  entry->name,
                                  !entry->emittedName.empty() ? entry->emittedName : entry->name,
                                  aliasData->deprecationMessage,
                                  diagNode ? diagNode : (ASTStmt *)type->getParentNode());
            }
        }

        size_t expectedArity = 0;
        const std::vector<Semantics::SymbolTable::GenericParam> *declaredGenericParams = nullptr;
        if(entry->type == Semantics::SymbolTable::Entry::Class){
            auto *classData = (Semantics::SymbolTable::Class *)entry->data;
            if(classData){
                expectedArity = classData->genericParams.size();
                declaredGenericParams = &classData->genericParams;
            }
        }
        else if(entry->type == Semantics::SymbolTable::Entry::Interface){
            auto *interfaceData = (Semantics::SymbolTable::Interface *)entry->data;
            if(interfaceData){
                expectedArity = interfaceData->genericParams.size();
                declaredGenericParams = &interfaceData->genericParams;
            }
        }
        else if(entry->type == Semantics::SymbolTable::Entry::TypeAlias){
            auto *aliasData = (Semantics::SymbolTable::TypeAlias *)entry->data;
            if(aliasData){
                expectedArity = aliasData->genericParams.size();
                declaredGenericParams = &aliasData->genericParams;
            }
        }

        if(type->typeParams.size() > expectedArity){
            std::ostringstream ss;
            ss << "Type `" << type->getName() << "` expects " << expectedArity
               << " type argument(s), but got " << type->typeParams.size() << ".";
            errStream.push(SemanticADiagnostic::create(ss.str(),diagNode ? diagNode : (ASTStmt *)type->getParentNode(),Diagnostic::Error));
            return false;
        }
        if(declaredGenericParams && type->typeParams.size() < expectedArity){
            string_map<ASTType *> bindings;
            for(size_t i = 0;i < type->typeParams.size();++i){
                bindings.insert(std::make_pair((*declaredGenericParams)[i].name,type->typeParams[i]));
            }
            for(size_t i = type->typeParams.size();i < declaredGenericParams->size();++i){
                const auto &param = (*declaredGenericParams)[i];
                if(!param.defaultType){
                    std::ostringstream ss;
                    ss << "Type `" << type->getName() << "` expects " << expectedArity
                       << " type argument(s), but got " << type->typeParams.size() << ".";
                    errStream.push(SemanticADiagnostic::create(ss.str(),diagNode ? diagNode : (ASTStmt *)type->getParentNode(),Diagnostic::Error));
                    return false;
                }
                auto *materializedDefault = substituteTypeParams(param.defaultType,bindings,diagNode ? diagNode : (ASTStmt *)type->getParentNode());
                if(!materializedDefault){
                    errStream.push(SemanticADiagnostic::create("Failed to materialize generic default type argument.",diagNode ? diagNode : (ASTStmt *)type->getParentNode(),Diagnostic::Error));
                    return false;
                }
                if(!typeExists(materializedDefault,contextTableContext,scope,genericTypeParams,diagNode)){
                    return false;
                }
                materializedDefault = resolveAliasType(materializedDefault,contextTableContext,scope,genericTypeParams);
                type->addTypeParam(materializedDefault);
                bindings[param.name] = materializedDefault;
            }
        }
        return true;
    }



    bool SemanticA::typeMatches(ASTType *type,
                                ASTExpr *expr_to_eval,
                                Semantics::STableContext & symbolTableContext,
                                ASTScopeSemanticsContext & scopeContext,
                                const char *contextLabel){
        
        auto other_type_id = evalExprForTypeId(expr_to_eval,symbolTableContext,scopeContext);
        if(!other_type_id){
            return false;
        };

        auto *resolvedType = resolveAliasType(type,symbolTableContext,scopeContext.scope,scopeContext.genericTypeParams);
        auto *resolvedOtherType = resolveAliasType(other_type_id,symbolTableContext,scopeContext.scope,scopeContext.genericTypeParams);
        if((resolvedType->nameMatches(ARRAY_TYPE) || resolvedType->nameMatches(DICTIONARY_TYPE))
           && resolvedType->typeParams.empty()
           && resolvedType->nameMatches(resolvedOtherType)){
            return true;
        }

        // Contextual inference for empty collection literals:
        // allow explicit typed destinations to accept empty [] / {} initializers.
        if(expr_to_eval) {
            if(expr_to_eval->type == ARRAY_EXPR &&
               expr_to_eval->exprArrayData.empty() &&
               resolvedType->nameMatches(ARRAY_TYPE) &&
               !resolvedType->typeParams.empty() &&
               resolvedOtherType->nameMatches(ARRAY_TYPE) &&
               resolvedOtherType->typeParams.empty()) {
                return true;
            }

            if(expr_to_eval->type == DICT_EXPR &&
               expr_to_eval->dictExpr.empty() &&
               resolvedType->nameMatches(MAP_TYPE) &&
               resolvedType->typeParams.size() == 2 &&
               resolvedOtherType->nameMatches(DICTIONARY_TYPE) &&
               resolvedOtherType->typeParams.empty()) {
                return true;
            }
        }

        // Recursive contextual matching for collection literals:
        // handles nested empty literals in arrays/maps and ternary branches.
        auto silentTypeMatch = [&](ASTType *lhs,ASTType *rhs) -> bool {
            if(!lhs || !rhs){
                return false;
            }
            return lhs->match(rhs,[&](std::string){});
        };

        auto literalMatchesExpected = [&](auto &&self,ASTType *expectedType,ASTExpr *expr) -> bool {
            if(!expectedType || !expr){
                return false;
            }
            auto *resolvedExpectedType = resolveAliasType(expectedType,symbolTableContext,scopeContext.scope,scopeContext.genericTypeParams);
            if(!resolvedExpectedType){
                return false;
            }

            if(expr->type == ARRAY_EXPR){
                if(!resolvedExpectedType->nameMatches(ARRAY_TYPE) || resolvedExpectedType->typeParams.size() != 1){
                    return false;
                }
                auto *expectedElementType = resolveAliasType(resolvedExpectedType->typeParams.front(),symbolTableContext,scopeContext.scope,scopeContext.genericTypeParams);
                if(!expectedElementType){
                    return false;
                }
                if(expr->exprArrayData.empty()){
                    return true;
                }
                for(auto *elementExpr : expr->exprArrayData){
                    if(!elementExpr){
                        return false;
                    }
                    if(self(self,expectedElementType,elementExpr)){
                        continue;
                    }
                    auto *elementType = evalExprForTypeId(elementExpr,symbolTableContext,scopeContext);
                    if(!elementType){
                        return false;
                    }
                    auto *resolvedElementType = resolveAliasType(elementType,symbolTableContext,scopeContext.scope,scopeContext.genericTypeParams);
                    if(!resolvedElementType || !silentTypeMatch(expectedElementType,resolvedElementType)){
                        return false;
                    }
                    applyImplicitNumericCast(expectedElementType,resolvedElementType,elementExpr);
                }
                return true;
            }

            if(expr->type == DICT_EXPR){
                if(!resolvedExpectedType->nameMatches(MAP_TYPE) || resolvedExpectedType->typeParams.size() != 2){
                    return false;
                }
                auto *expectedKeyType = resolveAliasType(resolvedExpectedType->typeParams[0],symbolTableContext,scopeContext.scope,scopeContext.genericTypeParams);
                auto *expectedValueType = resolveAliasType(resolvedExpectedType->typeParams[1],symbolTableContext,scopeContext.scope,scopeContext.genericTypeParams);
                if(!expectedKeyType || !expectedValueType){
                    return false;
                }
                if(expr->dictExpr.empty()){
                    return true;
                }
                for(auto &entry : expr->dictExpr){
                    if(!entry.first || !entry.second){
                        return false;
                    }

                    if(self(self,expectedKeyType,entry.first)){
                        // Nested collection keys are unusual but keep recursion symmetric.
                    }
                    else {
                        auto *keyType = evalExprForTypeId(entry.first,symbolTableContext,scopeContext);
                        if(!keyType){
                            return false;
                        }
                        auto *resolvedKeyType = resolveAliasType(keyType,symbolTableContext,scopeContext.scope,scopeContext.genericTypeParams);
                        if(!resolvedKeyType || !silentTypeMatch(expectedKeyType,resolvedKeyType)){
                            return false;
                        }
                        applyImplicitNumericCast(expectedKeyType,resolvedKeyType,entry.first);
                    }

                    if(self(self,expectedValueType,entry.second)){
                        continue;
                    }
                    auto *valueType = evalExprForTypeId(entry.second,symbolTableContext,scopeContext);
                    if(!valueType){
                        return false;
                    }
                    auto *resolvedValueType = resolveAliasType(valueType,symbolTableContext,scopeContext.scope,scopeContext.genericTypeParams);
                    if(!resolvedValueType || !silentTypeMatch(expectedValueType,resolvedValueType)){
                        return false;
                    }
                    applyImplicitNumericCast(expectedValueType,resolvedValueType,entry.second);
                }
                return true;
            }

            if(expr->type == TERNARY_EXPR){
                if(!expr->leftExpr || !expr->middleExpr || !expr->rightExpr){
                    return false;
                }
                auto *conditionType = evalExprForTypeId(expr->leftExpr,symbolTableContext,scopeContext);
                if(!conditionType){
                    return false;
                }
                auto *resolvedConditionType = resolveAliasType(conditionType,symbolTableContext,scopeContext.scope,scopeContext.genericTypeParams);
                if(!resolvedConditionType || !resolvedConditionType->nameMatches(BOOL_TYPE)){
                    return false;
                }

                auto branchMatches = [&](ASTExpr *branch) -> bool {
                    if(self(self,resolvedExpectedType,branch)){
                        return true;
                    }
                    auto *branchType = evalExprForTypeId(branch,symbolTableContext,scopeContext);
                    if(!branchType){
                        return false;
                    }
                    auto *resolvedBranchType = resolveAliasType(branchType,symbolTableContext,scopeContext.scope,scopeContext.genericTypeParams);
                    if(!resolvedBranchType || !silentTypeMatch(resolvedExpectedType,resolvedBranchType)){
                        return false;
                    }
                    applyImplicitNumericCast(resolvedExpectedType,resolvedBranchType,branch);
                    return true;
                };

                return branchMatches(expr->middleExpr) && branchMatches(expr->rightExpr);
            }

            return false;
        };

        if(expr_to_eval && literalMatchesExpected(literalMatchesExpected,resolvedType,expr_to_eval)){
            applyImplicitNumericCast(resolvedType,resolvedOtherType,expr_to_eval);
            return true;
        }

        if(resolvedType->nameMatches(FUNCTION_TYPE) && expr_to_eval){
            Semantics::SymbolTable::Entry *funcEntry = nullptr;
            if(expr_to_eval->type == ID_EXPR && expr_to_eval->id){
                funcEntry = findFunctionEntryNoDiag(symbolTableContext,expr_to_eval->id->val,scopeContext.scope);
            }
            else if(expr_to_eval->type == MEMBER_EXPR && expr_to_eval->rightExpr && expr_to_eval->rightExpr->id){
                if(expr_to_eval->isScopeAccess && expr_to_eval->resolvedScope){
                    auto *entry = symbolTableContext.findEntryInExactScopeNoDiag(expr_to_eval->rightExpr->id->val,expr_to_eval->resolvedScope);
                    if(entry && entry->type == Semantics::SymbolTable::Entry::Function){
                        funcEntry = entry;
                    }
                }
            }

            if(funcEntry){
                auto *funcData = (Semantics::SymbolTable::Function *)funcEntry->data;
                auto *funcExprType = buildFunctionTypeFromFunctionData(funcData,expr_to_eval);
                if(funcExprType && resolvedType->match(funcExprType,[&](std::string message){
                    std::ostringstream ss;
                    ss << message << "\nContext: Type `" << resolvedOtherType->getName() << "` was implied from " << contextLabel;
                    errStream.push(SemanticADiagnostic::create(ss.str(),expr_to_eval,Diagnostic::Error));
                })){
                    return true;
                }
            }
        }
        
        if(resolvedType->match(resolvedOtherType,[&](std::string message){
            std::ostringstream ss;
            ss << message << "\nContext: Type `" << resolvedOtherType->getName() << "` was implied from " << contextLabel;
            auto res = ss.str();
            errStream.push(SemanticADiagnostic::create(res,expr_to_eval,Diagnostic::Error));
        })){
            applyImplicitNumericCast(resolvedType,resolvedOtherType,expr_to_eval);
            return true;
        }
        return false;
    }



    bool SemanticA::checkSymbolsForStmtInScope(ASTStmt *stmt,Semantics::STableContext & symbolTableContext,std::shared_ptr<ASTScope> scope,std::optional<Semantics::SymbolTable> tempSTable){
        ASTScopeSemanticsContext scopeContext {scope};
        if(stmt->type & DECL){
            auto *decl = (ASTDecl *)stmt;
            if(!validateAttributesForDecl(decl,errStream)){
                return false;
            }
            bool hasErrored = false;
            auto rc = evalGenericDecl(decl,symbolTableContext,scopeContext,&hasErrored);
            if(hasErrored && !rc){
                auto validateGenericParamDecls = [&](const std::vector<ASTGenericParamDecl *> &params,
                                                    const string_set *outerGenericParams,
                                                    ASTStmt *owner) -> bool {
                    string_set visibleGenericParams;
                    if(outerGenericParams){
                        visibleGenericParams = *outerGenericParams;
                    }
                    bool sawDefault = false;
                    for(auto *param : params){
                        if(!param || !param->id){
                            errStream.push(SemanticADiagnostic::create("Malformed generic parameter declaration.",owner,Diagnostic::Error));
                            return false;
                        }
                        for(auto *bound : param->bounds){
                            if(!typeExists(bound,symbolTableContext,scope,&visibleGenericParams,owner)){
                                return false;
                            }
                        }
                        if(param->defaultType){
                            sawDefault = true;
                            if(!typeExists(param->defaultType,symbolTableContext,scope,&visibleGenericParams,owner)){
                                return false;
                            }
                            param->defaultType = resolveAliasType(param->defaultType,symbolTableContext,scope,&visibleGenericParams);
                        }
                        else if(sawDefault){
                            errStream.push(SemanticADiagnostic::create("Generic parameters with defaults must be trailing.",owner,Diagnostic::Error));
                            return false;
                        }
                        visibleGenericParams.insert(param->id->val);
                    }
                    return true;
                };
                switch (decl->type) {
                    /// FuncDecl
                    case FUNC_DECL : {
    //                    std::cout << "FuncDecl" << std::endl;
                        auto funcNode = (ASTFuncDecl *)decl;
                        

                        ASTIdentifier *func_id = funcNode->funcId;
                        if(!validateGenericParamDecls(funcNode->genericParams,scopeContext.genericTypeParams,funcNode)){
                            return false;
                        }
                        auto funcGenericParams = genericParamSet(funcNode->genericParams);
                        
                        /// Ensure that Function declared is unique within the current scope.
                        auto symEntry = symbolTableContext.main->symbolExists(func_id->val,scope);
                        if(symEntry){
                            return false;
                        };
                        
    //                    std::cout << "Func is Unique" << std::endl;

                        for(auto & paramPair : funcNode->params){
                            if(!typeExists(paramPair.second,symbolTableContext,scope,&funcGenericParams,funcNode)){
                                return false;
                                break;
                            };
                            paramPair.second = resolveAliasType(paramPair.second,symbolTableContext,scope,&funcGenericParams);
                        };
                        if(funcNode->returnType != nullptr){
                            if(!typeExists(funcNode->returnType,symbolTableContext,scope,&funcGenericParams,funcNode)){
                                return false;
                            }
                            funcNode->returnType = resolveAliasType(funcNode->returnType,symbolTableContext,scope,&funcGenericParams);
                        };

                        bool isNativeFunc = hasAttributeNamed(funcNode->attributes,"native");
                        if(funcNode->declarationOnly){
                            if(!isNativeFunc){
                                errStream.push(SemanticADiagnostic::create("Function declaration without a body must be marked @native.",funcNode,Diagnostic::Error));
                                return false;
                            }
                            if(funcNode->returnType == nullptr){
                                errStream.push(SemanticADiagnostic::create("Native function declaration without a body must declare a return type.",funcNode,Diagnostic::Error));
                                return false;
                            }
                        }
                        else {
                            if(!funcNode->blockStmt){
                                errStream.push(SemanticADiagnostic::create("Function declaration is missing a body.",funcNode,Diagnostic::Error));
                                return false;
                            }

                            auto recursionSymbols = std::make_shared<Semantics::SymbolTable>();
                            addTempDeclEntryForSelfReference(funcNode,recursionSymbols.get());
                            ScopedAdditionalSymbolTable recursionSymbolScope(symbolTableContext,recursionSymbols);

                            bool hasFailed = false;
                            ASTScopeSemanticsContext funcScopeContext {funcNode->blockStmt->parentScope,&funcNode->params,&funcGenericParams};
                            ASTType *return_type_implied = evalBlockStmtForASTType(funcNode->blockStmt,
                                                                                  symbolTableContext,
                                                                                  &hasFailed,
                                                                                  funcScopeContext,
                                                                                  true,
                                                                                  funcNode->returnType);
                            if(!return_type_implied && hasFailed){
                                return false;
                            };
                            /// Implied Type and Declared Type Comparison.
                            if(funcNode->returnType != nullptr){
                                auto *resolvedImplied = resolveAliasType(return_type_implied,symbolTableContext,scope,&funcGenericParams);
                                if(!funcNode->returnType->match(resolvedImplied,[&](std::string message){
                                    std::ostringstream ss;
                                    ss << message << "\nContext: Declared return type of func `" << func_id->val << "` does not match implied return type.";
                                    auto out = ss.str();
                                    errStream.push(SemanticADiagnostic::create(out,funcNode,Diagnostic::Error));
                                }))
                                    return false;
                            }
                            /// Assume return type is implied type.
                            else {
                                funcNode->returnType = return_type_implied;
                            };
                            if(!validateCallableSemanticFlow(funcNode->blockStmt,
                                                             funcNode->params,
                                                             funcNode->returnType,
                                                             errStream,
                                                             funcNode,
                                                             func_id ? func_id->val : "<anonymous>")){
                                return false;
                            }
                        }

                        break;
                    }
                    case CLASS_DECL : {
                        if(scope->type != ASTScope::Namespace && scope->type != ASTScope::Neutral){
                            errStream.push(SemanticADiagnostic::create("Class decl not allowed in class scope",decl,Diagnostic::Error));
                            return false;
                        }
                        auto classDecl = (ASTClassDecl *)decl;
                        
                        auto symEntry = symbolTableContext.main->symbolExists(classDecl->id->val, scope);
                        
                        if(symEntry){
                            return false;
                        }

                        auto classSelfSymbols = std::make_shared<Semantics::SymbolTable>();
                        addTempDeclEntryForSelfReference(classDecl,classSelfSymbols.get());
                        ScopedAdditionalSymbolTable classSelfSymbolScope(symbolTableContext,classSelfSymbols);

                        if(!validateGenericParamDecls(classDecl->genericParams,scopeContext.genericTypeParams,classDecl)){
                            return false;
                        }
                        auto classGenericParams = genericParamSet(classDecl->genericParams);
                        ASTType *resolvedSuperClass = nullptr;
                        std::vector<ASTType *> resolvedInterfaces;
                        for(auto *baseType : classDecl->interfaces){
                            if(!baseType){
                                errStream.push(SemanticADiagnostic::create("Invalid type in class inheritance list.",classDecl,Diagnostic::Error));
                                return false;
                            }
                            if(!typeExists(baseType,symbolTableContext,scope,&classGenericParams,classDecl)){
                                return false;
                            }
                            auto *resolvedBaseType = resolveAliasType(baseType,symbolTableContext,scope,&classGenericParams);
                            auto *baseEntry = findTypeEntryNoDiag(symbolTableContext,resolvedBaseType->getName(),scope);
                            if(!baseEntry){
                                errStream.push(SemanticADiagnostic::create("Unknown type in class inheritance list.",classDecl,Diagnostic::Error));
                                return false;
                            }
                            if(baseEntry->type == Semantics::SymbolTable::Entry::Class){
                                if(resolvedSuperClass){
                                    errStream.push(SemanticADiagnostic::create("Class can only extend one superclass.",classDecl,Diagnostic::Error));
                                    return false;
                                }
                                auto *baseClassData = (Semantics::SymbolTable::Class *)baseEntry->data;
                                if(!baseClassData || !baseClassData->classType){
                                    errStream.push(SemanticADiagnostic::create("Superclass symbol is missing metadata.",classDecl,Diagnostic::Error));
                                    return false;
                                }
                                resolvedSuperClass = cloneTypeNode(baseClassData->classType,classDecl);
                                if(!resolvedBaseType->typeParams.empty()){
                                    resolvedSuperClass->typeParams.clear();
                                    for(auto *baseParam : resolvedBaseType->typeParams){
                                        resolvedSuperClass->addTypeParam(cloneTypeNode(baseParam,classDecl));
                                    }
                                }
                                continue;
                            }
                            if(baseEntry->type == Semantics::SymbolTable::Entry::Interface){
                                auto *baseInterfaceData = (Semantics::SymbolTable::Interface *)baseEntry->data;
                                if(!baseInterfaceData || !baseInterfaceData->interfaceType){
                                    errStream.push(SemanticADiagnostic::create("Interface symbol is missing metadata.",classDecl,Diagnostic::Error));
                                    return false;
                                }
                                auto *resolvedInterfaceType = cloneTypeNode(baseInterfaceData->interfaceType,classDecl);
                                if(!resolvedBaseType->typeParams.empty()){
                                    resolvedInterfaceType->typeParams.clear();
                                    for(auto *baseParam : resolvedBaseType->typeParams){
                                        resolvedInterfaceType->addTypeParam(cloneTypeNode(baseParam,classDecl));
                                    }
                                }
                                resolvedInterfaces.push_back(resolvedInterfaceType);
                                continue;
                            }
                            errStream.push(SemanticADiagnostic::create("Only class or interface types are allowed in class inheritance list.",classDecl,Diagnostic::Error));
                            return false;
                        }
                        classDecl->superClass = resolvedSuperClass;
                        classDecl->interfaces = resolvedInterfaces;

                        if(classDecl->superClass){
                            string_set seenTypes;
                            seenTypes.insert(classDecl->id->val);
                            auto *cursorType = classDecl->superClass;
                            while(cursorType){
                                auto cursorName = cursorType->getName();
                                if(seenTypes.find(cursorName.view()) != seenTypes.end()){
                                    errStream.push(SemanticADiagnostic::create("Circular class inheritance detected.",classDecl,Diagnostic::Error));
                                    return false;
                                }
                                seenTypes.insert(cursorName.str());
                                auto *cursorEntry = findTypeEntryNoDiag(symbolTableContext,cursorType->getName(),scope);
                                if(!cursorEntry || cursorEntry->type != Semantics::SymbolTable::Entry::Class){
                                    break;
                                }
                                auto *cursorData = (Semantics::SymbolTable::Class *)cursorEntry->data;
                                if(!cursorData){
                                    break;
                                }
                                cursorType = cursorData->superClassType;
                            }
                        }

                        bool hasErrored = false;
                        ASTScopeSemanticsContext classScopeContext {classDecl->scope,nullptr,&classGenericParams};
                        for(auto &f : classDecl->fields){
                            auto rcField = evalGenericDecl(f,symbolTableContext,classScopeContext,&hasErrored);
                            if(!validateAttributesForDecl(f,errStream)){
                                return false;
                            }
                            if(hasErrored && !rcField){
                                return false;
                            }
                        }

                        string_set methodNames;
                        for(auto &m : classDecl->methods){
                            if(methodNames.find(m->funcId->val) != methodNames.end()){
                                errStream.push(SemanticADiagnostic::create("Duplicate class method name.",m,Diagnostic::Error));
                                return false;
                            }
                            methodNames.insert(m->funcId->val);
                            if(!validateGenericParamDecls(m->genericParams,&classGenericParams,m)){
                                return false;
                            }
                            auto methodGenericParams = classGenericParams;
                            auto ownMethodGenericParams = genericParamSet(m->genericParams);
                            methodGenericParams.insert(ownMethodGenericParams.begin(),ownMethodGenericParams.end());
                            for(auto &paramPair : m->params){
                                if(!typeExists(paramPair.second,symbolTableContext,scope,&methodGenericParams,m)){
                                    return false;
                                }
                                paramPair.second = resolveAliasType(paramPair.second,symbolTableContext,scope,&methodGenericParams);
                            }
                            if(m->returnType){
                                if(!typeExists(m->returnType,symbolTableContext,scope,&methodGenericParams,m)){
                                    return false;
                                }
                                m->returnType = resolveAliasType(m->returnType,symbolTableContext,scope,&methodGenericParams);
                            }
                            bool methodIsNative = hasAttributeNamed(m->attributes,"native");
                            if(m->declarationOnly){
                                if(!methodIsNative){
                                    errStream.push(SemanticADiagnostic::create("Class method declaration without a body must be marked @native.",m,Diagnostic::Error));
                                    return false;
                                }
                                if(!m->returnType){
                                    errStream.push(SemanticADiagnostic::create("Native class method declaration without a body must declare a return type.",m,Diagnostic::Error));
                                    return false;
                                }
                                continue;
                            }
                            if(!m->blockStmt){
                                errStream.push(SemanticADiagnostic::create("Class method declaration is missing a body.",m,Diagnostic::Error));
                                return false;
                            }
                            std::map<ASTIdentifier *,ASTType *> methodParams = m->params;
                            auto *selfId = new ASTIdentifier();
                            selfId->val = "self";
                            methodParams.insert(std::make_pair(selfId,classDecl->classType));
                            bool methodHasFailed = false;
                            ASTScopeSemanticsContext methodScopeContext {m->blockStmt->parentScope,&methodParams,&methodGenericParams};
                            ASTType *returnTypeImplied = evalBlockStmtForASTType(m->blockStmt,
                                                                                symbolTableContext,
                                                                                &methodHasFailed,
                                                                                methodScopeContext,
                                                                                true,
                                                                                m->returnType);
                            if(methodHasFailed || !returnTypeImplied){
                                return false;
                            }
                            if(m->returnType != nullptr){
                                auto *resolvedImplied = resolveAliasType(returnTypeImplied,symbolTableContext,scope,&methodGenericParams);
                                if(!m->returnType->match(resolvedImplied,[&](std::string message){
                                    std::ostringstream ss;
                                    ss << message << "\nContext: Declared return type of class method `" << m->funcId->val << "` does not match implied return type.";
                                    errStream.push(SemanticADiagnostic::create(ss.str(),m,Diagnostic::Error));
                                })){
                                    return false;
                                }
                            }
                            else {
                                m->returnType = returnTypeImplied;
                            }
                            if(!validateCallableSemanticFlow(m->blockStmt,
                                                             methodParams,
                                                             m->returnType,
                                                             errStream,
                                                             m,
                                                             m->funcId ? m->funcId->val : "<method>")){
                                return false;
                            }
                        }

                        std::set<size_t> ctorArities;
                        for(auto &c : classDecl->constructors){
                            size_t arity = c->params.size();
                            if(ctorArities.find(arity) != ctorArities.end()){
                                errStream.push(SemanticADiagnostic::create("Duplicate constructor arity.",c,Diagnostic::Error));
                                return false;
                            }
                            ctorArities.insert(arity);
                            if(!validateGenericParamDecls(c->genericParams,&classGenericParams,c)){
                                return false;
                            }
                            auto ctorGenericParams = classGenericParams;
                            auto ownCtorGenericParams = genericParamSet(c->genericParams);
                            ctorGenericParams.insert(ownCtorGenericParams.begin(),ownCtorGenericParams.end());
                            for(auto &paramPair : c->params){
                                if(!typeExists(paramPair.second,symbolTableContext,scope,&ctorGenericParams,c)){
                                    return false;
                                }
                                paramPair.second = resolveAliasType(paramPair.second,symbolTableContext,scope,&ctorGenericParams);
                            }
                            std::map<ASTIdentifier *,ASTType *> ctorParams = c->params;
                            auto *selfId = new ASTIdentifier();
                            selfId->val = "self";
                            ctorParams.insert(std::make_pair(selfId,classDecl->classType));
                            bool ctorHasFailed = false;
                            ASTScopeSemanticsContext ctorScopeContext {c->blockStmt->parentScope,&ctorParams,&ctorGenericParams};
                            ASTType *ctorReturnType = evalBlockStmtForASTType(c->blockStmt,
                                                                             symbolTableContext,
                                                                             &ctorHasFailed,
                                                                             ctorScopeContext,
                                                                             true);
                            if(ctorHasFailed || !ctorReturnType){
                                return false;
                            }
                            if(ctorReturnType != VOID_TYPE){
                                errStream.push(SemanticADiagnostic::create("Constructor cannot return a value.",c,Diagnostic::Error));
                                return false;
                            }
                            if(!validateCallableSemanticFlow(c->blockStmt,
                                                             ctorParams,
                                                             VOID_TYPE,
                                                             errStream,
                                                             c,
                                                             "__ctor__")){
                                return false;
                            }
                        }

                        for(auto *implementedInterfaceType : classDecl->interfaces){
                            if(!implementedInterfaceType){
                                errStream.push(SemanticADiagnostic::create("Invalid interface in class implements list.",classDecl,Diagnostic::Error));
                                return false;
                            }
                            auto *interfaceEntry = findTypeEntryNoDiag(symbolTableContext,implementedInterfaceType->getName(),scope);
                            if(!interfaceEntry){
                                std::ostringstream ss;
                                ss << "Unknown interface `" << implementedInterfaceType->getName() << "` in class implements list.";
                                errStream.push(SemanticADiagnostic::create(ss.str(),classDecl,Diagnostic::Error));
                                return false;
                            }
                            if(interfaceEntry->type != Semantics::SymbolTable::Entry::Interface){
                                std::ostringstream ss;
                                ss << "`" << implementedInterfaceType->getName() << "` is not an interface.";
                                errStream.push(SemanticADiagnostic::create(ss.str(),classDecl,Diagnostic::Error));
                                return false;
                            }
                            auto *interfaceData = (Semantics::SymbolTable::Interface *)interfaceEntry->data;
                            if(!interfaceData){
                                errStream.push(SemanticADiagnostic::create("Interface symbol is missing metadata.",classDecl,Diagnostic::Error));
                                return false;
                            }

                            string_map<ASTType *> interfaceBindings;
                            for(size_t i = 0;i < interfaceData->genericParams.size() && i < implementedInterfaceType->typeParams.size();++i){
                                interfaceBindings.insert(std::make_pair(interfaceData->genericParams[i].name,implementedInterfaceType->typeParams[i]));
                            }

                            for(auto *requiredField : interfaceData->fields){
                                if(!requiredField || !requiredField->type){
                                    continue;
                                }
                                string_set visited;
                                auto *classFieldType = findFieldTypeByNameRecursive(classDecl,symbolTableContext,scope,requiredField->name,visited);
                                if(!classFieldType){
                                    std::ostringstream ss;
                                    ss << "Class `" << classDecl->id->val << "` does not implement interface field `" << requiredField->name << "`.";
                                    errStream.push(SemanticADiagnostic::create(ss.str(),classDecl,Diagnostic::Error));
                                    return false;
                                }
                                auto *requiredFieldType = substituteTypeParams(requiredField->type,interfaceBindings,classDecl);
                                requiredFieldType = resolveAliasType(requiredFieldType,symbolTableContext,scope,&classGenericParams);
                                auto *resolvedClassFieldType = resolveAliasType(classFieldType,symbolTableContext,scope,&classGenericParams);
                                if(!requiredFieldType->match(resolvedClassFieldType,[&](std::string message){
                                    std::ostringstream ss;
                                    ss << message << "\nContext: Interface field `" << requiredField->name << "` does not match class field type.";
                                    errStream.push(SemanticADiagnostic::create(ss.str(),classDecl,Diagnostic::Error));
                                })){
                                    return false;
                                }
                            }

                            for(auto *requiredMethod : interfaceData->methods){
                                if(!requiredMethod){
                                    continue;
                                }
                                string_set visited;
                                auto *classMethod = findMethodByNameRecursive(classDecl,symbolTableContext,scope,requiredMethod->name,visited);
                                if(!classMethod){
                                    std::ostringstream ss;
                                    ss << "Class `" << classDecl->id->val << "` does not implement interface method `" << requiredMethod->name << "`.";
                                    errStream.push(SemanticADiagnostic::create(ss.str(),classDecl,Diagnostic::Error));
                                    return false;
                                }
                                if(requiredMethod->genericParams.size() != classMethod->genericParams.size()){
                                    std::ostringstream ss;
                                    ss << "Class method `" << classMethod->funcId->val
                                       << "` generic parameter count does not match interface method declaration.";
                                    errStream.push(SemanticADiagnostic::create(ss.str(),classMethod,Diagnostic::Error));
                                    return false;
                                }

                                auto classMethodGenericParams = classGenericParams;
                                string_map<ASTType *> requiredMethodBindings = interfaceBindings;
                                for(size_t i = 0;i < classMethod->genericParams.size();++i){
                                    auto *classMethodGeneric = classMethod->genericParams[i];
                                    if(!classMethodGeneric || !classMethodGeneric->id){
                                        errStream.push(SemanticADiagnostic::create("Malformed class method generic parameter.",classMethod,Diagnostic::Error));
                                        return false;
                                    }
                                    classMethodGenericParams.insert(classMethodGeneric->id->val);
                                    auto *boundGenericType = ASTType::Create(classMethodGeneric->id->val,classDecl,true,false);
                                    boundGenericType->isGenericParam = true;
                                    requiredMethodBindings[requiredMethod->genericParams[i].name] = boundGenericType;
                                }
                                if(!classMethod->returnType || !requiredMethod->returnType){
                                    errStream.push(SemanticADiagnostic::create("Interface method return type could not be resolved.",classMethod,Diagnostic::Error));
                                    return false;
                                }
                                auto *requiredReturnType = substituteTypeParams(requiredMethod->returnType,requiredMethodBindings,classDecl);
                                requiredReturnType = resolveAliasType(requiredReturnType,symbolTableContext,scope,&classMethodGenericParams);
                                auto *classReturnType = resolveAliasType(classMethod->returnType,symbolTableContext,scope,&classMethodGenericParams);
                                if(!typesStructurallyEqual(requiredReturnType,classReturnType)){
                                    std::ostringstream ss;
                                    ss << "Interface method `" << requiredMethod->name << "` return type mismatch.";
                                    errStream.push(SemanticADiagnostic::create(ss.str(),classMethod,Diagnostic::Error));
                                    return false;
                                }
                                string_map<ASTType *> requiredParamMap = requiredMethod->paramMap;
                                for(auto &requiredParam : requiredParamMap){
                                    auto *substituted = substituteTypeParams(requiredParam.second,requiredMethodBindings,classDecl);
                                    requiredParam.second = resolveAliasType(substituted,symbolTableContext,scope,&classMethodGenericParams);
                                }
                                std::map<ASTIdentifier *,ASTType *> resolvedClassParams = classMethod->params;
                                for(auto &classParam : resolvedClassParams){
                                    classParam.second = resolveAliasType(classParam.second,symbolTableContext,scope,&classMethodGenericParams);
                                }
                                if(!methodParamsStructurallyMatch(resolvedClassParams,requiredParamMap)){
                                    std::ostringstream ss;
                                    ss << "Method `" << requiredMethod->name << "` does not match interface parameter signature.";
                                    errStream.push(SemanticADiagnostic::create(ss.str(),classMethod,Diagnostic::Error));
                                    return false;
                                }
                            }
                        }
                        
                        
                        return true;
                        break;
                    }
                    case INTERFACE_DECL : {
                        if(scope->type != ASTScope::Namespace && scope->type != ASTScope::Neutral){
                            errStream.push(SemanticADiagnostic::create("Interface declaration is not allowed in class/function scope.",decl,Diagnostic::Error));
                            return false;
                        }
                        auto *interfaceDecl = (ASTInterfaceDecl *)decl;
                        if(symbolTableContext.main->symbolExists(interfaceDecl->id->val,scope)){
                            errStream.push(SemanticADiagnostic::create("Duplicate interface name in current scope.",decl,Diagnostic::Error));
                            return false;
                        }

                        auto interfaceSelfSymbols = std::make_shared<Semantics::SymbolTable>();
                        addTempDeclEntryForSelfReference(interfaceDecl,interfaceSelfSymbols.get());
                        ScopedAdditionalSymbolTable interfaceSelfSymbolScope(symbolTableContext,interfaceSelfSymbols);

                        if(!validateGenericParamDecls(interfaceDecl->genericParams,scopeContext.genericTypeParams,interfaceDecl)){
                            return false;
                        }
                        auto interfaceGenericParams = genericParamSet(interfaceDecl->genericParams);
                        bool hasInterfaceError = false;
                        ASTScopeSemanticsContext interfaceScopeContext {interfaceDecl->scope,nullptr,&interfaceGenericParams};
                        for(auto *fieldDecl : interfaceDecl->fields){
                            auto rc = evalGenericDecl(fieldDecl,symbolTableContext,interfaceScopeContext,&hasInterfaceError);
                            if(!validateAttributesForDecl(fieldDecl,errStream)){
                                return false;
                            }
                            if(hasInterfaceError && !rc){
                                return false;
                            }
                        }

                        string_set methodNames;
                        for(auto *methodDecl : interfaceDecl->methods){
                            if(!methodDecl->funcId){
                                errStream.push(SemanticADiagnostic::create("Interface method is missing an identifier.",methodDecl,Diagnostic::Error));
                                return false;
                            }
                            if(methodNames.find(methodDecl->funcId->val) != methodNames.end()){
                                errStream.push(SemanticADiagnostic::create("Duplicate interface method name.",methodDecl,Diagnostic::Error));
                                return false;
                            }
                            methodNames.insert(methodDecl->funcId->val);

                            if(!validateGenericParamDecls(methodDecl->genericParams,&interfaceGenericParams,methodDecl)){
                                return false;
                            }
                            auto methodGenericParams = interfaceGenericParams;
                            auto ownMethodGenericParams = genericParamSet(methodDecl->genericParams);
                            methodGenericParams.insert(ownMethodGenericParams.begin(),ownMethodGenericParams.end());

                            for(auto &paramPair : methodDecl->params){
                                if(!typeExists(paramPair.second,symbolTableContext,scope,&methodGenericParams,methodDecl)){
                                    return false;
                                }
                                paramPair.second = resolveAliasType(paramPair.second,symbolTableContext,scope,&methodGenericParams);
                            }
                            if(methodDecl->returnType){
                                if(!typeExists(methodDecl->returnType,symbolTableContext,scope,&methodGenericParams,methodDecl)){
                                    return false;
                                }
                                methodDecl->returnType = resolveAliasType(methodDecl->returnType,symbolTableContext,scope,&methodGenericParams);
                            }

                            if(methodDecl->declarationOnly || !methodDecl->blockStmt){
                                if(!methodDecl->returnType){
                                    errStream.push(SemanticADiagnostic::create("Interface method declaration without a body must declare a return type.",methodDecl,Diagnostic::Error));
                                    return false;
                                }
                                continue;
                            }

                            std::map<ASTIdentifier *,ASTType *> methodParams = methodDecl->params;
                            auto *selfId = new ASTIdentifier();
                            selfId->val = "self";
                            auto *selfType = interfaceDecl->interfaceType ? interfaceDecl->interfaceType
                                                                          : ASTType::Create(interfaceDecl->id->val,interfaceDecl,false,false);
                            methodParams.insert(std::make_pair(selfId,selfType));

                            bool methodHasFailed = false;
                            ASTScopeSemanticsContext methodScopeContext {methodDecl->blockStmt->parentScope,&methodParams,&methodGenericParams};
                            ASTType *returnTypeImplied = evalBlockStmtForASTType(methodDecl->blockStmt,
                                                                                symbolTableContext,
                                                                                &methodHasFailed,
                                                                                methodScopeContext,
                                                                                true,
                                                                                methodDecl->returnType);
                            if(methodHasFailed || !returnTypeImplied){
                                return false;
                            }
                            if(methodDecl->returnType != nullptr){
                                auto *resolvedImplied = resolveAliasType(returnTypeImplied,symbolTableContext,scope,&methodGenericParams);
                                if(!methodDecl->returnType->match(resolvedImplied,[&](std::string message){
                                    std::ostringstream ss;
                                    ss << message << "\nContext: Declared return type of interface method `" << methodDecl->funcId->val << "` does not match implied return type.";
                                    errStream.push(SemanticADiagnostic::create(ss.str(),methodDecl,Diagnostic::Error));
                                })){
                                    return false;
                                }
                            }
                            else {
                                methodDecl->returnType = returnTypeImplied;
                            }
                            if(!validateCallableSemanticFlow(methodDecl->blockStmt,
                                                             methodParams,
                                                             methodDecl->returnType,
                                                             errStream,
                                                             methodDecl,
                                                             methodDecl->funcId ? methodDecl->funcId->val : "<method>")){
                                return false;
                            }
                        }
                        return true;
                        break;
                    }
                    case TYPE_ALIAS_DECL : {
                        auto *aliasDecl = (ASTTypeAliasDecl *)decl;
                        if(scope->type != ASTScope::Namespace && scope->type != ASTScope::Neutral){
                            errStream.push(SemanticADiagnostic::create("Type alias declaration is not allowed in class/function scope.",decl,Diagnostic::Error));
                            return false;
                        }
                        if(symbolTableContext.main->symbolExists(aliasDecl->id->val,scope)){
                            errStream.push(SemanticADiagnostic::create("Duplicate type alias name in current scope.",decl,Diagnostic::Error));
                            return false;
                        }
                        if(!validateGenericParamDecls(aliasDecl->genericParams,scopeContext.genericTypeParams,aliasDecl)){
                            return false;
                        }
                        auto aliasGenericParams = genericParamSet(aliasDecl->genericParams);
                        if(!aliasDecl->aliasedType){
                            errStream.push(SemanticADiagnostic::create("Type alias is missing target type.",decl,Diagnostic::Error));
                            return false;
                        }
                        if(!typeExists(aliasDecl->aliasedType,symbolTableContext,scope,&aliasGenericParams,aliasDecl)){
                            return false;
                        }
                        aliasDecl->aliasedType = resolveAliasType(aliasDecl->aliasedType,symbolTableContext,scope,&aliasGenericParams);
                        return true;
                    }
                    case IMPORT_DECL : {
                        if(scope->type == ASTScope::Class || scope->type == ASTScope::Function){
                            errStream.push(SemanticADiagnostic::create("Import declaration is not allowed in class/function scope.",decl,Diagnostic::Error));
                            return false;
                        }
                        auto *importDecl = (ASTImportDecl *)decl;
                        if(!importDecl->moduleName || importDecl->moduleName->val.empty()){
                            errStream.push(SemanticADiagnostic::create("Import declaration is missing a module name.",decl,Diagnostic::Error));
                            return false;
                        }
                        return true;
                    }
                    case SCOPE_DECL : {
                        if(scope->type == ASTScope::Class || scope->type == ASTScope::Function){
                            errStream.push(SemanticADiagnostic::create("Scope declaration is not allowed in class/function scope.",decl,Diagnostic::Error));
                            return false;
                        }
                        auto *scopeDecl = (ASTScopeDecl *)decl;
                        if(symbolTableContext.findEntryInExactScopeNoDiag(scopeDecl->scopeId->val,scope)){
                            errStream.push(SemanticADiagnostic::create("Duplicate scope name in current scope.",decl,Diagnostic::Error));
                            return false;
                        }
                        SemanticFlowState scopeFlowState;
                        pushSemanticFlowFrame(scopeFlowState);
                        for(auto *innerStmt : scopeDecl->blockStmt->body){
                            auto ok = checkSymbolsForStmtInScope(innerStmt,symbolTableContext,scopeDecl->blockStmt->parentScope);
                            if(!ok){
                                return false;
                            }
                            auto flowResult = analyzeSemanticFlowStmt(innerStmt,std::move(scopeFlowState),errStream);
                            scopeFlowState = std::move(flowResult.state);
                            if(flowResult.errored){
                                return false;
                            }
                            if(innerStmt->type & DECL){
                                addSTableEntryForDecl((ASTDecl *)innerStmt,symbolTableContext.main.get());
                            }
                        }
                        return true;
                    }
                    case RETURN_DECL : {
                        errStream.push(SemanticADiagnostic::create("Return can not be declared in a namespace scope",stmt,Diagnostic::Error));
                        return false;
                        break;
                    }
                    default: {
                        return false;
                    }
                }
            }
        }
        else if(stmt->type & EXPR){
            ASTExpr *expr = (ASTExpr *)stmt;
            switch (expr->type) {
                case IVKE_EXPR:
                {
                    /// An Invoke Expression with no return variable capture..
                    /// Example:
                    ///
                    /// func myFunc(){
                    ///     // Do Stuff Here
                    /// }
                    ///
                    /// myfunc()
                    ///
                    
                    ASTType *return_type = evalExprForTypeId(expr,symbolTableContext,scopeContext);
                    // std::cout << "Log" << return_type << std::endl;
                    if(!return_type){
                        return false;
                    };
                    
                    if(return_type != VOID_TYPE && !shouldSuppressUnusedInvocationWarning(expr)){
                        std::ostringstream ss;
                        std::string calleeName = "<invoke>";
                        if(expr->callee){
                            if(expr->callee->id){
                                calleeName = expr->callee->id->val;
                            }
                            else if(expr->callee->type == MEMBER_EXPR && expr->callee->rightExpr && expr->callee->rightExpr->id){
                                calleeName = expr->callee->rightExpr->id->val;
                            }
                        }
                        ss << "Func `" << calleeName << "` returns a value but is not being stored by a variable.";
                        auto res = ss.str();
                        errStream.push(SemanticADiagnostic::create(res,expr,Diagnostic::Warning));
                        /// Warn of non void object not being stored after creation from function invocation.
                    };
                    
                    break;
                }
                default:
                {
                    ASTType *exprType = evalExprForTypeId(expr,symbolTableContext,scopeContext);
                    if(!exprType){
                        return false;
                    }
                    break;
                }
            }
        };
        return true;
    }

    bool SemanticA::checkSymbolsForStmt(ASTStmt *stmt, Semantics::STableContext &symbolTableContext){
        auto ok = checkSymbolsForStmtInScope(stmt,symbolTableContext,ASTScopeGlobal);
        if(!ok){
            return false;
        }
        auto stateIt = gNamespaceFlowStates.find(this);
        if(stateIt == gNamespaceFlowStates.end()){
            gNamespaceFlowStates[this] = {};
            pushSemanticFlowFrame(gNamespaceFlowStates[this]);
            stateIt = gNamespaceFlowStates.find(this);
        }
        auto flowResult = analyzeSemanticFlowStmt(stmt,std::move(stateIt->second),errStream);
        stateIt->second = std::move(flowResult.state);
        if(flowResult.errored){
            return false;
        }
        auto usageIt = gNamespaceUsageStates.find(this);
        if(usageIt == gNamespaceUsageStates.end()){
            gNamespaceUsageStates[this] = {};
            pushSemanticUsageFrame(gNamespaceUsageStates[this]);
            usageIt = gNamespaceUsageStates.find(this);
        }
        analyzeSemanticUsageStmt(stmt,usageIt->second,errStream,false);
        return true;
    }

    
}
