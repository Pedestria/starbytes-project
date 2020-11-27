#include "ASTTraveler.h"

#ifndef AST_TREEPRINTER_H
#define AST_TREEPRINTER_H

STARBYTES_STD_NAMESPACE

namespace AST {

    class TreePrinter {
        private:
        AbstractSyntaxTree * tree;
        unsigned indent_count = 0;
        std::string tabs = "";
        inline void moveDown(){
            tabs.append("\t");
        };
        inline void moveUp(){
            tabs.pop_back();
        };
        template<typename MsgT>
        inline void ln_msg_to_stdout(MsgT message){
            std::cout << tabs << message << std::endl;
        };
        inline void printDocPos(DocumentPosition & pos,const char * start_text){
            std::cout << start_text <<": {StartChar:" << std::to_string(pos.start) << " ,EndChar:" << std::to_string(pos.end) << " ,LineNum:" << std::to_string(pos.line) << "}" << std::endl;
        };
        void printId(ASTIdentifier * node){
            ln_msg_to_stdout("Identifier {");
            moveDown();
            printDocPos(node->position,"Position");
            std::cout << tabs << "Value:" << node->value << std::endl;
            moveUp();
            ln_msg_to_stdout("}");
        };
        void printTypeId(ASTTypeCastIdentifier * node){
            ln_msg_to_stdout("TypeIdentifier {");
            std::cout << tabs << "Id:";
            moveDown();
            printDocPos(node->BeginFold,"BeginFold");
            printId(node->id);
            printDocPos(node->EndFold,"EndFold");
            moveUp();
            ln_msg_to_stdout("}");
        };
        void printExpr(ASTExpression * node){
            if(AST_NODE_IS(node,ASTIdentifier)){
                printId(ASSERT_AST_NODE(node,ASTIdentifier));
            }
            else if(AST_NODE_IS(node,ASTNumericLiteral)){
                ln_msg_to_stdout("NumLiteral {");
                moveDown();
                printDocPos(node->BeginFold,"BeginFold");
                std::cout << "Value:" << ASSERT_AST_NODE(node,ASTNumericLiteral)->value << std::endl;
                printDocPos(node->EndFold,"EndFold");
                moveUp();
                ln_msg_to_stdout("}");
            }
            else if(AST_NODE_IS(node,ASTBooleanLiteral)){
                ln_msg_to_stdout("BoolLiteral {");
                moveDown();
                printDocPos(node->BeginFold,"BeginFold");
                std::cout << "Value:" << ASSERT_AST_NODE(node,ASTBooleanLiteral)->value << std::endl;
                printDocPos(node->EndFold,"EndFold");
                moveUp();
                ln_msg_to_stdout("}");
            }
            else if(AST_NODE_IS(node,ASTStringLiteral)){
                ln_msg_to_stdout("StrLiteral {");
                moveDown();
                printDocPos(node->BeginFold,"BeginFold");
                std::cout << "Value:" <<  ASSERT_AST_NODE(node,ASTStringLiteral)->value << std::endl;
                printDocPos(node->EndFold,"EndFold");
                moveUp();
                ln_msg_to_stdout("}");
            }
        };
        void printVarDecl(ASTVariableDeclaration * node){
            ln_msg_to_stdout("VarDecl {");
            moveDown();
            printDocPos(node->BeginFold,"BeginFold");
            ln_msg_to_stdout("VarSpecs [");
            moveDown();
            for(auto & sp : node->specifiers){
                ln_msg_to_stdout("VarSpec {");
                moveDown();
                printDocPos(node->BeginFold,"BeginFold");
                std::cout << tabs << "Id:";
                if(AST_NODE_IS(sp->id,ASTIdentifier)){
                    printId(ASSERT_AST_NODE(sp->id,ASTIdentifier));
                }
                else if(AST_NODE_IS(sp->id, ASTTypeCastIdentifier)){
                    printTypeId(ASSERT_AST_NODE(sp->id,ASTTypeCastIdentifier));
                }

                if(sp->initializer.has_value()){
                    std::cout << tabs << "Initializer:";
                    printExpr(sp->initializer.value());
                }
                printDocPos(node->EndFold,"EndFold");
                moveUp();
                ln_msg_to_stdout("}");
            }
            moveUp();
            ln_msg_to_stdout("]");
            printDocPos(node->EndFold,"EndFold");
            moveUp();
            ln_msg_to_stdout("}");
        };
        void printConstDecl(ASTConstantDeclaration * node){
            ln_msg_to_stdout("ConstDecl {");
            moveDown();
            printDocPos(node->BeginFold,"BeginFold");
            ln_msg_to_stdout("ConstSpecs [");
            moveDown();
            for(auto & sp : node->specifiers){
                ln_msg_to_stdout("ConstSpec {");
                moveDown();
                printDocPos(node->BeginFold,"BeginFold");
                std::cout << tabs << "Id:";
                if(AST_NODE_IS(sp->id,ASTIdentifier)){
                    printId(ASSERT_AST_NODE(sp->id,ASTIdentifier));
                }
                else if(AST_NODE_IS(sp->id, ASTTypeCastIdentifier)){
                    printTypeId(ASSERT_AST_NODE(sp->id,ASTTypeCastIdentifier));
                }

                if(sp->initializer.has_value()) {
                    std::cout << tabs << "Initializer:";
                    printExpr(sp->initializer.value());
                }
                printDocPos(node->EndFold,"EndFold");
                moveUp();
                ln_msg_to_stdout("}");
            }
            moveUp();
            ln_msg_to_stdout("]");
            printDocPos(node->EndFold,"EndFold");
            moveUp();
            ln_msg_to_stdout("}");
        };
        public:
        TreePrinter() = default;
        ~TreePrinter();
        void operator=(TreePrinter &) = delete;
        void print(AbstractSyntaxTree *& _tree){
            tree = _tree;
        };
    };

  

};

NAMESPACE_END

#endif