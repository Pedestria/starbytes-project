#include "starbytes/Base/Base.h"

#ifndef BYTECODE_BCDEF_H
#define BYTECODE_BCDEF_H

STARBYTES_BYTECODE_NAMESPACE

#define END 0
/// Create Variable (Name:BCId)
#define CRTVR 1
/// Set Variable (Var:BCId,Val:StarbytesObject)
#define STVR 2
/// Call Function (Name:BCId,Args:List<StarbytesObject> )
#define CLFNC 3
/// Refer Variable Address (Name:BCId)
#define RFVR_A 4
/// Refer Variable Direct (Name:BCId)
#define RFVR_D 5
/// Create Starbytes String (Val:BCId)
#define CRT_STB_STR 10
/// Create Starbytes Boolean (Val:bool)
#define CRT_STB_BOOL 11
/// Create Starbytes Number (Type:int,Val:Number)
#define CRT_STB_NUM  12
// Starbytes Number Types
#define STB_NUM_INT 1
#define STB_NUM_FLOAT 2
#define STB_NUM_LONG 3
#define STB_NUM_DOUBLE 4

/// Store a value.
#define TMP_STORE 20
/// List Begin
#define LST_BEG 21
/// List End
#define LST_END 22
/// Bytecode Identifier!
using BCId = std::string;
/// Defines the End of the Program!
#define PROG_END 100



NAMESPACE_END

#endif