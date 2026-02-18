#include "starbytes/compiler/ASTNodes.def"
#include "starbytes/compiler/SemanticA.h"
#include <set>
#include <map>

namespace starbytes {

 #define PRINT_FUNC_ID "print"
  #define EXIT_FUNC_ID "exit"

auto print_func_type = ASTType::Create(PRINT_FUNC_ID,nullptr);
auto exit_func_type = ASTType::Create(EXIT_FUNC_ID,nullptr);
auto scope_ref_type = ASTType::Create("__scope__",nullptr);

ASTType *isBuiltinType(ASTIdentifier *id){
    if(id->val == PRINT_FUNC_ID) return print_func_type;
    if(id->val == EXIT_FUNC_ID)  return exit_func_type;
    return nullptr;
}

static Semantics::SymbolTable::Entry *findClassEntry(Semantics::STableContext &symbolTableContext,
                                                     string_ref className,
                                                     std::shared_ptr<ASTScope> scope){
    auto *entry = symbolTableContext.findEntryNoDiag(className,scope);
    if(entry && entry->type == Semantics::SymbolTable::Entry::Class){
        return entry;
    }
    entry = symbolTableContext.findEntryByEmittedNoDiag(className);
    if(entry && entry->type == Semantics::SymbolTable::Entry::Class){
        return entry;
    }
    return nullptr;
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
                                     const std::map<std::string,ASTType *> &bindings,
                                     ASTStmt *parent){
    if(!type){
        return nullptr;
    }
    auto bound = bindings.find(type->getName().getBuffer());
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

static bool isGenericParamName(const std::set<std::string> *genericTypeParams,string_ref name){
    return genericTypeParams && genericTypeParams->find(name.getBuffer()) != genericTypeParams->end();
}

static ASTType *resolveAliasType(ASTType *type,
                                 Semantics::STableContext &symbolTableContext,
                                 std::shared_ptr<ASTScope> scope,
                                 const std::set<std::string> *genericTypeParams,
                                 std::set<std::string> &visiting){
    if(!type){
        return nullptr;
    }
    auto *resolved = cloneTypeNode(type,type->getParentNode());
    if(!resolved){
        return nullptr;
    }
    if(isGenericParamName(genericTypeParams,resolved->getName()) || resolved->isGenericParam){
        resolved->isGenericParam = true;
        return resolved;
    }
    auto *entry = findTypeEntryNoDiag(symbolTableContext,resolved->getName(),scope);
    if(!entry || entry->type != Semantics::SymbolTable::Entry::TypeAlias){
        return resolved;
    }
    auto key = entry->emittedName.empty()? entry->name : entry->emittedName;
    if(visiting.find(key) != visiting.end()){
        return resolved;
    }
    auto *aliasData = (Semantics::SymbolTable::TypeAlias *)entry->data;
    if(!aliasData || !aliasData->aliasType || aliasData->genericParams.size() != resolved->typeParams.size()){
        return resolved;
    }
    std::map<std::string,ASTType *> bindings;
    for(size_t i = 0;i < aliasData->genericParams.size();++i){
        bindings.insert(std::make_pair(aliasData->genericParams[i],resolved->typeParams[i]));
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
                                 const std::set<std::string> *genericTypeParams){
    std::set<std::string> visiting;
    return resolveAliasType(type,symbolTableContext,scope,genericTypeParams,visiting);
}

static std::map<std::string,ASTType *> classBindingsFromInstanceType(Semantics::SymbolTable::Class *classData,
                                                                     ASTType *instanceType){
    std::map<std::string,ASTType *> bindings;
    if(!classData || !instanceType){
        return bindings;
    }
    if(classData->genericParams.size() != instanceType->typeParams.size()){
        return bindings;
    }
    for(size_t i = 0;i < classData->genericParams.size();++i){
        bindings.insert(std::make_pair(classData->genericParams[i],instanceType->typeParams[i]));
    }
    return bindings;
}

static std::map<std::string,ASTType *> interfaceBindingsFromInstanceType(Semantics::SymbolTable::Interface *interfaceData,
                                                                         ASTType *instanceType){
    std::map<std::string,ASTType *> bindings;
    if(!interfaceData || !instanceType){
        return bindings;
    }
    if(interfaceData->genericParams.size() != instanceType->typeParams.size()){
        return bindings;
    }
    for(size_t i = 0;i < interfaceData->genericParams.size();++i){
        bindings.insert(std::make_pair(interfaceData->genericParams[i],instanceType->typeParams[i]));
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
                                                            std::set<std::string> &visited){
    if(!classData || !classData->classType){
        return nullptr;
    }
    auto className = classData->classType->getName().getBuffer();
    if(visited.find(className) != visited.end()){
        return nullptr;
    }
    visited.insert(className);
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
                                                        std::set<std::string> &visited){
    ClassMethodLookupResult result;
    if(!classData || !classData->classType){
        return result;
    }
    auto className = classData->classType->getName().getBuffer();
    if(visited.find(className) != visited.end()){
        return result;
    }
    visited.insert(className);
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
                                                    std::set<std::string> &visited){
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
        return ownField;
    }
    if(!classDecl->superClass){
        return nullptr;
    }
    auto *superDecl = resolveClassDeclFromType(classDecl->superClass,symbolTableContext,scope);
    return findClassFieldTypeFromDeclRecursive(superDecl,symbolTableContext,scope,fieldName,isReadonly,visited);
}

static ASTFuncDecl *findClassMethodFromDeclRecursive(ASTClassDecl *classDecl,
                                                     Semantics::STableContext &symbolTableContext,
                                                     std::shared_ptr<ASTScope> scope,
                                                     string_ref methodName,
                                                     std::set<std::string> &visited){
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
    return type && (type->nameMatches(INT_TYPE) || type->nameMatches(FLOAT_TYPE));
}

static bool isStringType(ASTType *type){
    return type && type->nameMatches(STRING_TYPE);
}

static bool isBoolType(ASTType *type){
    return type && type->nameMatches(BOOL_TYPE);
}

static bool isDictType(ASTType *type){
    return type && type->nameMatches(DICTIONARY_TYPE);
}

static bool isArrayType(ASTType *type){
    return type && type->nameMatches(ARRAY_TYPE);
}

static ASTType *promoteNumericType(ASTType *lhs,ASTType *rhs,ASTStmt *node){
    if(!isNumericType(lhs) || !isNumericType(rhs)){
        return nullptr;
    }
    if(lhs->nameMatches(FLOAT_TYPE) || rhs->nameMatches(FLOAT_TYPE)){
        return ASTType::Create(FLOAT_TYPE->getName(),node,false,false);
    }
    return ASTType::Create(INT_TYPE->getName(),node,false,false);
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

                /// 2. Check if scope context has args.. (In Function Context)

                if(scopeContext.args != nullptr){
                    for(auto & __arg : *scopeContext.args){
                        if(__arg.first->match(id_)){
                            id_->type = ASTIdentifier::Var;
                            return normalizeType(__arg.second);
                            break;
                        };
                    };
                    
                };

                /// 3. Else, do normal symbol matching.

                auto symbol_ = symbolTableContext.findEntry(id_->val,ctxt,scopeContext.scope);
                // std::cout << "SYMBOL_PTR:" << symbol_ << std::endl;
                if(!symbol_){
                    return nullptr;
                    break;
                };
                
                if(symbol_->type == Semantics::SymbolTable::Entry::Var){
                    auto varData = (Semantics::SymbolTable::Var *)symbol_->data;
                    id_->type = ASTIdentifier::Var;
                    if(!symbol_->emittedName.empty()){
                        id_->val = symbol_->emittedName;
                    }
                    type = varData->type;
                }
                else if(symbol_->type == Semantics::SymbolTable::Entry::Function){
                    auto funcData = (Semantics::SymbolTable::Function *)symbol_->data;
                    id_->type = ASTIdentifier::Function;
                    if(!symbol_->emittedName.empty()){
                        id_->val = symbol_->emittedName;
                    }
                    type = funcData->funcType;
                }
                else if(symbol_->type == Semantics::SymbolTable::Entry::Class){
                    auto classData = (Semantics::SymbolTable::Class *)symbol_->data;
                    id_->type = ASTIdentifier::Class;
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
                for(auto *elementExpr : expr_to_eval->exprArrayData){
                    auto *elementType = evalExprForTypeId(elementExpr,symbolTableContext,scopeContext);
                    if(!elementType){
                        return nullptr;
                    }
                }
                type = ARRAY_TYPE;
                break;
            }
            case DICT_EXPR : {
                for(auto &entry : expr_to_eval->dictExpr){
                    auto *keyType = evalExprForTypeId(entry.first,symbolTableContext,scopeContext);
                    if(!keyType){
                        return nullptr;
                    }
                    keyType = normalizeType(keyType);
                    if(!(isNumericType(keyType) || isStringType(keyType))){
                        errStream.push(SemanticADiagnostic::create("Dictionary keys must be String/Int/Float.",entry.first,Diagnostic::Error));
                        return nullptr;
                    }
                    auto *valueType = evalExprForTypeId(entry.second,symbolTableContext,scopeContext);
                    if(!valueType){
                        return nullptr;
                    }
                    valueType = normalizeType(valueType);
                }
                type = DICTIONARY_TYPE;
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
                    type = INT_TYPE;
                }
                /// Else it is a Floating Point
                else {
                    type = FLOAT_TYPE;
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
                    type = ANY_TYPE;
                    break;
                }
                if(isDictType(baseType)){
                    if(!(isStringType(indexType) || isNumericType(indexType))){
                        errStream.push(SemanticADiagnostic::create("Dictionary indexing requires String/Int/Float key.",expr_to_eval,Diagnostic::Error));
                        return nullptr;
                    }
                    type = cloneTypeWithQualifiers(ANY_TYPE,expr_to_eval,true,false);
                    break;
                }
                errStream.push(SemanticADiagnostic::create("Index expression requires Array or Dict base.",expr_to_eval,Diagnostic::Error));
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
                errStream.push(SemanticADiagnostic::create("Unsupported unary operator.",expr_to_eval,Diagnostic::Error));
                return nullptr;
            }
            case BINARY_EXPR : {
                if(!expr_to_eval->leftExpr || !expr_to_eval->rightExpr){
                    errStream.push(SemanticADiagnostic::create("Malformed binary expression.",expr_to_eval,Diagnostic::Error));
                    return nullptr;
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
                    auto *leftScopeEntry = symbolTableContext.findEntryNoDiag(expr_to_eval->leftExpr->id->val,scopeContext.scope);
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
                    auto *scopeMemberEntry = symbolTableContext.findEntryInExactScopeNoDiag(memberName,accessScope);
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
                        type = normalizeType(varData->type);
                        break;
                    }
                    if(scopeMemberEntry->type == Semantics::SymbolTable::Entry::Function){
                        auto *funcData = (Semantics::SymbolTable::Function *)scopeMemberEntry->data;
                        expr_to_eval->rightExpr->id->type = ASTIdentifier::Function;
                        type = normalizeType(funcData->funcType);
                        break;
                    }
                    if(scopeMemberEntry->type == Semantics::SymbolTable::Entry::Class){
                        auto *classData = (Semantics::SymbolTable::Class *)scopeMemberEntry->data;
                        expr_to_eval->rightExpr->id->type = ASTIdentifier::Class;
                        type = normalizeType(classData->classType);
                        break;
                    }
                    errStream.push(SemanticADiagnostic::create("Unsupported scope member type.",expr_to_eval,Diagnostic::Error));
                    return nullptr;
                }

                auto classEntry = findClassEntry(symbolTableContext,leftType->getName(),scopeContext.scope);
                if(classEntry){
                    auto *classData = (Semantics::SymbolTable::Class *)classEntry->data;
                    auto classBindings = classBindingsFromInstanceType(classData,leftType);
                    std::set<std::string> visited;
                    if(auto *field = findClassFieldRecursive(symbolTableContext,classData,memberName,scopeContext.scope,visited)){
                        expr_to_eval->rightExpr->id->type = ASTIdentifier::Var;
                        auto *boundFieldType = substituteTypeParams(field->type,classBindings,expr_to_eval);
                        type = normalizeType(boundFieldType);
                        break;
                    }
                    visited.clear();
                    auto methodLookup = findClassMethodRecursive(symbolTableContext,classData,memberName,scopeContext.scope,visited);
                    if(methodLookup.method){
                        expr_to_eval->rightExpr->id->type = ASTIdentifier::Function;
                        auto *boundMethodType = substituteTypeParams(methodLookup.method->funcType,classBindings,expr_to_eval);
                        type = normalizeType(boundMethodType);
                        break;
                    }
                }

                auto *typeEntry = findTypeEntryNoDiag(symbolTableContext,leftType->getName(),scopeContext.scope);
                if(typeEntry && typeEntry->type == Semantics::SymbolTable::Entry::Interface){
                    auto *interfaceData = (Semantics::SymbolTable::Interface *)typeEntry->data;
                    auto interfaceBindings = interfaceBindingsFromInstanceType(interfaceData,leftType);
                    bool resolvedMember = false;
                    for(auto *field : interfaceData->fields){
                        if(field && field->name == memberName){
                            expr_to_eval->rightExpr->id->type = ASTIdentifier::Var;
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
                            auto *boundMethodType = substituteTypeParams(method->funcType,interfaceBindings,expr_to_eval);
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
                    std::set<std::string> visited;
                    if(auto *fieldType = findClassFieldTypeFromDeclRecursive(classDecl,symbolTableContext,scopeContext.scope,memberName,&readonly,visited)){
                        (void)readonly;
                        expr_to_eval->rightExpr->id->type = ASTIdentifier::Var;
                        type = normalizeType(fieldType);
                        break;
                    }
                    visited.clear();
                    if(auto *methodDecl = findClassMethodFromDeclRecursive(classDecl,symbolTableContext,scopeContext.scope,memberName,visited)){
                        expr_to_eval->rightExpr->id->type = ASTIdentifier::Function;
                        type = normalizeType(methodDecl->funcType);
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
                    if(!lhsType->match(rhsType,[&](std::string){
                        errStream.push(SemanticADiagnostic::create("Assignment type mismatch.",expr_to_eval,Diagnostic::Error));
                    })){
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
                    auto classEntry = findClassEntry(symbolTableContext,baseType->getName(),scopeContext.scope);
                    if(classEntry){
                        auto *classData = (Semantics::SymbolTable::Class *)classEntry->data;
                        std::set<std::string> visited;
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
                            std::set<std::string> visited;
                            auto *fieldType = findClassFieldTypeFromDeclRecursive((ASTClassDecl *)classNode,symbolTableContext,scopeContext.scope,memberExpr->rightExpr->id->val,&readonly,visited);
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
                            errStream.push(SemanticADiagnostic::create("Dictionary assignment key must be String/Int/Float.",expr_to_eval,Diagnostic::Error));
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
                ASTType *funcType = evalExprForTypeId(expr_to_eval->callee, symbolTableContext,scopeContext);
                if(!funcType){
                    return nullptr;
                }
                funcType = normalizeType(funcType);

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
                    if(expr_to_eval->exprArrayData.size() != funcData->paramMap.size()){
                        errStream.push(SemanticADiagnostic::create("Incorrect number of function arguments.",expr_to_eval,Diagnostic::Error));
                        return nullptr;
                    }
                    auto paramIt = funcData->paramMap.begin();
                    for(auto *arg : expr_to_eval->exprArrayData){
                        auto *argType = evalExprForTypeId(arg,symbolTableContext,scopeContext);
                        if(!argType){
                            return nullptr;
                        }
                        argType = normalizeType(argType);
                        auto *expectedParamType = normalizeType(paramIt->second);
                        if(!expectedParamType->match(argType,[&](std::string){
                            errStream.push(SemanticADiagnostic::create("Function argument type mismatch.",arg,Diagnostic::Error));
                        })){
                            return nullptr;
                        }
                        ++paramIt;
                    }
                    type = normalizeType(funcData->returnType ? funcData->returnType : VOID_TYPE);
                    break;
                }

                if(!expr_to_eval->isConstructorCall && expr_to_eval->callee->type == MEMBER_EXPR){
                    auto *memberExpr = expr_to_eval->callee;
                    ASTType *baseType = evalExprForTypeId(memberExpr->leftExpr,symbolTableContext,scopeContext);
                    if(!baseType){
                        return nullptr;
                    }
                    baseType = normalizeType(baseType);
                    auto classEntry = findClassEntry(symbolTableContext,baseType->getName(),scopeContext.scope);
                    if(classEntry){
                        auto *classData = (Semantics::SymbolTable::Class *)classEntry->data;
                        auto classBindings = classBindingsFromInstanceType(classData,baseType);
                        std::set<std::string> visited;
                        auto lookup = findClassMethodRecursive(symbolTableContext,classData,memberExpr->rightExpr->id->val,scopeContext.scope,visited);
                        if(!lookup.method){
                            errStream.push(SemanticADiagnostic::create("Unknown method.",expr_to_eval,Diagnostic::Error));
                            return nullptr;
                        }
                        if(expr_to_eval->exprArrayData.size() != lookup.method->paramMap.size()){
                            errStream.push(SemanticADiagnostic::create("Incorrect number of method arguments.",expr_to_eval,Diagnostic::Error));
                            return nullptr;
                        }
                        auto paramIt = lookup.method->paramMap.begin();
                        for(auto *arg : expr_to_eval->exprArrayData){
                            auto *argType = evalExprForTypeId(arg,symbolTableContext,scopeContext);
                            if(!argType){
                                return nullptr;
                            }
                            argType = normalizeType(argType);
                            auto *boundExpected = substituteTypeParams(paramIt->second,classBindings,expr_to_eval);
                            boundExpected = normalizeType(boundExpected);
                            if(!boundExpected->match(argType,[&](std::string){
                                errStream.push(SemanticADiagnostic::create("Method argument type mismatch.",arg,Diagnostic::Error));
                            })){
                                return nullptr;
                            }
                            ++paramIt;
                        }
                        auto *boundReturn = substituteTypeParams(lookup.method->returnType ? lookup.method->returnType : VOID_TYPE,classBindings,expr_to_eval);
                        type = normalizeType(boundReturn);
                        break;
                    }
                    auto *typeEntry = findTypeEntryNoDiag(symbolTableContext,baseType->getName(),scopeContext.scope);
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
                        if(expr_to_eval->exprArrayData.size() != method->paramMap.size()){
                            errStream.push(SemanticADiagnostic::create("Incorrect number of method arguments.",expr_to_eval,Diagnostic::Error));
                            return nullptr;
                        }
                        auto paramIt = method->paramMap.begin();
                        for(auto *arg : expr_to_eval->exprArrayData){
                            auto *argType = evalExprForTypeId(arg,symbolTableContext,scopeContext);
                            if(!argType){
                                return nullptr;
                            }
                            argType = normalizeType(argType);
                            auto *boundExpected = substituteTypeParams(paramIt->second,interfaceBindings,expr_to_eval);
                            boundExpected = normalizeType(boundExpected);
                            if(!boundExpected->match(argType,[&](std::string){
                                errStream.push(SemanticADiagnostic::create("Method argument type mismatch.",arg,Diagnostic::Error));
                            })){
                                return nullptr;
                            }
                            ++paramIt;
                        }
                        auto *boundReturn = substituteTypeParams(method->returnType ? method->returnType : VOID_TYPE,interfaceBindings,expr_to_eval);
                        type = normalizeType(boundReturn);
                        break;
                    }
                    ASTStmt *classNode = baseType->getParentNode();
                    if(classNode && classNode->type == CLASS_DECL){
                        auto *classDecl = (ASTClassDecl *)classNode;
                        std::set<std::string> visited;
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
                        type = normalizeType(methodDecl->returnType ? methodDecl->returnType : VOID_TYPE);
                        break;
                    }
                    return nullptr;
                }

                if(expr_to_eval->isConstructorCall){
                    Semantics::SymbolTable::Class *classData = nullptr;
                    auto *classEntry = findClassEntry(symbolTableContext,funcType->getName(),scopeContext.scope);
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
                    for(auto *arg : expr_to_eval->exprArrayData){
                        if(!evalExprForTypeId(arg,symbolTableContext,scopeContext)){
                            return nullptr;
                        }
                    }

                    if(classData){
                        size_t expectedTypeArgCount = classData->genericParams.size();
                        if(!explicitTypeArgs.empty() && explicitTypeArgs.size() != expectedTypeArgCount){
                            errStream.push(SemanticADiagnostic::create("Constructor generic argument count does not match class generic parameter count.",expr_to_eval,Diagnostic::Error));
                            return nullptr;
                        }
                        if(classData->constructors.empty()){
                            if(argCount != 0){
                                errStream.push(SemanticADiagnostic::create("No constructor matches argument count.",expr_to_eval,Diagnostic::Error));
                                return nullptr;
                            }
                        }
                        else if(!findConstructorByArity(classData,argCount)){
                            errStream.push(SemanticADiagnostic::create("No constructor matches argument count.",expr_to_eval,Diagnostic::Error));
                            return nullptr;
                        }
                        auto *resolvedCtorType = cloneTypeNode(classData->classType,expr_to_eval);
                        if(!explicitTypeArgs.empty()){
                            resolvedCtorType->typeParams.clear();
                            for(auto *argType : explicitTypeArgs){
                                resolvedCtorType->addTypeParam(cloneTypeNode(argType,expr_to_eval));
                            }
                        }
                        type = normalizeType(resolvedCtorType);
                        break;
                    }

                    if(classDecl){
                        size_t expectedTypeArgCount = classDecl->genericTypeParams.size();
                        if(!explicitTypeArgs.empty() && explicitTypeArgs.size() != expectedTypeArgCount){
                            errStream.push(SemanticADiagnostic::create("Constructor generic argument count does not match class generic parameter count.",expr_to_eval,Diagnostic::Error));
                            return nullptr;
                        }
                        if(classDecl->constructors.empty()){
                            if(argCount != 0){
                                errStream.push(SemanticADiagnostic::create("No constructor matches argument count.",expr_to_eval,Diagnostic::Error));
                                return nullptr;
                            }
                        }
                        else if(!findConstructorByArityFromDecl(classDecl,argCount)){
                            errStream.push(SemanticADiagnostic::create("No constructor matches argument count.",expr_to_eval,Diagnostic::Error));
                            return nullptr;
                        }
                        auto *resolvedCtorType = cloneTypeNode(classDecl->classType,expr_to_eval);
                        if(!explicitTypeArgs.empty()){
                            resolvedCtorType->typeParams.clear();
                            for(auto *argType : explicitTypeArgs){
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
                    
                    if(expr_to_eval->exprArrayData.size() != funcData->paramMap.size()){
                        //errStream << new SemanticADiagnostic(SemanticADiagnostic::Error,llvm::formatv("Incorrect number of arguments. Expected {0} args, but got {1}\nContext: Invocation of func `{2}`",funcData->paramMap.size(),expr_to_eval->exprArrayData.size(),funcType->getName()),expr_to_eval);
                        return nullptr;
                        break;
                    }
                    
                    auto param_decls_it = funcData->paramMap.begin();
                    for(unsigned i = 0;i < expr_to_eval->exprArrayData.size();i++){
                        auto expr_arg = expr_to_eval->exprArrayData[i];
                        ASTType *_id = evalExprForTypeId(expr_arg,symbolTableContext,scopeContext);
                        if(!_id){
                            return nullptr;
                            break;
                        };
                        _id = normalizeType(_id);
                        auto & param_decl_pair = *param_decls_it;
                        auto *expectedType = normalizeType(param_decl_pair.second);
                        if(!expectedType->match(_id,[&](std::string message){
                            /// errStream << new SemanticADiagnostic(SemanticADiagnostic::Error,llvm::formatv("{0}\nContext: Param in invocation of func `{1}`",message,funcType->getName()),expr_arg);
                        })){
                            return nullptr;
                            break;
                        };
                        ++param_decls_it;
                    };
                    /// return return-type
                    type = normalizeType(funcData->returnType);
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

}
