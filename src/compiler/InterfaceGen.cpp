#include "starbytes/compiler/InterfaceGen.h"
#include "starbytes/compiler/ASTNodes.def"
#include "starbytes/compiler/Gen.h"

#include <algorithm>
#include <cctype>
#include <sstream>

namespace {

std::string trimLeft(const std::string &value){
    size_t idx = 0;
    while(idx < value.size() && std::isspace(static_cast<unsigned char>(value[idx]))){
        ++idx;
    }
    return value.substr(idx);
}

std::string trimBoth(const std::string &value){
    if(value.empty()){
        return value;
    }
    size_t begin = 0;
    while(begin < value.size() && std::isspace(static_cast<unsigned char>(value[begin]))){
        ++begin;
    }
    if(begin >= value.size()){
        return "";
    }
    size_t end = value.size();
    while(end > begin && std::isspace(static_cast<unsigned char>(value[end - 1]))){
        --end;
    }
    return value.substr(begin,end - begin);
}

bool startsWithKeyword(const std::string &line,const std::string &keyword){
    auto trimmed = trimLeft(line);
    if(trimmed.size() < keyword.size()){
        return false;
    }
    if(trimmed.compare(0,keyword.size(),keyword) != 0){
        return false;
    }
    if(trimmed.size() == keyword.size()){
        return true;
    }
    auto next = trimmed[keyword.size()];
    return std::isspace(static_cast<unsigned char>(next)) || next == '<' || next == '{' || next == ':';
}

std::vector<std::pair<starbytes::ASTIdentifier *,starbytes::ASTType *>> sortedParams(
    const std::map<starbytes::ASTIdentifier *,starbytes::ASTType *> &params){
    std::vector<std::pair<starbytes::ASTIdentifier *,starbytes::ASTType *>> ordered;
    ordered.reserve(params.size());
    for(auto &entry : params){
        ordered.push_back(entry);
    }
    std::sort(ordered.begin(),ordered.end(),[](const auto &lhs,const auto &rhs){
        auto *leftId = lhs.first;
        auto *rightId = rhs.first;
        if(leftId && rightId){
            if(leftId->codeRegion.startLine != rightId->codeRegion.startLine){
                return leftId->codeRegion.startLine < rightId->codeRegion.startLine;
            }
            if(leftId->codeRegion.startCol != rightId->codeRegion.startCol){
                return leftId->codeRegion.startCol < rightId->codeRegion.startCol;
            }
            return leftId->val < rightId->val;
        }
        if(leftId){
            return true;
        }
        return false;
    });
    return ordered;
}

}

