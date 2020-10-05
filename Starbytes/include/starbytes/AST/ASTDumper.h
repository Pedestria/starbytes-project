#include "AST.h"
#include "starbytes/Base/Base.h"
#include <sstream>
#include <vector>

#ifndef AST_ASTDUMPER_H
#define AST_ASTDUMPER_H

STARBYTES_STD_NAMESPACE

namespace AST {

    // TYPED_ENUM XmlType {
        
    // };

    class ASTDumper {
        private:
            AbstractSyntaxTree *& ast;
            std::ostringstream output;
            bool toXMLDeclaration(ASTDeclaration *& decl);
            bool toXMLExpression(ASTExpression *& expr);
            void toXMLVariableDeclaration(){};
            template<typename _Ty = ASTStatement *>
            void toXMLVector(std::vector<_Ty> & vec){
                output << "<vector>";
                for(auto & n : vec){
                    toXMLStatement(n);
                }
            };
        public:
            ASTDumper(AbstractSyntaxTree *& _ast):ast(_ast){};
            std::string * generateToXML();
            ~ASTDumper(){};
    };
}

NAMESPACE_END

#endif