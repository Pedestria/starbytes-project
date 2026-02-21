#include "starbytes/compiler/SemanticA.h"
#include "starbytes/compiler/SymTable.h"
#include "starbytes/compiler/AST.h"
#include "starbytes/compiler/ASTNodes.def"
#include <iostream>
#include <sstream>
#include <algorithm>
#include <set>

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

        if(attr.name == "deprecated"){
            if(!(decl->type == VAR_DECL || decl->type == FUNC_DECL || decl->type == CLASS_DECL)){
                pushAttrError("@deprecated is only valid on class/function/field declarations.");
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
            if(!(decl->type == FUNC_DECL || decl->type == CLASS_DECL)){
                pushAttrError("@native is only valid on class/function declarations.");
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
            if(attr.name == "readonly" || attr.name == "deprecated" || attr.name == "native"){
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

    static Region deriveRegionFromExpr(ASTExpr *expr);
    static Semantics::SymbolTable::Entry *findTypeEntryNoDiag(Semantics::STableContext &symbolTableContext,
                                                               string_ref typeName,
                                                               std::shared_ptr<ASTScope> scope);

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

    static bool methodParamsMatch(const std::map<ASTIdentifier *,ASTType *> &classParams,
                                  const string_map<ASTType *> &interfaceParams){
        if(classParams.size() != interfaceParams.size()){
            return false;
        }
        for(auto &classParam : classParams){
            if(!classParam.first || !classParam.second){
                return false;
            }
            auto it = interfaceParams.find(classParam.first->val);
            if(it == interfaceParams.end()){
                return false;
            }
            if(!it->second){
                return false;
            }
            if(!classParam.second->match(it->second,[&](std::string){
                return;
            })){
                return false;
            }
        }
        return true;
    }

    static string_set genericParamSet(const std::vector<ASTIdentifier *> &params){
        string_set names;
        for(auto *param : params){
            if(param){
                names.insert(param->val);
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
        if(!type){
            return nullptr;
        }
        if(type->nameMatches(LONG_TYPE)){
            auto *canonical = ASTType::Create(INT_TYPE->getName(),type->getParentNode(),false,false);
            canonical->isOptional = type->isOptional;
            canonical->isThrowable = type->isThrowable;
            canonical->isGenericParam = type->isGenericParam;
            return canonical;
        }
        if(type->nameMatches(DOUBLE_TYPE)){
            auto *canonical = ASTType::Create(FLOAT_TYPE->getName(),type->getParentNode(),false,false);
            canonical->isOptional = type->isOptional;
            canonical->isThrowable = type->isThrowable;
            canonical->isGenericParam = type->isGenericParam;
            return canonical;
        }
        return type;
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
        if(aliasData->genericParams.size() != resolved->typeParams.size()){
            return resolved;
        }

        string_map<ASTType *> bindings;
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
                                     const string_set *genericTypeParams){
        string_set visiting;
        return resolveAliasType(type,symbolTableContext,scope,genericTypeParams,visiting);
    }

    DiagnosticPtr SemanticADiagnostic::create(string_ref message, ASTStmt *stmt, Type ty){
        auto region = deriveRegionFromStmt(stmt);
        if(ty == Diagnostic::Warning){
            if(regionHasLocation(region)){
                return StandardDiagnostic::createWarning(message,region);
            }
            return StandardDiagnostic::createWarning(message);
        }
        if(regionHasLocation(region)){
            return StandardDiagnostic::createError(message,region);
        }
        return StandardDiagnostic::createError(message);
    }

    SemanticA::SemanticA(Syntax::SyntaxA & syntaxARef,DiagnosticHandler & errStream):syntaxARef(syntaxARef),errStream(errStream){
            
    }

    void SemanticA::start(){
    }

    void SemanticA::finish(){
        
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
                 for(auto *genericParam : classDecl->genericTypeParams){
                     if(!genericParam){
                         continue;
                     }
                     data->genericParams.push_back(genericParam->val);
                     auto *paramType = ASTType::Create(genericParam->val,classDecl,true,false);
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
                         data->fields.push_back(field);
                     }
                 }
                 for(auto & m : classDecl->methods){
                     auto *method = tablePtr->allocate<Semantics::SymbolTable::Function>();
                     method->name = m->funcId->val;
                     method->returnType = m->returnType;
                     method->funcType = m->funcType;
                     method->isLazy = m->isLazy;
                     fillFunctionParamsFromDecl(method,m->params);
                     method->funcType = buildFunctionTypeFromFunctionData(method,m);
                     data->instMethods.push_back(method);
                 }
                 for(auto & c : classDecl->constructors){
                     auto *ctor = tablePtr->allocate<Semantics::SymbolTable::Function>();
                     ctor->name = "__ctor__" + std::to_string(c->params.size());
                     ctor->returnType = VOID_TYPE;
                     ctor->funcType = data->classType;
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
                for(auto *genericParam : interfaceDecl->genericTypeParams){
                    if(!genericParam){
                        continue;
                    }
                    data->genericParams.push_back(genericParam->val);
                    auto *paramType = ASTType::Create(genericParam->val,interfaceDecl,true,false);
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
                        data->fields.push_back(field);
                    }
                }

                for(auto *methodDecl : interfaceDecl->methods){
                    auto *method = tablePtr->allocate<Semantics::SymbolTable::Function>();
                    method->name = methodDecl->funcId->val;
                    method->returnType = methodDecl->returnType;
                    method->funcType = methodDecl->funcType;
                    method->isLazy = methodDecl->isLazy;
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
                for(auto *genericParam : aliasDecl->genericTypeParams){
                    if(genericParam){
                        data->genericParams.push_back(genericParam->val);
                    }
                }
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

        size_t expectedArity = 0;
        if(entry->type == Semantics::SymbolTable::Entry::Class){
            auto *classData = (Semantics::SymbolTable::Class *)entry->data;
            if(classData){
                expectedArity = classData->genericParams.size();
            }
        }
        else if(entry->type == Semantics::SymbolTable::Entry::Interface){
            auto *interfaceData = (Semantics::SymbolTable::Interface *)entry->data;
            if(interfaceData){
                expectedArity = interfaceData->genericParams.size();
            }
        }
        else if(entry->type == Semantics::SymbolTable::Entry::TypeAlias){
            auto *aliasData = (Semantics::SymbolTable::TypeAlias *)entry->data;
            if(aliasData){
                expectedArity = aliasData->genericParams.size();
            }
        }

        if(type->typeParams.size() != expectedArity){
            std::ostringstream ss;
            ss << "Type `" << type->getName() << "` expects " << expectedArity
               << " type argument(s), but got " << type->typeParams.size() << ".";
            errStream.push(SemanticADiagnostic::create(ss.str(),diagNode ? diagNode : (ASTStmt *)type->getParentNode(),Diagnostic::Error));
            return false;
        }
        return true;
    }



    bool SemanticA::typeMatches(ASTType *type,ASTExpr *expr_to_eval,Semantics::STableContext & symbolTableContext,ASTScopeSemanticsContext & scopeContext){
        
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
                    ss << message << "\nContext: Type `" << resolvedOtherType->getName() << "` was implied from var initializer";
                    errStream.push(SemanticADiagnostic::create(ss.str(),expr_to_eval,Diagnostic::Error));
                })){
                    return true;
                }
            }
        }
        
        return resolvedType->match(resolvedOtherType,[&](std::string message){
            std::ostringstream ss;
            ss << message << "\nContext: Type `" << resolvedOtherType->getName() << "` was implied from var initializer";
            auto res = ss.str();
            errStream.push(SemanticADiagnostic::create(res,expr_to_eval,Diagnostic::Error));
        });
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
                switch (decl->type) {
                    /// FuncDecl
                    case FUNC_DECL : {
    //                    std::cout << "FuncDecl" << std::endl;
                        auto funcNode = (ASTFuncDecl *)decl;
                        

                        ASTIdentifier *func_id = funcNode->funcId;
                        
                        /// Ensure that Function declared is unique within the current scope.
                        auto symEntry = symbolTableContext.main->symbolExists(func_id->val,scope);
                        if(symEntry){
                            return false;
                        };
                        
    //                    std::cout << "Func is Unique" << std::endl;

                        for(auto & paramPair : funcNode->params){
                            if(!typeExists(paramPair.second,symbolTableContext,scope,nullptr,funcNode)){
                                return false;
                                break;
                            };
                            paramPair.second = resolveAliasType(paramPair.second,symbolTableContext,scope,nullptr);
                        };
                        if(funcNode->returnType != nullptr){
                            if(!typeExists(funcNode->returnType,symbolTableContext,scope,nullptr,funcNode)){
                                return false;
                            }
                            funcNode->returnType = resolveAliasType(funcNode->returnType,symbolTableContext,scope,nullptr);
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

                            bool hasFailed = false;
                            ASTScopeSemanticsContext funcScopeContext {funcNode->blockStmt->parentScope,&funcNode->params};
                            ASTType *return_type_implied = evalBlockStmtForASTType(funcNode->blockStmt,symbolTableContext,&hasFailed,funcScopeContext,true);
                            if(!return_type_implied && hasFailed){
                                return false;
                            };
                            /// Implied Type and Declared Type Comparison.
                            if(funcNode->returnType != nullptr){
                                auto *resolvedImplied = resolveAliasType(return_type_implied,symbolTableContext,scope,nullptr);
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

                        auto classGenericParams = genericParamSet(classDecl->genericTypeParams);
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
                            for(auto &paramPair : m->params){
                                if(!typeExists(paramPair.second,symbolTableContext,scope,&classGenericParams,m)){
                                    return false;
                                }
                                paramPair.second = resolveAliasType(paramPair.second,symbolTableContext,scope,&classGenericParams);
                            }
                            if(m->returnType){
                                if(!typeExists(m->returnType,symbolTableContext,scope,&classGenericParams,m)){
                                    return false;
                                }
                                m->returnType = resolveAliasType(m->returnType,symbolTableContext,scope,&classGenericParams);
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
                            ASTScopeSemanticsContext methodScopeContext {m->blockStmt->parentScope,&methodParams,&classGenericParams};
                            ASTType *returnTypeImplied = evalBlockStmtForASTType(m->blockStmt,symbolTableContext,&methodHasFailed,methodScopeContext,true);
                            if(methodHasFailed || !returnTypeImplied){
                                return false;
                            }
                            if(m->returnType != nullptr){
                                auto *resolvedImplied = resolveAliasType(returnTypeImplied,symbolTableContext,scope,&classGenericParams);
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
                        }

                        std::set<size_t> ctorArities;
                        for(auto &c : classDecl->constructors){
                            size_t arity = c->params.size();
                            if(ctorArities.find(arity) != ctorArities.end()){
                                errStream.push(SemanticADiagnostic::create("Duplicate constructor arity.",c,Diagnostic::Error));
                                return false;
                            }
                            ctorArities.insert(arity);
                            for(auto &paramPair : c->params){
                                if(!typeExists(paramPair.second,symbolTableContext,scope,&classGenericParams,c)){
                                    return false;
                                }
                                paramPair.second = resolveAliasType(paramPair.second,symbolTableContext,scope,&classGenericParams);
                            }
                            std::map<ASTIdentifier *,ASTType *> ctorParams = c->params;
                            auto *selfId = new ASTIdentifier();
                            selfId->val = "self";
                            ctorParams.insert(std::make_pair(selfId,classDecl->classType));
                            bool ctorHasFailed = false;
                            ASTScopeSemanticsContext ctorScopeContext {c->blockStmt->parentScope,&ctorParams,&classGenericParams};
                            ASTType *ctorReturnType = evalBlockStmtForASTType(c->blockStmt,symbolTableContext,&ctorHasFailed,ctorScopeContext,true);
                            if(ctorHasFailed || !ctorReturnType){
                                return false;
                            }
                            if(ctorReturnType != VOID_TYPE){
                                errStream.push(SemanticADiagnostic::create("Constructor cannot return a value.",c,Diagnostic::Error));
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
                                interfaceBindings.insert(std::make_pair(interfaceData->genericParams[i],implementedInterfaceType->typeParams[i]));
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
                                if(!classMethod->returnType || !requiredMethod->returnType){
                                    errStream.push(SemanticADiagnostic::create("Interface method return type could not be resolved.",classMethod,Diagnostic::Error));
                                    return false;
                                }
                                auto *requiredReturnType = substituteTypeParams(requiredMethod->returnType,interfaceBindings,classDecl);
                                requiredReturnType = resolveAliasType(requiredReturnType,symbolTableContext,scope,&classGenericParams);
                                auto *classReturnType = resolveAliasType(classMethod->returnType,symbolTableContext,scope,&classGenericParams);
                                if(!requiredReturnType->match(classReturnType,[&](std::string message){
                                    std::ostringstream ss;
                                    ss << message << "\nContext: Interface method `" << requiredMethod->name << "` return type mismatch.";
                                    errStream.push(SemanticADiagnostic::create(ss.str(),classMethod,Diagnostic::Error));
                                })){
                                    return false;
                                }
                                string_map<ASTType *> requiredParamMap = requiredMethod->paramMap;
                                for(auto &requiredParam : requiredParamMap){
                                    auto *substituted = substituteTypeParams(requiredParam.second,interfaceBindings,classDecl);
                                    requiredParam.second = resolveAliasType(substituted,symbolTableContext,scope,&classGenericParams);
                                }
                                if(!methodParamsMatch(classMethod->params,requiredParamMap)){
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

                        auto interfaceGenericParams = genericParamSet(interfaceDecl->genericTypeParams);
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

                            for(auto &paramPair : methodDecl->params){
                                if(!typeExists(paramPair.second,symbolTableContext,scope,&interfaceGenericParams,methodDecl)){
                                    return false;
                                }
                                paramPair.second = resolveAliasType(paramPair.second,symbolTableContext,scope,&interfaceGenericParams);
                            }
                            if(methodDecl->returnType){
                                if(!typeExists(methodDecl->returnType,symbolTableContext,scope,&interfaceGenericParams,methodDecl)){
                                    return false;
                                }
                                methodDecl->returnType = resolveAliasType(methodDecl->returnType,symbolTableContext,scope,&interfaceGenericParams);
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
                            ASTScopeSemanticsContext methodScopeContext {methodDecl->blockStmt->parentScope,&methodParams,&interfaceGenericParams};
                            ASTType *returnTypeImplied = evalBlockStmtForASTType(methodDecl->blockStmt,symbolTableContext,&methodHasFailed,methodScopeContext,true);
                            if(methodHasFailed || !returnTypeImplied){
                                return false;
                            }
                            if(methodDecl->returnType != nullptr){
                                auto *resolvedImplied = resolveAliasType(returnTypeImplied,symbolTableContext,scope,&interfaceGenericParams);
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
                        auto aliasGenericParams = genericParamSet(aliasDecl->genericTypeParams);
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
                        for(auto *innerStmt : scopeDecl->blockStmt->body){
                            auto ok = checkSymbolsForStmtInScope(innerStmt,symbolTableContext,scopeDecl->blockStmt->parentScope);
                            if(!ok){
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
        return checkSymbolsForStmtInScope(stmt,symbolTableContext,ASTScopeGlobal);
    }

    
}
