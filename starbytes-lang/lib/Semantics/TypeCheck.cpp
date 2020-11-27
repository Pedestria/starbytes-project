#include "TypeCheck.h"
#include "starbytes/Semantics/SemanticsMain.h"

STARBYTES_SEMANTICS_NAMESPACE

using namespace AST;

#define SET_INIT_TYPE(value,type) STBTypeNumType type::static_type = STBTypeNumType::value

SET_INIT_TYPE(Class,STBClassType);
SET_INIT_TYPE(Interface,STBInterfaceType);

#define ASSERT_STBTYPE(name,type) (type*)name

template<typename _Node>
inline void construct_methods_and_props(std::vector<STBObjectMethod> *methods,std::vector<STBObjectProperty> *props,_Node *node,SemanticA *& sem){
    for(auto & state : node->body->nodes){
        if(AST_NODE_IS(state,ASTClassPropertyDeclaration)){
            ASTClassPropertyDeclaration *_prop = ASSERT_AST_NODE(state,ASTClassPropertyDeclaration);
            STBObjectProperty prop;
            prop.immutable = _prop->isConstant;
            prop.loose = _prop->loose;
            if(AST_NODE_IS(_prop->id,ASTTypeCastIdentifier)){
                ASTTypeCastIdentifier *tcid = ASSERT_AST_NODE(_prop->id,ASTTypeCastIdentifier);
                prop.name = tcid->id->value;
                if(sem->store.symbolExistsInCurrentScopes<ClassSymbol,ASTClassDeclaration>(tcid->tid->type_name)){
                    prop.type = sem->store.getSymbolRefFromCurrentScopes<ClassSymbol>(tcid->tid->type_name)->semantic_type;
                }
                //TODO: Check for Interface Symbols!
                else{
                    // throw StarbytesSemanticsError("Unknown Type: "+tcid->tid->type_name,tcid->BeginFold);
                }
            }
            else if(AST_NODE_IS(_prop->id,ASTIdentifier)){
                ASTIdentifier *id = ASSERT_AST_NODE(_prop,ASTIdentifier);
                prop.name = id->value;
                //TODO: Type Infer from Expression!
            }
            props->push_back(prop);
        }
        else if(AST_NODE_IS(state,ASTClassMethodDeclaration)){
            ASTClassMethodDeclaration *_method = ASSERT_AST_NODE(state,ASTClassMethodDeclaration);
            STBObjectMethod method;
            method.lazy = _method->isLazy;
            method.name = _method->id->value;
            if(sem->store.symbolExistsInCurrentScopes<ClassSymbol,ASTClassDeclaration>(_method->returnType->type_name)){
                method.return_type = sem->store.getSymbolRefFromCurrentScopes<ClassSymbol>(_method->returnType->type_name)->semantic_type;
            }
            //TODO: Check for Interface Symbols!
            else{
                // throw StarbytesSemanticsError("Unknown Type: "+_method->returnType->type_name,_method->returnType->BeginFold);
            }
            methods->push_back(method);
        }
        else if(AST_NODE_IS(state,ASTInterfacePropertyDeclaration)){
            ASTInterfacePropertyDeclaration *_prop = ASSERT_AST_NODE(state,ASTInterfacePropertyDeclaration);
            STBObjectProperty prop;
            prop.immutable = _prop->isConstant;
            prop.loose = _prop->loose;
            prop.name = _prop->tcid->id->value;
            ASTTypeCastIdentifier *tcid = _prop->tcid;
            if(sem->store.symbolExistsInCurrentScopes<ClassSymbol,ASTClassDeclaration>(tcid->tid->type_name)){
                prop.type = sem->store.getSymbolRefFromCurrentScopes<ClassSymbol>(tcid->tid->type_name)->semantic_type;
            }
            //TODO: Check for Interface Symbols!
            else{
                // throw StarbytesSemanticsError("Unknown Type: "+tcid->tid->type_name,tcid->BeginFold);
            }
            props->push_back(prop);
        }
        else if(AST_NODE_IS(state,ASTInterfaceMethodDeclaration)){
            ASTInterfaceMethodDeclaration *_method = ASSERT_AST_NODE(state,ASTInterfaceMethodDeclaration);
            STBObjectMethod method;
            method.lazy = _method->isLazy;
            method.name = _method->id->value;
            if(sem->store.symbolExistsInCurrentScopes<ClassSymbol,ASTClassDeclaration>(_method->returnType->type_name)){
                method.return_type = sem->store.getSymbolRefFromCurrentScopes<ClassSymbol>(_method->returnType->type_name)->semantic_type;
            }
            //TODO: Check for Interface Symbols!
            else{
                // throw StarbytesSemanticsError("Unknown Type: "+_method->returnType->type_name,_method->returnType->BeginFold);
            }
            methods->push_back(method);
        }
    }
};

