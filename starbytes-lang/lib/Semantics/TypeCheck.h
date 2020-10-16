#include "starbytes/AST/AST.h"
#include "starbytes/Base/Base.h"



#ifndef SEMANTICS_TYPECHECK_H
#define SEMANTICS_TYPECHECK_H


STARBYTES_SEMANTICS_NAMESPACE

/*Type Classes!*/
TYPED_ENUM STBTypeNumType:int{
    Class,Interface
};

class STBType {
    STBTypeNumType type;
    public:
    STBType(STBTypeNumType _type):type(_type){};
    ~STBType(){};
};

#define STARBYTES_TYPE(name,enum_name) class name : public STBType { public: name():STBType(STBTypeNumType::enum_name){}; ~name(){}; static STBTypeNumType static_type;

STARBYTES_TYPE(STBClassType,Class) 

};

STARBYTES_TYPE(STBInterfaceType,Interface)

};

#undef STARBYTES_TYPE

NAMESPACE_END

#endif