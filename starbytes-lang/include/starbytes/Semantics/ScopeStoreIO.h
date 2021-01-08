#include "Scope.h"

#ifndef SEMANTICS_SCOPESTOREIO_H
#define SEMANTICS_SCOPESTOREIO_H

STARBYTES_SEMANTICS_NAMESPACE

void getScopeStore(Foundation::StrRef file,ScopeStore & store_ref);
void dumpScopeStore(Foundation::StrRef file,ScopeStore & store_ref);

NAMESPACE_END

#endif