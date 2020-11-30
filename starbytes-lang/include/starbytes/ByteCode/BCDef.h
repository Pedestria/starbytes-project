#include "starbytes/Base/Base.h"

#ifndef BYTECODE_BCDEF_H
#define BYTECODE_BCDEF_H

STARBYTES_BYTECODE_NAMESPACE

#define END 0
#define CRTVR 1
#define STVR 2
#define CLFNC 3
#define RFVR_A 4
#define RFVR_D 5

#define CRT_STB_STR 10
#define CRT_STB_BOOL 11
#define CRT_STB_NUM  12

#define TMP_STORE 20
#define LST_BEG 21
#define LST_END 22

using BCId = std::string;

#define PROG_END 100

NAMESPACE_END

#endif