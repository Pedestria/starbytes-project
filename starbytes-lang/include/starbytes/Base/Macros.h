
#ifndef BASE_MACROS_H
#define BASE_MACROS_H

#define STARBYTES_STD_NAMESPACE namespace Starbytes {
#define STARBYTES_FOUNDATION_NAMESPACE namespace Starbytes::Foundation {
#define STARBYTES_INTERPRETER_NAMESPACE namespace Starbytes::Interpreter {
#define STARBYTES_SEMANTICS_NAMESPACE namespace Starbytes::Semantics {
#define STARBYTES_BYTECODE_NAMESPACE namespace Starbytes::ByteCode {
#define STARBYTES_GEN_NAMESPACE namespace Starbytes::CodeGen {
#define NAMESPACE_END }
#define TYPED_ENUM enum class

#define __NO_DISCARD [[nodiscard]]
#define __UNLIKELY(expr) [[unlikely]] expr


#endif
