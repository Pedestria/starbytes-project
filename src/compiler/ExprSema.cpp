#include "starbytes/compiler/ASTNodes.def"
#include "starbytes/compiler/SemanticA.h"
#include <algorithm>
#include <set>
#include <map>

namespace starbytes {

 #define PRINT_FUNC_ID "print"
  #define EXIT_FUNC_ID "exit"
  #define SQRT_FUNC_ID "sqrt"
  #define ABS_FUNC_ID "abs"
  #define MIN_FUNC_ID "min"
  #define MAX_FUNC_ID "max"

auto print_func_type = ASTType::Create(PRINT_FUNC_ID,nullptr);
auto exit_func_type = ASTType::Create(EXIT_FUNC_ID,nullptr);
auto sqrt_func_type = ASTType::Create(SQRT_FUNC_ID,nullptr);
auto abs_func_type = ASTType::Create(ABS_FUNC_ID,nullptr);
auto min_func_type = ASTType::Create(MIN_FUNC_ID,nullptr);
auto max_func_type = ASTType::Create(MAX_FUNC_ID,nullptr);
auto scope_ref_type = ASTType::Create("__scope__",nullptr);

ASTType *isBuiltinType(ASTIdentifier *id){
    if(id->val == PRINT_FUNC_ID) return print_func_type;
    if(id->val == EXIT_FUNC_ID)  return exit_func_type;
    if(id->val == SQRT_FUNC_ID) return sqrt_func_type;
    if(id->val == ABS_FUNC_ID) return abs_func_type;
    if(id->val == MIN_FUNC_ID) return min_func_type;
    if(id->val == MAX_FUNC_ID) return max_func_type;
    return nullptr;
}

static bool isTypeSymbolEntry(const Semantics::SymbolTable::Entry *entry){
    return entry && (entry->type == Semantics::SymbolTable::Entry::Class
                     || entry->type == Semantics::SymbolTable::Entry::Interface
                     || entry->type == Semantics::SymbolTable::Entry::TypeAlias);
}

static std::vector<Semantics::SymbolTable::Entry *> filterTypeEntries(const std::vector<Semantics::SymbolTable::Entry *> &entries){
    std::vector<Semantics::SymbolTable::Entry *> filtered;
    filtered.reserve(entries.size());
    for(auto *entry : entries){
        if(isTypeSymbolEntry(entry)){
            filtered.push_back(entry);
        }
    }
    return filtered;
}

static void emitAmbiguousLookupDiagnostic(DiagnosticHandler &errStream,
                                          ASTStmt *diagNode,
                                          string_ref symbolName,
                                          const char *kindMessage){
    std::ostringstream out;
    out << "Ambiguous " << (kindMessage ? kindMessage : "symbol") << " lookup for `"
        << symbolName << "`; multiple visible declarations match this name. Qualify it with its module or scope name.";
    errStream.push(SemanticADiagnostic::create(out.str(),diagNode,Diagnostic::Error));
}

static Semantics::SymbolTable::Entry *findVisibleEntryOrDiag(Semantics::STableContext &symbolTableContext,
                                                             string_ref symbolName,
                                                             std::shared_ptr<ASTScope> scope,
                                                             DiagnosticHandler &errStream,
                                                             ASTStmt *diagNode){
    auto matches = symbolTableContext.collectVisibleEntriesNoDiag(symbolName,scope);
    if(matches.size() > 1){
        emitAmbiguousLookupDiagnostic(errStream,diagNode,symbolName,"symbol");
        return nullptr;
    }
    return matches.empty() ? nullptr : matches.front();
}

static Semantics::SymbolTable::Entry *findExactScopeEntryOrDiag(Semantics::STableContext &symbolTableContext,
                                                                string_ref symbolName,
                                                                std::shared_ptr<ASTScope> scope,
                                                                DiagnosticHandler &errStream,
                                                                ASTStmt *diagNode){
    auto matches = symbolTableContext.collectEntriesInExactScopeNoDiag(symbolName,scope);
    if(matches.size() > 1){
        emitAmbiguousLookupDiagnostic(errStream,diagNode,symbolName,"symbol");
        return nullptr;
    }
    return matches.empty() ? nullptr : matches.front();
}

static Semantics::SymbolTable::Entry *findClassEntry(Semantics::STableContext &symbolTableContext,
                                                     string_ref className,
                                                     std::shared_ptr<ASTScope> scope,
                                                     DiagnosticHandler *errStream = nullptr,
                                                     ASTStmt *diagNode = nullptr){
    auto *entry = errStream && diagNode
                      ? findVisibleEntryOrDiag(symbolTableContext,className,scope,*errStream,diagNode)
                      : symbolTableContext.findEntryNoDiag(className,scope);
    if(entry && entry->type == Semantics::SymbolTable::Entry::Class){
        return entry;
    }
    auto emittedMatches = filterTypeEntries(symbolTableContext.collectEntriesByEmittedNoDiag(className));
    std::vector<Semantics::SymbolTable::Entry *> classMatches;
    classMatches.reserve(emittedMatches.size());
    for(auto *candidate : emittedMatches){
        if(candidate->type == Semantics::SymbolTable::Entry::Class){
            classMatches.push_back(candidate);
        }
    }
    if(classMatches.size() > 1){
        if(errStream && diagNode){
            emitAmbiguousLookupDiagnostic(*errStream,diagNode,className,"type");
        }
        return nullptr;
    }
    return classMatches.empty() ? nullptr : classMatches.front();
}

static Semantics::SymbolTable::Entry *findTypeEntryNoDiag(Semantics::STableContext &symbolTableContext,
                                                           string_ref typeName,
                                                           std::shared_ptr<ASTScope> scope,
                                                           DiagnosticHandler *errStream = nullptr,
                                                           ASTStmt *diagNode = nullptr){
    auto *entry = errStream && diagNode
                      ? findVisibleEntryOrDiag(symbolTableContext,typeName,scope,*errStream,diagNode)
                      : symbolTableContext.findEntryNoDiag(typeName,scope);
    if(isTypeSymbolEntry(entry)){
        return entry;
    }
    auto emittedMatches = filterTypeEntries(symbolTableContext.collectEntriesByEmittedNoDiag(typeName));
    if(emittedMatches.size() > 1){
        if(errStream && diagNode){
            emitAmbiguousLookupDiagnostic(*errStream,diagNode,typeName,"type");
        }
        return nullptr;
    }
    return emittedMatches.empty() ? nullptr : emittedMatches.front();
}

struct CompileTimeScalarValue {
    enum class Kind : uint8_t {
        Bool,
        Number,
        String
    };
    Kind kind = Kind::Bool;
    bool boolValue = false;
    long double numberValue = 0.0;
    std::string stringValue;
};

static bool isExactRuntimeTypeForConstantIs(ASTType *type){
    if(!type){
        return false;
    }
    return type->nameMatches(STRING_TYPE)
        || type->nameMatches(BOOL_TYPE)
        || type->nameMatches(ARRAY_TYPE)
        || type->nameMatches(DICTIONARY_TYPE)
        || type->nameMatches(MAP_TYPE)
        || type->nameMatches(REGEX_TYPE)
        || type->nameMatches(TASK_TYPE)
        || type->nameMatches(INT_TYPE)
        || type->nameMatches(FLOAT_TYPE)
        || type->nameMatches(LONG_TYPE)
        || type->nameMatches(DOUBLE_TYPE)
        || type->nameMatches(FUNCTION_TYPE);
}

static bool runtimeTypeCheckAlwaysMatches(ASTType *lhsType,const std::string &targetRuntimeName){
    if(!lhsType){
        return false;
    }
    if(lhsType->nameMatches(FUNCTION_TYPE)){
        return targetRuntimeName == "__func__";
    }
    if(lhsType->nameMatches(DICTIONARY_TYPE) || lhsType->nameMatches(MAP_TYPE)){
        return targetRuntimeName == "Dict" || targetRuntimeName == "Map";
    }
    return lhsType->getName().str() == targetRuntimeName;
}

static bool runtimeTypeCheckAlwaysFails(ASTType *lhsType,const std::string &targetRuntimeName){
    if(!lhsType || !isExactRuntimeTypeForConstantIs(lhsType)){
        return false;
    }
    return !runtimeTypeCheckAlwaysMatches(lhsType,targetRuntimeName);
}

static std::string displayTypeNameForConstantIs(ASTType *type){
    if(!type){
        return "<type>";
    }
    if(type->nameMatches(FUNCTION_TYPE)){
        return "Function";
    }
    return type->getName().str();
}

static std::optional<CompileTimeScalarValue> evaluateCompileTimeScalarLiteral(ASTExpr *expr){
    if(!expr){
        return std::nullopt;
    }
    if(expr->type == BOOL_LITERAL){
        auto *literal = static_cast<ASTLiteralExpr *>(expr);
        if(!literal->boolValue.has_value()){
            return std::nullopt;
        }
        CompileTimeScalarValue value;
        value.kind = CompileTimeScalarValue::Kind::Bool;
        value.boolValue = literal->boolValue.value();
        return value;
    }
    if(expr->type == NUM_LITERAL){
        auto *literal = static_cast<ASTLiteralExpr *>(expr);
        CompileTimeScalarValue value;
        value.kind = CompileTimeScalarValue::Kind::Number;
        if(literal->floatValue.has_value()){
            value.numberValue = literal->floatValue.value();
            return value;
        }
        if(literal->intValue.has_value()){
            value.numberValue = static_cast<long double>(literal->intValue.value());
            return value;
        }
        return std::nullopt;
    }
    if(expr->type == STR_LITERAL){
        auto *literal = static_cast<ASTLiteralExpr *>(expr);
        if(!literal->strValue.has_value()){
            return std::nullopt;
        }
        CompileTimeScalarValue value;
        value.kind = CompileTimeScalarValue::Kind::String;
        value.stringValue = literal->strValue.value();
        return value;
    }
    if(expr->type == UNARY_EXPR && expr->oprtr_str.has_value() && expr->oprtr_str.value() == "-"){
        auto nested = evaluateCompileTimeScalarLiteral(expr->leftExpr);
        if(!nested || nested->kind != CompileTimeScalarValue::Kind::Number){
            return std::nullopt;
        }
        nested->numberValue = -nested->numberValue;
        return nested;
    }
    return std::nullopt;
}

static ASTType *canonicalizeBuiltinAliasType(ASTType *type){
    return type;
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
        auto *paramClone = cloneTypeNode(param,parent);
        if(paramClone){
            cloned->addTypeParam(paramClone);
        }
    }
    return cloned;
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

static bool isGenericParamName(const string_set *genericTypeParams,string_ref name){
    return genericTypeParams && genericTypeParams->find(name.view()) != genericTypeParams->end();
}

static ASTType *resolveAliasType(ASTType *type,
                                 Semantics::STableContext &symbolTableContext,
                                 std::shared_ptr<ASTScope> scope,
                                 const string_set *genericTypeParams,
                                 string_set &visiting){
    if(!type){
        return nullptr;
    }
    auto *resolved = cloneTypeNode(type,type->getParentNode());
    if(!resolved){
        return nullptr;
    }
    for(size_t i = 0;i < resolved->typeParams.size();++i){
        auto *param = resolved->typeParams[i];
        auto *resolvedParam = resolveAliasType(param,symbolTableContext,scope,genericTypeParams,visiting);
        if(resolvedParam){
            resolved->typeParams[i] = resolvedParam;
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
    if(!aliasData || !aliasData->aliasType || aliasData->genericParams.size() < resolved->typeParams.size()){
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

static ASTType *resolveAliasType(ASTType *type,
                                 Semantics::STableContext &symbolTableContext,
                                 std::shared_ptr<ASTScope> scope,
                                 const string_set *genericTypeParams){
    string_set visiting;
    return resolveAliasType(type,symbolTableContext,scope,genericTypeParams,visiting);
}

static string_map<ASTType *> classBindingsFromInstanceType(Semantics::SymbolTable::Class *classData,
                                                                     ASTType *instanceType){
    string_map<ASTType *> bindings;
    if(!classData || !instanceType){
        return bindings;
    }
    if(classData->genericParams.size() != instanceType->typeParams.size()){
        return bindings;
    }
    for(size_t i = 0;i < classData->genericParams.size();++i){
        bindings.insert(std::make_pair(classData->genericParams[i].name,instanceType->typeParams[i]));
    }
    return bindings;
}

static string_map<ASTType *> interfaceBindingsFromInstanceType(Semantics::SymbolTable::Interface *interfaceData,
                                                                         ASTType *instanceType){
    string_map<ASTType *> bindings;
    if(!interfaceData || !instanceType){
        return bindings;
    }
    if(interfaceData->genericParams.size() != instanceType->typeParams.size()){
        return bindings;
    }
    for(size_t i = 0;i < interfaceData->genericParams.size();++i){
        bindings.insert(std::make_pair(interfaceData->genericParams[i].name,instanceType->typeParams[i]));
    }
    return bindings;
}

struct ClassMethodLookupResult {
    Semantics::SymbolTable::Function *method = nullptr;
    Semantics::SymbolTable::Class *owner = nullptr;
};

static Semantics::SymbolTable::Var *findClassFieldRecursive(Semantics::STableContext &symbolTableContext,
                                                            Semantics::SymbolTable::Class *classData,
                                                            string_ref fieldName,
                                                            std::shared_ptr<ASTScope> scope,
                                                            string_set &visited){
    if(!classData || !classData->classType){
        return nullptr;
    }
    auto className = classData->classType->getName();
    if(visited.find(className.view()) != visited.end()){
        return nullptr;
    }
    visited.insert(className.str());
    for(auto *field : classData->fields){
        if(field->name == fieldName){
            return field;
        }
    }
    if(!classData->superClassType){
        return nullptr;
    }
    auto *superEntry = findClassEntry(symbolTableContext,classData->superClassType->getName(),scope);
    if(!superEntry){
        return nullptr;
    }
    auto *superData = (Semantics::SymbolTable::Class *)superEntry->data;
    return findClassFieldRecursive(symbolTableContext,superData,fieldName,scope,visited);
}

static ClassMethodLookupResult findClassMethodRecursive(Semantics::STableContext &symbolTableContext,
                                                        Semantics::SymbolTable::Class *classData,
                                                        string_ref methodName,
                                                        std::shared_ptr<ASTScope> scope,
                                                        string_set &visited){
    ClassMethodLookupResult result;
    if(!classData || !classData->classType){
        return result;
    }
    auto className = classData->classType->getName();
    if(visited.find(className.view()) != visited.end()){
        return result;
    }
    visited.insert(className.str());
    for(auto *method : classData->instMethods){
        if(method->name == methodName){
            result.method = method;
            result.owner = classData;
            return result;
        }
    }
    if(!classData->superClassType){
        return result;
    }
    auto *superEntry = findClassEntry(symbolTableContext,classData->superClassType->getName(),scope);
    if(!superEntry){
        return result;
    }
    auto *superData = (Semantics::SymbolTable::Class *)superEntry->data;
    return findClassMethodRecursive(symbolTableContext,superData,methodName,scope,visited);
}

static ASTType *findClassFieldTypeFromDecl(ASTClassDecl *classDecl,string_ref fieldName,bool *isReadonly){
    for(auto *fieldDecl : classDecl->fields){
        bool readonlyField = fieldDecl->isConst;
        for(auto &attr : fieldDecl->attributes){
            if(attr.name == "readonly"){
                readonlyField = true;
                break;
            }
        }
        for(auto &spec : fieldDecl->specs){
            if(spec.id && spec.id->val == fieldName){
                if(isReadonly){
                    *isReadonly = readonlyField;
                }
                return spec.type;
            }
        }
    }
    return nullptr;
}

static ASTFuncDecl *findClassMethodFromDecl(ASTClassDecl *classDecl,string_ref methodName){
    for(auto *methodDecl : classDecl->methods){
        if(methodDecl->funcId && methodDecl->funcId->val == methodName){
            return methodDecl;
        }
    }
    return nullptr;
}

static ASTClassDecl *resolveClassDeclFromType(ASTType *type,
                                              Semantics::STableContext &symbolTableContext,
                                              std::shared_ptr<ASTScope> scope){
    if(!type){
        return nullptr;
    }
    auto *classEntry = findClassEntry(symbolTableContext,type->getName(),scope);
    if(!classEntry){
        return nullptr;
    }
    auto *classData = (Semantics::SymbolTable::Class *)classEntry->data;
    if(!classData || !classData->classType){
        return nullptr;
    }
    auto *parentNode = classData->classType->getParentNode();
    if(!parentNode || parentNode->type != CLASS_DECL){
        return nullptr;
    }
    return (ASTClassDecl *)parentNode;
}

static ASTType *findClassFieldTypeFromDeclRecursive(ASTClassDecl *classDecl,
                                                    Semantics::STableContext &symbolTableContext,
                                                    std::shared_ptr<ASTScope> scope,
                                                    string_ref fieldName,
                                                    bool *isReadonly,
                                                    bool *isDeprecated,
                                                    std::string *deprecationMessage,
                                                    string_set &visited){
    if(!classDecl){
        return nullptr;
    }
    if(classDecl->id){
        auto className = classDecl->id->val;
        if(visited.find(className) != visited.end()){
            return nullptr;
        }
        visited.insert(className);
    }
    auto *ownField = findClassFieldTypeFromDecl(classDecl,fieldName,isReadonly);
    if(ownField){
        if(isDeprecated || deprecationMessage){
            for(auto *fieldDecl : classDecl->fields){
                if(!fieldDecl){
                    continue;
                }
                bool readonlyField = fieldDecl->isConst;
                bool deprecatedField = false;
                std::string deprecatedMessageValue;
                for(auto &attr : fieldDecl->attributes){
                    if(attr.name == "readonly"){
                        readonlyField = true;
                    }
                    else if(attr.name == "deprecated"){
                        deprecatedField = true;
                        for(const auto &arg : attr.args){
                            if(arg.key.has_value() && arg.key.value() != "message"){
                                continue;
                            }
                            if(arg.value && arg.value->type == STR_LITERAL){
                                auto *literal = static_cast<ASTLiteralExpr *>(arg.value);
                                if(literal->strValue.has_value()){
                                    deprecatedMessageValue = literal->strValue.value();
                                }
                            }
                        }
                    }
                }
                for(auto &spec : fieldDecl->specs){
                    if(spec.id && spec.id->val == fieldName){
                        if(isReadonly){
                            *isReadonly = readonlyField;
                        }
                        if(isDeprecated){
                            *isDeprecated = deprecatedField;
                        }
                        if(deprecationMessage){
                            *deprecationMessage = deprecatedMessageValue;
                        }
                        return ownField;
                    }
                }
            }
        }
        return ownField;
    }
    if(!classDecl->superClass){
        return nullptr;
    }
    auto *superDecl = resolveClassDeclFromType(classDecl->superClass,symbolTableContext,scope);
    return findClassFieldTypeFromDeclRecursive(superDecl,
                                               symbolTableContext,
                                               scope,
                                               fieldName,
                                               isReadonly,
                                               isDeprecated,
                                               deprecationMessage,
                                               visited);
}

static ASTFuncDecl *findClassMethodFromDeclRecursive(ASTClassDecl *classDecl,
                                                     Semantics::STableContext &symbolTableContext,
                                                     std::shared_ptr<ASTScope> scope,
                                                     string_ref methodName,
                                                     string_set &visited){
    if(!classDecl){
        return nullptr;
    }
    if(classDecl->id){
        auto className = classDecl->id->val;
        if(visited.find(className) != visited.end()){
            return nullptr;
        }
        visited.insert(className);
    }
    auto *ownMethod = findClassMethodFromDecl(classDecl,methodName);
    if(ownMethod){
        return ownMethod;
    }
    if(!classDecl->superClass){
        return nullptr;
    }
    auto *superDecl = resolveClassDeclFromType(classDecl->superClass,symbolTableContext,scope);
    return findClassMethodFromDeclRecursive(superDecl,symbolTableContext,scope,methodName,visited);
}

static ASTConstructorDecl *findConstructorByArityFromDecl(ASTClassDecl *classDecl,size_t argCount){
    for(auto *ctorDecl : classDecl->constructors){
        if(ctorDecl->params.size() == argCount){
            return ctorDecl;
        }
    }
    return nullptr;
}

static Semantics::SymbolTable::Function *findConstructorByArity(Semantics::SymbolTable::Class *classData,size_t argCount){
    for(auto *ctor : classData->constructors){
        if(ctor->paramMap.size() == argCount){
            return ctor;
        }
    }
    return nullptr;
}

static Semantics::SymbolTable::Entry *findVarEntry(Semantics::STableContext &symbolTableContext,
                                                   string_ref symbolName,
                                                   std::shared_ptr<ASTScope> scope){
    auto *entry = symbolTableContext.findEntryNoDiag(symbolName,scope);
    if(entry && entry->type == Semantics::SymbolTable::Entry::Var){
        return entry;
    }
    entry = symbolTableContext.findEntryByEmittedNoDiag(symbolName);
    if(entry && entry->type == Semantics::SymbolTable::Entry::Var){
        return entry;
    }
    return nullptr;
}

static std::shared_ptr<ASTScope> scopeFromEntry(Semantics::SymbolTable::Entry *entry){
    if(!entry || entry->type != Semantics::SymbolTable::Entry::Scope || !entry->data){
        return nullptr;
    }
    return *((std::shared_ptr<ASTScope> *)entry->data);
}

static ASTType *cloneTypeWithQualifiers(ASTType *baseType,ASTStmt *parentNode,bool optional,bool throwable){
    if(!baseType){
        return nullptr;
    }
    auto *result = cloneTypeNode(baseType,parentNode);
    if(!result){
        return nullptr;
    }
    result->isOptional = optional;
    result->isThrowable = throwable;
    return result;
}

static bool isNumericType(ASTType *type){
    return type && (type->nameMatches(INT_TYPE)
                    || type->nameMatches(FLOAT_TYPE)
                    || type->nameMatches(LONG_TYPE)
                    || type->nameMatches(DOUBLE_TYPE));
}

static int numericTypeRank(ASTType *type){
    if(!type){
        return -1;
    }
    if(type->nameMatches(INT_TYPE)){
        return 0;
    }
    if(type->nameMatches(LONG_TYPE)){
        return 1;
    }
    if(type->nameMatches(FLOAT_TYPE)){
        return 2;
    }
    if(type->nameMatches(DOUBLE_TYPE)){
        return 3;
    }
    return -1;
}

static bool isWideningNumericCast(ASTType *expected,ASTType *actual){
    if(!expected || !actual || !isNumericType(expected) || !isNumericType(actual)){
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

static bool matchExpectedExprType(ASTType *expected,
                                  ASTExpr *expr,
                                  ASTType *actual,
                                  DiagnosticHandler &errStream,
                                  const char *message){
    if(!expected || !expr || !actual){
        return false;
    }
    if(!expected->match(actual,[&](std::string){
        errStream.push(SemanticADiagnostic::create(message,expr,Diagnostic::Error));
    })){
        return false;
    }
    applyImplicitNumericCast(expected,actual,expr);
    return true;
}

static bool isIntType(ASTType *type){
    return type && (type->nameMatches(INT_TYPE) || type->nameMatches(LONG_TYPE));
}

static bool isStringType(ASTType *type){
    return type && type->nameMatches(STRING_TYPE);
}

static bool isRegexType(ASTType *type){
    return type && type->nameMatches(REGEX_TYPE);
}

static bool isBoolType(ASTType *type){
    return type && type->nameMatches(BOOL_TYPE);
}

static bool isDictType(ASTType *type){
    return type && type->nameMatches(DICTIONARY_TYPE);
}

static bool isMapType(ASTType *type){
    return type && type->nameMatches(MAP_TYPE);
}

static bool isArrayType(ASTType *type){
    return type && type->nameMatches(ARRAY_TYPE);
}

static bool isDictKeyType(ASTType *type){
    return type && (isStringType(type) || isNumericType(type));
}

static bool isTaskType(ASTType *type){
    return type && type->nameMatches(TASK_TYPE);
}

static bool isFunctionType(ASTType *type){
    return type && type->nameMatches(FUNCTION_TYPE);
}

static bool isBuiltinCastTargetName(string_ref name){
    return name == INT_TYPE->getName()
        || name == FLOAT_TYPE->getName()
        || name == LONG_TYPE->getName()
        || name == DOUBLE_TYPE->getName()
        || name == STRING_TYPE->getName()
        || name == BOOL_TYPE->getName()
        || name == ANY_TYPE->getName();
}

static bool isBuiltinCastTargetType(ASTType *type){
    return type && isBuiltinCastTargetName(type->getName());
}

static std::optional<std::string> resolveRuntimeTypeNameFromType(ASTType *candidateType,
                                                                  Semantics::STableContext &symbolTableContext,
                                                                  std::shared_ptr<ASTScope> scope,
                                                                  const string_set *genericTypeParams,
                                                                  DiagnosticHandler &errStream,
                                                                  ASTStmt *diagNode){
    if(!candidateType){
        return std::nullopt;
    }
    auto *resolved = resolveAliasType(candidateType,symbolTableContext,scope,genericTypeParams);
    if(!resolved){
        return std::nullopt;
    }
    if(resolved->nameMatches(STRING_TYPE) || resolved->nameMatches(ARRAY_TYPE)
       || resolved->nameMatches(DICTIONARY_TYPE) || resolved->nameMatches(MAP_TYPE) || resolved->nameMatches(BOOL_TYPE)
       || resolved->nameMatches(INT_TYPE) || resolved->nameMatches(FLOAT_TYPE)
       || resolved->nameMatches(LONG_TYPE) || resolved->nameMatches(DOUBLE_TYPE)
       || resolved->nameMatches(REGEX_TYPE) || resolved->nameMatches(ANY_TYPE)
       || resolved->nameMatches(TASK_TYPE) || resolved->nameMatches(FUNCTION_TYPE)){
        return resolved->getName().str();
    }

    auto *entry = findTypeEntryNoDiag(symbolTableContext,resolved->getName(),scope);
    if(!entry){
        std::ostringstream ss;
        ss << "Unknown type `" << resolved->getName() << "` in runtime type check.";
        errStream.push(SemanticADiagnostic::create(ss.str(),diagNode,Diagnostic::Error));
        return std::nullopt;
    }
    if(entry->type == Semantics::SymbolTable::Entry::Interface){
        errStream.push(SemanticADiagnostic::create("Runtime `is` does not support interface types.",diagNode,Diagnostic::Error));
        return std::nullopt;
    }
    if(entry->type == Semantics::SymbolTable::Entry::Class){
        if(!entry->emittedName.empty()){
            return entry->emittedName;
        }
        return entry->name;
    }
    if(entry->type == Semantics::SymbolTable::Entry::TypeAlias){
        auto *aliasData = (Semantics::SymbolTable::TypeAlias *)entry->data;
        if(!aliasData || !aliasData->aliasType){
            errStream.push(SemanticADiagnostic::create("Type alias symbol is missing target type.",diagNode,Diagnostic::Error));
            return std::nullopt;
        }
        return resolveRuntimeTypeNameFromType(aliasData->aliasType,symbolTableContext,scope,genericTypeParams,errStream,diagNode);
    }
    errStream.push(SemanticADiagnostic::create("Right side of `is` must be a runtime type.",diagNode,Diagnostic::Error));
    return std::nullopt;
}

static bool classExtendsOrEqualsRecursive(Semantics::STableContext &symbolTableContext,
                                          string_ref className,
                                          string_ref expectedAncestor,
                                          std::shared_ptr<ASTScope> scope,
                                          string_set &visited){
    auto *entry = findClassEntry(symbolTableContext,className,scope);
    if(!entry){
        return false;
    }
    auto *classData = (Semantics::SymbolTable::Class *)entry->data;
    string_ref currentName = classData && classData->classType? classData->classType->getName() : string_ref(entry->name);
    auto visitedKey = currentName.str();
    if(visited.find(visitedKey) != visited.end()){
        return false;
    }
    visited.insert(visitedKey);
    if(currentName == expectedAncestor || entry->name == expectedAncestor
       || (!entry->emittedName.empty() && entry->emittedName == expectedAncestor)){
        return true;
    }
    if(!classData || !classData->superClassType){
        return false;
    }
    return classExtendsOrEqualsRecursive(symbolTableContext,classData->superClassType->getName(),expectedAncestor,scope,visited);
}

static bool classExtendsOrEquals(Semantics::STableContext &symbolTableContext,
                                 string_ref className,
                                 string_ref expectedAncestor,
                                 std::shared_ptr<ASTScope> scope){
    string_set visited;
    return classExtendsOrEqualsRecursive(symbolTableContext,className,expectedAncestor,scope,visited);
}

static ASTType *makeFunctionType(ASTType *returnType,
                                 const std::vector<ASTType *> &paramTypes,
                                 ASTStmt *node){
    if(!returnType){
        return nullptr;
    }
    auto *funcType = ASTType::Create(FUNCTION_TYPE->getName(),node,false,false);
    funcType->addTypeParam(cloneTypeNode(returnType,node));
    for(auto *paramType : paramTypes){
        if(!paramType){
            continue;
        }
        funcType->addTypeParam(cloneTypeNode(paramType,node));
    }
    return funcType;
}

static std::vector<ASTType *> orderedFunctionParamTypes(const Semantics::SymbolTable::Function *funcData){
    std::vector<ASTType *> ordered;
    if(!funcData){
        return ordered;
    }
    if(!funcData->orderedParams.empty()){
        ordered.reserve(funcData->orderedParams.size());
        for(auto &param : funcData->orderedParams){
            if(param.second){
                ordered.push_back(param.second);
            }
        }
        return ordered;
    }
    ordered.reserve(funcData->paramMap.size());
    for(auto &param : funcData->paramMap){
        if(param.second){
            ordered.push_back(param.second);
        }
    }
    return ordered;
}

static ASTType *buildFunctionTypeFromFunctionData(Semantics::SymbolTable::Function *funcData,ASTStmt *node){
    if(!funcData){
        return nullptr;
    }
    if(funcData->funcType && funcData->funcType->nameMatches(FUNCTION_TYPE)){
        auto *funcType = cloneTypeNode(funcData->funcType,node);
        if(!funcType){
            return nullptr;
        }
        if(funcData->isLazy &&
           !funcType->typeParams.empty() &&
           funcType->typeParams.front() &&
           !funcType->typeParams.front()->nameMatches(TASK_TYPE)){
            auto *lazyReturn = ASTType::Create(TASK_TYPE->getName(),node,false,false);
            lazyReturn->addTypeParam(cloneTypeNode(funcType->typeParams.front(),node));
            funcType->typeParams[0] = lazyReturn;
        }
        return funcType;
    }

    auto *funcType = ASTType::Create(FUNCTION_TYPE->getName(),node,false,false);
    auto *returnType = cloneTypeNode(funcData->returnType ? funcData->returnType : VOID_TYPE,node);
    if(!returnType){
        return nullptr;
    }
    if(funcData->isLazy){
        auto *taskReturnType = ASTType::Create(TASK_TYPE->getName(),node,false,false);
        taskReturnType->addTypeParam(returnType);
        funcType->addTypeParam(taskReturnType);
    }
    else {
        funcType->addTypeParam(returnType);
    }
    auto orderedParams = orderedFunctionParamTypes(funcData);
    for(auto *paramType : orderedParams){
        if(paramType){
            funcType->addTypeParam(cloneTypeNode(paramType,node));
        }
    }
    return funcType;
}

static ASTType *buildFunctionTypeFromDecl(ASTFuncDecl *funcDecl,ASTStmt *node){
    if(!funcDecl){
        return nullptr;
    }
    if(funcDecl->funcType && funcDecl->funcType->nameMatches(FUNCTION_TYPE)){
        auto *funcType = cloneTypeNode(funcDecl->funcType,node);
        if(!funcType){
            return nullptr;
        }
        if(funcDecl->isLazy &&
           !funcType->typeParams.empty() &&
           funcType->typeParams.front() &&
           !funcType->typeParams.front()->nameMatches(TASK_TYPE)){
            auto *lazyReturn = ASTType::Create(TASK_TYPE->getName(),node,false,false);
            lazyReturn->addTypeParam(cloneTypeNode(funcType->typeParams.front(),node));
            funcType->typeParams[0] = lazyReturn;
        }
        return funcType;
    }
    auto *funcType = ASTType::Create(FUNCTION_TYPE->getName(),node,false,false);
    auto *returnType = cloneTypeNode(funcDecl->returnType ? funcDecl->returnType : VOID_TYPE,node);
    if(!returnType){
        return nullptr;
    }
    if(funcDecl->isLazy){
        auto *taskReturnType = ASTType::Create(TASK_TYPE->getName(),node,false,false);
        taskReturnType->addTypeParam(returnType);
        funcType->addTypeParam(taskReturnType);
    }
    else {
        funcType->addTypeParam(returnType);
    }
    auto orderedParams = orderedParamPairs(funcDecl->params);
    for(auto &paramPair : orderedParams){
        if(paramPair.second){
            funcType->addTypeParam(cloneTypeNode(paramPair.second,node));
        }
    }
    return funcType;
}

static ASTType *functionReturnType(ASTType *funcType,ASTStmt *node){
    if(!isFunctionType(funcType) || funcType->typeParams.empty() || !funcType->typeParams.front()){
        return nullptr;
    }
    return cloneTypeNode(funcType->typeParams.front(),node);
}

static ASTType *functionParamType(ASTType *funcType,size_t index,ASTStmt *node){
    size_t paramIndex = index + 1;
    if(!isFunctionType(funcType) || paramIndex >= funcType->typeParams.size() || !funcType->typeParams[paramIndex]){
        return nullptr;
    }
    return cloneTypeNode(funcType->typeParams[paramIndex],node);
}

static ASTType *makeTaskType(ASTType *innerType,ASTStmt *node){
    if(!innerType){
        return nullptr;
    }
    auto *taskType = ASTType::Create(TASK_TYPE->getName(),node,false,false);
    taskType->addTypeParam(cloneTypeNode(innerType,node));
    return taskType;
}

static ASTType *makeArrayType(ASTType *elementType,ASTStmt *node){
    auto *arrayType = ASTType::Create(ARRAY_TYPE->getName(),node,false,false);
    if(elementType){
        arrayType->addTypeParam(cloneTypeNode(elementType,node));
    }
    return arrayType;
}

static ASTType *arrayElementType(ASTType *arrayType,ASTStmt *node,bool optional){
    if(arrayType && arrayType->nameMatches(ARRAY_TYPE) && arrayType->typeParams.size() == 1 && arrayType->typeParams.front()){
        auto *elementType = cloneTypeNode(arrayType->typeParams.front(),node);
        if(!elementType){
            return nullptr;
        }
        if(optional){
            elementType->isOptional = true;
        }
        return elementType;
    }
    return cloneTypeWithQualifiers(ANY_TYPE,node,optional,false);
}

static ASTType *makeMapType(ASTType *keyType,ASTType *valueType,ASTStmt *node){
    auto *mapType = ASTType::Create(MAP_TYPE->getName(),node,false,false);
    if(keyType){
        mapType->addTypeParam(cloneTypeNode(keyType,node));
    }
    if(valueType){
        mapType->addTypeParam(cloneTypeNode(valueType,node));
    }
    return mapType;
}

static ASTType *mapKeyType(ASTType *mapType,ASTStmt *node){
    if(mapType && mapType->nameMatches(MAP_TYPE) && mapType->typeParams.size() == 2 && mapType->typeParams[0]){
        return canonicalizeBuiltinAliasType(cloneTypeNode(mapType->typeParams[0],node));
    }
    return cloneTypeWithQualifiers(ANY_TYPE,node,false,false);
}

static ASTType *mapValueType(ASTType *mapType,ASTStmt *node,bool optional){
    if(mapType && mapType->nameMatches(MAP_TYPE) && mapType->typeParams.size() == 2 && mapType->typeParams[1]){
        auto *valueType = canonicalizeBuiltinAliasType(cloneTypeNode(mapType->typeParams[1],node));
        if(!valueType){
            return nullptr;
        }
        if(optional){
            valueType->isOptional = true;
        }
        return valueType;
    }
    return cloneTypeWithQualifiers(ANY_TYPE,node,optional,false);
}

static ASTType *promoteNumericType(ASTType *lhs,ASTType *rhs,ASTStmt *node){
    if(!isNumericType(lhs) || !isNumericType(rhs)){
        return nullptr;
    }
    int lhsRank = numericTypeRank(lhs);
    int rhsRank = numericTypeRank(rhs);
    if(lhsRank < 0 || rhsRank < 0){
        return nullptr;
    }
    if(lhsRank >= rhsRank){
        return ASTType::Create(lhs->getName(),node,false,false);
    }
    return ASTType::Create(rhs->getName(),node,false,false);
}

static ASTType *inferBinaryResultType(ASTExpr *expr,
                                      ASTType *lhs,
                                      ASTType *rhs,
                                      DiagnosticHandler &errStream){
    if(!lhs || !rhs){
        return nullptr;
    }
    auto op = expr->oprtr_str.value_or("");
    if(op.empty()){
        errStream.push(SemanticADiagnostic::create("Binary expression is missing operator.",expr,Diagnostic::Error));
        return nullptr;
    }

    if(lhs->nameMatches(ANY_TYPE) || rhs->nameMatches(ANY_TYPE)){
        if(op == "==" || op == "!=" || op == "<" || op == "<=" || op == ">" || op == ">=" || op == "&&" || op == "||"){
            return BOOL_TYPE;
        }
        return ANY_TYPE;
    }

    if(op == "+" || op == "-" || op == "*" || op == "/" || op == "%"){
        if(op == "+" && (isStringType(lhs) || isStringType(rhs))){
            return ASTType::Create(STRING_TYPE->getName(),expr,false,false);
        }
        auto numericType = promoteNumericType(lhs,rhs,expr);
        if(!numericType){
            errStream.push(SemanticADiagnostic::create("Arithmetic operators require numeric operands (or String concatenation for `+`).",expr,Diagnostic::Error));
            return nullptr;
        }
        return numericType;
    }

    if(op == "&" || op == "|" || op == "^" || op == "<<" || op == ">>"){
        if(isIntType(lhs) && isIntType(rhs)){
            return INT_TYPE;
        }
        errStream.push(SemanticADiagnostic::create("Bitwise and shift operators require Int operands.",expr,Diagnostic::Error));
        return nullptr;
    }

    if(op == "==" || op == "!="){
        if((isNumericType(lhs) && isNumericType(rhs)) ||
           lhs->nameMatches(rhs) ||
           lhs->nameMatches(ANY_TYPE) ||
           rhs->nameMatches(ANY_TYPE)){
            return BOOL_TYPE;
        }
        errStream.push(SemanticADiagnostic::create("Equality comparison requires matching operand types.",expr,Diagnostic::Error));
        return nullptr;
    }

    if(op == "<" || op == "<=" || op == ">" || op == ">="){
        if((isNumericType(lhs) && isNumericType(rhs)) ||
           (isStringType(lhs) && isStringType(rhs))){
            return BOOL_TYPE;
        }
        errStream.push(SemanticADiagnostic::create("Relational comparison requires numeric or string operands.",expr,Diagnostic::Error));
        return nullptr;
    }

    if(op == "&&" || op == "||"){
        if(isBoolType(lhs) && isBoolType(rhs)){
            return BOOL_TYPE;
        }
        errStream.push(SemanticADiagnostic::create("Logical operators require Bool operands.",expr,Diagnostic::Error));
        return nullptr;
    }

    errStream.push(SemanticADiagnostic::create("Unsupported binary operator.",expr,Diagnostic::Error));
    return nullptr;
}


ASTType * SemanticA::evalExprForTypeId(ASTExpr *expr_to_eval,
                                       Semantics::STableContext & symbolTableContext,
                                       ASTScopeSemanticsContext & scopeContext){
        ASTType *type;
        auto normalizeType = [&](ASTType *candidate) -> ASTType * {
            return resolveAliasType(candidate,symbolTableContext,scopeContext.scope,scopeContext.genericTypeParams);
        };
        struct GenericInferenceFailure {
            enum Kind {
                None,
                ArgMismatch,
                Conflict
            } kind = None;
            std::string genericParam;
        };
        auto typesStructurallyEqual = [&](auto &&self, ASTType *lhs, ASTType *rhs) -> bool {
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
                if(!self(self,lhs->typeParams[i],rhs->typeParams[i])){
                    return false;
                }
            }
            return true;
        };
        auto inferGenericBindingFromPattern = [&](auto &&self,
                                                 ASTType *pattern,
                                                 ASTType *actual,
                                                 const string_set &inferableParams,
                                                 string_map<ASTType *> &bindings,
                                                 GenericInferenceFailure &failure) -> bool {
            if(!pattern || !actual){
                failure.kind = GenericInferenceFailure::ArgMismatch;
                return false;
            }

            auto patternName = pattern->getName();
            if(pattern->isGenericParam && inferableParams.find(patternName.view()) != inferableParams.end()){
                auto *inferredType = cloneTypeNode(actual,expr_to_eval);
                if(!inferredType){
                    failure.kind = GenericInferenceFailure::ArgMismatch;
                    return false;
                }
                if(pattern->isOptional && inferredType->isOptional){
                    inferredType->isOptional = false;
                }
                if(pattern->isThrowable && inferredType->isThrowable){
                    inferredType->isThrowable = false;
                }
                inferredType = normalizeType(inferredType);
                auto existing = bindings.find(patternName.view());
                if(existing != bindings.end()){
                    auto *resolvedExisting = normalizeType(existing->second);
                    if(!typesStructurallyEqual(typesStructurallyEqual,resolvedExisting,inferredType)){
                        failure.kind = GenericInferenceFailure::Conflict;
                        failure.genericParam = patternName.str();
                        return false;
                    }
                    return true;
                }
                bindings[patternName.str()] = inferredType;
                return true;
            }

            if(pattern->nameMatches(ANY_TYPE)){
                return true;
            }

            if(!pattern->match(actual,[&](std::string){
                return;
            })){
                failure.kind = GenericInferenceFailure::ArgMismatch;
                return false;
            }

            if(pattern->typeParams.size() != actual->typeParams.size()){
                return true;
            }
            for(size_t i = 0;i < pattern->typeParams.size();++i){
                if(!self(self,pattern->typeParams[i],actual->typeParams[i],inferableParams,bindings,failure)){
                    return false;
                }
            }
            return true;
        };
        auto materializeGenericDefaults = [&](const std::vector<Semantics::SymbolTable::GenericParam> &genericParams,
                                              string_map<ASTType *> &bindings) -> bool {
            for(auto &genericParam : genericParams){
                if(bindings.find(genericParam.name) != bindings.end()){
                    continue;
                }
                if(!genericParam.defaultType){
                    std::ostringstream ss;
                    ss << "Could not infer generic type parameter `" << genericParam.name << "` from invocation arguments.";
                    errStream.push(SemanticADiagnostic::create(ss.str(),expr_to_eval,Diagnostic::Error));
                    return false;
                }
                auto *defaultType = substituteTypeParams(genericParam.defaultType,bindings,expr_to_eval);
                if(!defaultType){
                    errStream.push(SemanticADiagnostic::create("Failed to materialize generic default type argument.",expr_to_eval,Diagnostic::Error));
                    return false;
                }
                if(!typeExists(defaultType,symbolTableContext,scopeContext.scope,scopeContext.genericTypeParams,expr_to_eval)){
                    return false;
                }
                defaultType = normalizeType(defaultType);
                if(!defaultType){
                    errStream.push(SemanticADiagnostic::create("Failed to resolve generic default type argument.",expr_to_eval,Diagnostic::Error));
                    return false;
                }
                bindings[genericParam.name] = defaultType;
            }
            return true;
        };
        auto inferGenericInvocationBindings = [&](const std::vector<Semantics::SymbolTable::GenericParam> &genericParams,
                                                  const std::vector<ASTType *> &expectedParams,
                                                  const string_map<ASTType *> &seedBindings,
                                                  const char *argMismatchMessage,
                                                  string_map<ASTType *> &outBindings) -> bool {
            outBindings = seedBindings;
            string_set inferableParams;
            for(auto &genericParam : genericParams){
                inferableParams.insert(genericParam.name);
            }

            for(size_t i = 0;i < expr_to_eval->exprArrayData.size();++i){
                auto *argExpr = expr_to_eval->exprArrayData[i];
                auto *argType = evalExprForTypeId(argExpr,symbolTableContext,scopeContext);
                if(!argType){
                    return false;
                }
                argType = normalizeType(argType);

                auto *expectedPattern = expectedParams[i];
                if(!seedBindings.empty()){
                    expectedPattern = substituteTypeParams(expectedPattern,seedBindings,expr_to_eval);
                }
                expectedPattern = normalizeType(expectedPattern);

                GenericInferenceFailure failure;
                if(!inferGenericBindingFromPattern(inferGenericBindingFromPattern,
                                                  expectedPattern,
                                                  argType,
                                                  inferableParams,
                                                  outBindings,
                                                  failure)){
                    if(failure.kind == GenericInferenceFailure::Conflict){
                        std::ostringstream ss;
                        ss << "Conflicting inferred types for generic parameter `" << failure.genericParam << "`.";
                        errStream.push(SemanticADiagnostic::create(ss.str(),argExpr,Diagnostic::Error));
                    }
                    else {
                        errStream.push(SemanticADiagnostic::create(argMismatchMessage,argExpr,Diagnostic::Error));
                    }
                    return false;
                }
            }

            return materializeGenericDefaults(genericParams,outBindings);
        };
        auto resolveTypeArgsFromSymbolDefaults = [&](const std::vector<Semantics::SymbolTable::GenericParam> &genericParams,
                                                     const std::vector<ASTType *> &explicitTypeArgs,
                                                     const char *countMismatchMessage,
                                                     std::vector<ASTType *> &outTypeArgs) -> bool {
            if(explicitTypeArgs.size() > genericParams.size()){
                errStream.push(SemanticADiagnostic::create(countMismatchMessage,expr_to_eval,Diagnostic::Error));
                return false;
            }
            outTypeArgs = explicitTypeArgs;
            string_map<ASTType *> bindings;
            for(size_t i = 0;i < outTypeArgs.size();++i){
                bindings[genericParams[i].name] = outTypeArgs[i];
            }
            for(size_t i = outTypeArgs.size();i < genericParams.size();++i){
                auto &genericParam = genericParams[i];
                if(!genericParam.defaultType){
                    errStream.push(SemanticADiagnostic::create(countMismatchMessage,expr_to_eval,Diagnostic::Error));
                    return false;
                }
                auto *defaultType = substituteTypeParams(genericParam.defaultType,bindings,expr_to_eval);
                if(!defaultType){
                    errStream.push(SemanticADiagnostic::create("Failed to materialize generic default type argument.",expr_to_eval,Diagnostic::Error));
                    return false;
                }
                if(!typeExists(defaultType,symbolTableContext,scopeContext.scope,scopeContext.genericTypeParams,expr_to_eval)){
                    return false;
                }
                defaultType = normalizeType(defaultType);
                if(!defaultType){
                    errStream.push(SemanticADiagnostic::create("Failed to resolve generic default type argument.",expr_to_eval,Diagnostic::Error));
                    return false;
                }
                outTypeArgs.push_back(defaultType);
                bindings[genericParam.name] = defaultType;
            }
            return true;
        };
        auto resolveTypeArgsFromAstDefaults = [&](const std::vector<ASTGenericParamDecl *> &genericParams,
                                                  const std::vector<ASTType *> &explicitTypeArgs,
                                                  const char *countMismatchMessage,
                                                  std::vector<ASTType *> &outTypeArgs) -> bool {
            if(explicitTypeArgs.size() > genericParams.size()){
                errStream.push(SemanticADiagnostic::create(countMismatchMessage,expr_to_eval,Diagnostic::Error));
                return false;
            }
            outTypeArgs = explicitTypeArgs;
            string_map<ASTType *> bindings;
            for(size_t i = 0;i < outTypeArgs.size();++i){
                auto *genericParam = genericParams[i];
                if(!genericParam || !genericParam->id){
                    errStream.push(SemanticADiagnostic::create("Malformed generic parameter declaration.",expr_to_eval,Diagnostic::Error));
                    return false;
                }
                bindings[genericParam->id->val] = outTypeArgs[i];
            }
            for(size_t i = outTypeArgs.size();i < genericParams.size();++i){
                auto *genericParam = genericParams[i];
                if(!genericParam || !genericParam->id){
                    errStream.push(SemanticADiagnostic::create("Malformed generic parameter declaration.",expr_to_eval,Diagnostic::Error));
                    return false;
                }
                if(!genericParam->defaultType){
                    errStream.push(SemanticADiagnostic::create(countMismatchMessage,expr_to_eval,Diagnostic::Error));
                    return false;
                }
                auto *defaultType = substituteTypeParams(genericParam->defaultType,bindings,expr_to_eval);
                if(!defaultType){
                    errStream.push(SemanticADiagnostic::create("Failed to materialize generic default type argument.",expr_to_eval,Diagnostic::Error));
                    return false;
                }
                if(!typeExists(defaultType,symbolTableContext,scopeContext.scope,scopeContext.genericTypeParams,expr_to_eval)){
                    return false;
                }
                defaultType = normalizeType(defaultType);
                if(!defaultType){
                    errStream.push(SemanticADiagnostic::create("Failed to resolve generic default type argument.",expr_to_eval,Diagnostic::Error));
                    return false;
                }
                outTypeArgs.push_back(defaultType);
                bindings[genericParam->id->val] = defaultType;
            }
            return true;
        };
        auto buildGenericParamsFromAst = [&](const std::vector<ASTGenericParamDecl *> &genericParams) {
            std::vector<Semantics::SymbolTable::GenericParam> params;
            params.reserve(genericParams.size());
            for(auto *genericParam : genericParams){
                if(!genericParam || !genericParam->id){
                    continue;
                }
                Semantics::SymbolTable::GenericParam param;
                param.name = genericParam->id->val;
                param.defaultType = genericParam->defaultType;
                param.bounds = genericParam->bounds;
                switch(genericParam->variance){
                    case ASTGenericVariance::In:
                        param.variance = Semantics::SymbolTable::GenericParam::In;
                        break;
                    case ASTGenericVariance::Out:
                        param.variance = Semantics::SymbolTable::GenericParam::Out;
                        break;
                    case ASTGenericVariance::Invariant:
                    default:
                        param.variance = Semantics::SymbolTable::GenericParam::Invariant;
                        break;
                }
                params.push_back(std::move(param));
            }
            return params;
        };
        auto deprecationMessageFromAttrs = [&](const std::vector<ASTAttribute> &attrs) -> std::string {
            for(const auto &attr : attrs){
                if(attr.name != "deprecated"){
                    continue;
                }
                for(const auto &arg : attr.args){
                    if(arg.key.has_value() && arg.key.value() != "message"){
                        continue;
                    }
                    if(arg.value && arg.value->type == STR_LITERAL){
                        auto *literal = static_cast<ASTLiteralExpr *>(arg.value);
                        if(literal->strValue.has_value()){
                            return literal->strValue.value();
                        }
                    }
                }
            }
            return "";
        };
        auto warnDeprecatedEntry = [&](Semantics::SymbolTable::Entry *entry,
                                       ASTStmt *diagNode,
                                       const char *overrideKind = nullptr) {
            if(!entry || !diagNode){
                return;
            }
            const char *kind = overrideKind;
            std::string message;
            switch(entry->type){
                case Semantics::SymbolTable::Entry::Var: {
                    auto *varData = (Semantics::SymbolTable::Var *)entry->data;
                    if(!varData || !varData->isDeprecated){
                        return;
                    }
                    kind = kind ? kind : "variable";
                    message = varData->deprecationMessage;
                    break;
                }
                case Semantics::SymbolTable::Entry::Function: {
                    auto *funcData = (Semantics::SymbolTable::Function *)entry->data;
                    if(!funcData || !funcData->isDeprecated){
                        return;
                    }
                    kind = kind ? kind : "function";
                    message = funcData->deprecationMessage;
                    break;
                }
                case Semantics::SymbolTable::Entry::Class: {
                    auto *classData = (Semantics::SymbolTable::Class *)entry->data;
                    if(!classData || !classData->isDeprecated){
                        return;
                    }
                    kind = kind ? kind : "class";
                    message = classData->deprecationMessage;
                    break;
                }
                case Semantics::SymbolTable::Entry::Interface: {
                    auto *interfaceData = (Semantics::SymbolTable::Interface *)entry->data;
                    if(!interfaceData || !interfaceData->isDeprecated){
                        return;
                    }
                    kind = kind ? kind : "interface";
                    message = interfaceData->deprecationMessage;
                    break;
                }
                case Semantics::SymbolTable::Entry::TypeAlias: {
                    auto *aliasData = (Semantics::SymbolTable::TypeAlias *)entry->data;
                    if(!aliasData || !aliasData->isDeprecated){
                        return;
                    }
                    kind = kind ? kind : "type alias";
                    message = aliasData->deprecationMessage;
                    break;
                }
                default:
                    return;
            }
            warnDeprecatedUse(kind ? kind : "symbol",
                              entry->name,
                              !entry->emittedName.empty() ? entry->emittedName : entry->name,
                              message,
                              diagNode);
        };
        auto warnDeprecatedDeclAttrs = [&](const char *kind,
                                           ASTIdentifier *id,
                                           const std::vector<ASTAttribute> &attrs,
                                           ASTStmt *diagNode) {
            bool isDeprecated = false;
            for(const auto &attr : attrs){
                if(attr.name == "deprecated"){
                    isDeprecated = true;
                    break;
                }
            }
            if(!isDeprecated || !id || !diagNode){
                return;
            }
            auto displayName = id->sourceName.empty() ? id->val : id->sourceName;
            warnDeprecatedUse(kind ? kind : "symbol",
                              displayName,
                              std::string("decl:") + (kind ? kind : "symbol") + ":" + displayName,
                              deprecationMessageFromAttrs(attrs),
                              diagNode);
        };
        switch (expr_to_eval->type) {
            case ID_EXPR : {
                ASTIdentifier *id_ = expr_to_eval->id;
                SemanticsContext ctxt {errStream,expr_to_eval};

                auto _is_builtin_type = isBuiltinType(id_);
                /// 1. Check and see if id is a builtin func.
                if(_is_builtin_type){
                    id_->type = ASTIdentifier::Function;
                    return _is_builtin_type;
                };

                /// 2. Resolve lexical scoped symbols before synthetic arg bindings so
                /// catch/local shadowing stays deterministic.
                auto *symbol_ = static_cast<Semantics::SymbolTable::Entry *>(nullptr);
                std::shared_ptr<ASTScope> outerScopeStart = nullptr;
                for(auto currentScope = scopeContext.scope; currentScope != nullptr; currentScope = currentScope->parentScope){
                    symbol_ = findExactScopeEntryOrDiag(symbolTableContext,id_->val,currentScope,errStream,expr_to_eval);
                    if(!symbol_ && !symbolTableContext.collectEntriesInExactScopeNoDiag(id_->val,currentScope).empty()){
                        return nullptr;
                    }
                    if(symbol_){
                        break;
                    }
                    for(auto bindingIt = scopeContext.scopedBindings.rbegin();
                        bindingIt != scopeContext.scopedBindings.rend();
                        ++bindingIt){
                        if(bindingIt->scope == currentScope &&
                           bindingIt->id &&
                           bindingIt->type &&
                           bindingIt->id->match(id_)){
                            id_->type = ASTIdentifier::Var;
                            return normalizeType(bindingIt->type);
                        }
                    }
                    if(currentScope->type == ASTScope::Function){
                        outerScopeStart = currentScope->parentScope;
                        break;
                    }
                }

                /// 3. Fall back to synthetic function bindings (params/self) only if no
                /// lexical symbol in the current function/catch/block chain matched.
                if(!symbol_ && scopeContext.args != nullptr){
                    for(auto & __arg : *scopeContext.args){
                        if(__arg.first->match(id_)){
                            id_->type = ASTIdentifier::Var;
                            return normalizeType(__arg.second);
                        };
                    };
                };

                /// 4. Finally, continue lookup outside the current function boundary.
                if(!symbol_ && outerScopeStart){
                    symbol_ = findVisibleEntryOrDiag(symbolTableContext,id_->val,outerScopeStart,errStream,expr_to_eval);
                    if(!symbol_ && !symbolTableContext.collectVisibleEntriesNoDiag(id_->val,outerScopeStart).empty()){
                        return nullptr;
                    }
                }

                if(!symbol_ && !outerScopeStart){
                    symbol_ = findVisibleEntryOrDiag(symbolTableContext,id_->val,scopeContext.scope,errStream,expr_to_eval);
                    if(!symbol_ && !symbolTableContext.collectVisibleEntriesNoDiag(id_->val,scopeContext.scope).empty()){
                        return nullptr;
                    }
                }

                // std::cout << "SYMBOL_PTR:" << symbol_ << std::endl;
                if(!symbol_){
                    auto importedEntries = symbolTableContext.collectImportedGlobalEntriesNoDiag(id_->val);
                    bool importedValueVisible = std::any_of(importedEntries.begin(),importedEntries.end(),[](auto *entry){
                        return entry && (entry->type == Semantics::SymbolTable::Entry::Var
                                         || entry->type == Semantics::SymbolTable::Entry::Function);
                    });
                    if(importedEntries.size() > 1 && importedValueVisible){
                        std::ostringstream out;
                        out << "Ambiguous imported symbol `" << id_->val
                            << "`; multiple imported modules export this name. Reference it with its module name.";
                        errStream.push(SemanticADiagnostic::create(out.str(),expr_to_eval,Diagnostic::Error));
                        return nullptr;
                    }
                    if(importedValueVisible){
                        std::ostringstream out;
                        out << "Imported symbol `" << id_->val << "` must be referenced with its module name.";
                        errStream.push(SemanticADiagnostic::create(out.str(),expr_to_eval,Diagnostic::Error));
                        return nullptr;
                    }
                    std::ostringstream out;
                    out <<"Undefined symbol `" << id_->val << "`";
                    errStream.push(SemanticADiagnostic::create(out.str(),expr_to_eval,Diagnostic::Error));
                    return nullptr;
                };
                
                if(symbol_->type == Semantics::SymbolTable::Entry::Var){
                    auto varData = (Semantics::SymbolTable::Var *)symbol_->data;
                    id_->type = ASTIdentifier::Var;
                    warnDeprecatedEntry(symbol_,expr_to_eval,"variable");
                    if(!symbol_->emittedName.empty()){
                        id_->val = symbol_->emittedName;
                    }
                    type = varData->type;
                }
                else if(symbol_->type == Semantics::SymbolTable::Entry::Function){
                    auto funcData = (Semantics::SymbolTable::Function *)symbol_->data;
                    id_->type = ASTIdentifier::Function;
                    warnDeprecatedEntry(symbol_,expr_to_eval,"function");
                    if(!symbol_->emittedName.empty()){
                        id_->val = symbol_->emittedName;
                    }
                    type = buildFunctionTypeFromFunctionData(funcData,expr_to_eval);
                    if(!type){
                        type = funcData->funcType;
                    }
                }
                else if(symbol_->type == Semantics::SymbolTable::Entry::Class){
                    auto classData = (Semantics::SymbolTable::Class *)symbol_->data;
                    id_->type = ASTIdentifier::Class;
                    warnDeprecatedEntry(symbol_,expr_to_eval,"class");
                    if(!symbol_->emittedName.empty()){
                        id_->val = symbol_->emittedName;
                    }
                    type = classData->classType;
                }
                else if(symbol_->type == Semantics::SymbolTable::Entry::TypeAlias){
                    auto *aliasData = (Semantics::SymbolTable::TypeAlias *)symbol_->data;
                    if(!aliasData || !aliasData->aliasType){
                        errStream.push(SemanticADiagnostic::create("Type alias symbol is missing target type.",expr_to_eval,Diagnostic::Error));
                        return nullptr;
                    }
                    id_->type = ASTIdentifier::Class;
                    warnDeprecatedEntry(symbol_,expr_to_eval,"type alias");
                    type = aliasData->aliasType;
                }
                else if(symbol_->type == Semantics::SymbolTable::Entry::Scope){
                    id_->type = ASTIdentifier::Scope;
                    type = scope_ref_type;
                }
                else {
                    errStream.push(SemanticADiagnostic::create("Identifier in this context cannot identify any other symbol type except another variable.",expr_to_eval,Diagnostic::Error));
                    return nullptr;
                };
                
                type = normalizeType(type);
                break;
            }
            case ARRAY_EXPR : {
                ASTType *inferredElementType = nullptr;
                for(auto *elementExpr : expr_to_eval->exprArrayData){
                    auto *elementType = evalExprForTypeId(elementExpr,symbolTableContext,scopeContext);
                    if(!elementType){
                        return nullptr;
                    }
                    elementType = normalizeType(elementType);
                    if(!inferredElementType){
                        inferredElementType = cloneTypeNode(elementType,expr_to_eval);
                        continue;
                    }
                    if(!inferredElementType->match(elementType,[&](std::string){
                        inferredElementType = cloneTypeWithQualifiers(ANY_TYPE,expr_to_eval,false,false);
                    })){
                        continue;
                    }
                }
                type = inferredElementType? makeArrayType(inferredElementType,expr_to_eval) : ARRAY_TYPE;
                break;
            }
            case DICT_EXPR : {
                ASTType *inferredKeyType = nullptr;
                ASTType *inferredValueType = nullptr;
                bool canInferStrictMap = !expr_to_eval->dictExpr.empty();
                for(auto &entry : expr_to_eval->dictExpr){
                    auto *keyType = evalExprForTypeId(entry.first,symbolTableContext,scopeContext);
                    if(!keyType){
                        return nullptr;
                    }
                    keyType = normalizeType(keyType);
                    if(!(isNumericType(keyType) || isStringType(keyType))){
                        errStream.push(SemanticADiagnostic::create("Dictionary keys must be String/Int/Long/Float/Double.",entry.first,Diagnostic::Error));
                        return nullptr;
                    }
                    auto *valueType = evalExprForTypeId(entry.second,symbolTableContext,scopeContext);
                    if(!valueType){
                        return nullptr;
                    }
                    valueType = normalizeType(valueType);

                    if(canInferStrictMap){
                        if(!inferredKeyType){
                            inferredKeyType = cloneTypeNode(keyType,expr_to_eval);
                        }
                        else if(!inferredKeyType->match(keyType,[&](std::string){
                            canInferStrictMap = false;
                        })){
                            canInferStrictMap = false;
                        }

                        if(!inferredValueType){
                            inferredValueType = cloneTypeNode(valueType,expr_to_eval);
                        }
                        else if(!inferredValueType->match(valueType,[&](std::string){
                            canInferStrictMap = false;
                        })){
                            canInferStrictMap = false;
                        }
                    }
                }
                if(canInferStrictMap && inferredKeyType && inferredValueType){
                    type = makeMapType(inferredKeyType,inferredValueType,expr_to_eval);
                }
                else {
                    type = DICTIONARY_TYPE;
                }
                break;
            }
            case INLINE_FUNC_EXPR : {
                if(!expr_to_eval->inlineFuncReturnType || !expr_to_eval->inlineFuncBlock){
                    errStream.push(SemanticADiagnostic::create("Inline function is missing a return type or body.",expr_to_eval,Diagnostic::Error));
                    return nullptr;
                }

                std::vector<ASTType *> inlineParamTypes;
                std::map<ASTIdentifier *,ASTType *> inlineArgs;
                auto orderedInlineParams = orderedParamPairs(expr_to_eval->inlineFuncParams);
                for(auto &paramPair : orderedInlineParams){
                    if(!paramPair.first || !paramPair.second){
                        errStream.push(SemanticADiagnostic::create("Inline function parameter is invalid.",expr_to_eval,Diagnostic::Error));
                        return nullptr;
                    }
                    if(!typeExists(paramPair.second,symbolTableContext,scopeContext.scope,scopeContext.genericTypeParams,expr_to_eval)){
                        return nullptr;
                    }
                    auto *resolvedParamType = normalizeType(paramPair.second);
                    inlineParamTypes.push_back(resolvedParamType);
                    inlineArgs.insert(std::make_pair(paramPair.first,resolvedParamType));
                }

                if(!typeExists(expr_to_eval->inlineFuncReturnType,symbolTableContext,scopeContext.scope,scopeContext.genericTypeParams,expr_to_eval)){
                    return nullptr;
                }
                auto *resolvedReturnType = normalizeType(expr_to_eval->inlineFuncReturnType);

                bool hasFailed = false;
                ASTScopeSemanticsContext inlineScopeContext {expr_to_eval->inlineFuncBlock->parentScope,&inlineArgs,scopeContext.genericTypeParams};
                ASTType *impliedReturnType = evalBlockStmtForASTType(expr_to_eval->inlineFuncBlock,
                                                                     symbolTableContext,
                                                                     &hasFailed,
                                                                     inlineScopeContext,
                                                                     true,
                                                                     expr_to_eval->inlineFuncReturnType);
                if(hasFailed || !impliedReturnType){
                    return nullptr;
                }
                auto *resolvedImpliedReturn = normalizeType(impliedReturnType);
                if(!resolvedReturnType->match(resolvedImpliedReturn,[&](std::string message){
                    std::ostringstream ss;
                    ss << message << "\nContext: Inline function declared return type does not match implied return type.";
                    errStream.push(SemanticADiagnostic::create(ss.str(),expr_to_eval,Diagnostic::Error));
                })){
                    return nullptr;
                }

                type = makeFunctionType(resolvedReturnType,inlineParamTypes,expr_to_eval);
                break;
            }
                /// Literals
            case BOOL_LITERAL : {
                type = BOOL_TYPE;
                break;
            }
            case STR_LITERAL:
            {
                type = STRING_TYPE;
                break;
            }
            case REGEX_LITERAL:
            {
                type = cloneTypeWithQualifiers(REGEX_TYPE,expr_to_eval,false,true);
                break;
            }
            case NUM_LITERAL : {
                auto *literal = (ASTLiteralExpr *)expr_to_eval;
                /// If is Integer
                if(literal->intValue.has_value()){
                    type = prefer64BitNumberInference ? LONG_TYPE : INT_TYPE;
                }
                /// Else it is a Floating Point
                else {
                    type = prefer64BitNumberInference ? DOUBLE_TYPE : FLOAT_TYPE;
                };
                break;
            }
            case INDEX_EXPR : {
                if(!expr_to_eval->leftExpr || !expr_to_eval->rightExpr){
                    errStream.push(SemanticADiagnostic::create("Malformed index expression.",expr_to_eval,Diagnostic::Error));
                    return nullptr;
                }
                auto *baseType = evalExprForTypeId(expr_to_eval->leftExpr,symbolTableContext,scopeContext);
                auto *indexType = evalExprForTypeId(expr_to_eval->rightExpr,symbolTableContext,scopeContext);
                if(!baseType || !indexType){
                    return nullptr;
                }
                baseType = normalizeType(baseType);
                indexType = normalizeType(indexType);
                if(isArrayType(baseType)){
                    if(!indexType->nameMatches(INT_TYPE)){
                        errStream.push(SemanticADiagnostic::create("Array indexing requires Int index.",expr_to_eval,Diagnostic::Error));
                        return nullptr;
                    }
                    type = arrayElementType(baseType,expr_to_eval,false);
                    break;
                }
                if(isDictType(baseType)){
                    if(!(isStringType(indexType) || isNumericType(indexType))){
                        errStream.push(SemanticADiagnostic::create("Dictionary indexing requires String/Int/Long/Float/Double key.",expr_to_eval,Diagnostic::Error));
                        return nullptr;
                    }
                    type = cloneTypeWithQualifiers(ANY_TYPE,expr_to_eval,true,false);
                    break;
                }
                if(isMapType(baseType)){
                    auto *expectedKeyType = mapKeyType(baseType,expr_to_eval);
                    if(!expectedKeyType){
                        return nullptr;
                    }
                    if(!expectedKeyType->match(indexType,[&](std::string){
                        errStream.push(SemanticADiagnostic::create("Map indexing key type mismatch.",expr_to_eval,Diagnostic::Error));
                    })){
                        return nullptr;
                    }
                    type = mapValueType(baseType,expr_to_eval,true);
                    break;
                }
                errStream.push(SemanticADiagnostic::create("Index expression requires Array, Dict, or Map base.",expr_to_eval,Diagnostic::Error));
                return nullptr;
            }
            case UNARY_EXPR : {
                if(!expr_to_eval->leftExpr){
                    errStream.push(SemanticADiagnostic::create("Malformed unary expression.",expr_to_eval,Diagnostic::Error));
                    return nullptr;
                }
                auto *operandType = evalExprForTypeId(expr_to_eval->leftExpr,symbolTableContext,scopeContext);
                if(!operandType){
                    return nullptr;
                }
                operandType = normalizeType(operandType);
                auto op = expr_to_eval->oprtr_str.value_or("");
                if(op == "+"){
                    if(!isNumericType(operandType)){
                        errStream.push(SemanticADiagnostic::create("Unary `+` requires numeric operand.",expr_to_eval,Diagnostic::Error));
                        return nullptr;
                    }
                    type = operandType;
                    break;
                }
                if(op == "-"){
                    if(!isNumericType(operandType)){
                        errStream.push(SemanticADiagnostic::create("Unary `-` requires numeric operand.",expr_to_eval,Diagnostic::Error));
                        return nullptr;
                    }
                    type = operandType;
                    break;
                }
                if(op == "!"){
                    if(!isBoolType(operandType)){
                        errStream.push(SemanticADiagnostic::create("Unary `!` requires Bool operand.",expr_to_eval,Diagnostic::Error));
                        return nullptr;
                    }
                    type = BOOL_TYPE;
                    break;
                }
                if(op == "~"){
                    if(!isIntType(operandType)){
                        errStream.push(SemanticADiagnostic::create("Unary `~` requires Int operand.",expr_to_eval,Diagnostic::Error));
                        return nullptr;
                    }
                    type = INT_TYPE;
                    break;
                }
                if(op == "await"){
                    if(!isTaskType(operandType)){
                        errStream.push(SemanticADiagnostic::create("`await` requires a Task value.",expr_to_eval,Diagnostic::Error));
                        return nullptr;
                    }
                    if(operandType->typeParams.size() != 1 || !operandType->typeParams.front()){
                        errStream.push(SemanticADiagnostic::create("Task type must include exactly one type argument for `await`.",expr_to_eval,Diagnostic::Error));
                        return nullptr;
                    }
                    type = normalizeType(cloneTypeNode(operandType->typeParams.front(),expr_to_eval));
                    break;
                }
                errStream.push(SemanticADiagnostic::create("Unsupported unary operator.",expr_to_eval,Diagnostic::Error));
                return nullptr;
            }
            case BINARY_EXPR : {
                if(!expr_to_eval->leftExpr || !expr_to_eval->rightExpr){
                    errStream.push(SemanticADiagnostic::create("Malformed binary expression.",expr_to_eval,Diagnostic::Error));
                    return nullptr;
                }
                auto op = expr_to_eval->oprtr_str.value_or("");
                if(op == KW_IS){
                    auto *lhsType = evalExprForTypeId(expr_to_eval->leftExpr,symbolTableContext,scopeContext);
                    if(!lhsType){
                        return nullptr;
                    }

                    std::optional<std::string> runtimeTypeName;
                    auto *rhsExpr = expr_to_eval->rightExpr;
                    if(rhsExpr->type == ID_EXPR && rhsExpr->id){
                        string_ref rawName = rhsExpr->id->val;
                        if(rawName == STRING_TYPE->getName() || rawName == ARRAY_TYPE->getName()
                           || rawName == DICTIONARY_TYPE->getName() || rawName == MAP_TYPE->getName() || rawName == BOOL_TYPE->getName()
                           || rawName == INT_TYPE->getName() || rawName == FLOAT_TYPE->getName()
                           || rawName == LONG_TYPE->getName() || rawName == DOUBLE_TYPE->getName()
                           || rawName == REGEX_TYPE->getName() || rawName == ANY_TYPE->getName()
                           || rawName == TASK_TYPE->getName() || rawName == FUNCTION_TYPE->getName()){
                            runtimeTypeName = rawName.str();
                        }
                        else {
                            auto *entry = findTypeEntryNoDiag(symbolTableContext,rawName,scopeContext.scope,&errStream,rhsExpr);
                            if(!entry){
                                std::ostringstream ss;
                                ss << "Unknown type `" << rawName.str() << "` in runtime type check.";
                                errStream.push(SemanticADiagnostic::create(ss.str(),rhsExpr,Diagnostic::Error));
                                return nullptr;
                            }
                            if(entry->type == Semantics::SymbolTable::Entry::Interface){
                                errStream.push(SemanticADiagnostic::create("Runtime `is` does not support interface types.",rhsExpr,Diagnostic::Error));
                                return nullptr;
                            }
                            if(entry->type == Semantics::SymbolTable::Entry::Class){
                                rhsExpr->id->type = ASTIdentifier::Class;
                                if(!entry->emittedName.empty()){
                                    rhsExpr->id->val = entry->emittedName;
                                    runtimeTypeName = entry->emittedName;
                                }
                                else {
                                    runtimeTypeName = entry->name;
                                }
                            }
                            else if(entry->type == Semantics::SymbolTable::Entry::TypeAlias){
                                auto *aliasData = (Semantics::SymbolTable::TypeAlias *)entry->data;
                                if(!aliasData || !aliasData->aliasType){
                                    errStream.push(SemanticADiagnostic::create("Type alias symbol is missing target type.",rhsExpr,Diagnostic::Error));
                                    return nullptr;
                                }
                                runtimeTypeName = resolveRuntimeTypeNameFromType(aliasData->aliasType,
                                                                                 symbolTableContext,
                                                                                 scopeContext.scope,
                                                                                 scopeContext.genericTypeParams,
                                                                                 errStream,
                                                                                 rhsExpr);
                            }
                            else {
                                errStream.push(SemanticADiagnostic::create("Right side of `is` must be a type.",rhsExpr,Diagnostic::Error));
                                return nullptr;
                            }
                        }
                    }
                    else if(rhsExpr->type == MEMBER_EXPR){
                        auto *rhsType = evalExprForTypeId(rhsExpr,symbolTableContext,scopeContext);
                        if(!rhsType){
                            return nullptr;
                        }
                        bool isClassRef = rhsExpr->rightExpr && rhsExpr->rightExpr->id
                            && rhsExpr->rightExpr->id->type == ASTIdentifier::Class;
                        if(!isClassRef){
                            errStream.push(SemanticADiagnostic::create("Right side of `is` must be a class or builtin type.",rhsExpr,Diagnostic::Error));
                            return nullptr;
                        }
                        runtimeTypeName = resolveRuntimeTypeNameFromType(rhsType,
                                                                         symbolTableContext,
                                                                         scopeContext.scope,
                                                                         scopeContext.genericTypeParams,
                                                                         errStream,
                                                                         rhsExpr);
                    }
                    else {
                        errStream.push(SemanticADiagnostic::create("Right side of `is` must be a type identifier.",rhsExpr,Diagnostic::Error));
                        return nullptr;
                    }

                    if(!runtimeTypeName.has_value() || runtimeTypeName->empty()){
                        return nullptr;
                    }
                    expr_to_eval->runtimeTypeCheckName = runtimeTypeName;
                    type = BOOL_TYPE;
                    break;
                }
                auto *lhsType = evalExprForTypeId(expr_to_eval->leftExpr,symbolTableContext,scopeContext);
                auto *rhsType = evalExprForTypeId(expr_to_eval->rightExpr,symbolTableContext,scopeContext);
                if(!lhsType || !rhsType){
                    return nullptr;
                }
                lhsType = normalizeType(lhsType);
                rhsType = normalizeType(rhsType);
                type = inferBinaryResultType(expr_to_eval,lhsType,rhsType,errStream);
                if(!type){
                    return nullptr;
                }
                break;
            }
            case TERNARY_EXPR : {
                if(!expr_to_eval->leftExpr || !expr_to_eval->middleExpr || !expr_to_eval->rightExpr){
                    errStream.push(SemanticADiagnostic::create("Malformed ternary expression.",expr_to_eval,Diagnostic::Error));
                    return nullptr;
                }

                auto *conditionType = evalExprForTypeId(expr_to_eval->leftExpr,symbolTableContext,scopeContext);
                if(!conditionType){
                    return nullptr;
                }
                conditionType = normalizeType(conditionType);
                if(!isBoolType(conditionType)){
                    errStream.push(SemanticADiagnostic::create("Ternary condition must be Bool.",expr_to_eval->leftExpr,Diagnostic::Error));
                    return nullptr;
                }
                if(auto constantInfo = evaluateCompileTimeBoolExpr(expr_to_eval->leftExpr,symbolTableContext,scopeContext)){
                    std::ostringstream warning;
                    if(constantInfo->value){
                        warning << "Ternary condition is always true; false branch is unreachable.";
                    }
                    else {
                        warning << "Ternary condition is always false; true branch is unreachable.";
                    }
                    if(!constantInfo->reason.empty()){
                        warning << " Reason: " << constantInfo->reason << ".";
                    }
                    errStream.push(SemanticADiagnostic::create(warning.str(),expr_to_eval->leftExpr,Diagnostic::Warning));
                }

                auto *trueType = evalExprForTypeId(expr_to_eval->middleExpr,symbolTableContext,scopeContext);
                auto *falseType = evalExprForTypeId(expr_to_eval->rightExpr,symbolTableContext,scopeContext);
                if(!trueType || !falseType){
                    return nullptr;
                }
                trueType = normalizeType(trueType);
                falseType = normalizeType(falseType);

                if(trueType->match(falseType,[&](std::string){})){
                    type = trueType;
                    break;
                }
                if(falseType->match(trueType,[&](std::string){})){
                    type = falseType;
                    break;
                }

                errStream.push(SemanticADiagnostic::create("Ternary branch type mismatch.",expr_to_eval,Diagnostic::Error));
                return nullptr;
            }
            case MEMBER_EXPR : {
                if(!expr_to_eval->leftExpr || !expr_to_eval->rightExpr || !expr_to_eval->rightExpr->id){
                    errStream.push(SemanticADiagnostic::create("Malformed member expression.",expr_to_eval,Diagnostic::Error));
                    return nullptr;
                }
                ASTType *leftType = evalExprForTypeId(expr_to_eval->leftExpr,symbolTableContext,scopeContext);
                if(!leftType){
                    return nullptr;
                }
                leftType = normalizeType(leftType);

                std::shared_ptr<ASTScope> accessScope = nullptr;
                if(expr_to_eval->leftExpr->type == ID_EXPR && expr_to_eval->leftExpr->id &&
                   expr_to_eval->leftExpr->id->type == ASTIdentifier::Scope){
                    auto *leftScopeEntry = findVisibleEntryOrDiag(symbolTableContext,
                                                                  expr_to_eval->leftExpr->id->val,
                                                                  scopeContext.scope,
                                                                  errStream,
                                                                  expr_to_eval->leftExpr);
                    if(!leftScopeEntry &&
                       !symbolTableContext.collectVisibleEntriesNoDiag(expr_to_eval->leftExpr->id->val,scopeContext.scope).empty()){
                        return nullptr;
                    }
                    accessScope = scopeFromEntry(leftScopeEntry);
                }
                else if(expr_to_eval->leftExpr->type == MEMBER_EXPR){
                    auto *leftMember = expr_to_eval->leftExpr;
                    if(leftMember->isScopeAccess && leftMember->resolvedScope){
                        accessScope = leftMember->resolvedScope;
                    }
                }

                auto memberName = expr_to_eval->rightExpr->id->val;
                if(accessScope){
                    auto scopeMemberMatches = symbolTableContext.collectEntriesInExactScopeNoDiag(memberName,accessScope);
                    if(scopeMemberMatches.size() > 1){
                        emitAmbiguousLookupDiagnostic(errStream,expr_to_eval,memberName,"scope member");
                        return nullptr;
                    }
                    auto *scopeMemberEntry = scopeMemberMatches.empty() ? nullptr : scopeMemberMatches.front();
                    if(!scopeMemberEntry){
                        errStream.push(SemanticADiagnostic::create("Unknown symbol in scope member access.",expr_to_eval,Diagnostic::Error));
                        return nullptr;
                    }
                    expr_to_eval->isScopeAccess = true;
                    expr_to_eval->resolvedScope = accessScope;
                    if(scopeMemberEntry->type == Semantics::SymbolTable::Entry::Scope){
                        expr_to_eval->rightExpr->id->type = ASTIdentifier::Scope;
                        expr_to_eval->resolvedScope = scopeFromEntry(scopeMemberEntry);
                        type = scope_ref_type;
                        break;
                    }
                    if(scopeMemberEntry->type == Semantics::SymbolTable::Entry::Var){
                        auto *varData = (Semantics::SymbolTable::Var *)scopeMemberEntry->data;
                        expr_to_eval->rightExpr->id->type = ASTIdentifier::Var;
                        warnDeprecatedEntry(scopeMemberEntry,expr_to_eval,"variable");
                        type = normalizeType(varData->type);
                        break;
                    }
                    if(scopeMemberEntry->type == Semantics::SymbolTable::Entry::Function){
                        auto *funcData = (Semantics::SymbolTable::Function *)scopeMemberEntry->data;
                        expr_to_eval->rightExpr->id->type = ASTIdentifier::Function;
                        warnDeprecatedEntry(scopeMemberEntry,expr_to_eval,"function");
                        auto *memberFuncType = buildFunctionTypeFromFunctionData(funcData,expr_to_eval);
                        type = normalizeType(memberFuncType ? memberFuncType : funcData->funcType);
                        break;
                    }
                    if(scopeMemberEntry->type == Semantics::SymbolTable::Entry::Class){
                        auto *classData = (Semantics::SymbolTable::Class *)scopeMemberEntry->data;
                        expr_to_eval->rightExpr->id->type = ASTIdentifier::Class;
                        warnDeprecatedEntry(scopeMemberEntry,expr_to_eval,"class");
                        type = normalizeType(classData->classType);
                        break;
                    }
                    errStream.push(SemanticADiagnostic::create("Unsupported scope member type.",expr_to_eval,Diagnostic::Error));
                    return nullptr;
                }

                auto setBuiltinProperty = [&](ASTType *propertyType){
                    expr_to_eval->rightExpr->id->type = ASTIdentifier::Var;
                    type = propertyType;
                };
                auto setBuiltinMethod = [&](){
                    expr_to_eval->rightExpr->id->type = ASTIdentifier::Function;
                    type = ANY_TYPE;
                };

                if(isStringType(leftType)){
                    if(memberName == "length"){
                        setBuiltinProperty(INT_TYPE);
                        break;
                    }
                    if(memberName == "isEmpty" || memberName == "at" || memberName == "slice" ||
                       memberName == "contains" || memberName == "startsWith" || memberName == "endsWith" ||
                       memberName == "indexOf" || memberName == "lastIndexOf" || memberName == "lower" ||
                       memberName == "upper" || memberName == "trim" || memberName == "replace" ||
                       memberName == "split" || memberName == "repeat"){
                        setBuiltinMethod();
                        break;
                    }
                    errStream.push(SemanticADiagnostic::create("Unknown String member.",expr_to_eval,Diagnostic::Error));
                    return nullptr;
                }
                if(isRegexType(leftType)){
                    if(memberName == "match" || memberName == "findAll" || memberName == "replace"){
                        setBuiltinMethod();
                        break;
                    }
                    errStream.push(SemanticADiagnostic::create("Unknown Regex member.",expr_to_eval,Diagnostic::Error));
                    return nullptr;
                }
                if(isArrayType(leftType)){
                    if(memberName == "length"){
                        setBuiltinProperty(INT_TYPE);
                        break;
                    }
                    if(memberName == "isEmpty" || memberName == "push" || memberName == "pop" ||
                       memberName == "at" || memberName == "set" || memberName == "insert" ||
                       memberName == "removeAt" || memberName == "clear" || memberName == "contains" ||
                       memberName == "indexOf" || memberName == "slice" || memberName == "join" ||
                       memberName == "copy" || memberName == "reverse"){
                        setBuiltinMethod();
                        break;
                    }
                    errStream.push(SemanticADiagnostic::create("Unknown Array member.",expr_to_eval,Diagnostic::Error));
                    return nullptr;
                }
                if(isDictType(leftType)){
                    if(memberName == "length"){
                        setBuiltinProperty(INT_TYPE);
                        break;
                    }
                    if(memberName == "isEmpty" || memberName == "has" || memberName == "get" ||
                       memberName == "set" || memberName == "remove" || memberName == "keys" ||
                       memberName == "values" || memberName == "clear" || memberName == "copy"){
                        setBuiltinMethod();
                        break;
                    }
                    errStream.push(SemanticADiagnostic::create("Unknown Dict member.",expr_to_eval,Diagnostic::Error));
                    return nullptr;
                }
                if(isMapType(leftType)){
                    if(memberName == "length"){
                        setBuiltinProperty(INT_TYPE);
                        break;
                    }
                    if(memberName == "isEmpty" || memberName == "has" || memberName == "get" ||
                       memberName == "set" || memberName == "remove" || memberName == "keys" ||
                       memberName == "values" || memberName == "clear" || memberName == "copy"){
                        setBuiltinMethod();
                        break;
                    }
                    errStream.push(SemanticADiagnostic::create("Unknown Map member.",expr_to_eval,Diagnostic::Error));
                    return nullptr;
                }

                auto classEntry = findClassEntry(symbolTableContext,leftType->getName(),scopeContext.scope,&errStream,expr_to_eval);
                if(classEntry){
                    auto *classData = (Semantics::SymbolTable::Class *)classEntry->data;
                    auto classBindings = classBindingsFromInstanceType(classData,leftType);
                    string_set visited;
                    if(auto *field = findClassFieldRecursive(symbolTableContext,classData,memberName,scopeContext.scope,visited)){
                        expr_to_eval->rightExpr->id->type = ASTIdentifier::Var;
                        if(field->isDeprecated){
                            warnDeprecatedUse("field",
                                              memberName,
                                              std::string("field:") + leftType->getName().str() + "::" + memberName,
                                              field->deprecationMessage,
                                              expr_to_eval);
                        }
                        auto *boundFieldType = substituteTypeParams(field->type,classBindings,expr_to_eval);
                        type = normalizeType(boundFieldType);
                        break;
                    }
                    visited.clear();
                    auto methodLookup = findClassMethodRecursive(symbolTableContext,classData,memberName,scopeContext.scope,visited);
                    if(methodLookup.method){
                        expr_to_eval->rightExpr->id->type = ASTIdentifier::Function;
                        if(methodLookup.method->isDeprecated){
                            warnDeprecatedUse("method",
                                              memberName,
                                              std::string("method:") + leftType->getName().str() + "::" + memberName,
                                              methodLookup.method->deprecationMessage,
                                              expr_to_eval);
                        }
                        auto *methodType = buildFunctionTypeFromFunctionData(methodLookup.method,expr_to_eval);
                        if(!methodType){
                            methodType = methodLookup.method->funcType;
                        }
                        auto *boundMethodType = substituteTypeParams(methodType,classBindings,expr_to_eval);
                        type = normalizeType(boundMethodType);
                        break;
                    }
                }

                auto *typeEntry = findTypeEntryNoDiag(symbolTableContext,leftType->getName(),scopeContext.scope,&errStream,expr_to_eval);
                if(typeEntry && typeEntry->type == Semantics::SymbolTable::Entry::Interface){
                    auto *interfaceData = (Semantics::SymbolTable::Interface *)typeEntry->data;
                    auto interfaceBindings = interfaceBindingsFromInstanceType(interfaceData,leftType);
                    bool resolvedMember = false;
                    for(auto *field : interfaceData->fields){
                        if(field && field->name == memberName){
                            expr_to_eval->rightExpr->id->type = ASTIdentifier::Var;
                            if(field->isDeprecated){
                                warnDeprecatedUse("field",
                                                  memberName,
                                                  std::string("field:") + leftType->getName().str() + "::" + memberName,
                                                  field->deprecationMessage,
                                                  expr_to_eval);
                            }
                            auto *boundFieldType = substituteTypeParams(field->type,interfaceBindings,expr_to_eval);
                            type = normalizeType(boundFieldType);
                            resolvedMember = true;
                            break;
                        }
                    }
                    if(resolvedMember){
                        break;
                    }
                    for(auto *method : interfaceData->methods){
                        if(method && method->name == memberName){
                            expr_to_eval->rightExpr->id->type = ASTIdentifier::Function;
                            if(method->isDeprecated){
                                warnDeprecatedUse("method",
                                                  memberName,
                                                  std::string("method:") + leftType->getName().str() + "::" + memberName,
                                                  method->deprecationMessage,
                                                  expr_to_eval);
                            }
                            auto *methodType = buildFunctionTypeFromFunctionData(method,expr_to_eval);
                            if(!methodType){
                                methodType = method->funcType;
                            }
                            auto *boundMethodType = substituteTypeParams(methodType,interfaceBindings,expr_to_eval);
                            type = normalizeType(boundMethodType);
                            resolvedMember = true;
                            break;
                        }
                    }
                    if(resolvedMember){
                        break;
                    }
                }

                ASTStmt *classNode = leftType->getParentNode();
                if(classNode && classNode->type == CLASS_DECL){
                    auto *classDecl = (ASTClassDecl *)classNode;
                    bool readonly = false;
                    bool deprecated = false;
                    std::string deprecationMessage;
                    string_set visited;
                    if(auto *fieldType = findClassFieldTypeFromDeclRecursive(classDecl,
                                                                            symbolTableContext,
                                                                            scopeContext.scope,
                                                                            memberName,
                                                                            &readonly,
                                                                            &deprecated,
                                                                            &deprecationMessage,
                                                                            visited)){
                        (void)readonly;
                        expr_to_eval->rightExpr->id->type = ASTIdentifier::Var;
                        if(deprecated){
                            warnDeprecatedUse("field",
                                              memberName,
                                              std::string("field-decl:") + memberName,
                                              deprecationMessage,
                                              expr_to_eval);
                        }
                        type = normalizeType(fieldType);
                        break;
                    }
                    visited.clear();
                    if(auto *methodDecl = findClassMethodFromDeclRecursive(classDecl,symbolTableContext,scopeContext.scope,memberName,visited)){
                        expr_to_eval->rightExpr->id->type = ASTIdentifier::Function;
                        warnDeprecatedDeclAttrs("method",methodDecl->funcId,methodDecl->attributes,expr_to_eval);
                        auto *methodType = buildFunctionTypeFromDecl(methodDecl,expr_to_eval);
                        type = normalizeType(methodType ? methodType : methodDecl->funcType);
                        break;
                    }
                }
                errStream.push(SemanticADiagnostic::create("Unknown class member.",expr_to_eval,Diagnostic::Error));
                return nullptr;
            }
            case ASSIGN_EXPR : {
                if(!expr_to_eval->leftExpr || !expr_to_eval->rightExpr){
                    errStream.push(SemanticADiagnostic::create("Malformed assignment expression.",expr_to_eval,Diagnostic::Error));
                    return nullptr;
                }
                ASTType *lhsType = evalExprForTypeId(expr_to_eval->leftExpr,symbolTableContext,scopeContext);
                ASTType *rhsType = evalExprForTypeId(expr_to_eval->rightExpr,symbolTableContext,scopeContext);
                if(!lhsType || !rhsType){
                    return nullptr;
                }
                lhsType = normalizeType(lhsType);
                rhsType = normalizeType(rhsType);

                auto assignOp = expr_to_eval->oprtr_str.value_or("=");
                if(assignOp != "="){
                    auto *syntheticBinary = new ASTExpr();
                    syntheticBinary->type = BINARY_EXPR;
                    if(assignOp == "+="){
                        syntheticBinary->oprtr_str = "+";
                    }
                    else if(assignOp == "-="){
                        syntheticBinary->oprtr_str = "-";
                    }
                    else if(assignOp == "*="){
                        syntheticBinary->oprtr_str = "*";
                    }
                    else if(assignOp == "/="){
                        syntheticBinary->oprtr_str = "/";
                    }
                    else if(assignOp == "%="){
                        syntheticBinary->oprtr_str = "%";
                    }
                    else if(assignOp == "&="){
                        syntheticBinary->oprtr_str = "&";
                    }
                    else if(assignOp == "|="){
                        syntheticBinary->oprtr_str = "|";
                    }
                    else if(assignOp == "^="){
                        syntheticBinary->oprtr_str = "^";
                    }
                    else if(assignOp == "<<="){
                        syntheticBinary->oprtr_str = "<<";
                    }
                    else if(assignOp == ">>="){
                        syntheticBinary->oprtr_str = ">>";
                    }
                    else {
                        errStream.push(SemanticADiagnostic::create("Unsupported compound assignment operator.",expr_to_eval,Diagnostic::Error));
                        delete syntheticBinary;
                        return nullptr;
                    }
                    auto *compoundResultType = inferBinaryResultType(syntheticBinary,lhsType,rhsType,errStream);
                    delete syntheticBinary;
                    if(!compoundResultType){
                        return nullptr;
                    }
                    if(!lhsType->match(compoundResultType,[&](std::string){
                        errStream.push(SemanticADiagnostic::create("Compound assignment result type mismatch.",expr_to_eval,Diagnostic::Error));
                    })){
                        return nullptr;
                    }
                }
                else {
                    if(!matchExpectedExprType(lhsType,expr_to_eval->rightExpr,rhsType,errStream,"Assignment type mismatch.")){
                        return nullptr;
                    }
                }

                if(expr_to_eval->leftExpr->type == ID_EXPR && expr_to_eval->leftExpr->id){
                    auto *varEntry = findVarEntry(symbolTableContext,expr_to_eval->leftExpr->id->val,scopeContext.scope);
                    if(varEntry){
                        auto *varData = (Semantics::SymbolTable::Var *)varEntry->data;
                        if(varData && varData->isReadonly){
                            errStream.push(SemanticADiagnostic::create("Cannot assign to const variable.",expr_to_eval,Diagnostic::Error));
                            return nullptr;
                        }
                    }
                }
                else if(expr_to_eval->leftExpr->type == MEMBER_EXPR){
                    auto *memberExpr = expr_to_eval->leftExpr;
                    if(memberExpr->isScopeAccess){
                        errStream.push(SemanticADiagnostic::create("Assignment to scope-qualified symbols is not supported.",expr_to_eval,Diagnostic::Error));
                        return nullptr;
                    }
                    ASTType *baseType = evalExprForTypeId(memberExpr->leftExpr,symbolTableContext,scopeContext);
                    if(!baseType){
                        return nullptr;
                    }
                    baseType = normalizeType(baseType);
                    auto classEntry = findClassEntry(symbolTableContext,baseType->getName(),scopeContext.scope,&errStream,expr_to_eval);
                    if(classEntry){
                        auto *classData = (Semantics::SymbolTable::Class *)classEntry->data;
                        string_set visited;
                        auto *field = findClassFieldRecursive(symbolTableContext,classData,memberExpr->rightExpr->id->val,scopeContext.scope,visited);
                        if(field && field->isReadonly){
                            errStream.push(SemanticADiagnostic::create("Cannot assign to readonly/const field.",expr_to_eval,Diagnostic::Error));
                            return nullptr;
                        }
                    }
                    else {
                        ASTStmt *classNode = baseType->getParentNode();
                        if(classNode && classNode->type == CLASS_DECL){
                            bool readonly = false;
                            string_set visited;
                            auto *fieldType = findClassFieldTypeFromDeclRecursive((ASTClassDecl *)classNode,
                                                                                  symbolTableContext,
                                                                                  scopeContext.scope,
                                                                                  memberExpr->rightExpr->id->val,
                                                                                  &readonly,
                                                                                  nullptr,
                                                                                  nullptr,
                                                                                  visited);
                            (void)fieldType;
                            if(readonly){
                                errStream.push(SemanticADiagnostic::create("Cannot assign to readonly/const field.",expr_to_eval,Diagnostic::Error));
                                return nullptr;
                            }
                        }
                    }
                }
                else if(expr_to_eval->leftExpr->type == INDEX_EXPR){
                    auto *indexExpr = expr_to_eval->leftExpr;
                    ASTType *baseType = evalExprForTypeId(indexExpr->leftExpr,symbolTableContext,scopeContext);
                    ASTType *indexType = evalExprForTypeId(indexExpr->rightExpr,symbolTableContext,scopeContext);
                    if(!baseType || !indexType){
                        return nullptr;
                    }
                    baseType = normalizeType(baseType);
                    indexType = normalizeType(indexType);
                    if(isArrayType(baseType)){
                        if(!indexType->nameMatches(INT_TYPE)){
                            errStream.push(SemanticADiagnostic::create("Array assignment index must be Int.",expr_to_eval,Diagnostic::Error));
                            return nullptr;
                        }
                    }
                    else if(isDictType(baseType)){
                        if(!(isStringType(indexType) || isNumericType(indexType))){
                            errStream.push(SemanticADiagnostic::create("Dictionary assignment key must be String/Int/Long/Float/Double.",expr_to_eval,Diagnostic::Error));
                            return nullptr;
                        }
                    }
                    else if(isMapType(baseType)){
                        auto *expectedKeyType = mapKeyType(baseType,expr_to_eval);
                        if(!expectedKeyType){
                            return nullptr;
                        }
                        if(!expectedKeyType->match(indexType,[&](std::string){
                            errStream.push(SemanticADiagnostic::create("Map assignment key type mismatch.",expr_to_eval,Diagnostic::Error));
                        })){
                            return nullptr;
                        }
                    }
                    else {
                        errStream.push(SemanticADiagnostic::create("Assignment target is not indexable.",expr_to_eval,Diagnostic::Error));
                        return nullptr;
                    }
                }
                else {
                    errStream.push(SemanticADiagnostic::create("Unsupported assignment target.",expr_to_eval,Diagnostic::Error));
                    return nullptr;
                }
                type = lhsType;
                break;
            }
            case IVKE_EXPR : {
//                std::cout << expr_to_eval->callee->type << std::endl;
                if(!expr_to_eval->callee){
                    return nullptr;
                }
                expr_to_eval->runtimeCastTargetName.reset();

                auto evaluateTypeCastInvocation = [&](ASTType *targetType) -> ASTType * {
                    if(!targetType){
                        errStream.push(SemanticADiagnostic::create("Cast target type is invalid.",expr_to_eval,Diagnostic::Error));
                        return nullptr;
                    }
                    if(!expr_to_eval->genericTypeArgs.empty()){
                        errStream.push(SemanticADiagnostic::create("Cast invocation does not support generic type arguments.",expr_to_eval,Diagnostic::Error));
                        return nullptr;
                    }
                    if(expr_to_eval->exprArrayData.size() != 1){
                        errStream.push(SemanticADiagnostic::create("Type cast expects exactly one argument.",expr_to_eval,Diagnostic::Error));
                        return nullptr;
                    }
                    auto *sourceType = evalExprForTypeId(expr_to_eval->exprArrayData.front(),symbolTableContext,scopeContext);
                    if(!sourceType){
                        return nullptr;
                    }
                    sourceType = normalizeType(sourceType);
                    auto *resolvedTargetType = normalizeType(targetType);
                    if(!resolvedTargetType){
                        return nullptr;
                    }
                    if(resolvedTargetType->nameMatches(VOID_TYPE)){
                        errStream.push(SemanticADiagnostic::create("Cannot cast values to Void.",expr_to_eval,Diagnostic::Error));
                        return nullptr;
                    }

                    bool castIsThrowable = false;
                    if(resolvedTargetType->nameMatches(ANY_TYPE)){
                        castIsThrowable = false;
                    }
                    else if(isBuiltinCastTargetType(resolvedTargetType)){
                        bool conversionIsValid = false;
                        if(resolvedTargetType->nameMatches(INT_TYPE)
                           || resolvedTargetType->nameMatches(LONG_TYPE)
                           || resolvedTargetType->nameMatches(FLOAT_TYPE)
                           || resolvedTargetType->nameMatches(DOUBLE_TYPE)){
                            conversionIsValid = isNumericType(sourceType)
                                || isBoolType(sourceType)
                                || isStringType(sourceType)
                                || sourceType->nameMatches(ANY_TYPE);
                        }
                        else if(resolvedTargetType->nameMatches(BOOL_TYPE)){
                            conversionIsValid = isBoolType(sourceType)
                                || isNumericType(sourceType)
                                || isStringType(sourceType)
                                || sourceType->nameMatches(ANY_TYPE);
                        }
                        else if(resolvedTargetType->nameMatches(STRING_TYPE)){
                            conversionIsValid = true;
                        }
                        if(!conversionIsValid){
                            errStream.push(SemanticADiagnostic::create("Invalid builtin conversion for cast target type.",expr_to_eval,Diagnostic::Error));
                            return nullptr;
                        }
                    }
                    else {
                        auto *targetEntry = findTypeEntryNoDiag(symbolTableContext,resolvedTargetType->getName(),scopeContext.scope,&errStream,expr_to_eval);
                        if(!targetEntry){
                            std::ostringstream ss;
                            ss << "Unknown cast target type `" << resolvedTargetType->getName() << "`.";
                            errStream.push(SemanticADiagnostic::create(ss.str(),expr_to_eval,Diagnostic::Error));
                            return nullptr;
                        }
                        if(targetEntry->type == Semantics::SymbolTable::Entry::Interface){
                            errStream.push(SemanticADiagnostic::create("Casting to interface types is not currently supported.",expr_to_eval,Diagnostic::Error));
                            return nullptr;
                        }
                        if(targetEntry->type != Semantics::SymbolTable::Entry::Class){
                            errStream.push(SemanticADiagnostic::create("Cast target must resolve to a builtin or class type.",expr_to_eval,Diagnostic::Error));
                            return nullptr;
                        }
                        if(sourceType->nameMatches(ANY_TYPE)){
                            castIsThrowable = true;
                        }
                        else {
                            auto *sourceEntry = findTypeEntryNoDiag(symbolTableContext,sourceType->getName(),scopeContext.scope,&errStream,expr_to_eval);
                            if(!sourceEntry || sourceEntry->type != Semantics::SymbolTable::Entry::Class){
                                errStream.push(SemanticADiagnostic::create("Class casts require a class-typed source value.",expr_to_eval->exprArrayData.front(),Diagnostic::Error));
                                return nullptr;
                            }

                            bool sourceIsTarget = classExtendsOrEquals(symbolTableContext,sourceType->getName(),resolvedTargetType->getName(),scopeContext.scope);
                            bool targetIsSource = classExtendsOrEquals(symbolTableContext,resolvedTargetType->getName(),sourceType->getName(),scopeContext.scope);
                            if(sourceIsTarget){
                                castIsThrowable = false;
                            }
                            else if(targetIsSource){
                                castIsThrowable = true;
                            }
                            else {
                                errStream.push(SemanticADiagnostic::create("Class cast source and target types are unrelated.",expr_to_eval,Diagnostic::Error));
                                return nullptr;
                            }
                        }
                    }

                    auto runtimeCastName = resolveRuntimeTypeNameFromType(resolvedTargetType,
                                                                          symbolTableContext,
                                                                          scopeContext.scope,
                                                                          scopeContext.genericTypeParams,
                                                                          errStream,
                                                                          expr_to_eval);
                    if(!runtimeCastName.has_value() || runtimeCastName->empty()){
                        return nullptr;
                    }

                    auto *castResultType = cloneTypeNode(resolvedTargetType,expr_to_eval);
                    if(!castResultType){
                        return nullptr;
                    }
                    castResultType->isOptional = sourceType->isOptional;
                    castResultType->isThrowable = sourceType->isThrowable || castIsThrowable;
                    expr_to_eval->runtimeCastTargetName = runtimeCastName;
                    return normalizeType(castResultType);
                };

                if(!expr_to_eval->isConstructorCall
                   && expr_to_eval->callee->type == ID_EXPR
                   && expr_to_eval->callee->id
                   && isBuiltinCastTargetName(expr_to_eval->callee->id->val)){
                    auto *entry = symbolTableContext.findEntryNoDiag(expr_to_eval->callee->id->val,scopeContext.scope);
                    if(!entry){
                        expr_to_eval->callee->id->type = ASTIdentifier::Class;
                        auto *builtinTargetType = ASTType::Create(expr_to_eval->callee->id->val,expr_to_eval,false,false);
                        type = evaluateTypeCastInvocation(builtinTargetType);
                        if(!type){
                            return nullptr;
                        }
                        break;
                    }
                }

                if(!expr_to_eval->isConstructorCall
                   && expr_to_eval->callee->type == ID_EXPR
                   && expr_to_eval->callee->id){
                    auto calleeName = expr_to_eval->callee->id->val;
                    auto visibleMatches = symbolTableContext.collectVisibleEntriesNoDiag(calleeName,scopeContext.scope);
                    auto importedEntries = symbolTableContext.collectImportedGlobalEntriesNoDiag(calleeName);
                    bool importedValueVisible = std::any_of(importedEntries.begin(),importedEntries.end(),[](auto *entry){
                        return entry && (entry->type == Semantics::SymbolTable::Entry::Var
                                         || entry->type == Semantics::SymbolTable::Entry::Function);
                    });
                    if(visibleMatches.empty() && importedEntries.size() > 1 && importedValueVisible){
                        std::ostringstream out;
                        out << "Ambiguous imported symbol `" << calleeName
                            << "`; multiple imported modules export this name. Reference it with its module name.";
                        errStream.push(SemanticADiagnostic::create(out.str(),expr_to_eval->callee,Diagnostic::Error));
                        return nullptr;
                    }
                    if(visibleMatches.empty() && importedValueVisible){
                        std::ostringstream out;
                        out << "Imported symbol `" << calleeName << "` must be referenced with its module name.";
                        errStream.push(SemanticADiagnostic::create(out.str(),expr_to_eval->callee,Diagnostic::Error));
                        return nullptr;
                    }
                }

                ASTType *funcType = evalExprForTypeId(expr_to_eval->callee, symbolTableContext,scopeContext);
                if(!funcType){
                    return nullptr;
                }
                funcType = normalizeType(funcType);

                auto rejectUnsupportedGenericInvocation = [&](const char *message) -> ASTType * {
                    errStream.push(SemanticADiagnostic::create(message,expr_to_eval,Diagnostic::Error));
                    return nullptr;
                };
                auto *namedFunctionEntry = static_cast<Semantics::SymbolTable::Entry *>(nullptr);
                if(!expr_to_eval->isConstructorCall){
                    if(expr_to_eval->callee->type == ID_EXPR && expr_to_eval->callee->id &&
                       expr_to_eval->callee->id->type == ASTIdentifier::Function){
                        namedFunctionEntry = symbolTableContext.findEntryByEmittedNoDiag(expr_to_eval->callee->id->val);
                    }
                    else if(expr_to_eval->callee->type == MEMBER_EXPR &&
                            expr_to_eval->callee->isScopeAccess &&
                            expr_to_eval->callee->resolvedScope &&
                            expr_to_eval->callee->rightExpr &&
                            expr_to_eval->callee->rightExpr->id){
                        namedFunctionEntry = symbolTableContext.findEntryInExactScopeNoDiag(expr_to_eval->callee->rightExpr->id->val,
                                                                                             expr_to_eval->callee->resolvedScope);
                    }
                }
                auto *namedFunctionData = (namedFunctionEntry && namedFunctionEntry->type == Semantics::SymbolTable::Entry::Function)
                                        ? (Semantics::SymbolTable::Function *)namedFunctionEntry->data
                                        : nullptr;
                bool resolvedInstanceMethod = false;
                if(!expr_to_eval->isConstructorCall &&
                   expr_to_eval->callee->type == MEMBER_EXPR &&
                   !expr_to_eval->callee->isScopeAccess &&
                   expr_to_eval->callee->rightExpr &&
                   expr_to_eval->callee->rightExpr->id){
                    auto *memberExpr = expr_to_eval->callee;
                    auto *baseType = evalExprForTypeId(memberExpr->leftExpr,symbolTableContext,scopeContext);
                    if(!baseType){
                        return nullptr;
                    }
                    baseType = normalizeType(baseType);
                    auto memberName = memberExpr->rightExpr->id->val;

                    auto *classEntry = findClassEntry(symbolTableContext,baseType->getName(),scopeContext.scope,&errStream,expr_to_eval);
                    if(classEntry){
                        auto *classData = (Semantics::SymbolTable::Class *)classEntry->data;
                        auto classBindings = classBindingsFromInstanceType(classData,baseType);
                        string_set visited;
                        auto methodLookup = findClassMethodRecursive(symbolTableContext,classData,memberName,scopeContext.scope,visited);
                        if(methodLookup.method){
                            resolvedInstanceMethod = true;
                            if(!methodLookup.method->genericParams.empty()){
                                auto expectedParams = orderedFunctionParamTypes(methodLookup.method);
                                string_map<ASTType *> methodBindings = classBindings;
                                if(!expr_to_eval->genericTypeArgs.empty()){
                                    if(expr_to_eval->genericTypeArgs.size() > methodLookup.method->genericParams.size()){
                                        return rejectUnsupportedGenericInvocation("Generic method invocation type argument count does not match method generic parameter count.");
                                    }

                                    for(size_t i = 0;i < expr_to_eval->genericTypeArgs.size();++i){
                                        auto *typeArg = expr_to_eval->genericTypeArgs[i];
                                        if(!typeArg){
                                            return rejectUnsupportedGenericInvocation("Invalid generic type argument in method invocation.");
                                        }
                                        if(!typeExists(typeArg,symbolTableContext,scopeContext.scope,scopeContext.genericTypeParams,expr_to_eval)){
                                            return nullptr;
                                        }
                                        auto *resolvedTypeArg = normalizeType(typeArg);
                                        if(!resolvedTypeArg){
                                            return nullptr;
                                        }
                                        methodBindings[methodLookup.method->genericParams[i].name] = resolvedTypeArg;
                                    }
                                }
                                if(expr_to_eval->exprArrayData.size() == expectedParams.size()){
                                    if(!inferGenericInvocationBindings(methodLookup.method->genericParams,
                                                                      expectedParams,
                                                                      methodBindings,
                                                                      "Method argument type mismatch.",
                                                                      methodBindings)){
                                        return nullptr;
                                    }
                                }
                                else if(!materializeGenericDefaults(methodLookup.method->genericParams,methodBindings)){
                                    return nullptr;
                                }

                                auto *baseMethodType = buildFunctionTypeFromFunctionData(methodLookup.method,expr_to_eval);
                                if(!baseMethodType){
                                    return rejectUnsupportedGenericInvocation("Malformed generic method type.");
                                }
                                auto *specializedMethodType = substituteTypeParams(baseMethodType,methodBindings,expr_to_eval);
                                if(!specializedMethodType){
                                    return rejectUnsupportedGenericInvocation("Failed to specialize generic method invocation.");
                                }
                                funcType = normalizeType(specializedMethodType);
                            }
                            else if(!expr_to_eval->genericTypeArgs.empty()){
                                return rejectUnsupportedGenericInvocation("Explicit generic type arguments require a generic method.");
                            }
                        }
                    }
                    else {
                        auto *typeEntry = findTypeEntryNoDiag(symbolTableContext,baseType->getName(),scopeContext.scope,&errStream,expr_to_eval);
                        if(typeEntry && typeEntry->type == Semantics::SymbolTable::Entry::Interface){
                            auto *interfaceData = (Semantics::SymbolTable::Interface *)typeEntry->data;
                            auto interfaceBindings = interfaceBindingsFromInstanceType(interfaceData,baseType);
                            Semantics::SymbolTable::Function *method = nullptr;
                            for(auto *candidate : interfaceData->methods){
                                if(candidate && candidate->name == memberName){
                                    method = candidate;
                                    break;
                                }
                            }
                            if(method){
                                resolvedInstanceMethod = true;
                                if(!method->genericParams.empty()){
                                    auto expectedParams = orderedFunctionParamTypes(method);
                                    string_map<ASTType *> methodBindings = interfaceBindings;
                                    if(!expr_to_eval->genericTypeArgs.empty()){
                                        if(expr_to_eval->genericTypeArgs.size() > method->genericParams.size()){
                                            return rejectUnsupportedGenericInvocation("Generic method invocation type argument count does not match method generic parameter count.");
                                        }

                                        for(size_t i = 0;i < expr_to_eval->genericTypeArgs.size();++i){
                                            auto *typeArg = expr_to_eval->genericTypeArgs[i];
                                            if(!typeArg){
                                                return rejectUnsupportedGenericInvocation("Invalid generic type argument in method invocation.");
                                            }
                                            if(!typeExists(typeArg,symbolTableContext,scopeContext.scope,scopeContext.genericTypeParams,expr_to_eval)){
                                                return nullptr;
                                            }
                                            auto *resolvedTypeArg = normalizeType(typeArg);
                                            if(!resolvedTypeArg){
                                                return nullptr;
                                            }
                                            methodBindings[method->genericParams[i].name] = resolvedTypeArg;
                                        }
                                    }
                                    if(expr_to_eval->exprArrayData.size() == expectedParams.size()){
                                        if(!inferGenericInvocationBindings(method->genericParams,
                                                                          expectedParams,
                                                                          methodBindings,
                                                                          "Method argument type mismatch.",
                                                                          methodBindings)){
                                            return nullptr;
                                        }
                                    }
                                    else if(!materializeGenericDefaults(method->genericParams,methodBindings)){
                                        return nullptr;
                                    }

                                    auto *baseMethodType = buildFunctionTypeFromFunctionData(method,expr_to_eval);
                                    if(!baseMethodType){
                                        return rejectUnsupportedGenericInvocation("Malformed generic interface method type.");
                                    }
                                    auto *specializedMethodType = substituteTypeParams(baseMethodType,methodBindings,expr_to_eval);
                                    if(!specializedMethodType){
                                        return rejectUnsupportedGenericInvocation("Failed to specialize generic interface method invocation.");
                                    }
                                    funcType = normalizeType(specializedMethodType);
                                }
                                else if(!expr_to_eval->genericTypeArgs.empty()){
                                    return rejectUnsupportedGenericInvocation("Explicit generic type arguments require a generic method.");
                                }
                            }
                        }
                    }
                }
                if(namedFunctionData){
                    if(!namedFunctionData->genericParams.empty()){
                        auto expectedParams = orderedFunctionParamTypes(namedFunctionData);
                        string_map<ASTType *> genericBindings;
                        if(!expr_to_eval->genericTypeArgs.empty()){
                            if(expr_to_eval->genericTypeArgs.size() > namedFunctionData->genericParams.size()){
                                return rejectUnsupportedGenericInvocation("Generic free-function invocation type argument count does not match function generic parameter count.");
                            }

                            for(size_t i = 0;i < expr_to_eval->genericTypeArgs.size();++i){
                                auto *typeArg = expr_to_eval->genericTypeArgs[i];
                                if(!typeArg){
                                    return rejectUnsupportedGenericInvocation("Invalid generic type argument in free-function invocation.");
                                }
                                if(!typeExists(typeArg,symbolTableContext,scopeContext.scope,scopeContext.genericTypeParams,expr_to_eval)){
                                    return nullptr;
                                }
                                auto *resolvedTypeArg = normalizeType(typeArg);
                                if(!resolvedTypeArg){
                                    return nullptr;
                                }
                                genericBindings.insert(std::make_pair(namedFunctionData->genericParams[i].name,resolvedTypeArg));
                            }
                        }
                        if(expr_to_eval->exprArrayData.size() == expectedParams.size()){
                            if(!inferGenericInvocationBindings(namedFunctionData->genericParams,
                                                              expectedParams,
                                                              genericBindings,
                                                              "Function argument type mismatch.",
                                                              genericBindings)){
                                return nullptr;
                            }
                        }
                        else if(!materializeGenericDefaults(namedFunctionData->genericParams,genericBindings)){
                            return nullptr;
                        }

                        auto *baseFuncType = buildFunctionTypeFromFunctionData(namedFunctionData,expr_to_eval);
                        if(!baseFuncType){
                            return rejectUnsupportedGenericInvocation("Malformed generic free-function type.");
                        }
                        auto *specializedFuncType = substituteTypeParams(baseFuncType,genericBindings,expr_to_eval);
                        if(!specializedFuncType){
                            return rejectUnsupportedGenericInvocation("Failed to specialize generic free-function invocation.");
                        }
                        funcType = normalizeType(specializedFuncType);
                    }
                    else if(!expr_to_eval->genericTypeArgs.empty()){
                        return rejectUnsupportedGenericInvocation("Explicit generic type arguments require a generic free function.");
                    }
                }
                else if(!resolvedInstanceMethod && !expr_to_eval->genericTypeArgs.empty() && !expr_to_eval->isConstructorCall){
                    if(expr_to_eval->callee->type == MEMBER_EXPR){
                        return rejectUnsupportedGenericInvocation("Explicit generic type arguments require a generic method.");
                    }
                    return rejectUnsupportedGenericInvocation("Explicit generic type arguments are only supported on named free functions.");
                }

                if(!expr_to_eval->isConstructorCall){
                    bool calleeRepresentsType = false;
                    if(expr_to_eval->callee->type == ID_EXPR && expr_to_eval->callee->id
                       && expr_to_eval->callee->id->type == ASTIdentifier::Class){
                        calleeRepresentsType = true;
                    }
                    else if(expr_to_eval->callee->type == MEMBER_EXPR
                            && expr_to_eval->callee->rightExpr && expr_to_eval->callee->rightExpr->id
                            && expr_to_eval->callee->rightExpr->id->type == ASTIdentifier::Class){
                        calleeRepresentsType = true;
                    }
                    if(calleeRepresentsType){
                        type = evaluateTypeCastInvocation(funcType);
                        if(!type){
                            return nullptr;
                        }
                        break;
                    }
                }

                if(!expr_to_eval->isConstructorCall && isFunctionType(funcType)){
                    if(funcType->typeParams.empty()){
                        errStream.push(SemanticADiagnostic::create("Malformed function type.",expr_to_eval,Diagnostic::Error));
                        return nullptr;
                    }
                    size_t expectedArgCount = funcType->typeParams.size() - 1;
                    if(expr_to_eval->exprArrayData.size() != expectedArgCount){
                        errStream.push(SemanticADiagnostic::create("Incorrect number of function arguments.",expr_to_eval,Diagnostic::Error));
                        return nullptr;
                    }
                    for(size_t i = 0;i < expectedArgCount;++i){
                        auto *argType = evalExprForTypeId(expr_to_eval->exprArrayData[i],symbolTableContext,scopeContext);
                        if(!argType){
                            return nullptr;
                        }
                        argType = normalizeType(argType);
                        auto *expectedType = normalizeType(functionParamType(funcType,i,expr_to_eval));
                        if(!expectedType){
                            errStream.push(SemanticADiagnostic::create("Malformed function parameter type.",expr_to_eval,Diagnostic::Error));
                            return nullptr;
                        }
                        if(!matchExpectedExprType(expectedType,expr_to_eval->exprArrayData[i],argType,errStream,"Function argument type mismatch.")){
                            return nullptr;
                        }
                    }
                    auto *retType = functionReturnType(funcType,expr_to_eval);
                    if(!retType){
                        errStream.push(SemanticADiagnostic::create("Malformed function return type.",expr_to_eval,Diagnostic::Error));
                        return nullptr;
                    }
                    type = normalizeType(retType);
                    break;
                }

                if(!expr_to_eval->isConstructorCall &&
                   expr_to_eval->callee->type == MEMBER_EXPR && expr_to_eval->callee->isScopeAccess){
                    auto *memberExpr = expr_to_eval->callee;
                    if(!memberExpr->resolvedScope){
                        return nullptr;
                    }
                    auto *entry = symbolTableContext.findEntryInExactScopeNoDiag(memberExpr->rightExpr->id->val,memberExpr->resolvedScope);
                    if(!entry || entry->type != Semantics::SymbolTable::Entry::Function){
                        errStream.push(SemanticADiagnostic::create("Scope member invocation requires a function symbol.",expr_to_eval,Diagnostic::Error));
                        return nullptr;
                    }
                    auto *funcData = (Semantics::SymbolTable::Function *)entry->data;
                    auto expectedParams = orderedFunctionParamTypes(funcData);
                    if(expr_to_eval->exprArrayData.size() != expectedParams.size()){
                        errStream.push(SemanticADiagnostic::create("Incorrect number of function arguments.",expr_to_eval,Diagnostic::Error));
                        return nullptr;
                    }
                    for(size_t i = 0;i < expr_to_eval->exprArrayData.size();++i){
                        auto *arg = expr_to_eval->exprArrayData[i];
                        auto *argType = evalExprForTypeId(arg,symbolTableContext,scopeContext);
                        if(!argType){
                            return nullptr;
                        }
                        argType = normalizeType(argType);
                        auto *expectedParamType = normalizeType(expectedParams[i]);
                        if(!matchExpectedExprType(expectedParamType,arg,argType,errStream,"Function argument type mismatch.")){
                            return nullptr;
                        }
                    }
                    auto *resolvedReturnType = normalizeType(funcData->returnType ? funcData->returnType : VOID_TYPE);
                    type = funcData->isLazy ? normalizeType(makeTaskType(resolvedReturnType,expr_to_eval)) : resolvedReturnType;
                    break;
                }

                if(!expr_to_eval->isConstructorCall && expr_to_eval->callee->type == MEMBER_EXPR){
                    auto *memberExpr = expr_to_eval->callee;
                    ASTType *baseType = evalExprForTypeId(memberExpr->leftExpr,symbolTableContext,scopeContext);
                    if(!baseType){
                        return nullptr;
                    }
                    baseType = normalizeType(baseType);

                    auto requireArgCount = [&](size_t expected) -> bool {
                        if(expr_to_eval->exprArrayData.size() != expected){
                            errStream.push(SemanticADiagnostic::create("Incorrect number of method arguments.",expr_to_eval,Diagnostic::Error));
                            return false;
                        }
                        return true;
                    };
                    auto evalArgType = [&](size_t index) -> ASTType * {
                        if(index >= expr_to_eval->exprArrayData.size()){
                            return nullptr;
                        }
                        auto *argType = evalExprForTypeId(expr_to_eval->exprArrayData[index],symbolTableContext,scopeContext);
                        if(!argType){
                            return nullptr;
                        }
                        return normalizeType(argType);
                    };
                    auto requireIntArg = [&](size_t index) -> bool {
                        auto *argType = evalArgType(index);
                        if(!argType){
                            return false;
                        }
                        if(!isIntType(argType)){
                            errStream.push(SemanticADiagnostic::create("Method argument type mismatch.",expr_to_eval->exprArrayData[index],Diagnostic::Error));
                            return false;
                        }
                        return true;
                    };
                    auto requireStringArg = [&](size_t index) -> bool {
                        auto *argType = evalArgType(index);
                        if(!argType){
                            return false;
                        }
                        if(!isStringType(argType)){
                            errStream.push(SemanticADiagnostic::create("Method argument type mismatch.",expr_to_eval->exprArrayData[index],Diagnostic::Error));
                            return false;
                        }
                        return true;
                    };
                    auto requireDictKeyArg = [&](size_t index) -> bool {
                        auto *argType = evalArgType(index);
                        if(!argType){
                            return false;
                        }
                        if(!isDictKeyType(argType)){
                            errStream.push(SemanticADiagnostic::create("Method argument type mismatch.",expr_to_eval->exprArrayData[index],Diagnostic::Error));
                            return false;
                        }
                        return true;
                    };
                    auto requireMapKeyArg = [&](size_t index) -> bool {
                        auto *argType = evalArgType(index);
                        if(!argType){
                            return false;
                        }
                        auto *expectedKeyType = mapKeyType(baseType,expr_to_eval);
                        if(!expectedKeyType){
                            return false;
                        }
                        if(!matchExpectedExprType(expectedKeyType,expr_to_eval->exprArrayData[index],argType,errStream,"Method argument type mismatch.")){
                            return false;
                        }
                        return true;
                    };
                    auto requireAnyArg = [&](size_t index) -> bool {
                        auto *argType = evalArgType(index);
                        return argType != nullptr;
                    };
                    auto requireMapValueArg = [&](size_t index) -> bool {
                        auto *argType = evalArgType(index);
                        if(!argType){
                            return false;
                        }
                        auto *expectedValueType = mapValueType(baseType,expr_to_eval,false);
                        if(!expectedValueType){
                            return false;
                        }
                        if(!matchExpectedExprType(expectedValueType,expr_to_eval->exprArrayData[index],argType,errStream,"Method argument type mismatch.")){
                            return false;
                        }
                        return true;
                    };
                    auto requireArrayElementArg = [&](size_t index) -> bool {
                        auto *argType = evalArgType(index);
                        if(!argType){
                            return false;
                        }
                        auto *expectedElementType = arrayElementType(baseType,expr_to_eval,false);
                        if(!expectedElementType){
                            return false;
                        }
                        if(!matchExpectedExprType(expectedElementType,expr_to_eval->exprArrayData[index],argType,errStream,"Method argument type mismatch.")){
                            return false;
                        }
                        return true;
                    };
                    auto requireArraySliceArgs = [&]() -> bool {
                        return requireArgCount(2) && requireIntArg(0) && requireIntArg(1);
                    };

                    auto memberName = memberExpr->rightExpr ? memberExpr->rightExpr->id->val : std::string();
                    if(isStringType(baseType)){
                        if(memberName == "isEmpty"){
                            if(!requireArgCount(0)) return nullptr;
                            type = BOOL_TYPE;
                            break;
                        }
                        if(memberName == "at"){
                            if(!requireArgCount(1) || !requireIntArg(0)) return nullptr;
                            type = cloneTypeWithQualifiers(STRING_TYPE,expr_to_eval,true,false);
                            break;
                        }
                        if(memberName == "slice"){
                            if(!requireArraySliceArgs()) return nullptr;
                            type = STRING_TYPE;
                            break;
                        }
                        if(memberName == "contains" || memberName == "startsWith" || memberName == "endsWith"){
                            if(!requireArgCount(1) || !requireStringArg(0)) return nullptr;
                            type = BOOL_TYPE;
                            break;
                        }
                        if(memberName == "indexOf" || memberName == "lastIndexOf"){
                            if(!requireArgCount(1) || !requireStringArg(0)) return nullptr;
                            type = INT_TYPE;
                            break;
                        }
                        if(memberName == "lower" || memberName == "upper" || memberName == "trim"){
                            if(!requireArgCount(0)) return nullptr;
                            type = STRING_TYPE;
                            break;
                        }
                        if(memberName == "replace"){
                            if(!requireArgCount(2) || !requireStringArg(0) || !requireStringArg(1)) return nullptr;
                            type = STRING_TYPE;
                            break;
                        }
                        if(memberName == "split"){
                            if(!requireArgCount(1) || !requireStringArg(0)) return nullptr;
                            type = makeArrayType(STRING_TYPE,expr_to_eval);
                            break;
                        }
                        if(memberName == "repeat"){
                            if(!requireArgCount(1) || !requireIntArg(0)) return nullptr;
                            type = STRING_TYPE;
                            break;
                        }
                        errStream.push(SemanticADiagnostic::create("Unknown String method.",expr_to_eval,Diagnostic::Error));
                        return nullptr;
                    }
                    if(isRegexType(baseType)){
                        if(memberName == "match"){
                            if(!requireArgCount(1) || !requireStringArg(0)) return nullptr;
                            type = cloneTypeWithQualifiers(BOOL_TYPE,expr_to_eval,false,true);
                            break;
                        }
                        if(memberName == "findAll"){
                            if(!requireArgCount(1) || !requireStringArg(0)) return nullptr;
                            auto *matchesType = makeArrayType(STRING_TYPE,expr_to_eval);
                            if(matchesType){
                                matchesType->isThrowable = true;
                            }
                            type = matchesType;
                            break;
                        }
                        if(memberName == "replace"){
                            if(!requireArgCount(2) || !requireStringArg(0) || !requireStringArg(1)) return nullptr;
                            type = cloneTypeWithQualifiers(STRING_TYPE,expr_to_eval,false,true);
                            break;
                        }
                        errStream.push(SemanticADiagnostic::create("Unknown Regex method.",expr_to_eval,Diagnostic::Error));
                        return nullptr;
                    }
                    if(isArrayType(baseType)){
                        if(memberName == "isEmpty"){
                            if(!requireArgCount(0)) return nullptr;
                            type = BOOL_TYPE;
                            break;
                        }
                        if(memberName == "push"){
                            if(!requireArgCount(1) || !requireArrayElementArg(0)) return nullptr;
                            type = INT_TYPE;
                            break;
                        }
                        if(memberName == "pop"){
                            if(!requireArgCount(0)) return nullptr;
                            type = arrayElementType(baseType,expr_to_eval,true);
                            break;
                        }
                        if(memberName == "at"){
                            if(!requireArgCount(1) || !requireIntArg(0)) return nullptr;
                            type = arrayElementType(baseType,expr_to_eval,true);
                            break;
                        }
                        if(memberName == "set"){
                            if(!requireArgCount(2) || !requireIntArg(0) || !requireArrayElementArg(1)) return nullptr;
                            type = BOOL_TYPE;
                            break;
                        }
                        if(memberName == "insert"){
                            if(!requireArgCount(2) || !requireIntArg(0) || !requireArrayElementArg(1)) return nullptr;
                            type = BOOL_TYPE;
                            break;
                        }
                        if(memberName == "removeAt"){
                            if(!requireArgCount(1) || !requireIntArg(0)) return nullptr;
                            type = arrayElementType(baseType,expr_to_eval,true);
                            break;
                        }
                        if(memberName == "clear"){
                            if(!requireArgCount(0)) return nullptr;
                            type = BOOL_TYPE;
                            break;
                        }
                        if(memberName == "contains"){
                            if(!requireArgCount(1) || !requireArrayElementArg(0)) return nullptr;
                            type = BOOL_TYPE;
                            break;
                        }
                        if(memberName == "indexOf"){
                            if(!requireArgCount(1) || !requireArrayElementArg(0)) return nullptr;
                            type = INT_TYPE;
                            break;
                        }
                        if(memberName == "slice"){
                            if(!requireArraySliceArgs()) return nullptr;
                            type = cloneTypeNode(baseType,expr_to_eval);
                            break;
                        }
                        if(memberName == "join"){
                            if(!requireArgCount(1) || !requireStringArg(0)) return nullptr;
                            type = STRING_TYPE;
                            break;
                        }
                        if(memberName == "copy" || memberName == "reverse"){
                            if(!requireArgCount(0)) return nullptr;
                            type = cloneTypeNode(baseType,expr_to_eval);
                            break;
                        }
                        errStream.push(SemanticADiagnostic::create("Unknown Array method.",expr_to_eval,Diagnostic::Error));
                        return nullptr;
                    }
                    if(isDictType(baseType)){
                        if(memberName == "isEmpty"){
                            if(!requireArgCount(0)) return nullptr;
                            type = BOOL_TYPE;
                            break;
                        }
                        if(memberName == "has"){
                            if(!requireArgCount(1) || !requireDictKeyArg(0)) return nullptr;
                            type = BOOL_TYPE;
                            break;
                        }
                        if(memberName == "get"){
                            if(!requireArgCount(1) || !requireDictKeyArg(0)) return nullptr;
                            type = cloneTypeWithQualifiers(ANY_TYPE,expr_to_eval,true,false);
                            break;
                        }
                        if(memberName == "set"){
                            if(!requireArgCount(2) || !requireDictKeyArg(0) || !requireAnyArg(1)) return nullptr;
                            type = BOOL_TYPE;
                            break;
                        }
                        if(memberName == "remove"){
                            if(!requireArgCount(1) || !requireDictKeyArg(0)) return nullptr;
                            type = cloneTypeWithQualifiers(ANY_TYPE,expr_to_eval,true,false);
                            break;
                        }
                        if(memberName == "keys" || memberName == "values"){
                            if(!requireArgCount(0)) return nullptr;
                            type = ARRAY_TYPE;
                            break;
                        }
                        if(memberName == "clear"){
                            if(!requireArgCount(0)) return nullptr;
                            type = BOOL_TYPE;
                            break;
                        }
                        if(memberName == "copy"){
                            if(!requireArgCount(0)) return nullptr;
                            type = DICTIONARY_TYPE;
                            break;
                        }
                        errStream.push(SemanticADiagnostic::create("Unknown Dict method.",expr_to_eval,Diagnostic::Error));
                        return nullptr;
                    }
                    if(isMapType(baseType)){
                        if(memberName == "isEmpty"){
                            if(!requireArgCount(0)) return nullptr;
                            type = BOOL_TYPE;
                            break;
                        }
                        if(memberName == "has"){
                            if(!requireArgCount(1) || !requireMapKeyArg(0)) return nullptr;
                            type = BOOL_TYPE;
                            break;
                        }
                        if(memberName == "get"){
                            if(!requireArgCount(1) || !requireMapKeyArg(0)) return nullptr;
                            type = mapValueType(baseType,expr_to_eval,true);
                            break;
                        }
                        if(memberName == "set"){
                            if(!requireArgCount(2) || !requireMapKeyArg(0) || !requireMapValueArg(1)) return nullptr;
                            type = BOOL_TYPE;
                            break;
                        }
                        if(memberName == "remove"){
                            if(!requireArgCount(1) || !requireMapKeyArg(0)) return nullptr;
                            type = mapValueType(baseType,expr_to_eval,true);
                            break;
                        }
                        if(memberName == "keys"){
                            if(!requireArgCount(0)) return nullptr;
                            type = makeArrayType(mapKeyType(baseType,expr_to_eval),expr_to_eval);
                            break;
                        }
                        if(memberName == "values"){
                            if(!requireArgCount(0)) return nullptr;
                            type = makeArrayType(mapValueType(baseType,expr_to_eval,false),expr_to_eval);
                            break;
                        }
                        if(memberName == "clear"){
                            if(!requireArgCount(0)) return nullptr;
                            type = BOOL_TYPE;
                            break;
                        }
                        if(memberName == "copy"){
                            if(!requireArgCount(0)) return nullptr;
                            type = cloneTypeNode(baseType,expr_to_eval);
                            break;
                        }
                        errStream.push(SemanticADiagnostic::create("Unknown Map method.",expr_to_eval,Diagnostic::Error));
                        return nullptr;
                    }

                    auto classEntry = findClassEntry(symbolTableContext,baseType->getName(),scopeContext.scope,&errStream,expr_to_eval);
                    if(classEntry){
                        auto *classData = (Semantics::SymbolTable::Class *)classEntry->data;
                        auto classBindings = classBindingsFromInstanceType(classData,baseType);
                        string_set visited;
                        auto lookup = findClassMethodRecursive(symbolTableContext,classData,memberExpr->rightExpr->id->val,scopeContext.scope,visited);
                        if(!lookup.method){
                            errStream.push(SemanticADiagnostic::create("Unknown method.",expr_to_eval,Diagnostic::Error));
                            return nullptr;
                        }
                        auto expectedParams = orderedFunctionParamTypes(lookup.method);
                        if(expr_to_eval->exprArrayData.size() != expectedParams.size()){
                            errStream.push(SemanticADiagnostic::create("Incorrect number of method arguments.",expr_to_eval,Diagnostic::Error));
                            return nullptr;
                        }
                        for(size_t i = 0;i < expr_to_eval->exprArrayData.size();++i){
                            auto *arg = expr_to_eval->exprArrayData[i];
                            auto *argType = evalExprForTypeId(arg,symbolTableContext,scopeContext);
                            if(!argType){
                                return nullptr;
                            }
                            argType = normalizeType(argType);
                            auto *boundExpected = substituteTypeParams(expectedParams[i],classBindings,expr_to_eval);
                            boundExpected = normalizeType(boundExpected);
                            if(!matchExpectedExprType(boundExpected,arg,argType,errStream,"Method argument type mismatch.")){
                                return nullptr;
                            }
                        }
                        auto *boundReturn = substituteTypeParams(lookup.method->returnType ? lookup.method->returnType : VOID_TYPE,classBindings,expr_to_eval);
                        auto *resolvedReturnType = normalizeType(boundReturn);
                        type = lookup.method->isLazy ? normalizeType(makeTaskType(resolvedReturnType,expr_to_eval)) : resolvedReturnType;
                        break;
                    }
                    auto *typeEntry = findTypeEntryNoDiag(symbolTableContext,baseType->getName(),scopeContext.scope,&errStream,expr_to_eval);
                    if(typeEntry && typeEntry->type == Semantics::SymbolTable::Entry::Interface){
                        auto *interfaceData = (Semantics::SymbolTable::Interface *)typeEntry->data;
                        auto interfaceBindings = interfaceBindingsFromInstanceType(interfaceData,baseType);
                        Semantics::SymbolTable::Function *method = nullptr;
                        for(auto *candidate : interfaceData->methods){
                            if(candidate && candidate->name == memberExpr->rightExpr->id->val){
                                method = candidate;
                                break;
                            }
                        }
                        if(!method){
                            errStream.push(SemanticADiagnostic::create("Unknown method.",expr_to_eval,Diagnostic::Error));
                            return nullptr;
                        }
                        auto expectedParams = orderedFunctionParamTypes(method);
                        if(expr_to_eval->exprArrayData.size() != expectedParams.size()){
                            errStream.push(SemanticADiagnostic::create("Incorrect number of method arguments.",expr_to_eval,Diagnostic::Error));
                            return nullptr;
                        }
                        for(size_t i = 0;i < expr_to_eval->exprArrayData.size();++i){
                            auto *arg = expr_to_eval->exprArrayData[i];
                            auto *argType = evalExprForTypeId(arg,symbolTableContext,scopeContext);
                            if(!argType){
                                return nullptr;
                            }
                            argType = normalizeType(argType);
                            auto *boundExpected = substituteTypeParams(expectedParams[i],interfaceBindings,expr_to_eval);
                            boundExpected = normalizeType(boundExpected);
                            if(!matchExpectedExprType(boundExpected,arg,argType,errStream,"Method argument type mismatch.")){
                                return nullptr;
                            }
                        }
                        auto *boundReturn = substituteTypeParams(method->returnType ? method->returnType : VOID_TYPE,interfaceBindings,expr_to_eval);
                        auto *resolvedReturnType = normalizeType(boundReturn);
                        type = method->isLazy ? normalizeType(makeTaskType(resolvedReturnType,expr_to_eval)) : resolvedReturnType;
                        break;
                    }
                    ASTStmt *classNode = baseType->getParentNode();
                    if(classNode && classNode->type == CLASS_DECL){
                        auto *classDecl = (ASTClassDecl *)classNode;
                        string_set visited;
                        auto *methodDecl = findClassMethodFromDeclRecursive(classDecl,symbolTableContext,scopeContext.scope,memberExpr->rightExpr->id->val,visited);
                        if(!methodDecl){
                            errStream.push(SemanticADiagnostic::create("Unknown method.",expr_to_eval,Diagnostic::Error));
                            return nullptr;
                        }
                        if(expr_to_eval->exprArrayData.size() != methodDecl->params.size()){
                            errStream.push(SemanticADiagnostic::create("Incorrect number of method arguments.",expr_to_eval,Diagnostic::Error));
                            return nullptr;
                        }
                        for(auto *arg : expr_to_eval->exprArrayData){
                            auto *argType = evalExprForTypeId(arg,symbolTableContext,scopeContext);
                            if(!argType){
                                return nullptr;
                            }
                        }
                        auto *resolvedReturnType = normalizeType(methodDecl->returnType ? methodDecl->returnType : VOID_TYPE);
                        type = methodDecl->isLazy ? normalizeType(makeTaskType(resolvedReturnType,expr_to_eval)) : resolvedReturnType;
                        break;
                    }
                    return nullptr;
                }

                if(expr_to_eval->isConstructorCall){
                    Semantics::SymbolTable::Class *classData = nullptr;
                    auto *classEntry = findClassEntry(symbolTableContext,funcType->getName(),scopeContext.scope,&errStream,expr_to_eval);
                    if(classEntry){
                        classData = (Semantics::SymbolTable::Class *)classEntry->data;
                    }
                    ASTClassDecl *classDecl = nullptr;
                    ASTStmt *classNode = funcType->getParentNode();
                    if(classNode && classNode->type == CLASS_DECL){
                        classDecl = (ASTClassDecl *)classNode;
                    }

                    std::vector<ASTType *> explicitTypeArgs;
                    for(auto *genericArg : expr_to_eval->genericTypeArgs){
                        if(!genericArg){
                            errStream.push(SemanticADiagnostic::create("Invalid generic type argument in constructor call.",expr_to_eval,Diagnostic::Error));
                            return nullptr;
                        }
                        if(!typeExists(genericArg,symbolTableContext,scopeContext.scope,scopeContext.genericTypeParams,expr_to_eval)){
                            return nullptr;
                        }
                        explicitTypeArgs.push_back(normalizeType(genericArg));
                    }

                    size_t argCount = expr_to_eval->exprArrayData.size();
                    if(classData){
                        std::vector<ASTType *> resolvedTypeArgs;
                        if(!resolveTypeArgsFromSymbolDefaults(classData->genericParams,
                                                              explicitTypeArgs,
                                                              "Constructor generic argument count does not match class generic parameter count.",
                                                              resolvedTypeArgs)){
                            return nullptr;
                        }
                        if(classData->constructors.empty()){
                            if(argCount != 0){
                                errStream.push(SemanticADiagnostic::create("No constructor matches argument count.",expr_to_eval,Diagnostic::Error));
                                return nullptr;
                            }
                        }
                        else {
                            auto *ctor = findConstructorByArity(classData,argCount);
                            if(!ctor){
                                errStream.push(SemanticADiagnostic::create("No constructor matches argument count.",expr_to_eval,Diagnostic::Error));
                                return nullptr;
                            }
                            auto expectedParams = orderedFunctionParamTypes(ctor);
                            string_map<ASTType *> classBindings;
                            for(size_t i = 0;i < resolvedTypeArgs.size() && i < classData->genericParams.size();++i){
                                classBindings[classData->genericParams[i].name] = resolvedTypeArgs[i];
                            }
                            if(!ctor->genericParams.empty()){
                                string_map<ASTType *> ctorBindings;
                                if(!inferGenericInvocationBindings(ctor->genericParams,
                                                                  expectedParams,
                                                                  classBindings,
                                                                  "Constructor argument type mismatch.",
                                                                  ctorBindings)){
                                    return nullptr;
                                }
                            }
                            else {
                                for(size_t i = 0;i < expectedParams.size();++i){
                                    auto *argType = evalExprForTypeId(expr_to_eval->exprArrayData[i],symbolTableContext,scopeContext);
                                    if(!argType){
                                        return nullptr;
                                    }
                                    argType = normalizeType(argType);
                                    auto *expectedType = substituteTypeParams(expectedParams[i],classBindings,expr_to_eval);
                                    expectedType = normalizeType(expectedType);
                                    if(!expectedType || !matchExpectedExprType(expectedType,expr_to_eval->exprArrayData[i],argType,errStream,"Constructor argument type mismatch.")){
                                        return nullptr;
                                    }
                                }
                            }
                        }
                        auto *resolvedCtorType = cloneTypeNode(classData->classType,expr_to_eval);
                        if(!resolvedTypeArgs.empty()){
                            resolvedCtorType->typeParams.clear();
                            for(auto *argType : resolvedTypeArgs){
                                resolvedCtorType->addTypeParam(cloneTypeNode(argType,expr_to_eval));
                            }
                        }
                        type = normalizeType(resolvedCtorType);
                        break;
                    }

                    if(classDecl){
                        std::vector<ASTType *> resolvedTypeArgs;
                        if(!resolveTypeArgsFromAstDefaults(classDecl->genericParams,
                                                           explicitTypeArgs,
                                                           "Constructor generic argument count does not match class generic parameter count.",
                                                           resolvedTypeArgs)){
                            return nullptr;
                        }
                        if(classDecl->constructors.empty()){
                            if(argCount != 0){
                                errStream.push(SemanticADiagnostic::create("No constructor matches argument count.",expr_to_eval,Diagnostic::Error));
                                return nullptr;
                            }
                        }
                        else {
                            auto *ctorDecl = findConstructorByArityFromDecl(classDecl,argCount);
                            if(!ctorDecl){
                                errStream.push(SemanticADiagnostic::create("No constructor matches argument count.",expr_to_eval,Diagnostic::Error));
                                return nullptr;
                            }
                            auto ctorGenericParams = buildGenericParamsFromAst(ctorDecl->genericParams);
                            std::vector<ASTType *> expectedParams;
                            auto orderedCtorParams = orderedParamPairs(ctorDecl->params);
                            for(auto &paramPair : orderedCtorParams){
                                if(paramPair.second){
                                    expectedParams.push_back(paramPair.second);
                                }
                            }
                            string_map<ASTType *> classBindings;
                            for(size_t i = 0;i < resolvedTypeArgs.size() && i < classDecl->genericParams.size();++i){
                                auto *genericParam = classDecl->genericParams[i];
                                if(!genericParam || !genericParam->id){
                                    continue;
                                }
                                classBindings[genericParam->id->val] = resolvedTypeArgs[i];
                            }
                            if(!ctorGenericParams.empty()){
                                string_map<ASTType *> ctorBindings;
                                if(!inferGenericInvocationBindings(ctorGenericParams,
                                                                  expectedParams,
                                                                  classBindings,
                                                                  "Constructor argument type mismatch.",
                                                                  ctorBindings)){
                                    return nullptr;
                                }
                            }
                            else {
                                for(size_t i = 0;i < expectedParams.size();++i){
                                    auto *argType = evalExprForTypeId(expr_to_eval->exprArrayData[i],symbolTableContext,scopeContext);
                                    if(!argType){
                                        return nullptr;
                                    }
                                    argType = normalizeType(argType);
                                    auto *expectedType = substituteTypeParams(expectedParams[i],classBindings,expr_to_eval);
                                    expectedType = normalizeType(expectedType);
                                    if(!expectedType || !matchExpectedExprType(expectedType,expr_to_eval->exprArrayData[i],argType,errStream,"Constructor argument type mismatch.")){
                                        return nullptr;
                                    }
                                }
                            }
                        }
                        auto *resolvedCtorType = cloneTypeNode(classDecl->classType,expr_to_eval);
                        if(!resolvedTypeArgs.empty()){
                            resolvedCtorType->typeParams.clear();
                            for(auto *argType : resolvedTypeArgs){
                                resolvedCtorType->addTypeParam(cloneTypeNode(argType,expr_to_eval));
                            }
                        }
                        type = normalizeType(resolvedCtorType);
                        break;
                    }

                    errStream.push(SemanticADiagnostic::create("Constructor call requires a class type.",expr_to_eval,Diagnostic::Error));
                    return nullptr;
                }

                if(expr_to_eval->callee->type == ID_EXPR && expr_to_eval->callee->id &&
                   expr_to_eval->callee->id->type == ASTIdentifier::Class){
                    for(auto & arg : expr_to_eval->exprArrayData){
                        ASTType *_id = evalExprForTypeId(arg,symbolTableContext,scopeContext);
                        if(!_id){
                            return nullptr;
                        }
                    }
                    type = funcType;
                    break;
                }
                if(expr_to_eval->callee->type == MEMBER_EXPR && expr_to_eval->callee->isScopeAccess &&
                   expr_to_eval->callee->rightExpr && expr_to_eval->callee->rightExpr->id &&
                   expr_to_eval->callee->rightExpr->id->type == ASTIdentifier::Class){
                    for(auto & arg : expr_to_eval->exprArrayData){
                        ASTType *_id = evalExprForTypeId(arg,symbolTableContext,scopeContext);
                        if(!_id){
                            return nullptr;
                        }
                    }
                    type = funcType;
                    break;
                }
                /// a. Check internal function return types.
                string_ref func_name = funcType->getName();
                if(func_name == PRINT_FUNC_ID){
                    if(expr_to_eval->exprArrayData.size() > 1){
                        errStream.push(SemanticADiagnostic::create("Incorrect number of arguments",expr_to_eval,Diagnostic::Error));
                    }
                    /// print() -> Void
                    type = VOID_TYPE;
                 
                    for(auto & arg : expr_to_eval->exprArrayData){
                        ASTType *_id = evalExprForTypeId(arg,symbolTableContext,scopeContext);
                        if(!_id){
                            return nullptr;
                            break;
                        };
                        
                    };
                    // std::cout << "Print Func 2" << std::endl;
                }
                else if(func_name == SQRT_FUNC_ID){
                    if(expr_to_eval->exprArrayData.size() != 1){
                        errStream.push(SemanticADiagnostic::create("sqrt expects exactly one argument.",expr_to_eval,Diagnostic::Error));
                        return nullptr;
                    }
                    auto *argType = evalExprForTypeId(expr_to_eval->exprArrayData.front(),symbolTableContext,scopeContext);
                    if(!argType){
                        return nullptr;
                    }
                    argType = normalizeType(argType);
                    if(!isNumericType(argType)){
                        errStream.push(SemanticADiagnostic::create("sqrt requires a numeric argument.",expr_to_eval->exprArrayData.front(),Diagnostic::Error));
                        return nullptr;
                    }
                    type = DOUBLE_TYPE;
                }
                else if(func_name == ABS_FUNC_ID){
                    if(expr_to_eval->exprArrayData.size() != 1){
                        errStream.push(SemanticADiagnostic::create("abs expects exactly one argument.",expr_to_eval,Diagnostic::Error));
                        return nullptr;
                    }
                    auto *argType = evalExprForTypeId(expr_to_eval->exprArrayData.front(),symbolTableContext,scopeContext);
                    if(!argType){
                        return nullptr;
                    }
                    argType = normalizeType(argType);
                    if(!isNumericType(argType)){
                        errStream.push(SemanticADiagnostic::create("abs requires a numeric argument.",expr_to_eval->exprArrayData.front(),Diagnostic::Error));
                        return nullptr;
                    }
                    type = argType;
                }
                else if(func_name == MIN_FUNC_ID || func_name == MAX_FUNC_ID){
                    if(expr_to_eval->exprArrayData.size() != 2){
                        errStream.push(SemanticADiagnostic::create((func_name == MIN_FUNC_ID? "min expects exactly two arguments."
                                                                                          : "max expects exactly two arguments."),
                                                                   expr_to_eval,
                                                                   Diagnostic::Error));
                        return nullptr;
                    }
                    auto *lhsType = evalExprForTypeId(expr_to_eval->exprArrayData[0],symbolTableContext,scopeContext);
                    auto *rhsType = evalExprForTypeId(expr_to_eval->exprArrayData[1],symbolTableContext,scopeContext);
                    if(!lhsType || !rhsType){
                        return nullptr;
                    }
                    lhsType = normalizeType(lhsType);
                    rhsType = normalizeType(rhsType);
                    auto *resultType = promoteNumericType(lhsType,rhsType,expr_to_eval);
                    if(!resultType){
                        errStream.push(SemanticADiagnostic::create((func_name == MIN_FUNC_ID? "min requires numeric arguments."
                                                                                          : "max requires numeric arguments."),
                                                                   expr_to_eval,
                                                                   Diagnostic::Error));
                        return nullptr;
                    }
                    type = resultType;
                }
                else {
                    /// Check to see if function was previously defined.
                    SemanticsContext ctxt {errStream,expr_to_eval};
                    auto entry = symbolTableContext.findEntry(func_name,ctxt,scopeContext.scope);
                    if(!entry){
                        return nullptr;
                        break;
                    };
                    /// Check to see if symbol is function.
                    if(entry->type != Semantics::SymbolTable::Entry::Function){
                        //errStream << new SemanticADiagnostic(SemanticADiagnostic::Error,llvm::formatv("Identifier `{0}` does not identify a function",entry->name),expr_to_eval);
                        return nullptr;
                        break;
                    };

                    auto funcData = (Semantics::SymbolTable::Function *)entry->data;
                    if(!funcData->genericParams.empty()){
                        errStream.push(SemanticADiagnostic::create("Generic free-function invocation requires explicit type arguments.",expr_to_eval,Diagnostic::Error));
                        return nullptr;
                    }

                    auto expectedParams = orderedFunctionParamTypes(funcData);
                    if(expr_to_eval->exprArrayData.size() != expectedParams.size()){
                        //errStream << new SemanticADiagnostic(SemanticADiagnostic::Error,llvm::formatv("Incorrect number of arguments. Expected {0} args, but got {1}\nContext: Invocation of func `{2}`",funcData->paramMap.size(),expr_to_eval->exprArrayData.size(),funcType->getName()),expr_to_eval);
                        return nullptr;
                        break;
                    }
                    
                    for(unsigned i = 0;i < expr_to_eval->exprArrayData.size();i++){
                        auto expr_arg = expr_to_eval->exprArrayData[i];
                        ASTType *_id = evalExprForTypeId(expr_arg,symbolTableContext,scopeContext);
                        if(!_id){
                            return nullptr;
                            break;
                        };
                        _id = normalizeType(_id);
                        auto *expectedType = normalizeType(expectedParams[i]);
                        if(!expectedType->match(_id,[&](std::string message){
                            /// errStream << new SemanticADiagnostic(SemanticADiagnostic::Error,llvm::formatv("{0}\nContext: Param in invocation of func `{1}`",message,funcType->getName()),expr_arg);
                        })){
                            return nullptr;
                            break;
                        };
                    };
                    /// return return-type
                    auto *resolvedReturnType = normalizeType(funcData->returnType ? funcData->returnType : VOID_TYPE);
                    type = funcData->isLazy ? normalizeType(makeTaskType(resolvedReturnType,expr_to_eval)) : resolvedReturnType;
                };
                
                /// 1. Eval Args.
                
                
                break;
            }
            default:
                return nullptr;
                break;
        }
        return normalizeType(type);
    }

    std::optional<SemanticAConstantBoolInfo> SemanticA::evaluateCompileTimeBoolExpr(ASTExpr *expr_to_eval,
                                                                                     Semantics::STableContext &symbolTableContext,
                                                                                     ASTScopeSemanticsContext &scopeContext){
        if(!expr_to_eval){
            return std::nullopt;
        }

        if(expr_to_eval->type == BOOL_LITERAL){
            auto *literal = static_cast<ASTLiteralExpr *>(expr_to_eval);
            if(!literal->boolValue.has_value()){
                return std::nullopt;
            }
            return SemanticAConstantBoolInfo {literal->boolValue.value(),""};
        }

        if(expr_to_eval->type == UNARY_EXPR){
            auto op = expr_to_eval->oprtr_str.value_or("");
            if(op == "!"){
                auto nested = evaluateCompileTimeBoolExpr(expr_to_eval->leftExpr,symbolTableContext,scopeContext);
                if(!nested){
                    return std::nullopt;
                }
                nested->value = !nested->value;
                return nested;
            }
            return std::nullopt;
        }

        if(expr_to_eval->type == LOGIC_EXPR || expr_to_eval->type == BINARY_EXPR){
            auto op = expr_to_eval->oprtr_str.value_or("");
            if(op == "&&" || op == "||"){
                auto lhs = evaluateCompileTimeBoolExpr(expr_to_eval->leftExpr,symbolTableContext,scopeContext);
                auto rhs = evaluateCompileTimeBoolExpr(expr_to_eval->rightExpr,symbolTableContext,scopeContext);
                if(lhs && rhs){
                    return SemanticAConstantBoolInfo {
                        op == "&&" ? (lhs->value && rhs->value) : (lhs->value || rhs->value),
                        lhs->reason.empty() ? rhs->reason : lhs->reason
                    };
                }
                if(op == "&&"){
                    if(lhs && !lhs->value){
                        return SemanticAConstantBoolInfo {false,lhs->reason};
                    }
                    if(rhs && !rhs->value){
                        return SemanticAConstantBoolInfo {false,rhs->reason};
                    }
                }
                else {
                    if(lhs && lhs->value){
                        return SemanticAConstantBoolInfo {true,lhs->reason};
                    }
                    if(rhs && rhs->value){
                        return SemanticAConstantBoolInfo {true,rhs->reason};
                    }
                }
                return std::nullopt;
            }

            if(op == "==" || op == "!=" || op == "<" || op == "<=" || op == ">" || op == ">="){
                auto lhs = evaluateCompileTimeScalarLiteral(expr_to_eval->leftExpr);
                auto rhs = evaluateCompileTimeScalarLiteral(expr_to_eval->rightExpr);
                if(!lhs || !rhs || lhs->kind != rhs->kind){
                    return std::nullopt;
                }
                bool value = false;
                if(lhs->kind == CompileTimeScalarValue::Kind::Bool){
                    if(op == "==" || op == "!="){
                        value = lhs->boolValue == rhs->boolValue;
                    }
                    else {
                        return std::nullopt;
                    }
                }
                else if(lhs->kind == CompileTimeScalarValue::Kind::Number){
                    if(op == "=="){
                        value = lhs->numberValue == rhs->numberValue;
                    }
                    else if(op == "!="){
                        value = lhs->numberValue != rhs->numberValue;
                    }
                    else if(op == "<"){
                        value = lhs->numberValue < rhs->numberValue;
                    }
                    else if(op == "<="){
                        value = lhs->numberValue <= rhs->numberValue;
                    }
                    else if(op == ">"){
                        value = lhs->numberValue > rhs->numberValue;
                    }
                    else if(op == ">="){
                        value = lhs->numberValue >= rhs->numberValue;
                    }
                }
                else if(lhs->kind == CompileTimeScalarValue::Kind::String){
                    if(op == "=="){
                        value = lhs->stringValue == rhs->stringValue;
                    }
                    else if(op == "!="){
                        value = lhs->stringValue != rhs->stringValue;
                    }
                    else if(op == "<"){
                        value = lhs->stringValue < rhs->stringValue;
                    }
                    else if(op == "<="){
                        value = lhs->stringValue <= rhs->stringValue;
                    }
                    else if(op == ">"){
                        value = lhs->stringValue > rhs->stringValue;
                    }
                    else if(op == ">="){
                        value = lhs->stringValue >= rhs->stringValue;
                    }
                }
                return SemanticAConstantBoolInfo {value,""};
            }

            if(op == "is"){
                if(!expr_to_eval->runtimeTypeCheckName.has_value() || !expr_to_eval->leftExpr){
                    return std::nullopt;
                }
                auto *lhsType = evalExprForTypeId(expr_to_eval->leftExpr,symbolTableContext,scopeContext);
                if(!lhsType){
                    return std::nullopt;
                }
                lhsType = resolveAliasType(lhsType,symbolTableContext,scopeContext.scope,scopeContext.genericTypeParams);
                if(!lhsType || lhsType->isOptional || lhsType->isThrowable || lhsType->isGenericParam){
                    return std::nullopt;
                }

                const auto &targetRuntimeName = expr_to_eval->runtimeTypeCheckName.value();
                if(runtimeTypeCheckAlwaysMatches(lhsType,targetRuntimeName)){
                    std::ostringstream reason;
                    reason << "runtime type test `is` always succeeds for static type `"
                           << displayTypeNameForConstantIs(lhsType)
                           << "` against `" << targetRuntimeName << "`";
                    return SemanticAConstantBoolInfo {true,reason.str()};
                }
                if(runtimeTypeCheckAlwaysFails(lhsType,targetRuntimeName)){
                    std::ostringstream reason;
                    reason << "runtime type test `is` cannot succeed for static type `"
                           << displayTypeNameForConstantIs(lhsType)
                           << "` against `" << targetRuntimeName << "`";
                    return SemanticAConstantBoolInfo {false,reason.str()};
                }
            }
        }

        return std::nullopt;
    }

}
