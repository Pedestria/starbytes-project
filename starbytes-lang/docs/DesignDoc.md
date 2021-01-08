# Starbytes Infratstructure Design Summary.

## Driver 

> Drives the compiler.. (Multithreaded eventually.)

## Lexer

> Breaks down code into tokens

## Parser

> Converts token stream to an ast

## SemanticA

> Semantically analyzes the current module, and outputs diagnostics.

#### ScopeStoreIO
> Inputs/Outputs scope store from/to scope store files


## CodeGenR

> Generates bytecode from ast
