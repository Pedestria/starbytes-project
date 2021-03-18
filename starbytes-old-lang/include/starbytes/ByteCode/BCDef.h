#include "starbytes/Base/Base.h"
#include <string>
#include <cstdlib>
#include <cstring>

#ifndef BYTECODE_BCDEF_H
#define BYTECODE_BCDEF_H



STARBYTES_BYTECODE_NAMESPACE

#define END 0x00
/// Create Variable (Name:BCId)
#define CRTVR 0x01
/// Set Variable (Var:BCId,Val:StarbytesObject)
#define STVR 0x02
/// Call Function (Name:BCId,Args:List<StarbytesObject> )
#define CLFNC 0x03
/// Refer Variable Address (Name:BCId)
#define RFVR_A 0x04
/// Refer Variable Direct (Name:BCId)
#define RFVR_D 0x05
/// Create Function (Name:BCId,)
#define CRTFNC 0x06
/// Create Starbytes String (Val:BCId)
#define CRT_STB_STR 0x0a
/// Create Starbytes Boolean (Val:bool)
#define CRT_STB_BOOL 0x0b
/// Create Starbytes Number (Type:int,Val:Number)
#define CRT_STB_NUM  0x0c
// Starbytes Number Types
#define STB_NUM_INT 0x0d
#define STB_NUM_FLOAT 0x0e
#define STB_NUM_LONG 0x0f
#define STB_NUM_DOUBLE 0x10

/// Store a value.
#define TMP_STORE 0x11
/// List Begin
#define LST_BEG 0x12
/// List End
#define LST_END 0x13
// ByteCode (8 Bit Integer)
using BC = unsigned char;
/// Bytecode Identifier!
using BCId = std::string;

void bcid_to_cpp_str(BCId & id_ref,std::string & str_ref);
/// Defines the End of the Program!
#define PROG_END 0xff



NAMESPACE_END

#endif