STBClassType * create_stb_standard_class_type(std::string name){
    STBClassType *type = new STBClassType();
    type->name = name;
    return type;
};


STBClassType * create_stb_class_type(ASTClassDeclaration *node,SemanticA *sem){
    STBClassType *type = new STBClassType();
    type->name = node->id->value;
    construct_methods_and_props(&type->methods,&type->props,node,sem);
    return type;
};

STBInterfaceType * create_stb_interface_type(ASTInterfaceDeclaration *node,SemanticA *sem){
    STBInterfaceType *type = new STBInterfaceType();
    type->name = node->id->value;
    construct_methods_and_props(&type->methods,&type->props,node,sem);
    return type;
};


bool isStandardClassType(STBClassType *subject){
    bool returncode;
    if(subject->name == "String" || subject->name == "Number" || subject->name == "Boolean" || subject->name == "Dictionary"){
        returncode = true;
    }
    else{
        returncode = false;
    }

    return returncode;
};

bool isStandardInterfaceType(STBInterfaceType *subject){
    bool returncode;
    if(subject->name == "Printable"){
        returncode = true;
    }
    else{
        returncode = false;
    }

    return returncode;
};

bool STBInterfaceType::propExists(STBObjectProperty *_other){
    bool returncode = false;
    for(auto & own_prop : props){
        if(own_prop.immutable == _other->immutable && own_prop.loose == _other->loose && own_prop.name == _other->name){
            if(stbtype_is_class(own_prop.type) && stbtype_is_class(_other->type)){
                STBClassType *ptr = ASSERT_STBTYPE(own_prop.type,STBClassType);
                if(isStandardClassType(ptr)){
                    returncode = true;
                }
                if(ptr->match(ASSERT_STBTYPE(_other->type,STBClassType))){
                    returncode = true;
                }
            }
            else if(stbtype_is_interface(own_prop.type) && stbtype_is_interface(_other->type)){
                STBInterfaceType *ptr = ASSERT_STBTYPE(own_prop.type,STBInterfaceType);
                if(isStandardInterfaceType(ptr)){
                    returncode = true;
                }
                if(ptr->match(ASSERT_STBTYPE(_other->type,STBInterfaceType))){
                    returncode = true;
                }
            }
        }
    }
    return returncode;
};

bool STBInterfaceType::methodExists(STBObjectMethod *_other){
    bool returncode = false;
    for(auto & own_method : methods){
        if(own_method.lazy == _other->lazy && own_method.name == _other->name){
            if(stbtype_is_class(own_method.return_type) && stbtype_is_class(_other->return_type)){
                STBClassType *ptr = ASSERT_STBTYPE(own_method.return_type,STBClassType);
                if(isStandardClassType(ptr)){
                    returncode = true;
                }
                if(ptr->match(ASSERT_STBTYPE(_other->return_type,STBClassType))){
                    returncode = true;
                }
            }
            else if(stbtype_is_interface(own_method.return_type) && stbtype_is_interface(_other->return_type)){
                STBInterfaceType *ptr = ASSERT_STBTYPE(own_method.return_type,STBInterfaceType);
                if(isStandardInterfaceType(ptr)){
                    returncode = true;
                }
                if(ptr->match(ASSERT_STBTYPE(_other->return_type,STBInterfaceType))){
                    returncode = true;
                }
            }
        }
    }
    return returncode;
};

bool STBInterfaceType::match(STBInterfaceType *_other){
    bool returncode;
    if(this->name == _other->name){
        //Check props and methods to see if they are same!
        for(auto & prop : props){
            if(stbtype_is_interface(prop.type)){
                STBInterfaceType *rt = ASSERT_STBTYPE(prop.type,STBInterfaceType);
                if(isStandardInterfaceType(rt)){
                    returncode = true;
                }
                else if(_other->propExists(&prop)){
                    returncode = true;
                }
                else {
                    returncode = false;
                }
            }
            else if(stbtype_is_class(prop.type)){
                STBClassType *rt = ASSERT_STBTYPE(prop.type,STBClassType);
                if(isStandardClassType(rt)){
                    returncode = true;
                }
                else if(_other->propExists(&prop)){
                    returncode = true;
                }
                else{
                    returncode = false;
                }
            }
        }

        for(auto & method : methods){
            if(stbtype_is_interface(method.return_type)){
                STBInterfaceType *rt = ASSERT_STBTYPE(method.return_type,STBInterfaceType);
                if(isStandardInterfaceType(rt)){
                    returncode = true;
                }
                else if(_other->methodExists(&method)){
                    returncode = true;
                }
                else {
                    returncode = false;
                }
            }
            else if(stbtype_is_class(method.return_type)){
                STBClassType *rt = ASSERT_STBTYPE(method.return_type,STBClassType);
                if(isStandardClassType(rt)){
                    returncode = true;
                }
                else if(_other->methodExists(&method)){
                    returncode = true;
                }
                else{
                    returncode = false;
                }
            }
        }
    }
    else{
        returncode = false;
    }
    return returncode;
};

