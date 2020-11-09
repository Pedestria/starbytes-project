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

        inline void formatTypeId(ASTTypeIdentifier *tid){

        };

        void formatVariableDecl(ASTVariableDeclaration * node){
            writeToOutStream("decl",4);
            unsigned int len = node->specifiers.size();
            unsigned int index = 1;
            for(auto & spec : node->specifiers){
                
                if(AST_NODE_IS(spec->id,ASTTypeCastIdentifier)){
                    writeToOutStream(' ');
                    ASTTypeCastIdentifier * _id = ASSERT_AST_NODE(spec->id,ASTTypeCastIdentifier);
                    writeToOutStream(_id->id->value,_id->id->value.size());
                }
                else if(AST_NODE_IS(spec->id,ASTIdentifier)){
                    writeToOutStream(' ');
                    ASTIdentifier * _id = ASSERT_AST_NODE(spec->id,ASTIdentifier);
                    writeToOutStream(_id->value,_id->value.size());
                }
                ++index;
                if(index < len)
                    writeToOutStream(',',1);
            }
        };
        void formatStatement(ASTStatement *& node){
            if(AST_NODE_IS(node,ASTVariableDeclaration)){
                formatVariableDecl(ASSERT_AST_NODE(node,ASTVariableDeclaration));
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