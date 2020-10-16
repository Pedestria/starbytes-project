#include "starbytes/AST/AST.h"
#include "starbytes/Base/Base.h"
#include <vector>



#ifndef SEMANTICS_TYPECHECK_H
#define SEMANTICS_TYPECHECK_H


STARBYTES_SEMANTICS_NAMESPACE

/*Type Classes!*/
TYPED_ENUM STBTypeNumType:int{
    Class,Interface
};

class STBType {
    protected:
    STBTypeNumType type;
    friend bool stbtype_is_class(STBType *type);
    friend bool stbtype_is_interface(STBType *type);
    public:
    std::string name;
    STBType(STBTypeNumType _type):type(_type){};
    ~STBType(){};
};

#define STARBYTES_TYPE(name,enum_name) class name : public STBType { public: name():STBType(STBTypeNumType::enum_name){}; ~name(){}; static STBTypeNumType static_type;


struct STBObjectProperty {
    bool loose;
    bool immutable;
    std::string name;
    STBType *type;
};
struct STBObjectMethod {
    bool lazy;
    std::string name;
    STBType *return_type;
};
STARBYTES_TYPE(STBInterfaceType,Interface)
    std::vector<STBObjectProperty> props;
    std::vector<STBObjectMethod> methods;
    bool match(STBInterfaceType *_other);
    bool extendsInterface(STBInterfaceType *_super);
    bool propExists(STBObjectProperty *_other);
    bool methodExists(STBObjectMethod *_other);
};

STARBYTES_TYPE(STBClassType,Class)
    std::vector<STBObjectProperty> props;
    std::vector<STBObjectMethod> methods; 
    bool match(STBClassType *_other);
    bool extendsClass(STBClassType *_super);
    bool implementsInterface(STBInterfaceType *_interface);
    bool propExists(STBObjectProperty *_other);
    bool methodExists(STBObjectMethod *_other);
};


bool stbtype_is_class(STBType *type);
bool stbtype_is_interface(STBType *type);


STBClassType * create_stb_class_type(AST::ASTClassDeclaration *node);
STBInterfaceType * create_stb_interface_type(AST::ASTInterfaceDeclaration *node);

#undef STARBYTES_TYPE

NAMESPACE_END

#endif