bool STBInterfaceType::extendsInterface(STBInterfaceType *_super){

};

bool STBClassType::propExists(STBObjectProperty *_other){
    bool returncode = false;
    for(auto & own_prop : props){
        if(own_prop.immutable == _other->immutable && own_prop.loose == _other->loose && own_prop.name == _other->name){
            if(stbtype_is_class(own_prop.type) && stbtype_is_class(_other->type)){
                STBClassType *ptr = ASSERT_STBTYPE(own_prop.type,STBClassType);
                if(isStandardClassType(ptr)){
                    returncode = true;
                }
                if(ptr->match(ASSERT_STBTYPE(_other->type,STBClassType))){
                    returncode = true;
                }
            }
            else if(stbtype_is_interface(own_prop.type) && stbtype_is_interface(_other->type)){
                STBInterfaceType *ptr = ASSERT_STBTYPE(own_prop.type,STBInterfaceType);
                if(isStandardInterfaceType(ptr)){
                    returncode = true;
                }
                if(ptr->match(ASSERT_STBTYPE(_other->type,STBInterfaceType))){
                    returncode = true;
                }
            }
        }
    }
    return returncode;
};

bool STBClassType::methodExists(STBObjectMethod *_other){
    bool returncode = false;
    for(auto & own_method : methods){
        if(own_method.lazy == _other->lazy && own_method.name == _other->name){
            if(stbtype_is_class(own_method.return_type) && stbtype_is_class(_other->return_type)){
                STBClassType *ptr = ASSERT_STBTYPE(own_method.return_type,STBClassType);
                if(isStandardClassType(ptr)){
                    returncode = true;
                }
                if(ptr->match(ASSERT_STBTYPE(_other->return_type,STBClassType))){
                    returncode = true;
                }
            }
            else if(stbtype_is_interface(own_method.return_type) && stbtype_is_interface(_other->return_type)){
                STBInterfaceType *ptr = ASSERT_STBTYPE(own_method.return_type,STBInterfaceType);
                if(isStandardInterfaceType(ptr)){
                    returncode = true;
                }
                if(ptr->match(ASSERT_STBTYPE(_other->return_type,STBInterfaceType))){
                    returncode = true;
                }
            }
        }
    }
    return returncode;
};

bool STBClassType::match(STBClassType *_other){
    bool returncode;
    if(this->name == _other->name){
        //Check props and methods to see if they are same!
        for(auto & prop : props){
            if(stbtype_is_interface(prop.type)){
                STBInterfaceType *rt = ASSERT_STBTYPE(prop.type,STBInterfaceType);
                if(isStandardInterfaceType(rt)){
                    returncode = true;
                }
                else if(_other->propExists(&prop)){
                    returncode = true;
                }
                else {
                    returncode = false;
                }
            }
            else if(stbtype_is_class(prop.type)){
                STBClassType *rt = ASSERT_STBTYPE(prop.type,STBClassType);
                if(isStandardClassType(rt)){
                    returncode = true;
                }
                else if(_other->propExists(&prop)){
                    returncode = true;
                }
                else{
                    returncode = false;
                }
            }
        }

        for(auto & method : methods){
            if(stbtype_is_interface(method.return_type)){
                STBInterfaceType *rt = ASSERT_STBTYPE(method.return_type,STBInterfaceType);
                if(isStandardInterfaceType(rt)){
                    returncode = true;
                }
                else if(_other->methodExists(&method)){
                    returncode = true;
                }
                else {
                    returncode = false;
                }
            }
            else if(stbtype_is_class(method.return_type)){
                STBClassType *rt = ASSERT_STBTYPE(method.return_type,STBClassType);
                if(isStandardClassType(rt)){
                    returncode = true;
                }
                else if(_other->methodExists(&method)){
                    returncode = true;
                }
                else{
                    returncode = false;
                }
            }
        }
    }
    return returncode;
};

bool STBClassType::extendsClass(STBClassType *_super){

};

bool STBClassType::implementsInterface(STBInterfaceType *_interface){

};

bool stbtype_is_class(STBType *type){
    return type->type == STBTypeNumType::Class;
};

bool stbtype_is_interface(STBType *type){
    return type->type == STBTypeNumType::Interface;
};



NAMESPACE_END