namespace starbytes {

void InterfaceGen::setContext(ModuleGenContext *genContext){
    this->genContext = genContext;
    out.open(std::filesystem::path(genContext->outputPath).append(genContext->name).concat(".starbint"));
}

void InterfaceGen::finish(){
    out.close();
}

void InterfaceGen::consumeSTableContext(Semantics::STableContext *context){
    
}

const std::vector<std::string> &InterfaceGen::loadSourceLines(const std::string &path){
    auto cached = sourceLinesByPath.find(path);
    if(cached != sourceLinesByPath.end()){
        return cached->second;
    }
    auto &lines = sourceLinesByPath[path];
    if(path.empty()){
        return lines;
    }
    std::ifstream in(path,std::ios::in);
    if(!in.is_open()){
        return lines;
    }
    std::string line;
    while(std::getline(in,line)){
        lines.push_back(line);
    }
    return lines;
}

bool InterfaceGen::lineUsesKeyword(ASTStmt *stmt,string_ref keyword){
    if(!stmt || stmt->parentFile.empty() || stmt->codeRegion.startLine == 0){
        return false;
    }
    auto &lines = loadSourceLines(stmt->parentFile);
    if(lines.empty()){
        return false;
    }
    auto startIndex = stmt->codeRegion.startLine - 1;
    if(startIndex >= lines.size()){
        return false;
    }
    auto keywordStr = keyword.str();
    for(size_t offset = 0; offset < 6 && (startIndex + offset) < lines.size(); ++offset){
        auto line = trimLeft(lines[startIndex + offset]);
        if(line.empty()){
            continue;
        }
        if(line[0] == '@'){
            continue;
        }
        return startsWithKeyword(line,keywordStr);
    }
    return false;
}

std::vector<std::string> InterfaceGen::extractLeadingComments(ASTStmt *stmt){
    std::vector<std::string> comments;
    if(!stmt || stmt->parentFile.empty() || stmt->codeRegion.startLine < 2){
        return comments;
    }

    auto &lines = loadSourceLines(stmt->parentFile);
    if(lines.empty()){
        return comments;
    }

    long index = static_cast<long>(stmt->codeRegion.startLine) - 2;
    bool consumedComment = false;
    while(index >= 0){
        auto trimmed = trimBoth(lines[static_cast<size_t>(index)]);
        if(trimmed.empty()){
            if(consumedComment){
                break;
            }
            break;
        }
        if(trimmed.rfind("///",0) == 0){
            comments.push_back(trimBoth(trimmed.substr(3)));
            consumedComment = true;
            --index;
            continue;
        }
        if(trimmed.rfind("//",0) == 0){
            comments.push_back(trimBoth(trimmed.substr(2)));
            consumedComment = true;
            --index;
            continue;
        }
        if(trimmed.size() >= 2 && trimmed.substr(trimmed.size() - 2) == "*/"){
            consumedComment = true;
            while(index >= 0){
                auto blockLine = trimBoth(lines[static_cast<size_t>(index)]);
                bool hasEnd = false;
                bool hasStart = false;

                auto endPos = blockLine.rfind("*/");
                if(endPos != std::string::npos){
                    blockLine.erase(endPos,2);
                    hasEnd = true;
                }

                auto startPos = blockLine.find("/*");
                if(startPos != std::string::npos){
                    blockLine.erase(startPos,2);
                    hasStart = true;
                }

                blockLine = trimBoth(blockLine);
                if(!blockLine.empty()){
                    comments.push_back(blockLine);
                }
                --index;
                if(hasStart || (!hasEnd && startPos != std::string::npos)){
                    break;
                }
            }
            continue;
        }
        break;
    }

    std::reverse(comments.begin(),comments.end());
    return comments;
}

std::optional<std::string> InterfaceGen::renderLiteralExpr(ASTExpr *expr) const{
    if(!expr){
        return std::nullopt;
    }
    if(expr->type == STR_LITERAL){
        auto *lit = (ASTLiteralExpr *)expr;
        if(!lit->strValue.has_value()){
            return std::nullopt;
        }
        std::string escaped;
        escaped.reserve(lit->strValue->size());
        for(char ch : *lit->strValue){
            if(ch == '\\' || ch == '"'){
                escaped.push_back('\\');
            }
            escaped.push_back(ch);
        }
        return "\"" + escaped + "\"";
    }
    if(expr->type == BOOL_LITERAL){
        auto *lit = (ASTLiteralExpr *)expr;
        if(!lit->boolValue.has_value()){
            return std::nullopt;
        }
        return *lit->boolValue ? "true" : "false";
    }
    if(expr->type == NUM_LITERAL){
        auto *lit = (ASTLiteralExpr *)expr;
        if(lit->intValue.has_value()){
            return std::to_string(*lit->intValue);
        }
        if(lit->floatValue.has_value()){
            std::ostringstream out;
            out << *lit->floatValue;
            return out.str();
        }
        return std::nullopt;
    }
    if(expr->type == REGEX_LITERAL){
        auto *lit = (ASTLiteralExpr *)expr;
        if(!lit->regexPattern.has_value()){
            return std::nullopt;
        }
        std::string rendered = "/" + *lit->regexPattern + "/";
        if(lit->regexFlags.has_value()){
            rendered += *lit->regexFlags;
        }
        return rendered;
    }
    if(expr->type == ID_EXPR && expr->id){
        return expr->id->val;
    }
    if(expr->type == UNARY_EXPR && expr->oprtr_str.has_value() && *expr->oprtr_str == "-"){
        auto nested = renderLiteralExpr(expr->rightExpr);
        if(nested.has_value()){
            return "-" + *nested;
        }
    }
    return std::nullopt;
}

std::string InterfaceGen::renderType(ASTType *type) const{
    if(!type){
        return "Void";
    }

    std::string rendered;
    if(type->getName() == FUNCTION_TYPE->getName()){
        std::ostringstream fnType;
        fnType << "(";
        if(!type->typeParams.empty()){
            for(size_t i = 1;i < type->typeParams.size();++i){
                if(i > 1){
                    fnType << ",";
                }
                fnType << renderType(type->typeParams[i]);
            }
            fnType << ") " << renderType(type->typeParams.front());
        }
        else {
            fnType << ") Void";
        }
        rendered = fnType.str();
    }
    else if(type->getName() == ARRAY_TYPE->getName() && type->typeParams.size() == 1){
        rendered = renderType(type->typeParams.front()) + "[]";
    }
    else {
        rendered = type->getName().str();
        if(!type->typeParams.empty()){
            rendered += "<";
            for(size_t i = 0;i < type->typeParams.size();++i){
                if(i > 0){
                    rendered += ",";
                }
                rendered += renderType(type->typeParams[i]);
            }
            rendered += ">";
        }
    }

    if(type->isOptional){
        rendered += "?";
    }
    if(type->isThrowable){
        rendered += "!";
    }
    return rendered;
}

bool InterfaceGen::shouldEmitDecl(ASTDecl *stmt) const{
    if(!stmt || !genContext || !genContext->generateInterface){
        return false;
    }
    if(genContext->interfaceSourceAllowlist.empty()){
        return true;
    }
    return genContext->interfaceSourceAllowlist.find(stmt->parentFile) != genContext->interfaceSourceAllowlist.end();
}

void InterfaceGen::writeIndent(){
    for(unsigned i = 0;i < indentLevel;++i){
        out << "    ";
    }
}

void InterfaceGen::emitDocComments(ASTStmt *stmt){
    auto comments = extractLeadingComments(stmt);
    for(const auto &comment : comments){
        writeIndent();
        out << "///";
        if(!comment.empty()){
            out << " " << comment;
        }
        out << "\n";
    }
}

void InterfaceGen::emitVarDecl(ASTVarDecl *node){
    if(!node){
        return;
    }
    emitDocComments(node);
    for(size_t i = 0;i < node->specs.size();++i){
        auto &spec = node->specs[i];
        auto renderedInitExpr = renderLiteralExpr(spec.expr);
        bool emitConst = node->isConst && renderedInitExpr.has_value();
        writeIndent();
        out << KW_DECL << " ";
        if(emitConst){
            out << KW_IMUT << " ";
        }
        out << (spec.id ? spec.id->val : "_");
        if(spec.type){
            out << ":" << renderType(spec.type);
        }
        if(emitConst){
            out << " = " << *renderedInitExpr;
        }
        out << "\n";
    }
}

void InterfaceGen::emitFuncDecl(ASTFuncDecl *node){
    if(!node){
        return;
    }
    emitDocComments(node);
    writeIndent();
    out << "@native\n";
    writeIndent();
    if(node->isLazy){
        out << KW_LAZY << " ";
    }
    out << KW_FUNC << " " << (node->funcId ? node->funcId->val : "_") << "(";
    auto params = sortedParams(node->params);
    for(size_t i = 0;i < params.size();++i){
        auto *paramId = params[i].first;
        auto *paramType = params[i].second;
        if(i > 0){
            out << ",";
        }
        out << (paramId ? paramId->val : "_") << ":" << renderType(paramType);
    }
    out << ") " << renderType(node->returnType ? node->returnType : VOID_TYPE) << "\n";
}

void InterfaceGen::emitCtorDecl(ASTConstructorDecl *node){
    if(!node){
        return;
    }
    emitDocComments(node);
    writeIndent();
    out << KW_NEW << "(";
    auto params = sortedParams(node->params);
    for(size_t i = 0;i < params.size();++i){
        auto *paramId = params[i].first;
        auto *paramType = params[i].second;
        if(i > 0){
            out << ",";
        }
        out << (paramId ? paramId->val : "_") << ":" << renderType(paramType);
    }
    out << ") {}\n";
}

void InterfaceGen::emitClassDecl(ASTClassDecl *node){
    if(!node){
        return;
    }
    emitDocComments(node);
    writeIndent();
    out << (lineUsesKeyword(node,KW_STRUCT) ? KW_STRUCT : KW_CLASS) << " "
        << (node->id ? node->id->val : "_");
    if(!node->genericTypeParams.empty()){
        out << "<";
        for(size_t i = 0;i < node->genericTypeParams.size();++i){
            if(i > 0){
                out << ",";
            }
            out << node->genericTypeParams[i]->val;
        }
        out << ">";
    }
    if(!node->interfaces.empty()){
        out << " : ";
        for(size_t i = 0;i < node->interfaces.size();++i){
            if(i > 0){
                out << ",";
            }
            out << renderType(node->interfaces[i]);
        }
    }
    out << " {\n";

    ++indentLevel;
    for(auto *field : node->fields){
        emitDecl(field);
    }
    for(auto *method : node->methods){
        emitDecl(method);
    }
    if(!lineUsesKeyword(node,KW_STRUCT)){
        for(auto *ctor : node->constructors){
            emitDecl(ctor);
        }
    }
    --indentLevel;

    writeIndent();
    out << "}\n\n";
}

void InterfaceGen::emitInterfaceDecl(ASTInterfaceDecl *node){
    if(!node){
        return;
    }
    emitDocComments(node);
    writeIndent();
    out << KW_INTERFACE << " " << (node->id ? node->id->val : "_");
    if(!node->genericTypeParams.empty()){
        out << "<";
        for(size_t i = 0;i < node->genericTypeParams.size();++i){
            if(i > 0){
                out << ",";
            }
            out << node->genericTypeParams[i]->val;
        }
        out << ">";
    }
    out << " {\n";

    ++indentLevel;
    for(auto *field : node->fields){
        emitDecl(field);
    }
    for(auto *method : node->methods){
        emitDecl(method);
    }
    --indentLevel;

    writeIndent();
    out << "}\n\n";
}

void InterfaceGen::emitScopeDecl(ASTScopeDecl *node){
    if(!node || !node->scopeId){
        return;
    }
    emitDocComments(node);
    auto isEnum = lineUsesKeyword(node,KW_ENUM);

    if(isEnum){
        struct EnumMember {
            std::string name;
            std::optional<starbytes_int_t> value;
        };
        std::vector<EnumMember> members;
        if(node->blockStmt){
            for(auto *stmt : node->blockStmt->body){
                if(!stmt || stmt->type != VAR_DECL){
                    continue;
                }
                auto *varDecl = (ASTVarDecl *)stmt;
                for(auto &spec : varDecl->specs){
                    if(!spec.id){
                        continue;
                    }
                    EnumMember member{spec.id->val,std::nullopt};
                    if(spec.expr && spec.expr->type == NUM_LITERAL){
                        auto *lit = (ASTLiteralExpr *)spec.expr;
                        if(lit->intValue.has_value()){
                            member.value = *lit->intValue;
                        }
                    }
                    members.push_back(std::move(member));
                }
            }
        }

        writeIndent();
        out << KW_ENUM << " " << node->scopeId->val << " {\n";
        ++indentLevel;
        for(size_t i = 0;i < members.size();++i){
            writeIndent();
            out << members[i].name;
            if(members[i].value.has_value()){
                out << " = " << *members[i].value;
            }
            if(i + 1 < members.size()){
                out << ",";
            }
            out << "\n";
        }
        --indentLevel;
        writeIndent();
        out << "}\n\n";
        return;
    }

    writeIndent();
    out << KW_SCOPE << " " << node->scopeId->val << " {\n";
    ++indentLevel;
    if(node->blockStmt){
        for(auto *stmt : node->blockStmt->body){
            if(!stmt || !(stmt->type & DECL)){
                continue;
            }
            emitDecl((ASTDecl *)stmt);
        }
    }
    --indentLevel;
    writeIndent();
    out << "}\n\n";
}

void InterfaceGen::emitTypeAliasDecl(ASTTypeAliasDecl *node){
    if(!node || !node->id){
        return;
    }
    emitDocComments(node);
    writeIndent();
    out << KW_DEF << " " << node->id->val;
    if(!node->genericTypeParams.empty()){
        out << "<";
        for(size_t i = 0;i < node->genericTypeParams.size();++i){
            if(i > 0){
                out << ",";
            }
            out << node->genericTypeParams[i]->val;
        }
        out << ">";
    }
    out << " = " << renderType(node->aliasedType) << "\n\n";
}

void InterfaceGen::emitDecl(ASTDecl *stmt){
    if(!shouldEmitDecl(stmt)){
        return;
    }
    switch(stmt->type){
        case VAR_DECL:
            emitVarDecl((ASTVarDecl *)stmt);
            break;
        case FUNC_DECL:
            emitFuncDecl((ASTFuncDecl *)stmt);
            break;
        case CLASS_CTOR_DECL:
            emitCtorDecl((ASTConstructorDecl *)stmt);
            break;
        case CLASS_DECL:
            emitClassDecl((ASTClassDecl *)stmt);
            break;
        case INTERFACE_DECL:
            emitInterfaceDecl((ASTInterfaceDecl *)stmt);
            break;
        case SCOPE_DECL:
            emitScopeDecl((ASTScopeDecl *)stmt);
            break;
        case TYPE_ALIAS_DECL:
            emitTypeAliasDecl((ASTTypeAliasDecl *)stmt);
            break;
        default:
            break;
    }
}

void InterfaceGen::consumeDecl(ASTDecl *stmt) {
    emitDecl(stmt);
}

void InterfaceGen::consumeStmt(ASTStmt *stmt) {
    return;
}


};
