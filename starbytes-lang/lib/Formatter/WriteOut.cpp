#include "starbytes/Formatter/WriteOut.h"
#include "starbytes/Core/Core.h"
#include <fstream>
#include <sstream>

STARBYTES_STD_NAMESPACE

class STBFormatter {
    private:
        std::ofstream & _file_out;
        std::stringstream out;
        AbstractSyntaxTree *& tree;
        unsigned char_per_line_lim;
        unsigned char_count;
        template<typename Ty>
        inline void writeToOutStream(Ty subject,unsigned count = sizeof(Ty)){
            out << subject;
            char_count += count;
        };
        inline bool checkCharCount(){
            return char_per_line_lim <= char_count;
        };
        template<typename _Ty>
        inline void writeAndCheck(_Ty subject,unsigned count = sizeof(_Ty)){
            writeToOutStream(subject,count);
            if(checkCharCount()){
                writeToOutStream('\n');
            }
        };
        //
        //Format Functions!!
        //

        inline void formatTypeId(ASTTypeIdentifier *& tid){
            writeToOutStream(tid->type_name,tid->type_name.size());
            if(tid->isGeneric) {
                writeToOutStream('<');
                for(auto i = 0; i < tid->typeargs.size();++i){
                    formatTypeId(tid->typeargs[i]);
                    if(i > 0)
                        writeToOutStream(',');
                }
                writeToOutStream('>');
            }
            
            if(tid->isArrayType)
                for(int i = 0;i < tid->array_count;++i){
                    writeToOutStream("[]",2);
                }
        };

        inline void formatId(ASTIdentifier * id){
            writeToOutStream(id->value,id->value.size());
        };



        void formatExpr(ASTExpression *& exp){
            if(AST_NODE_IS(exp,ASTIdentifier)){
                formatId(ASSERT_AST_NODE(exp,ASTIdentifier));
            }
            else if(AST_NODE_IS(exp,ASTNumericLiteral)){
                ASTNumericLiteral *ptr = ASSERT_AST_NODE(exp,ASTNumericLiteral);
                writeToOutStream(ptr->value,ptr->value.size());
            }
            else if(AST_NODE_IS(exp,ASTBooleanLiteral)){
                ASTBooleanLiteral * ptr = ASSERT_AST_NODE(exp,ASTBooleanLiteral);
                writeToOutStream(ptr->value,ptr->value.size());
            }
            else if(AST_NODE_IS(exp,ASTStringLiteral)){
                ASTStringLiteral * ptr =  ASSERT_AST_NODE(exp,ASTStringLiteral);
                writeToOutStream('"');
                writeToOutStream(ptr->value,ptr->value.size());
                writeToOutStream('"');
            }
        };

        void formatVariableDecl(ASTVariableDeclaration * node){
            writeToOutStream("decl",4);
            unsigned int len = node->specifiers.size();
            unsigned int index = 1;
            for(auto & spec : node->specifiers){
                
                if(AST_NODE_IS(spec->id,ASTTypeCastIdentifier)){
                    writeToOutStream(' ');
                    ASTTypeCastIdentifier * _id = ASSERT_AST_NODE(spec->id,ASTTypeCastIdentifier);
                    formatId(_id->id);
                    writeToOutStream(':');
                    formatTypeId(_id->tid);
                    if(spec->initializer.has_value()){
                        writeToOutStream(' ');
                        writeToOutStream("= ",2);
                        formatExpr(spec->initializer.value());
                    }
                }
                else if(AST_NODE_IS(spec->id,ASTIdentifier)){
                    writeToOutStream(' ');
                    ASTIdentifier * _id = ASSERT_AST_NODE(spec->id,ASTIdentifier);
                    formatId(_id);
                    if(spec->initializer.has_value()){
                        writeToOutStream(' ');
                        writeToOutStream("= ",2);
                        formatExpr(spec->initializer.value());
                    }
                }
                ++index;
                if(index < len)
                    writeAndCheck(',',1);
            }
        };

        void formatConstantDecl(ASTConstantDeclaration * node){
            writeToOutStream("decl immutable",14);
            unsigned int len = node->specifiers.size();
            unsigned int index = 1;
            for(auto & spec : node->specifiers){
                
                if(AST_NODE_IS(spec->id,ASTTypeCastIdentifier)){
                    writeToOutStream(' ');
                    ASTTypeCastIdentifier * _id = ASSERT_AST_NODE(spec->id,ASTTypeCastIdentifier);
                    formatId(_id->id);
                    writeToOutStream(':');
                    formatTypeId(_id->tid);
                    if(spec->initializer.has_value()){
                        writeToOutStream(' ');
                        writeToOutStream("= ",2);
                        formatExpr(spec->initializer.value());
                    }
                }
                else if(AST_NODE_IS(spec->id,ASTIdentifier)){
                    writeToOutStream(' ');
                    ASTIdentifier * _id = ASSERT_AST_NODE(spec->id,ASTIdentifier);
                    formatId(_id);
                    if(spec->initializer.has_value()){
                        writeToOutStream(' ');
                        writeToOutStream("= ",2);
                        formatExpr(spec->initializer.value());
                    }
                }
                ++index;
                if(index < len)
                    writeAndCheck(',',1);
            }
        };
        void formatStatement(ASTStatement *& node){
            if(AST_NODE_IS(node,ASTVariableDeclaration)){
                formatVariableDecl(ASSERT_AST_NODE(node,ASTVariableDeclaration));
            }
            else if(AST_NODE_IS(node,ASTConstantDeclaration)){
                formatConstantDecl(ASSERT_AST_NODE(node,ASTConstantDeclaration));
            }
        };
    public:
        STBFormatter(std::ofstream & o,AbstractSyntaxTree *& _tree):_file_out(o),tree(_tree){};
        void format(){
            for(auto & n : *tree){
                formatStatement(n);
            }
        };
};

void format_source_file(std::string & file_to_format){
    std::string * src_code = Foundation::readFile(file_to_format);
    AST::AbstractSyntaxTree * src_ast = parseCode(*src_code);
    std::ofstream out;
    out.open(file_to_format);
    STBFormatter(out,src_ast).format();
};

NAMESPACE_END