#include "TypeCheck.h"

STARBYTES_SEMANTICS_NAMESPACE

using namespace AST;

#define SET_INIT_TYPE(value,type) STBTypeNumType type::static_type = STBTypeNumType::value

SET_INIT_TYPE(Class,STBClassType);
SET_INIT_TYPE(Interface,STBInterfaceType);

NAMESPACE_END