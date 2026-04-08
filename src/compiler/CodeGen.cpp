#include "starbytes/compiler/CodeGen.h"
#include "starbytes/compiler/ASTNodes.def"
#include "starbytes/compiler/Gen.h"
#include "starbytes/compiler/RTCode.h"
#include <algorithm>
#include <cstring>
#include <sstream>

namespace starbytes {

using namespace Runtime;

static RTID makeOwnedRTID(string_ref value);
static void emitRuntimeFunction(ASTBlockStmt *blockStmt,
                                const std::vector<std::pair<ASTIdentifier *,ASTType *>> &orderedParams,
                                ModuleGenContext *ctxt,
                                CodeGen *astConsumer,
                                RTFuncTemplate &templ,
                                bool allowV2);
static bool getMemberName(ASTExpr *memberExpr,std::string &memberName);
static std::string directCalleeRuntimeName(ModuleGenContext *context,ASTExpr *callee);
static RTTypedNumericKind promotedFastKind(RTTypedNumericKind lhs,RTTypedNumericKind rhs);
static bool isFastNumericKind(RTTypedNumericKind kind);
static bool typedBinaryOpFromSymbol(const std::string &op,RTTypedBinaryOp &out);
static bool typedCompareOpFromSymbol(const std::string &op,RTTypedCompareOp &out);

bool CodeGen::acceptsSymbolTableContext(){
    return genContext != nullptr;
}

void CodeGen::consumeSTableContext(Semantics::STableContext *table){
    genContext->tableContext = table;
}

void CodeGen::setContext(ModuleGenContext *context){
    genContext = context;
}

void CodeGen::finish() const {
    auto *self = const_cast<CodeGen *>(this);
    if(genContext && genContext->bytecodeVersion == RTBYTECODE_VERSION_V2 && !self->bufferedTopLevelStatements.empty()){
        ASTBlockStmt syntheticModuleBlock;
        syntheticModuleBlock.body = self->bufferedTopLevelStatements;

        RTFuncTemplate moduleMainTemplate;
        moduleMainTemplate.name = makeOwnedRTID("__module_main__");
        emitRuntimeFunction(&syntheticModuleBlock,{},genContext,self,moduleMainTemplate,true);

        RTCode code = CODE_RTCALL_DIRECT;
        genContext->out.write((char *)&code,sizeof(RTCode));
        RTID funcId = makeOwnedRTID("__module_main__");
        genContext->out << &funcId;
        unsigned argCount = 0;
        genContext->out.write((char *)&argCount,sizeof(argCount));
    }
    RTCode code = CODE_MODULE_END;
    genContext->out.write((char *)&code,sizeof(RTCode));
}

inline void write_ASTBlockStmt_to_context(ASTBlockStmt *blockStmt,ModuleGenContext *ctxt,CodeGen *astConsumer,bool isFunc = false){
    (void)isFunc;
    RTCode code = CODE_RTBLOCK_BEGIN;
    ctxt->out.write((char *)&code,sizeof(RTCode));
    for(auto & stmt : blockStmt->body){
        if(stmt->type & DECL){
            astConsumer->consumeDecl((ASTDecl *)stmt);
        }
        else {
            astConsumer->consumeStmt(stmt);
        };
    };
    code = CODE_RTBLOCK_END;
    ctxt->out.write((char *)&code,sizeof(RTCode));
}

static std::string emitBlockBodyToBuffer(ASTBlockStmt *blockStmt,ModuleGenContext *ctxt,CodeGen *astConsumer){
    if(!blockStmt){
        return {};
    }
    std::ostringstream bodyBuffer(std::ios::out | std::ios::binary);
    auto tempOutputPath = ctxt->outputPath;
    ModuleGenContext tempContext(ctxt->name,bodyBuffer,tempOutputPath);
    tempContext.tableContext = ctxt->tableContext;
    astConsumer->setContext(&tempContext);
    for(auto *stmt : blockStmt->body){
        if(stmt->type & DECL){
            astConsumer->consumeDecl((ASTDecl *)stmt);
        }
        else {
            astConsumer->consumeStmt(stmt);
        }
    }
    astConsumer->setContext(ctxt);
    return bodyBuffer.str();
}

class CodeGen::BytecodeV2Lowerer {
    struct LoweredValue {
        bool valid = false;
        uint32_t slot = 0;
        RTTypedNumericKind kind = RTTYPED_NUM_OBJECT;
    };

    CodeGen *owner = nullptr;
    ModuleGenContext *context = nullptr;
    ASTBlockStmt *functionBlock = nullptr;
    std::vector<std::pair<ASTIdentifier *,ASTType *>> orderedParams;
    RTFuncTemplate &templ;
    RTV2FunctionImage image;
    size_t tempCounter = 0;

    struct Snapshot {
        size_t instructionCount = 0;
        size_t i64ConstCount = 0;
        size_t f64ConstCount = 0;
        size_t callSiteCount = 0;
        size_t fallbackCount = 0;
        size_t localSlotNameCount = 0;
        size_t slotKindCount = 0;
        size_t tempCounterValue = 0;
    };

    Snapshot takeSnapshot() const{
        return {
            image.instructions.size(),
            image.i64Consts.size(),
            image.f64Consts.size(),
            image.callSites.size(),
            image.fallbackStmtBlobs.size(),
            templ.localSlotNames.size(),
            templ.slotKinds.size(),
            tempCounter
        };
    }

    void restoreSnapshot(const Snapshot &snapshot){
        image.instructions.resize(snapshot.instructionCount);
        image.i64Consts.resize(snapshot.i64ConstCount);
        image.f64Consts.resize(snapshot.f64ConstCount);
        image.callSites.resize(snapshot.callSiteCount);
        image.fallbackStmtBlobs.resize(snapshot.fallbackCount);
        templ.localSlotNames.resize(snapshot.localSlotNameCount);
        templ.slotKinds.resize(snapshot.slotKindCount);
        tempCounter = snapshot.tempCounterValue;
    }

    RTTypedNumericKind kindFromTypeName(const std::string &name) const{
        if(name == INT_TYPE->getName().str()){
            return RTTYPED_NUM_INT;
        }
        if(name == LONG_TYPE->getName().str()){
            return RTTYPED_NUM_LONG;
        }
        if(name == FLOAT_TYPE->getName().str()){
            return RTTYPED_NUM_FLOAT;
        }
        if(name == DOUBLE_TYPE->getName().str()){
            return RTTYPED_NUM_DOUBLE;
        }
        return RTTYPED_NUM_OBJECT;
    }

    uint32_t allocateTemp(RTTypedNumericKind kind){
        auto name = "__v2tmp" + std::to_string(tempCounter++);
        templ.localSlotNames.push_back(makeOwnedRTID(name));
        templ.slotKinds.push_back(kind);
        return (uint32_t)(templ.slotKinds.size() - 1);
    }

    uint32_t addI64Const(int64_t value){
        auto index = (uint32_t)image.i64Consts.size();
        image.i64Consts.push_back(value);
        return index;
    }

    uint32_t addF64Const(double value){
        auto index = (uint32_t)image.f64Consts.size();
        image.f64Consts.push_back(value);
        return index;
    }

    uint32_t addCallSite(const std::string &targetName,const std::vector<uint32_t> &argSlots){
        auto index = (uint32_t)image.callSites.size();
        image.callSites.push_back({targetName,argSlots});
        return index;
    }

    std::vector<char> emitLegacyStmtBytes(ASTStmt *stmt){
        std::ostringstream bodyBuffer(std::ios::out | std::ios::binary);
        auto tempOutputPath = context->outputPath;
        ModuleGenContext tempContext(context->name,bodyBuffer,tempOutputPath);
        tempContext.tableContext = context->tableContext;
        tempContext.bytecodeVersion = RTBYTECODE_VERSION_V1;

        CodeGen legacyGen;
        legacyGen.setContext(&tempContext);
        RTFuncTemplate tempTemplate;
        legacyGen.pushLocalSlotContext(orderedParams,functionBlock,tempTemplate);
        if(stmt->type & DECL){
            legacyGen.consumeDecl((ASTDecl *)stmt);
        }
        else {
            legacyGen.consumeStmt(stmt);
        }
        legacyGen.popLocalSlotContext();

        auto bytes = bodyBuffer.str();
        return std::vector<char>(bytes.begin(),bytes.end());
    }

    bool emitFallbackStmt(ASTStmt *stmt){
        auto blob = emitLegacyStmtBytes(stmt);
        if(blob.empty()){
            return true;
        }
        auto index = (uint32_t)image.fallbackStmtBlobs.size();
        image.fallbackStmtBlobs.push_back(std::move(blob));
        RTV2Instruction instr;
        instr.opcode = RTV2_OP_V1_STMT;
        instr.a = index;
        image.instructions.push_back(instr);
        return true;
    }

    void emitMove(uint32_t destSlot,uint32_t srcSlot){
        if(destSlot == srcSlot){
            return;
        }
        RTV2Instruction instr;
        instr.opcode = RTV2_OP_MOVE;
        instr.a = destSlot;
        instr.b = srcSlot;
        image.instructions.push_back(instr);
    }

    LoweredValue lowerNumericCast(const LoweredValue &input,RTTypedNumericKind targetKind){
        if(!input.valid || targetKind == RTTYPED_NUM_OBJECT){
            return {};
        }
        if(input.kind == targetKind){
            return input;
        }
        auto destSlot = allocateTemp(targetKind);
        RTV2Instruction instr;
        instr.opcode = RTV2_OP_NUM_CAST;
        instr.kind = targetKind;
        instr.aux = input.kind;
        instr.a = destSlot;
        instr.b = input.slot;
        image.instructions.push_back(instr);
        return {true,destSlot,targetKind};
    }

    LoweredValue lowerExpr(ASTExpr *expr){
        if(!expr){
            return {};
        }

        if(expr->runtimeCastTargetName.has_value()){
            bool explicitCastInvocation = false;
            if(expr->type == IVKE_EXPR && expr->callee){
                explicitCastInvocation =
                    ((expr->callee->type == ID_EXPR && expr->callee->id
                      && expr->callee->id->type == ASTIdentifier::Class)
                     || (expr->callee->type == MEMBER_EXPR
                         && expr->callee->rightExpr && expr->callee->rightExpr->id
                         && expr->callee->rightExpr->id->type == ASTIdentifier::Class));
            }
            auto targetKind = kindFromTypeName(expr->runtimeCastTargetName.value());
            if(targetKind != RTTYPED_NUM_OBJECT){
                if(explicitCastInvocation){
                    if(expr->exprArrayData.size() != 1){
                        return {};
                    }
                    return lowerNumericCast(lowerExpr(expr->exprArrayData.front()),targetKind);
                }
                auto savedTarget = expr->runtimeCastTargetName;
                expr->runtimeCastTargetName.reset();
                auto lowered = lowerExpr(expr);
                expr->runtimeCastTargetName = savedTarget;
                return lowerNumericCast(lowered,targetKind);
            }
        }

        if(expr->type == NUM_LITERAL){
            auto *literal = static_cast<ASTLiteralExpr *>(expr);
            auto kind = owner->inferFastType(expr).scalarKind;
            if(literal->floatValue.has_value()){
                auto slot = allocateTemp(kind == RTTYPED_NUM_OBJECT ? RTTYPED_NUM_DOUBLE : kind);
                RTV2Instruction instr;
                instr.opcode = RTV2_OP_LOAD_F64_CONST;
                instr.a = slot;
                instr.b = addF64Const(*literal->floatValue);
                image.instructions.push_back(instr);
                return {true,slot,templ.slotKinds[slot]};
            }
            if(literal->intValue.has_value()){
                auto slot = allocateTemp(kind == RTTYPED_NUM_OBJECT ? RTTYPED_NUM_INT : kind);
                RTV2Instruction instr;
                instr.opcode = RTV2_OP_LOAD_I64_CONST;
                instr.a = slot;
                instr.b = addI64Const(*literal->intValue);
                image.instructions.push_back(instr);
                return {true,slot,templ.slotKinds[slot]};
            }
            return {};
        }

        if(expr->type == BOOL_LITERAL){
            auto *literal = static_cast<ASTLiteralExpr *>(expr);
            auto slot = allocateTemp(RTTYPED_NUM_INT);
            RTV2Instruction instr;
            instr.opcode = RTV2_OP_LOAD_I64_CONST;
            instr.a = slot;
            instr.b = addI64Const(literal->boolValue.value_or(false) ? 1 : 0);
            image.instructions.push_back(instr);
            return {true,slot,RTTYPED_NUM_INT};
        }

        if(expr->type == ID_EXPR && expr->id && expr->id->type == ASTIdentifier::Var){
            uint32_t slot = 0;
            if(!owner->currentLocalSlotForName(expr->id->val,slot)){
                return {};
            }
            auto kind = slot < templ.slotKinds.size() ? templ.slotKinds[slot] : RTTYPED_NUM_OBJECT;
            return {true,slot,(RTTypedNumericKind)kind};
        }

        if(expr->type == BINARY_EXPR){
            auto lhs = lowerExpr(expr->leftExpr);
            auto rhs = lowerExpr(expr->rightExpr);
            if(!lhs.valid || !rhs.valid){
                return {};
            }
            auto op = expr->oprtr_str.value_or("");
            RTTypedBinaryOp typedBinaryOp = RTTYPED_BINARY_ADD;
            if(isFastNumericKind(lhs.kind) && isFastNumericKind(rhs.kind) && typedBinaryOpFromSymbol(op,typedBinaryOp)){
                auto resultKind = promotedFastKind(lhs.kind,rhs.kind);
                auto destSlot = allocateTemp(resultKind);
                RTV2Instruction instr;
                instr.opcode = RTV2_OP_BINARY;
                instr.kind = resultKind;
                instr.aux = typedBinaryOp;
                instr.a = destSlot;
                instr.b = lhs.slot;
                instr.c = rhs.slot;
                image.instructions.push_back(instr);
                return {true,destSlot,resultKind};
            }
            RTTypedCompareOp typedCompareOp = RTTYPED_COMPARE_EQ;
            if(isFastNumericKind(lhs.kind) && isFastNumericKind(rhs.kind) && typedCompareOpFromSymbol(op,typedCompareOp)){
                auto operandKind = promotedFastKind(lhs.kind,rhs.kind);
                auto destSlot = allocateTemp(RTTYPED_NUM_INT);
                RTV2Instruction instr;
                instr.opcode = RTV2_OP_COMPARE;
                instr.kind = operandKind;
                instr.aux = typedCompareOp;
                instr.a = destSlot;
                instr.b = lhs.slot;
                instr.c = rhs.slot;
                image.instructions.push_back(instr);
                return {true,destSlot,RTTYPED_NUM_INT};
            }
            return {};
        }

        if(expr->type == INDEX_EXPR){
            auto baseType = owner->inferFastType(expr->leftExpr);
            if(!isFastNumericKind(baseType.arrayElementKind)){
                return {};
            }
            auto collection = lowerExpr(expr->leftExpr);
            auto index = lowerExpr(expr->rightExpr);
            if(!collection.valid || !index.valid){
                return {};
            }
            auto destSlot = allocateTemp(baseType.arrayElementKind);
            RTV2Instruction instr;
            instr.opcode = RTV2_OP_ARRAY_GET;
            instr.kind = baseType.arrayElementKind;
            instr.a = destSlot;
            instr.b = collection.slot;
            instr.c = index.slot;
            image.instructions.push_back(instr);
            return {true,destSlot,baseType.arrayElementKind};
        }

        if(expr->type == IVKE_EXPR){
            auto calleeName = [&]() -> std::string {
                if(!expr->callee){
                    return {};
                }
                if(expr->callee->type == ID_EXPR && expr->callee->id){
                    return expr->callee->id->val;
                }
                if(expr->callee->type == MEMBER_EXPR){
                    std::string memberName;
                    if(getMemberName(expr->callee,memberName)){
                        return memberName;
                    }
                }
                return {};
            }();
            bool canUseSqrtIntrinsic = false;
            if(calleeName == "sqrt" && expr->exprArrayData.size() == 1){
                if(expr->callee->type == ID_EXPR){
                    canUseSqrtIntrinsic = !(context && context->tableContext
                                            && context->tableContext->findEntryByEmittedNoDiag("sqrt"));
                }
                else if(expr->callee->type == MEMBER_EXPR && expr->callee->isScopeAccess){
                    canUseSqrtIntrinsic = true;
                }
            }
            if(canUseSqrtIntrinsic){
                auto arg = lowerExpr(expr->exprArrayData.front());
                if(!arg.valid || !isFastNumericKind(arg.kind)){
                    return {};
                }
                auto destSlot = allocateTemp(RTTYPED_NUM_DOUBLE);
                RTV2Instruction instr;
                instr.opcode = RTV2_OP_INTRINSIC_SQRT;
                instr.a = destSlot;
                instr.b = arg.slot;
                image.instructions.push_back(instr);
                return {true,destSlot,RTTYPED_NUM_DOUBLE};
            }

            auto directCalleeName = directCalleeRuntimeName(context,expr->callee);
            if(directCalleeName.empty()){
                return {};
            }
            std::vector<uint32_t> argSlots;
            argSlots.reserve(expr->exprArrayData.size());
            for(auto *argExpr : expr->exprArrayData){
                auto loweredArg = lowerExpr(argExpr);
                if(!loweredArg.valid){
                    return {};
                }
                argSlots.push_back(loweredArg.slot);
            }
            auto resultKind = owner->inferFastType(expr).scalarKind;
            auto destSlot = allocateTemp(resultKind == RTTYPED_NUM_OBJECT ? RTTYPED_NUM_OBJECT : resultKind);
            RTV2Instruction instr;
            instr.opcode = RTV2_OP_CALL_DIRECT;
            instr.kind = resultKind;
            instr.a = destSlot;
            instr.b = addCallSite(directCalleeName,argSlots);
            image.instructions.push_back(instr);
            return {true,destSlot,resultKind};
        }

        return {};
    }

    bool lowerAssignExpr(ASTExpr *expr){
        if(!expr || expr->type != ASSIGN_EXPR || !expr->leftExpr || !expr->rightExpr){
            return false;
        }
        auto assignOp = expr->oprtr_str.value_or("=");
        bool isCompound = assignOp != "=";

        if(expr->leftExpr->type == ID_EXPR && expr->leftExpr->id && expr->leftExpr->id->type == ASTIdentifier::Var){
            uint32_t destSlot = 0;
            if(!owner->currentLocalSlotForName(expr->leftExpr->id->val,destSlot)){
                return false;
            }
            if(!isCompound){
                auto value = lowerExpr(expr->rightExpr);
                if(!value.valid){
                    return false;
                }
                emitMove(destSlot,value.slot);
                return true;
            }
            auto lhsValue = LoweredValue{true,destSlot,(RTTypedNumericKind)(destSlot < templ.slotKinds.size() ? templ.slotKinds[destSlot] : RTTYPED_NUM_OBJECT)};
            auto rhsValue = lowerExpr(expr->rightExpr);
            if(!lhsValue.valid || !rhsValue.valid || !isFastNumericKind(lhsValue.kind) || !isFastNumericKind(rhsValue.kind)){
                return false;
            }
            RTTypedBinaryOp typedBinaryOp = RTTYPED_BINARY_ADD;
            std::string binarySymbol;
            if(assignOp == "+="){
                binarySymbol = "+";
            }
            else if(assignOp == "-="){
                binarySymbol = "-";
            }
            else if(assignOp == "*="){
                binarySymbol = "*";
            }
            else if(assignOp == "/="){
                binarySymbol = "/";
            }
            else if(assignOp == "%="){
                binarySymbol = "%";
            }
            if(binarySymbol.empty() || !typedBinaryOpFromSymbol(binarySymbol,typedBinaryOp)){
                return false;
            }
            auto resultKind = promotedFastKind(lhsValue.kind,rhsValue.kind);
            auto tempSlot = allocateTemp(resultKind);
            RTV2Instruction instr;
            instr.opcode = RTV2_OP_BINARY;
            instr.kind = resultKind;
            instr.aux = typedBinaryOp;
            instr.a = tempSlot;
            instr.b = lhsValue.slot;
            instr.c = rhsValue.slot;
            image.instructions.push_back(instr);
            emitMove(destSlot,tempSlot);
            return true;
        }

        if(expr->leftExpr->type == INDEX_EXPR){
            auto baseType = owner->inferFastType(expr->leftExpr->leftExpr);
            if(!isFastNumericKind(baseType.arrayElementKind)){
                return false;
            }
            auto collection = lowerExpr(expr->leftExpr->leftExpr);
            auto index = lowerExpr(expr->leftExpr->rightExpr);
            if(!collection.valid || !index.valid){
                return false;
            }
            LoweredValue value;
            if(!isCompound){
                value = lowerExpr(expr->rightExpr);
                if(!value.valid){
                    return false;
                }
            }
            else {
                RTTypedBinaryOp typedBinaryOp = RTTYPED_BINARY_ADD;
                std::string binarySymbol;
                if(assignOp == "+="){
                    binarySymbol = "+";
                }
                else if(assignOp == "-="){
                    binarySymbol = "-";
                }
                else if(assignOp == "*="){
                    binarySymbol = "*";
                }
                else if(assignOp == "/="){
                    binarySymbol = "/";
                }
                else if(assignOp == "%="){
                    binarySymbol = "%";
                }
                if(binarySymbol.empty() || !typedBinaryOpFromSymbol(binarySymbol,typedBinaryOp)){
                    return false;
                }
                auto currentValueSlot = allocateTemp(baseType.arrayElementKind);
                RTV2Instruction getInstr;
                getInstr.opcode = RTV2_OP_ARRAY_GET;
                getInstr.kind = baseType.arrayElementKind;
                getInstr.a = currentValueSlot;
                getInstr.b = collection.slot;
                getInstr.c = index.slot;
                image.instructions.push_back(getInstr);

                auto rhsValue = lowerExpr(expr->rightExpr);
                if(!rhsValue.valid){
                    return false;
                }
                auto resultKind = promotedFastKind(baseType.arrayElementKind,rhsValue.kind);
                auto tempSlot = allocateTemp(resultKind);
                RTV2Instruction binaryInstr;
                binaryInstr.opcode = RTV2_OP_BINARY;
                binaryInstr.kind = resultKind;
                binaryInstr.aux = typedBinaryOp;
                binaryInstr.a = tempSlot;
                binaryInstr.b = currentValueSlot;
                binaryInstr.c = rhsValue.slot;
                image.instructions.push_back(binaryInstr);
                value = {true,tempSlot,resultKind};
            }
            RTV2Instruction setInstr;
            setInstr.opcode = RTV2_OP_ARRAY_SET;
            setInstr.kind = baseType.arrayElementKind;
            setInstr.a = collection.slot;
            setInstr.b = index.slot;
            setInstr.c = value.slot;
            image.instructions.push_back(setInstr);
            return true;
        }

        return false;
    }

    bool lowerStmt(ASTStmt *stmt){
        if(!stmt){
            return true;
        }
        if(stmt->type == VAR_DECL){
            auto snapshot = takeSnapshot();
            auto *decl = (ASTVarDecl *)stmt;
            if(decl->specs.size() != 1){
                return emitFallbackStmt(stmt);
            }
            auto &spec = decl->specs.front();
            if(!spec.id){
                restoreSnapshot(snapshot);
                return emitFallbackStmt(stmt);
            }
            if(!spec.expr){
                return true;
            }
            uint32_t slot = 0;
            if(!owner->currentLocalSlotForName(spec.id->val,slot)){
                restoreSnapshot(snapshot);
                return emitFallbackStmt(stmt);
            }
            auto value = lowerExpr(spec.expr);
            if(!value.valid){
                restoreSnapshot(snapshot);
                return emitFallbackStmt(stmt);
            }
            emitMove(slot,value.slot);
            return true;
        }
        if(stmt->type == WHILE_DECL){
            auto snapshot = takeSnapshot();
            auto *whileDecl = (ASTWhileDecl *)stmt;
            if(!whileDecl->expr || !whileDecl->blockStmt){
                return emitFallbackStmt(stmt);
            }
            auto loopHead = (uint32_t)image.instructions.size();
            auto cond = lowerExpr(whileDecl->expr);
            if(!cond.valid){
                restoreSnapshot(snapshot);
                return emitFallbackStmt(stmt);
            }
            RTV2Instruction branchInstr;
            branchInstr.opcode = RTV2_OP_JUMP_IF_FALSE;
            branchInstr.a = cond.slot;
            branchInstr.b = 0;
            auto branchIndex = image.instructions.size();
            image.instructions.push_back(branchInstr);
            for(auto *innerStmt : whileDecl->blockStmt->body){
                if(!lowerStmt(innerStmt)){
                    return false;
                }
            }
            RTV2Instruction jumpInstr;
            jumpInstr.opcode = RTV2_OP_JUMP;
            jumpInstr.a = loopHead;
            image.instructions.push_back(jumpInstr);
            image.instructions[branchIndex].b = (uint32_t)image.instructions.size();
            return true;
        }
        if(stmt->type == RETURN_DECL){
            auto snapshot = takeSnapshot();
            auto *returnDecl = (ASTReturnDecl *)stmt;
            RTV2Instruction instr;
            instr.opcode = RTV2_OP_RETURN;
            instr.a = UINT32_MAX;
            if(returnDecl->expr){
                auto value = lowerExpr(returnDecl->expr);
                if(!value.valid){
                    restoreSnapshot(snapshot);
                    return emitFallbackStmt(stmt);
                }
                instr.kind = value.kind;
                instr.a = value.slot;
            }
            image.instructions.push_back(instr);
            return true;
        }
        if((stmt->type & EXPR) && ((ASTExpr *)stmt)->type == ASSIGN_EXPR){
            auto snapshot = takeSnapshot();
            if(lowerAssignExpr((ASTExpr *)stmt)){
                return true;
            }
            restoreSnapshot(snapshot);
            return emitFallbackStmt(stmt);
        }
        return emitFallbackStmt(stmt);
    }

public:
    BytecodeV2Lowerer(CodeGen *owner,
                      ModuleGenContext *context,
                      ASTBlockStmt *functionBlock,
                      const std::vector<std::pair<ASTIdentifier *,ASTType *>> &orderedParams,
                      RTFuncTemplate &templ)
        : owner(owner),
          context(context),
          functionBlock(functionBlock),
          orderedParams(orderedParams),
          templ(templ) {
    }

    bool lower(RTV2FunctionImage &imageOut){
        if(!functionBlock){
            return false;
        }
        for(auto *stmt : functionBlock->body){
            if((stmt->type & EXPR) && stmt->type != ASSIGN_EXPR){
                owner->ensureInlineExprTemplates((ASTExpr *)stmt);
            }
            if(!lowerStmt(stmt)){
                return false;
            }
        }
        imageOut = std::move(image);
        return true;
    }
};

static bool emitRuntimeFunctionV2(ASTBlockStmt *blockStmt,
                                  const std::vector<std::pair<ASTIdentifier *,ASTType *>> &orderedParams,
                                  ModuleGenContext *ctxt,
                                  CodeGen *astConsumer,
                                  RTFuncTemplate &templ){
    if(!blockStmt || !ctxt || !astConsumer){
        return false;
    }
    astConsumer->pushLocalSlotContext(orderedParams,blockStmt,templ);
    RTV2FunctionImage image;
    CodeGen::BytecodeV2Lowerer lowerer(astConsumer,ctxt,blockStmt,orderedParams,templ);
    bool lowered = lowerer.lower(image);
    if(lowered){
        std::ostringstream bodyBuffer(std::ios::out | std::ios::binary);
        lowered = writeRTV2FunctionImage(bodyBuffer,image);
        if(lowered){
            auto bodyBytes = bodyBuffer.str();
            templ.blockByteSize = bodyBytes.size();
            addRTInternalAttribute(templ,"__rt_body_v2");
            ctxt->out << &templ;
            RTCode code = CODE_RTFUNCBLOCK_BEGIN;
            ctxt->out.write((char *)&code,sizeof(RTCode));
            if(!bodyBytes.empty()){
                ctxt->out.write(bodyBytes.data(),(std::streamsize)bodyBytes.size());
            }
            code = CODE_RTFUNCBLOCK_END;
            ctxt->out.write((char *)&code,sizeof(RTCode));
        }
    }
    astConsumer->popLocalSlotContext();
    return lowered;
}

static void emitRuntimeFunction(ASTBlockStmt *blockStmt,
                                const std::vector<std::pair<ASTIdentifier *,ASTType *>> &orderedParams,
                                ModuleGenContext *ctxt,
                                CodeGen *astConsumer,
                                RTFuncTemplate &templ,
                                bool allowV2 = true){
    if(allowV2 && ctxt && ctxt->bytecodeVersion == RTBYTECODE_VERSION_V2
       && emitRuntimeFunctionV2(blockStmt,orderedParams,ctxt,astConsumer,templ)){
        return;
    }
    astConsumer->pushLocalSlotContext(orderedParams,blockStmt,templ);
    auto bodyBytes = emitBlockBodyToBuffer(blockStmt,ctxt,astConsumer);
    templ.blockByteSize = bodyBytes.size();
    ctxt->out << &templ;
    RTCode code = CODE_RTFUNCBLOCK_BEGIN;
    ctxt->out.write((char *)&code,sizeof(RTCode));
    if(!bodyBytes.empty()){
        ctxt->out.write(bodyBytes.data(),(std::streamsize)bodyBytes.size());
    }
    code = CODE_RTFUNCBLOCK_END;
    ctxt->out.write((char *)&code,sizeof(RTCode));
    astConsumer->popLocalSlotContext();
}

inline void ASTIdentifier_to_RTID(ASTIdentifier *var,RTID &out);
static RTID makeOwnedRTID(string_ref value);
struct LocalSlotSemanticInfo {
    std::string name;
    CodeGen::FastTypeInfo fastType;
    ASTType *semanticType = nullptr;
};
static void collectLocalSlotInfosFromBlock(ASTBlockStmt *blockStmt,
                                           std::set<std::string> &seen,
                                           std::vector<LocalSlotSemanticInfo> &out,
                                           const CodeGen *codeGen);

static bool paramComesBefore(ASTIdentifier *lhs,ASTIdentifier *rhs){
    if(lhs == rhs){
        return false;
    }
    if(!lhs){
        return false;
    }
    if(!rhs){
        return true;
    }
    if(lhs->codeRegion.startLine != rhs->codeRegion.startLine){
        return lhs->codeRegion.startLine < rhs->codeRegion.startLine;
    }
    if(lhs->codeRegion.startCol != rhs->codeRegion.startCol){
        return lhs->codeRegion.startCol < rhs->codeRegion.startCol;
    }
    return lhs->val < rhs->val;
}

static std::vector<std::pair<ASTIdentifier *,ASTType *>> orderedParamPairs(const std::map<ASTIdentifier *,ASTType *> &paramMap){
    std::vector<std::pair<ASTIdentifier *,ASTType *>> ordered;
    ordered.reserve(paramMap.size());
    for(auto &entry : paramMap){
        ordered.push_back(entry);
    }
    std::sort(ordered.begin(),ordered.end(),[](const auto &lhs,const auto &rhs){
        return paramComesBefore(lhs.first,rhs.first);
    });
    return ordered;
}

static void collectInlineFunctionExprs(ASTExpr *expr,std::vector<ASTExpr *> &out){
    if(!expr){
        return;
    }
    if(expr->type == INLINE_FUNC_EXPR){
        out.push_back(expr);
        return;
    }
    if(expr->leftExpr){
        collectInlineFunctionExprs(expr->leftExpr,out);
    }
    if(expr->middleExpr){
        collectInlineFunctionExprs(expr->middleExpr,out);
    }
    if(expr->rightExpr){
        collectInlineFunctionExprs(expr->rightExpr,out);
    }
    if(expr->callee){
        collectInlineFunctionExprs(expr->callee,out);
    }
    for(auto *arg : expr->exprArrayData){
        collectInlineFunctionExprs(arg,out);
    }
    for(auto &entry : expr->dictExpr){
        collectInlineFunctionExprs(entry.first,out);
        collectInlineFunctionExprs(entry.second,out);
    }
}

void CodeGen::ensureInlineFunctionTemplate(ASTExpr *inlineExpr,const std::string &hint){
    if(!inlineExpr || inlineExpr->type != INLINE_FUNC_EXPR || !inlineExpr->inlineFuncBlock){
        return;
    }
    if(inlineExpr->inlineFuncRuntimeName.empty()){
        auto suffix = hint.empty() ? "inline" : hint;
        inlineExpr->inlineFuncRuntimeName = "__inline__" + suffix + "_" + std::to_string(inlineFuncCounter++);
    }
    if(emittedInlineRuntimeFuncs.find(inlineExpr->inlineFuncRuntimeName) != emittedInlineRuntimeFuncs.end()){
        return;
    }

    RTFuncTemplate inlineTemplate;
    inlineTemplate.name = makeOwnedRTID(inlineExpr->inlineFuncRuntimeName);
    auto orderedParams = orderedParamPairs(inlineExpr->inlineFuncParams);
    for(auto &paramPair : orderedParams){
        RTID paramId;
        ASTIdentifier_to_RTID(paramPair.first,paramId);
        inlineTemplate.argsTemplate.push_back(paramId);
    }
    emitRuntimeFunction(inlineExpr->inlineFuncBlock,orderedParams,genContext,this,inlineTemplate);
    emittedInlineRuntimeFuncs.insert(inlineExpr->inlineFuncRuntimeName);
}

void CodeGen::ensureInlineExprTemplates(ASTExpr *expr){
    std::vector<ASTExpr *> inlineExprs;
    collectInlineFunctionExprs(expr,inlineExprs);
    for(auto *inlineExpr : inlineExprs){
        ensureInlineFunctionTemplate(inlineExpr,"expr");
    }
}

static RTTypedNumericKind fastScalarKindFromType(ASTType *type){
    if(!type){
        return RTTYPED_NUM_OBJECT;
    }
    if(type->nameMatches(INT_TYPE)){
        return RTTYPED_NUM_INT;
    }
    if(type->nameMatches(LONG_TYPE)){
        return RTTYPED_NUM_LONG;
    }
    if(type->nameMatches(FLOAT_TYPE)){
        return RTTYPED_NUM_FLOAT;
    }
    if(type->nameMatches(DOUBLE_TYPE)){
        return RTTYPED_NUM_DOUBLE;
    }
    return RTTYPED_NUM_OBJECT;
}

CodeGen::FastTypeInfo CodeGen::fastTypeFromTypeNode(ASTType *type) const{
    FastTypeInfo info;
    if(!type){
        return info;
    }
    info.scalarKind = fastScalarKindFromType(type);
    if(type->nameMatches(ARRAY_TYPE) && type->typeParams.size() == 1 && type->typeParams.front()){
        info.arrayElementKind = fastScalarKindFromType(type->typeParams.front());
    }
    return info;
}

static void recordLocalSlotInfo(ASTIdentifier *id,
                                ASTType *type,
                                std::set<std::string> &seen,
                                std::vector<LocalSlotSemanticInfo> &out,
                                const CodeGen *codeGen){
    if(!id){
        return;
    }
    auto inserted = seen.insert(id->val);
    if(inserted.second){
        (void)codeGen;
        CodeGen::FastTypeInfo info;
        if(type){
            info.scalarKind = fastScalarKindFromType(type);
            if(type->nameMatches(ARRAY_TYPE) && type->typeParams.size() == 1 && type->typeParams.front()){
                info.arrayElementKind = fastScalarKindFromType(type->typeParams.front());
            }
        }
        out.push_back({id->val,info,type});
    }
}

static void collectLocalSlotInfosFromDecl(ASTDecl *decl,
                                          std::set<std::string> &seen,
                                          std::vector<LocalSlotSemanticInfo> &out,
                                          const CodeGen *codeGen);

static void collectLocalSlotInfosFromBlock(ASTBlockStmt *blockStmt,
                                           std::set<std::string> &seen,
                                           std::vector<LocalSlotSemanticInfo> &out,
                                           const CodeGen *codeGen){
    if(!blockStmt){
        return;
    }
    for(auto *stmt : blockStmt->body){
        if(stmt && (stmt->type & DECL)){
            collectLocalSlotInfosFromDecl(static_cast<ASTDecl *>(stmt),seen,out,codeGen);
        }
    }
}

static void collectLocalSlotInfosFromDecl(ASTDecl *decl,
                                          std::set<std::string> &seen,
                                          std::vector<LocalSlotSemanticInfo> &out,
                                          const CodeGen *codeGen){
    if(!decl){
        return;
    }
    if(decl->type == VAR_DECL){
        auto *varDecl = static_cast<ASTVarDecl *>(decl);
        for(auto &spec : varDecl->specs){
            recordLocalSlotInfo(spec.id,spec.type,seen,out,codeGen);
        }
        return;
    }
    if(decl->type == SECURE_DECL){
        auto *secureDecl = static_cast<ASTSecureDecl *>(decl);
        if(secureDecl->guardedDecl){
            collectLocalSlotInfosFromDecl(secureDecl->guardedDecl,seen,out,codeGen);
        }
        recordLocalSlotInfo(secureDecl->catchErrorId,secureDecl->catchErrorType,seen,out,codeGen);
        collectLocalSlotInfosFromBlock(secureDecl->catchBlock,seen,out,codeGen);
        return;
    }
    if(decl->type == COND_DECL){
        auto *condDecl = static_cast<ASTConditionalDecl *>(decl);
        for(auto &spec : condDecl->specs){
            collectLocalSlotInfosFromBlock(spec.blockStmt,seen,out,codeGen);
        }
        return;
    }
    if(decl->type == FOR_DECL){
        auto *forDecl = static_cast<ASTForDecl *>(decl);
        collectLocalSlotInfosFromBlock(forDecl->blockStmt,seen,out,codeGen);
        return;
    }
    if(decl->type == WHILE_DECL){
        auto *whileDecl = static_cast<ASTWhileDecl *>(decl);
        collectLocalSlotInfosFromBlock(whileDecl->blockStmt,seen,out,codeGen);
        return;
    }
}

void CodeGen::pushLocalSlotContext(const std::vector<std::pair<ASTIdentifier *,ASTType *>> &orderedParams,
                                   ASTBlockStmt *blockStmt,
                                   RTFuncTemplate &templ){
    LocalSlotContext context;
    context.argSlotCount = (unsigned)orderedParams.size();

    std::set<std::string> seen;
    uint32_t nextSlot = 0;
    for(const auto &paramPair : orderedParams){
        if(!paramPair.first){
            continue;
        }
        std::string argName = paramPair.first->val;
        seen.insert(argName);
        context.slotMap.emplace(argName,nextSlot++);
        auto typeInfo = fastTypeFromTypeNode(paramPair.second);
        context.slotTypeMap[argName] = typeInfo;
        context.slotSemanticTypeMap[argName] = paramPair.second;
        context.slotKinds.push_back(typeInfo.scalarKind);
    }

    std::vector<LocalSlotSemanticInfo> localInfos;
    collectLocalSlotInfosFromBlock(blockStmt,seen,localInfos,this);
    for(const auto &localInfo : localInfos){
        context.slotMap.emplace(localInfo.name,nextSlot++);
        context.slotTypeMap[localInfo.name] = localInfo.fastType;
        context.slotSemanticTypeMap[localInfo.name] = localInfo.semanticType;
        auto slotId = makeOwnedRTID(localInfo.name);
        templ.localSlotNames.push_back(slotId);
        context.localSlotNames.push_back(slotId);
        context.slotKinds.push_back(localInfo.fastType.scalarKind);
    }

    templ.slotKinds = context.slotKinds;
    localSlotStack.push_back(std::move(context));
}

void CodeGen::popLocalSlotContext(){
    if(!localSlotStack.empty()){
        localSlotStack.pop_back();
    }
}

bool CodeGen::currentLocalSlotForName(string_ref name,uint32_t &slotOut) const{
    if(localSlotStack.empty()){
        return false;
    }
    auto &context = localSlotStack.back();
    auto found = context.slotMap.find(name.str());
    if(found == context.slotMap.end()){
        return false;
    }
    slotOut = found->second;
    return true;
}

bool CodeGen::currentLocalSlotFastType(string_ref name,FastTypeInfo &typeOut) const{
    if(localSlotStack.empty()){
        return false;
    }
    auto &context = localSlotStack.back();
    auto found = context.slotTypeMap.find(name.str());
    if(found == context.slotTypeMap.end()){
        return false;
    }
    typeOut = found->second;
    return true;
}

bool CodeGen::currentLocalSlotSemanticType(string_ref name,ASTType *&typeOut) const{
    if(localSlotStack.empty()){
        return false;
    }
    auto &context = localSlotStack.back();
    auto found = context.slotSemanticTypeMap.find(name.str());
    if(found == context.slotSemanticTypeMap.end()){
        return false;
    }
    typeOut = found->second;
    return true;
}


inline void ASTIdentifier_to_RTID(ASTIdentifier *var,RTID &out){
    string_ref name = var->val;
    out.len = name.size();
    out.value = name.getBuffer();
}

static RTID makeOwnedRTID(string_ref value){
    RTID id;
    id.len = value.size();
    char *buf = new char[id.len];
    std::memcpy(buf,value.data(),id.len);
    id.value = buf;
    return id;
}

static std::string mangleClassMethodName(string_ref className,string_ref methodName){
    Twine out;
    out + "__";
    out + className.str();
    out + "__";
    out + methodName.str();
    return out.str();
}

static std::string classCtorTemplateName(size_t arity){
    Twine out;
    out + "__ctor__";
    out + std::to_string(arity);
    return out.str();
}

static std::string mangleClassCtorName(string_ref className,size_t arity){
    return mangleClassMethodName(className,classCtorTemplateName(arity));
}

static std::string mangleClassFieldInitName(string_ref className){
    Twine out;
    out + "__";
    out + className.str();
    out + "__field_init";
    return out.str();
}

static void writeNamedVarRef(std::ostream &out,string_ref name){
    RTCode code = CODE_RTVAR_REF;
    out.write((const char *)&code,sizeof(RTCode));
    RTID id = makeOwnedRTID(name);
    out << &id;
}

static void writeLocalSlotRef(std::ostream &out,uint32_t slot){
    RTCode code = CODE_RTLOCAL_REF;
    out.write((const char *)&code,sizeof(RTCode));
    out.write((const char *)&slot,sizeof(slot));
}

static bool getMemberName(ASTExpr *memberExpr,std::string &memberName){
    if(!memberExpr || memberExpr->type != MEMBER_EXPR){
        return false;
    }
    if(!memberExpr->rightExpr || memberExpr->rightExpr->type != ID_EXPR || !memberExpr->rightExpr->id){
        return false;
    }
    memberName = memberExpr->rightExpr->id->val;
    return true;
}

static std::string emittedNameForScopeMember(ModuleGenContext *context,ASTExpr *memberExpr){
    if(!context || !context->tableContext || !memberExpr || !memberExpr->resolvedScope ||
       !memberExpr->rightExpr || !memberExpr->rightExpr->id){
        return {};
    }
    auto *entry = context->tableContext->findEntryInExactScopeNoDiag(memberExpr->rightExpr->id->val,memberExpr->resolvedScope);
    if(!entry){
        return {};
    }
    if(!entry->emittedName.empty()){
        return entry->emittedName;
    }
    return entry->name;
}

static std::string directCalleeRuntimeName(ModuleGenContext *context,ASTExpr *callee){
    if(!callee){
        return {};
    }
    if(callee->type == ID_EXPR && callee->id && callee->id->type == ASTIdentifier::Function){
        if(context && context->tableContext){
            if(auto *entry = context->tableContext->findEntryByEmittedNoDiag(callee->id->val)){
                if(entry->type == Semantics::SymbolTable::Entry::Function){
                    return entry->emittedName.empty() ? entry->name : entry->emittedName;
                }
            }
        }
        return callee->id->val;
    }
    if(callee->type == MEMBER_EXPR && callee->isScopeAccess
       && callee->rightExpr && callee->rightExpr->id
       && callee->rightExpr->id->type == ASTIdentifier::Function){
        return emittedNameForScopeMember(context,callee);
    }
    return {};
}

static RTTypedNumericKind promotedFastKind(RTTypedNumericKind lhs,RTTypedNumericKind rhs){
    return lhs >= rhs ? lhs : rhs;
}

static bool isFastNumericKind(RTTypedNumericKind kind){
    return kind != RTTYPED_NUM_OBJECT;
}

CodeGen::FastTypeInfo CodeGen::inferFastType(ASTExpr *expr) const{
    FastTypeInfo info;
    if(!expr){
        return info;
    }

    if(expr->type & LITERAL){
        auto *literal = static_cast<ASTLiteralExpr *>(expr);
        if(literal->intValue.has_value()){
            info.scalarKind = RTTYPED_NUM_INT;
        }
        else if(literal->floatValue.has_value()){
            info.scalarKind = RTTYPED_NUM_DOUBLE;
        }
        return info;
    }

    if(expr->type == ID_EXPR && expr->id){
        if(expr->id->type == ASTIdentifier::Var){
            if(currentLocalSlotFastType(expr->id->val,info)){
                return info;
            }
            if(genContext && genContext->tableContext){
                if(auto *entry = genContext->tableContext->findEntryByEmittedNoDiag(expr->id->val)){
                    if(entry->type == Semantics::SymbolTable::Entry::Var){
                        auto *varData = static_cast<Semantics::SymbolTable::Var *>(entry->data);
                        return fastTypeFromTypeNode(varData->type);
                    }
                }
            }
        }
        else if(expr->id->type == ASTIdentifier::Function && genContext && genContext->tableContext){
            if(auto *entry = genContext->tableContext->findEntryByEmittedNoDiag(expr->id->val)){
                if(entry->type == Semantics::SymbolTable::Entry::Function){
                    auto *funcData = static_cast<Semantics::SymbolTable::Function *>(entry->data);
                    return fastTypeFromTypeNode(funcData->returnType);
                }
            }
        }
        return info;
    }

    if(expr->type == MEMBER_EXPR){
        std::string memberName;
        if(!getMemberName(expr,memberName)){
            return info;
        }
        auto baseType = inferFastType(expr->leftExpr);
        if(memberName == "length"){
            info.scalarKind = RTTYPED_NUM_INT;
            return info;
        }
        if(expr->isScopeAccess && genContext && genContext->tableContext && expr->resolvedScope){
            if(auto *entry = genContext->tableContext->findEntryInExactScopeNoDiag(expr->rightExpr->id->val,expr->resolvedScope)){
                if(entry->type == Semantics::SymbolTable::Entry::Var){
                    auto *varData = static_cast<Semantics::SymbolTable::Var *>(entry->data);
                    return fastTypeFromTypeNode(varData->type);
                }
                if(entry->type == Semantics::SymbolTable::Entry::Function){
                    auto *funcData = static_cast<Semantics::SymbolTable::Function *>(entry->data);
                    return fastTypeFromTypeNode(funcData->returnType);
                }
            }
        }
        (void)baseType;
        return info;
    }

    if(expr->type == INDEX_EXPR){
        auto baseType = inferFastType(expr->leftExpr);
        info.scalarKind = baseType.arrayElementKind;
        return info;
    }

    if(expr->type == UNARY_EXPR){
        auto operandType = inferFastType(expr->leftExpr);
        auto op = expr->oprtr_str.value_or("");
        if(op == "+" || op == "-"){
            info.scalarKind = operandType.scalarKind;
        }
        return info;
    }

    if(expr->type == BINARY_EXPR){
        auto lhsType = inferFastType(expr->leftExpr);
        auto rhsType = inferFastType(expr->rightExpr);
        auto op = expr->oprtr_str.value_or("");
        if(op == "+" || op == "-" || op == "*" || op == "/" || op == "%"){
            if(isFastNumericKind(lhsType.scalarKind) && isFastNumericKind(rhsType.scalarKind)){
                info.scalarKind = promotedFastKind(lhsType.scalarKind,rhsType.scalarKind);
            }
        }
        return info;
    }

    if(expr->type == IVKE_EXPR){
        if(expr->runtimeCastTargetName.has_value()){
            auto targetName = expr->runtimeCastTargetName.value();
            if(targetName == INT_TYPE->getName().str()){
                info.scalarKind = RTTYPED_NUM_INT;
            }
            else if(targetName == LONG_TYPE->getName().str()){
                info.scalarKind = RTTYPED_NUM_LONG;
            }
            else if(targetName == FLOAT_TYPE->getName().str()){
                info.scalarKind = RTTYPED_NUM_FLOAT;
            }
            else if(targetName == DOUBLE_TYPE->getName().str()){
                info.scalarKind = RTTYPED_NUM_DOUBLE;
            }
            return info;
        }
        if(!expr->callee){
            return info;
        }
        auto *callee = expr->callee;
        if(callee->type == ID_EXPR && callee->id && callee->id->type == ASTIdentifier::Function){
            if(genContext && genContext->tableContext){
                if(auto *entry = genContext->tableContext->findEntryByEmittedNoDiag(callee->id->val)){
                    if(entry->type == Semantics::SymbolTable::Entry::Function){
                        auto *funcData = static_cast<Semantics::SymbolTable::Function *>(entry->data);
                        return fastTypeFromTypeNode(funcData->returnType);
                    }
                }
            }
        }
        if(callee->type == MEMBER_EXPR && callee->isScopeAccess && genContext && genContext->tableContext && callee->resolvedScope){
            if(auto *entry = genContext->tableContext->findEntryInExactScopeNoDiag(callee->rightExpr->id->val,callee->resolvedScope)){
                if(entry->type == Semantics::SymbolTable::Entry::Function){
                    auto *funcData = static_cast<Semantics::SymbolTable::Function *>(entry->data);
                    return fastTypeFromTypeNode(funcData->returnType);
                }
            }
        }
    }

    return info;
}

static Semantics::SymbolTable::Entry *findEntryByTypeName(Semantics::STableContext *tableContext,
                                                          ASTType *type,
                                                          std::shared_ptr<ASTScope> scope){
    if(!tableContext || !type){
        return nullptr;
    }
    auto typeName = type->getName();
    if(auto *entry = tableContext->findEntryByEmittedNoDiag(typeName)){
        return entry;
    }
    if(scope){
        return tableContext->findEntryNoDiag(typeName,scope);
    }
    return nullptr;
}

static ASTType *semanticTypeFromEntry(Semantics::SymbolTable::Entry *entry){
    if(!entry){
        return nullptr;
    }
    switch(entry->type){
        case Semantics::SymbolTable::Entry::Var:
            return static_cast<Semantics::SymbolTable::Var *>(entry->data)->type;
        case Semantics::SymbolTable::Entry::Function:
            return static_cast<Semantics::SymbolTable::Function *>(entry->data)->returnType;
        case Semantics::SymbolTable::Entry::Class:
            return static_cast<Semantics::SymbolTable::Class *>(entry->data)->classType;
        default:
            return nullptr;
    }
}

static ASTType *findSemanticClassFieldType(Semantics::STableContext *tableContext,
                                           ASTType *classType,
                                           std::shared_ptr<ASTScope> scope,
                                           string_ref memberName){
    if(!tableContext || !classType){
        return nullptr;
    }
    auto *entry = findEntryByTypeName(tableContext,classType,scope);
    if(!entry || entry->type != Semantics::SymbolTable::Entry::Class){
        return nullptr;
    }
    auto *classData = static_cast<Semantics::SymbolTable::Class *>(entry->data);
    for(auto *field : classData->fields){
        if(field && field->name == memberName){
            return field->type;
        }
    }
    if(classData->superClassType){
        return findSemanticClassFieldType(tableContext,classData->superClassType,scope,memberName);
    }
    return nullptr;
}

static ASTType *findSemanticClassMethodReturnType(Semantics::STableContext *tableContext,
                                                  ASTType *classType,
                                                  std::shared_ptr<ASTScope> scope,
                                                  string_ref memberName){
    if(!tableContext || !classType){
        return nullptr;
    }
    auto *entry = findEntryByTypeName(tableContext,classType,scope);
    if(!entry || entry->type != Semantics::SymbolTable::Entry::Class){
        return nullptr;
    }
    auto *classData = static_cast<Semantics::SymbolTable::Class *>(entry->data);
    for(auto *method : classData->instMethods){
        if(method && method->name == memberName){
            return method->returnType;
        }
    }
    if(classData->superClassType){
        return findSemanticClassMethodReturnType(tableContext,classData->superClassType,scope,memberName);
    }
    return nullptr;
}

static bool buildSemanticClassChain(Semantics::STableContext *tableContext,
                                    ASTType *classType,
                                    std::shared_ptr<ASTScope> scope,
                                    std::vector<Semantics::SymbolTable::Class *> &chain){
    if(!tableContext || !classType){
        return false;
    }
    auto *entry = findEntryByTypeName(tableContext,classType,scope);
    if(!entry || entry->type != Semantics::SymbolTable::Entry::Class){
        return false;
    }
    auto *classData = static_cast<Semantics::SymbolTable::Class *>(entry->data);
    if(classData->superClassType){
        buildSemanticClassChain(tableContext,classData->superClassType,scope,chain);
    }
    chain.push_back(classData);
    return true;
}

static bool resolveSemanticFieldSlot(Semantics::STableContext *tableContext,
                                     ASTType *classType,
                                     std::shared_ptr<ASTScope> scope,
                                     string_ref memberName,
                                     uint32_t &slotOut){
    slotOut = 0;
    std::vector<Semantics::SymbolTable::Class *> chain;
    if(!buildSemanticClassChain(tableContext,classType,scope,chain)){
        return false;
    }
    std::set<std::string> seen;
    uint32_t nextSlot = 0;
    for(auto *classData : chain){
        for(auto *field : classData->fields){
            if(!field){
                continue;
            }
            auto inserted = seen.insert(field->name);
            if(!inserted.second){
                continue;
            }
            if(field->name == memberName){
                slotOut = nextSlot;
                return true;
            }
            ++nextSlot;
        }
    }
    return false;
}

ASTType *CodeGen::inferSemanticType(ASTExpr *expr) const{
    if(!expr){
        return nullptr;
    }

    if(expr->type & LITERAL){
        auto *literal = static_cast<ASTLiteralExpr *>(expr);
        if(literal->strValue.has_value()){
            return STRING_TYPE;
        }
        if(literal->regexPattern.has_value()){
            return REGEX_TYPE;
        }
        if(literal->boolValue.has_value()){
            return BOOL_TYPE;
        }
        if(literal->floatValue.has_value()){
            return DOUBLE_TYPE;
        }
        if(literal->intValue.has_value()){
            return INT_TYPE;
        }
        if(expr->type == ARRAY_EXPR){
            return ARRAY_TYPE;
        }
        if(expr->type == DICT_EXPR){
            return DICTIONARY_TYPE;
        }
    }

    if(expr->type == ARRAY_EXPR){
        return ARRAY_TYPE;
    }
    if(expr->type == DICT_EXPR){
        return DICTIONARY_TYPE;
    }

    if(expr->type == ID_EXPR && expr->id){
        ASTType *localType = nullptr;
        if(expr->id->type == ASTIdentifier::Var && currentLocalSlotSemanticType(expr->id->val,localType)){
            return localType;
        }
        if(genContext && genContext->tableContext){
            if(auto *entry = genContext->tableContext->findEntryByEmittedNoDiag(expr->id->val)){
                if(auto *type = semanticTypeFromEntry(entry)){
                    return type;
                }
            }
        }
        return nullptr;
    }

    if(expr->type == MEMBER_EXPR){
        if(expr->isScopeAccess && genContext && genContext->tableContext && expr->resolvedScope
           && expr->rightExpr && expr->rightExpr->id){
            if(auto *entry = genContext->tableContext->findEntryInExactScopeNoDiag(expr->rightExpr->id->val,expr->resolvedScope)){
                return semanticTypeFromEntry(entry);
            }
            return nullptr;
        }

        std::string memberName;
        if(!getMemberName(expr,memberName)){
            return nullptr;
        }

        auto *baseType = inferSemanticType(expr->leftExpr);
        if(!baseType){
            return nullptr;
        }
        if(baseType->nameMatches(STRING_TYPE) || baseType->nameMatches(ARRAY_TYPE)
           || baseType->nameMatches(DICTIONARY_TYPE) || baseType->nameMatches(MAP_TYPE)){
            if(memberName == "length"){
                return INT_TYPE;
            }
            return nullptr;
        }
        if(!genContext || !genContext->tableContext){
            return nullptr;
        }
        if(auto *fieldType = findSemanticClassFieldType(genContext->tableContext,baseType,expr->scope,memberName)){
            return fieldType;
        }
        return findSemanticClassMethodReturnType(genContext->tableContext,baseType,expr->scope,memberName);
    }

    if(expr->type == IVKE_EXPR){
        if(expr->runtimeCastTargetName.has_value() && genContext && genContext->tableContext){
            if(auto *entry = genContext->tableContext->findEntryByEmittedNoDiag(expr->runtimeCastTargetName.value())){
                return semanticTypeFromEntry(entry);
            }
        }
        if(expr->isConstructorCall && expr->callee){
            if(expr->callee->type == ID_EXPR && expr->callee->id){
                if(genContext && genContext->tableContext){
                    if(auto *entry = genContext->tableContext->findEntryByEmittedNoDiag(expr->callee->id->val)){
                        return semanticTypeFromEntry(entry);
                    }
                }
            }
            if(expr->callee->type == MEMBER_EXPR){
                return inferSemanticType(expr->callee);
            }
        }
        if(!expr->callee){
            return nullptr;
        }
        if(expr->callee->type == MEMBER_EXPR && !expr->callee->isScopeAccess){
            std::string memberName;
            if(!getMemberName(expr->callee,memberName)){
                return nullptr;
            }
            auto *baseType = inferSemanticType(expr->callee->leftExpr);
            if(!baseType || !genContext || !genContext->tableContext){
                return nullptr;
            }
            return findSemanticClassMethodReturnType(genContext->tableContext,baseType,expr->callee->scope,memberName);
        }
        return inferSemanticType(expr->callee);
    }

    return nullptr;
}

bool CodeGen::tryResolveDirectFieldSlot(ASTExpr *memberExpr,uint32_t &slotOut) const{
    slotOut = 0;
    if(!memberExpr || memberExpr->type != MEMBER_EXPR || memberExpr->isScopeAccess
       || !memberExpr->rightExpr || !memberExpr->rightExpr->id
       || memberExpr->rightExpr->id->type != ASTIdentifier::Var
       || !genContext || !genContext->tableContext){
        return false;
    }
    auto *receiverType = inferSemanticType(memberExpr->leftExpr);
    if(!receiverType){
        return false;
    }
    return resolveSemanticFieldSlot(genContext->tableContext,
                                    receiverType,
                                    memberExpr->scope,
                                    memberExpr->rightExpr->id->val,
                                    slotOut);
}

static bool unaryOpCodeFromSymbol(const std::string &op,RTCode &out){
    if(op == "+"){
        out = UNARY_OP_PLUS;
        return true;
    }
    if(op == "-"){
        out = UNARY_OP_MINUS;
        return true;
    }
    if(op == "!"){
        out = UNARY_OP_NOT;
        return true;
    }
    if(op == "~"){
        out = UNARY_OP_BITWISE_NOT;
        return true;
    }
    if(op == "await"){
        out = UNARY_OP_AWAIT;
        return true;
    }
    return false;
}

static bool typedBinaryOpFromSymbol(const std::string &op,RTTypedBinaryOp &out){
    if(op == "+"){
        out = RTTYPED_BINARY_ADD;
        return true;
    }
    if(op == "-"){
        out = RTTYPED_BINARY_SUB;
        return true;
    }
    if(op == "*"){
        out = RTTYPED_BINARY_MUL;
        return true;
    }
    if(op == "/"){
        out = RTTYPED_BINARY_DIV;
        return true;
    }
    if(op == "%"){
        out = RTTYPED_BINARY_MOD;
        return true;
    }
    return false;
}

static bool typedCompareOpFromSymbol(const std::string &op,RTTypedCompareOp &out){
    if(op == "=="){
        out = RTTYPED_COMPARE_EQ;
        return true;
    }
    if(op == "!="){
        out = RTTYPED_COMPARE_NE;
        return true;
    }
    if(op == "<"){
        out = RTTYPED_COMPARE_LT;
        return true;
    }
    if(op == "<="){
        out = RTTYPED_COMPARE_LE;
        return true;
    }
    if(op == ">"){
        out = RTTYPED_COMPARE_GT;
        return true;
    }
    if(op == ">="){
        out = RTTYPED_COMPARE_GE;
        return true;
    }
    return false;
}

static bool binaryOpCodeFromSymbol(const std::string &op,RTCode &out){
    if(op == "+"){
        out = BINARY_OP_PLUS;
        return true;
    }
    if(op == "-"){
        out = BINARY_OP_MINUS;
        return true;
    }
    if(op == "*"){
        out = BINARY_OP_MUL;
        return true;
    }
    if(op == "/"){
        out = BINARY_OP_DIV;
        return true;
    }
    if(op == "%"){
        out = BINARY_OP_MOD;
        return true;
    }
    if(op == "=="){
        out = BINARY_EQUAL_EQUAL;
        return true;
    }
    if(op == "!="){
        out = BINARY_NOT_EQUAL;
        return true;
    }
    if(op == "<"){
        out = BINARY_LESS;
        return true;
    }
    if(op == "<="){
        out = BINARY_LESS_EQUAL;
        return true;
    }
    if(op == ">"){
        out = BINARY_GREATER;
        return true;
    }
    if(op == ">="){
        out = BINARY_GREATER_EQUAL;
        return true;
    }
    if(op == "&&"){
        out = BINARY_LOGIC_AND;
        return true;
    }
    if(op == "||"){
        out = BINARY_LOGIC_OR;
        return true;
    }
    if(op == "&"){
        out = BINARY_BITWISE_AND;
        return true;
    }
    if(op == "|"){
        out = BINARY_BITWISE_OR;
        return true;
    }
    if(op == "^"){
        out = BINARY_BITWISE_XOR;
        return true;
    }
    if(op == "<<"){
        out = BINARY_SHIFT_LEFT;
        return true;
    }
    if(op == ">>"){
        out = BINARY_SHIFT_RIGHT;
        return true;
    }
    return false;
}

static void writeTypedLocalSlotRef(std::ostream &out,RTTypedNumericKind kind,uint32_t slot){
    RTCode code = CODE_RTTYPED_LOCAL_REF;
    out.write((const char *)&code,sizeof(code));
    out.write((const char *)&kind,sizeof(kind));
    out.write((const char *)&slot,sizeof(slot));
}

static void writeTypedBinaryHeader(std::ostream &out,RTTypedNumericKind kind,RTTypedBinaryOp op){
    RTCode code = CODE_RTTYPED_BINARY;
    out.write((const char *)&code,sizeof(code));
    out.write((const char *)&kind,sizeof(kind));
    out.write((const char *)&op,sizeof(op));
}

static void writeTypedNegateHeader(std::ostream &out,RTTypedNumericKind kind){
    RTCode code = CODE_RTTYPED_NEGATE;
    out.write((const char *)&code,sizeof(code));
    out.write((const char *)&kind,sizeof(kind));
}

static void writeTypedCompareHeader(std::ostream &out,RTTypedNumericKind kind,RTTypedCompareOp op){
    RTCode code = CODE_RTTYPED_COMPARE;
    out.write((const char *)&code,sizeof(code));
    out.write((const char *)&kind,sizeof(kind));
    out.write((const char *)&op,sizeof(op));
}

static void writeTypedIndexGetHeader(std::ostream &out,RTTypedNumericKind kind){
    RTCode code = CODE_RTTYPED_INDEX_GET;
    out.write((const char *)&code,sizeof(code));
    out.write((const char *)&kind,sizeof(kind));
}

static void writeTypedIndexSetHeader(std::ostream &out,RTTypedNumericKind kind){
    RTCode code = CODE_RTTYPED_INDEX_SET;
    out.write((const char *)&code,sizeof(code));
    out.write((const char *)&kind,sizeof(kind));
}

static void writeTypedIntrinsicHeader(std::ostream &out,RTTypedNumericKind kind,RTTypedIntrinsicOp op){
    RTCode code = CODE_RTTYPED_INTRINSIC;
    out.write((const char *)&code,sizeof(code));
    out.write((const char *)&kind,sizeof(kind));
    out.write((const char *)&op,sizeof(op));
}

static std::vector<RTAttribute> convertAttributes(const std::vector<ASTAttribute> &astAttrs){
    std::vector<RTAttribute> out;
    out.reserve(astAttrs.size());
    for(const auto &astAttr : astAttrs){
        RTAttribute rtAttr;
        rtAttr.name = makeOwnedRTID(astAttr.name);
        for(const auto &astArg : astAttr.args){
            RTAttributeArg rtArg;
            rtArg.hasName = astArg.key.has_value();
            if(rtArg.hasName){
                rtArg.name = makeOwnedRTID(astArg.key.value());
            }

            auto *expr = (ASTExpr *)astArg.value;
            if(expr && expr->type == STR_LITERAL){
                auto *literal = (ASTLiteralExpr *)expr;
                rtArg.valueType = RTATTR_VALUE_STRING;
                rtArg.value = makeOwnedRTID(literal->strValue.value_or(""));
            }
            else if(expr && expr->type == BOOL_LITERAL){
                auto *literal = (ASTLiteralExpr *)expr;
                rtArg.valueType = RTATTR_VALUE_BOOL;
                rtArg.value = makeOwnedRTID(literal->boolValue.value_or(false) ? "true" : "false");
            }
            else if(expr && expr->type == NUM_LITERAL){
                auto *literal = (ASTLiteralExpr *)expr;
                rtArg.valueType = RTATTR_VALUE_NUMBER;
                if(literal->intValue.has_value()){
                    rtArg.value = makeOwnedRTID(std::to_string(literal->intValue.value()));
                }
                else {
                    rtArg.value = makeOwnedRTID(std::to_string(literal->floatValue.value_or(0.0)));
                }
            }
            else if(expr && expr->type == ID_EXPR && expr->id){
                rtArg.valueType = RTATTR_VALUE_IDENTIFIER;
                rtArg.value = makeOwnedRTID(expr->id->val);
            }
            else {
                rtArg.valueType = RTATTR_VALUE_IDENTIFIER;
                rtArg.value = makeOwnedRTID("");
            }
            rtAttr.args.push_back(std::move(rtArg));
        }
        out.push_back(std::move(rtAttr));
    }
    return out;
}

/*
 Generates runtime code. 
 (Breaks down complex/nested expressions to optimize runtime.)
*/


void CodeGen::consumeDecl(ASTDecl *stmt){
    if(stmt && genContext && genContext->bytecodeVersion == RTBYTECODE_VERSION_V2 && localSlotStack.empty()){
        if(stmt->type == VAR_DECL
           || stmt->type == COND_DECL
           || stmt->type == FOR_DECL
           || stmt->type == WHILE_DECL
           || stmt->type == SECURE_DECL
           || stmt->type == RETURN_DECL){
            bufferedTopLevelStatements.push_back(stmt);
            return;
        }
    }
    if(stmt->type == VAR_DECL){
        ASTVarDecl *varDecl = (ASTVarDecl *)stmt;
        for(auto & spec : varDecl->specs){
            ASTIdentifier *var_id = spec.id;
            if(spec.expr){
                if(spec.expr->type == INLINE_FUNC_EXPR){
                    auto *inlineExpr = (ASTExpr *)spec.expr;
                    ensureInlineFunctionTemplate(inlineExpr,var_id ? var_id->val : "var");
                }
                else {
                    ensureInlineExprTemplates(spec.expr);
                }
            }

            uint32_t localSlot = 0;
            if(var_id && currentLocalSlotForName(var_id->val,localSlot)){
                RTCode code = CODE_RTLOCAL_DECL;
                bool hasInitValue = (spec.expr != nullptr);
                genContext->out.write((const char *)&code,sizeof(RTCode));
                genContext->out.write((const char *)&localSlot,sizeof(localSlot));
                genContext->out.write((const char *)&hasInitValue,sizeof(hasInitValue));
                if(spec.expr){
                    ASTExpr *initialVal = spec.expr;
                    if(initialVal->type == INLINE_FUNC_EXPR){
                        ensureInlineFunctionTemplate(initialVal,var_id ? var_id->val : "var");
                        RTCode funcCode = CODE_RTFUNC_REF;
                        genContext->out.write((const char *)&funcCode,sizeof(RTCode));
                        RTID funcId = makeOwnedRTID(initialVal->inlineFuncRuntimeName);
                        genContext->out << &funcId;
                    }
                    else {
                        consumeStmt(initialVal);
                    }
                }
            }
            else {
                RTVar code_var;
                ASTIdentifier_to_RTID(var_id,code_var.id);
                code_var.hasInitValue = (spec.expr != nullptr);
                code_var.attributes = convertAttributes(varDecl->attributes);
                genContext->out << &code_var;
                if(spec.expr) {
                    ASTExpr *initialVal = spec.expr;
                    if(initialVal->type == INLINE_FUNC_EXPR){
                        ensureInlineFunctionTemplate(initialVal,var_id ? var_id->val : "var");
                        RTCode code = CODE_RTFUNC_REF;
                        genContext->out.write((const char *)&code,sizeof(RTCode));
                        RTID funcId = makeOwnedRTID(initialVal->inlineFuncRuntimeName);
                        genContext->out << &funcId;
                    }
                    else {
                        consumeStmt(initialVal);
                    }
                }
            }
        };
    }
    else if(stmt->type == SECURE_DECL){
        auto *secureDecl = (ASTSecureDecl *)stmt;
        if(!secureDecl->guardedDecl || secureDecl->guardedDecl->specs.empty() || !secureDecl->catchBlock){
            return;
        }
        auto &guardedSpec = secureDecl->guardedDecl->specs.front();
        if(!guardedSpec.id || !guardedSpec.expr){
            return;
        }
        ensureInlineExprTemplates(guardedSpec.expr);

        uint32_t targetSlot = 0;
        if(currentLocalSlotForName(guardedSpec.id->val,targetSlot)){
            RTCode code = CODE_RTSECURE_LOCAL_DECL;
            genContext->out.write((const char *)&code,sizeof(RTCode));
            genContext->out.write((const char *)&targetSlot,sizeof(targetSlot));

            bool hasCatchBinding = secureDecl->catchErrorId != nullptr;
            genContext->out.write((const char *)&hasCatchBinding,sizeof(hasCatchBinding));
            if(hasCatchBinding){
                uint32_t catchSlot = targetSlot;
                currentLocalSlotForName(secureDecl->catchErrorId->val,catchSlot);
                genContext->out.write((const char *)&catchSlot,sizeof(catchSlot));
            }

            bool hasCatchType = secureDecl->catchErrorType != nullptr;
            genContext->out.write((const char *)&hasCatchType,sizeof(hasCatchType));
            if(hasCatchType){
                RTID catchTypeId = makeOwnedRTID(std::string(secureDecl->catchErrorType->getName()));
                genContext->out << &catchTypeId;
            }
        }
        else {
            RTCode code = CODE_RTSECURE_DECL;
            genContext->out.write((const char *)&code,sizeof(RTCode));
            RTID targetVarId;
            ASTIdentifier_to_RTID(guardedSpec.id,targetVarId);
            genContext->out << &targetVarId;

            bool hasCatchBinding = secureDecl->catchErrorId != nullptr;
            genContext->out.write((const char *)&hasCatchBinding,sizeof(hasCatchBinding));
            if(hasCatchBinding){
                RTID catchBindingId;
                ASTIdentifier_to_RTID(secureDecl->catchErrorId,catchBindingId);
                genContext->out << &catchBindingId;
            }

            bool hasCatchType = secureDecl->catchErrorType != nullptr;
            genContext->out.write((const char *)&hasCatchType,sizeof(hasCatchType));
            if(hasCatchType){
                RTID catchTypeId = makeOwnedRTID(std::string(secureDecl->catchErrorType->getName()));
                genContext->out << &catchTypeId;
            }
        }

        consumeStmt(guardedSpec.expr);
        write_ASTBlockStmt_to_context(secureDecl->catchBlock,genContext,this);
    }
    else if(stmt->type == FUNC_DECL){
        ASTFuncDecl *func_node = (ASTFuncDecl *)stmt;
        RTFuncTemplate funcTemplate;
        ASTIdentifier *func_id = func_node->funcId;
        ASTIdentifier_to_RTID(func_id,funcTemplate.name);
        funcTemplate.attributes = convertAttributes(func_node->attributes);
        funcTemplate.isLazy = func_node->isLazy;
        auto orderedParams = orderedParamPairs(func_node->params);
        for(auto & param_pair : orderedParams){
            RTID param_id;
            ASTIdentifier_to_RTID(param_pair.first,param_id);
            funcTemplate.argsTemplate.push_back(param_id);
        };
        emitRuntimeFunction(func_node->blockStmt,orderedParams,genContext,this,funcTemplate,true);
    }
    else if(stmt->type == CLASS_DECL){
        auto class_decl = (ASTClassDecl *)stmt;
        RTClass cls;
        ASTIdentifier_to_RTID(class_decl->id,cls.name);
        cls.attributes = convertAttributes(class_decl->attributes);
        if(class_decl->superClass){
            cls.hasSuperClass = true;
            cls.superClassName = makeOwnedRTID(std::string(class_decl->superClass->getName()));
        }
        std::string className = class_decl->id->val;
        struct FieldInitSpec {
            std::string fieldName;
            ASTExpr *expr = nullptr;
        };
        std::vector<FieldInitSpec> fieldInitializers;
        for(auto &fieldDecl : class_decl->fields){
            for(auto &spec : fieldDecl->specs){
                RTVar fieldVar;
                ASTIdentifier_to_RTID(spec.id,fieldVar.id);
                fieldVar.hasInitValue = (spec.expr != nullptr);
                fieldVar.attributes = convertAttributes(fieldDecl->attributes);
                cls.fields.push_back(std::move(fieldVar));
                if(spec.expr){
                    FieldInitSpec fieldInit;
                    fieldInit.fieldName = spec.id->val;
                    fieldInit.expr = spec.expr;
                    fieldInitializers.push_back(std::move(fieldInit));
                }
            }
        }
        for(auto &methodDecl : class_decl->methods){
            RTFuncTemplate methodTemplate;
            ASTIdentifier_to_RTID(methodDecl->funcId,methodTemplate.name);
            methodTemplate.attributes = convertAttributes(methodDecl->attributes);
            methodTemplate.isLazy = methodDecl->isLazy;
            auto orderedMethodParams = orderedParamPairs(methodDecl->params);
            for(auto &paramPair : orderedMethodParams){
                RTID paramId;
                ASTIdentifier_to_RTID(paramPair.first,paramId);
                methodTemplate.argsTemplate.push_back(paramId);
            }
            cls.methods.push_back(std::move(methodTemplate));
        }
        for(auto &ctorDecl : class_decl->constructors){
            RTFuncTemplate ctorTemplate;
            ctorTemplate.name = makeOwnedRTID(classCtorTemplateName(ctorDecl->params.size()));
            ctorTemplate.attributes = convertAttributes(ctorDecl->attributes);
            auto orderedCtorParams = orderedParamPairs(ctorDecl->params);
            for(auto &paramPair : orderedCtorParams){
                RTID paramId;
                ASTIdentifier_to_RTID(paramPair.first,paramId);
                ctorTemplate.argsTemplate.push_back(paramId);
            }
            cls.constructors.push_back(std::move(ctorTemplate));
        }
        if(!fieldInitializers.empty()){
            cls.hasFieldInitFunc = true;
            cls.fieldInitFuncName = makeOwnedRTID(mangleClassFieldInitName(className));
        }
        genContext->out << &cls;

        for(auto &methodDecl : class_decl->methods){
            RTFuncTemplate runtimeMethod;
            runtimeMethod.name = makeOwnedRTID(mangleClassMethodName(className,methodDecl->funcId->val));
            runtimeMethod.attributes = convertAttributes(methodDecl->attributes);
            runtimeMethod.isLazy = methodDecl->isLazy;
            auto orderedMethodParams = orderedParamPairs(methodDecl->params);
            for(auto &paramPair : orderedMethodParams){
                RTID paramId;
                ASTIdentifier_to_RTID(paramPair.first,paramId);
                runtimeMethod.argsTemplate.push_back(paramId);
            }
            emitRuntimeFunction(methodDecl->blockStmt,orderedMethodParams,genContext,this,runtimeMethod,false);
        }
        for(auto &ctorDecl : class_decl->constructors){
            RTFuncTemplate runtimeCtor;
            runtimeCtor.name = makeOwnedRTID(mangleClassCtorName(className,ctorDecl->params.size()));
            runtimeCtor.attributes = convertAttributes(ctorDecl->attributes);
            auto orderedCtorParams = orderedParamPairs(ctorDecl->params);
            for(auto &paramPair : orderedCtorParams){
                RTID paramId;
                ASTIdentifier_to_RTID(paramPair.first,paramId);
                runtimeCtor.argsTemplate.push_back(paramId);
            }
            emitRuntimeFunction(ctorDecl->blockStmt,orderedCtorParams,genContext,this,runtimeCtor,false);
        }
        if(!fieldInitializers.empty()){
            RTFuncTemplate runtimeFieldInit;
            runtimeFieldInit.name = makeOwnedRTID(mangleClassFieldInitName(className));
            std::ostringstream bodyBuffer(std::ios::out | std::ios::binary);
            auto tempOutputPath = genContext->outputPath;
            ModuleGenContext tempContext(genContext->name,bodyBuffer,tempOutputPath);
            tempContext.tableContext = genContext->tableContext;
            auto *savedContext = genContext;
            setContext(&tempContext);
            RTCode code = CODE_RTMEMBER_SET;
            for(auto &fieldInit : fieldInitializers){
                ensureInlineExprTemplates(fieldInit.expr);
                tempContext.out.write((char *)&code,sizeof(RTCode));
                writeNamedVarRef(tempContext.out,"self");
                RTID memberId = makeOwnedRTID(fieldInit.fieldName);
                tempContext.out << &memberId;
                consumeStmt(fieldInit.expr);
            }
            setContext(savedContext);
            auto bodyBytes = bodyBuffer.str();
            runtimeFieldInit.blockByteSize = bodyBytes.size();
            genContext->out << &runtimeFieldInit;
            code = CODE_RTFUNCBLOCK_BEGIN;
            genContext->out.write((char *)&code,sizeof(RTCode));
            if(!bodyBytes.empty()){
                genContext->out.write(bodyBytes.data(),(std::streamsize)bodyBytes.size());
            }
            code = CODE_RTFUNCBLOCK_END;
            genContext->out.write((char *)&code,sizeof(RTCode));
        }
    }
    else if(stmt->type == SCOPE_DECL){
        auto *scopeDecl = (ASTScopeDecl *)stmt;
        for(auto *innerStmt : scopeDecl->blockStmt->body){
            if(innerStmt->type & DECL){
                consumeDecl((ASTDecl *)innerStmt);
            }
            else {
                consumeStmt(innerStmt);
            }
        }
    }
    else if(stmt->type == COND_DECL){
        ASTConditionalDecl *cond_decl = (ASTConditionalDecl *)stmt;
        for(auto & cond : cond_decl->specs){
            if(!cond.isElse() && cond.expr){
                ensureInlineExprTemplates(cond.expr);
            }
        }
        RTCode code = CODE_CONDITIONAL;
        genContext->out.write((char *)&code,sizeof(RTCode));
        unsigned count = cond_decl->specs.size();
        genContext->out.write((char *)&count,sizeof(count));
        for(auto & cond : cond_decl->specs){
            RTCode spec_ty = cond.isElse()? COND_TYPE_ELSE : COND_TYPE_IF;
            genContext->out.write((char *)&spec_ty,sizeof(RTCode));
            if(!cond.isElse())
                consumeStmt(cond.expr);
            write_ASTBlockStmt_to_context(cond.blockStmt,genContext,this);
        };
        code = CODE_CONDITIONAL_END;
        genContext->out.write((char *)&code,sizeof(RTCode));
    }
    else if(stmt->type == FOR_DECL || stmt->type == WHILE_DECL){
        ASTExpr *loopExpr = nullptr;
        ASTBlockStmt *loopBlock = nullptr;
        if(stmt->type == FOR_DECL){
            auto *forDecl = (ASTForDecl *)stmt;
            loopExpr = forDecl->expr;
            loopBlock = forDecl->blockStmt;
        }
        else {
            auto *whileDecl = (ASTWhileDecl *)stmt;
            loopExpr = whileDecl->expr;
            loopBlock = whileDecl->blockStmt;
        }
        if(!loopExpr || !loopBlock){
            return;
        }
        ensureInlineExprTemplates(loopExpr);

        RTCode code = CODE_CONDITIONAL;
        genContext->out.write((char *)&code,sizeof(RTCode));
        unsigned count = 1;
        genContext->out.write((char *)&count,sizeof(count));
        RTCode spec_ty = COND_TYPE_LOOPIF;
        genContext->out.write((char *)&spec_ty,sizeof(RTCode));
        consumeStmt(loopExpr);
        write_ASTBlockStmt_to_context(loopBlock,genContext,this);
        code = CODE_CONDITIONAL_END;
        genContext->out.write((char *)&code,sizeof(RTCode));
    }
    else if(stmt->type == RETURN_DECL){
        ASTReturnDecl *return_decl = (ASTReturnDecl *)stmt;
        bool hasValue = return_decl->expr != nullptr;
        if(hasValue){
            ensureInlineExprTemplates(return_decl->expr);
        }
        RTCode code = CODE_RTRETURN;
        genContext->out.write((char *)&code,sizeof(RTCode));
        genContext->out.write((char *)&hasValue,sizeof(hasValue));
        if(hasValue)
            consumeStmt(return_decl->expr);

    };
}

static bool exprCanBeConvertedToRTInternalObject(ASTExpr *expr){
    if(!expr){
        return false;
    }
    if(expr->type & LITERAL){
        return true;
    }
    if(expr->type == ARRAY_EXPR){
        for(auto *elementExpr : expr->exprArrayData){
            if(!exprCanBeConvertedToRTInternalObject(elementExpr)){
                return false;
            }
        }
        return true;
    }
    return false;
}

bool stmtCanBeConvertedToRTInternalObject(ASTStmt *stmt){
    if(!stmt){
        return false;
    }
    if(stmt->type & LITERAL){
        return true;
    }
    if(stmt->type == ARRAY_EXPR){
        return exprCanBeConvertedToRTInternalObject((ASTExpr *)stmt);
    }
    return false;
}

static bool stmtIsExpressionLike(ASTStmt *stmt){
    if(!stmt){
        return false;
    }
    return (stmt->type & EXPR)
        || stmt->type == STR_LITERAL
        || stmt->type == NUM_LITERAL
        || stmt->type == BOOL_LITERAL
        || stmt->type == REGEX_LITERAL;
}

StarbytesObject CodeGen::exprToRTInternalObject(ASTExpr *expr){
    StarbytesObject ob;
    if (expr->type == ARRAY_EXPR){
        ob = StarbytesArrayNew();
        
        for(auto *elementExpr : expr->exprArrayData){
            auto obj = exprToRTInternalObject(elementExpr);
            if(!obj){
                continue;
            }
            StarbytesArrayPush(ob,obj);
        };
    }
    else if(expr->type == STR_LITERAL){
        ASTLiteralExpr *literal_expr = (ASTLiteralExpr *)expr;
        ob = StarbytesStrNewWithData(literal_expr->strValue->data());
    }
    else if(expr->type == BOOL_LITERAL){
        ASTLiteralExpr *literal_expr = (ASTLiteralExpr *)expr;
        ob = StarbytesBoolNew((StarbytesBoolVal)*literal_expr->boolValue);
    }
    else if(expr->type == NUM_LITERAL){
        ASTLiteralExpr *literal_expr = (ASTLiteralExpr *)expr;
        auto targetType = expr->runtimeCastTargetName.value_or("");
        if(literal_expr->floatValue.has_value()){
            if(targetType == DOUBLE_TYPE->getName().str()){
                ob = StarbytesNumNew(NumTypeDouble,*literal_expr->floatValue);
            }
            else {
                ob = StarbytesNumNew(NumTypeFloat,*literal_expr->floatValue);
            }
        }
        else {
            if(targetType == LONG_TYPE->getName().str()){
                ob = StarbytesNumNew(NumTypeLong,(int64_t)*literal_expr->intValue);
            }
            else if(targetType == DOUBLE_TYPE->getName().str()){
                ob = StarbytesNumNew(NumTypeDouble,(double)*literal_expr->intValue);
            }
            else if(targetType == FLOAT_TYPE->getName().str()){
                ob = StarbytesNumNew(NumTypeFloat,(float)*literal_expr->intValue);
            }
            else {
                ob = StarbytesNumNew(NumTypeInt,(int)*literal_expr->intValue);
            }
        }
    }
    return ob;
}


void CodeGen::consumeStmt(ASTStmt *stmt){
    if(!stmt){
        return;
    }
    if(genContext && genContext->bytecodeVersion == RTBYTECODE_VERSION_V2 && localSlotStack.empty()){
        bufferedTopLevelStatements.push_back(stmt);
        return;
    }
    bool isExprLike = stmtIsExpressionLike(stmt);
    ASTExpr *expr = isExprLike ? (ASTExpr *)stmt : nullptr;
    if((stmt->type & EXPR) && exprEmitDepth == 0){
        ensureInlineExprTemplates(expr);
    }
    struct ExprDepthGuard {
        size_t &depth;
        explicit ExprDepthGuard(size_t &depthRef):depth(depthRef){ ++depth; }
        ~ExprDepthGuard(){ --depth; }
    } depthGuard(exprEmitDepth);
    if(isExprLike && expr->runtimeCastTargetName.has_value()){
        bool explicitCastInvocation = false;
        if(stmt->type == IVKE_EXPR && expr->callee){
            explicitCastInvocation =
                ((expr->callee->type == ID_EXPR && expr->callee->id
                  && expr->callee->id->type == ASTIdentifier::Class)
                 || (expr->callee->type == MEMBER_EXPR
                     && expr->callee->rightExpr && expr->callee->rightExpr->id
                     && expr->callee->rightExpr->id->type == ASTIdentifier::Class));
        }
        if(!explicitCastInvocation){
            auto targetName = expr->runtimeCastTargetName;
            expr->runtimeCastTargetName.reset();
            RTCode code = CODE_RTCAST;
            genContext->out.write((const char *)&code,sizeof(RTCode));
            consumeStmt(stmt);
            RTID targetTypeId = makeOwnedRTID(targetName.value());
            genContext->out << &targetTypeId;
            expr->runtimeCastTargetName = targetName;
            return;
        }
    }
    if(stmt->type == REGEX_LITERAL){
        auto *literalExpr = (ASTLiteralExpr *)stmt;
        RTCode code = CODE_RTREGEX_LITERAL;
        genContext->out.write((const char *)&code,sizeof(RTCode));
        RTID patternId = makeOwnedRTID(literalExpr->regexPattern.value_or(""));
        RTID flagsId = makeOwnedRTID(literalExpr->regexFlags.value_or(""));
        genContext->out << &patternId;
        genContext->out << &flagsId;
        return;
    }
    if(stmt->type == ARRAY_EXPR && !stmtCanBeConvertedToRTInternalObject(stmt)){
        RTCode code = CODE_RTARRAY_LITERAL;
        genContext->out.write((const char *)&code,sizeof(RTCode));
        unsigned elementCount = expr->exprArrayData.size();
        genContext->out.write((const char *)&elementCount,sizeof(elementCount));
        for(auto *elementExpr : expr->exprArrayData){
            consumeStmt(elementExpr);
        }
        return;
    }
    if(stmt->type == DICT_EXPR){
        RTCode code = CODE_RTDICT_LITERAL;
        genContext->out.write((const char *)&code,sizeof(RTCode));
        unsigned pairCount = expr->dictExpr.size();
        genContext->out.write((const char *)&pairCount,sizeof(pairCount));
        for(auto &pair : expr->dictExpr){
            consumeStmt(pair.first);
            consumeStmt(pair.second);
        }
        return;
    }
    /// Literals
    if(stmtCanBeConvertedToRTInternalObject(expr)){
        auto obj = exprToRTInternalObject(expr);
        genContext->out << &obj;
        StarbytesObjectRelease(obj);
    }
    else if(stmt->type == INLINE_FUNC_EXPR){
        ensureInlineFunctionTemplate(expr,"expr");
        if(expr->inlineFuncRuntimeName.empty()){
            return;
        }
        RTCode code = CODE_RTFUNC_REF;
        genContext->out.write((const char *)&code,sizeof(RTCode));
        RTID id = makeOwnedRTID(expr->inlineFuncRuntimeName);
        genContext->out << &id;
    }
    else if(stmt->type == ID_EXPR){
        if(expr->id->type == ASTIdentifier::Var) {
            uint32_t localSlot = 0;
            if(currentLocalSlotForName(expr->id->val,localSlot)){
                FastTypeInfo localType;
                if(currentLocalSlotFastType(expr->id->val,localType) && isFastNumericKind(localType.scalarKind)){
                    writeTypedLocalSlotRef(genContext->out,localType.scalarKind,localSlot);
                }
                else {
                    writeLocalSlotRef(genContext->out,localSlot);
                }
            }
            else {
                RTCode code = CODE_RTVAR_REF;
                genContext->out.write((const char *)&code,sizeof(RTCode));
                RTID id;
                ASTIdentifier_to_RTID(expr->id,id);
                genContext->out << &id;
            }
        }
        else if(expr->id->type == ASTIdentifier::Function) {
            RTCode code = CODE_RTFUNC_REF;
            genContext->out.write((const char *)&code,sizeof(RTCode));
            RTID id;
            ASTIdentifier_to_RTID(expr->id,id);
            genContext->out << &id;
        }
    }
    else if(stmt->type == MEMBER_EXPR){
        if(expr->isScopeAccess){
            auto emittedName = emittedNameForScopeMember(genContext,expr);
            if(emittedName.empty()){
                return;
            }
            RTID memberId = makeOwnedRTID(emittedName);
            if(expr->rightExpr && expr->rightExpr->id && expr->rightExpr->id->type == ASTIdentifier::Function){
                RTCode code = CODE_RTFUNC_REF;
                genContext->out.write((const char *)&code,sizeof(RTCode));
                genContext->out << &memberId;
                return;
            }
            if(expr->rightExpr && expr->rightExpr->id && expr->rightExpr->id->type == ASTIdentifier::Var){
                RTCode code = CODE_RTVAR_REF;
                genContext->out.write((const char *)&code,sizeof(RTCode));
                genContext->out << &memberId;
                return;
            }
            return;
        }
        std::string memberName;
        if(!getMemberName(expr,memberName)){
            return;
        }
        uint32_t fieldSlot = 0;
        RTCode code = tryResolveDirectFieldSlot(expr,fieldSlot) ? CODE_RTMEMBER_GET_FIELD_SLOT : CODE_RTMEMBER_GET;
        genContext->out.write((const char *)&code,sizeof(RTCode));
        consumeStmt(expr->leftExpr);
        if(code == CODE_RTMEMBER_GET_FIELD_SLOT){
            genContext->out.write((const char *)&fieldSlot,sizeof(fieldSlot));
        }
        else {
            RTID memberId = makeOwnedRTID(memberName);
            genContext->out << &memberId;
        }
    }
    else if(stmt->type == INDEX_EXPR){
        auto baseType = inferFastType(expr->leftExpr);
        if(isFastNumericKind(baseType.arrayElementKind)){
            writeTypedIndexGetHeader(genContext->out,baseType.arrayElementKind);
        }
        else {
            RTCode code = CODE_RTINDEX_GET;
            genContext->out.write((const char *)&code,sizeof(RTCode));
        }
        consumeStmt(expr->leftExpr);
        consumeStmt(expr->rightExpr);
    }
    else if(stmt->type == UNARY_EXPR){
        auto op = expr->oprtr_str.value_or("");
        auto operandType = inferFastType(expr->leftExpr);
        if(op == "-" && isFastNumericKind(operandType.scalarKind)){
            writeTypedNegateHeader(genContext->out,operandType.scalarKind);
            consumeStmt(expr->leftExpr);
            return;
        }
        RTCode unaryCode = UNARY_OP_NOT;
        if(!unaryOpCodeFromSymbol(op,unaryCode)){
            return;
        }
        RTCode code = CODE_UNARY_OPERATOR;
        genContext->out.write((const char *)&code,sizeof(RTCode));
        genContext->out.write((const char *)&unaryCode,sizeof(unaryCode));
        consumeStmt(expr->leftExpr);
    }
    else if(stmt->type == BINARY_EXPR){
        auto op = expr->oprtr_str.value_or("");
        if(op == KW_IS){
            if(!expr->leftExpr || !expr->runtimeTypeCheckName.has_value()){
                return;
            }
            RTCode code = CODE_RTTYPECHECK;
            genContext->out.write((const char *)&code,sizeof(RTCode));
            consumeStmt(expr->leftExpr);
            RTID typeId = makeOwnedRTID(expr->runtimeTypeCheckName.value());
            genContext->out << &typeId;
            return;
        }
        auto lhsType = inferFastType(expr->leftExpr);
        auto rhsType = inferFastType(expr->rightExpr);
        if(isFastNumericKind(lhsType.scalarKind) && isFastNumericKind(rhsType.scalarKind)){
            auto resultKind = promotedFastKind(lhsType.scalarKind,rhsType.scalarKind);
            RTTypedBinaryOp typedBinaryOp = RTTYPED_BINARY_ADD;
            if(typedBinaryOpFromSymbol(op,typedBinaryOp)){
                writeTypedBinaryHeader(genContext->out,resultKind,typedBinaryOp);
                consumeStmt(expr->leftExpr);
                consumeStmt(expr->rightExpr);
                return;
            }
            RTTypedCompareOp typedCompareOp = RTTYPED_COMPARE_EQ;
            if(typedCompareOpFromSymbol(op,typedCompareOp)){
                writeTypedCompareHeader(genContext->out,resultKind,typedCompareOp);
                consumeStmt(expr->leftExpr);
                consumeStmt(expr->rightExpr);
                return;
            }
        }
        RTCode binaryCode = BINARY_OP_PLUS;
        if(!binaryOpCodeFromSymbol(op,binaryCode)){
            return;
        }
        RTCode code = CODE_BINARY_OPERATOR;
        genContext->out.write((const char *)&code,sizeof(RTCode));
        genContext->out.write((const char *)&binaryCode,sizeof(binaryCode));
        consumeStmt(expr->leftExpr);
        consumeStmt(expr->rightExpr);
    }
    else if(stmt->type == TERNARY_EXPR){
        if(!expr->leftExpr || !expr->middleExpr || !expr->rightExpr){
            return;
        }
        RTCode code = CODE_RTTERNARY;
        genContext->out.write((const char *)&code,sizeof(RTCode));
        consumeStmt(expr->leftExpr);
        consumeStmt(expr->middleExpr);
        consumeStmt(expr->rightExpr);
    }
    else if(stmt->type == ASSIGN_EXPR){
        if(!expr->leftExpr || !expr->rightExpr){
            return;
        }
        auto assignOp = expr->oprtr_str.value_or("=");
        bool isCompound = assignOp != "=";
        RTCode compoundBinaryCode = BINARY_OP_PLUS;
        if(isCompound){
            std::string binaryOp;
            if(assignOp == "+="){
                binaryOp = "+";
            }
            else if(assignOp == "-="){
                binaryOp = "-";
            }
            else if(assignOp == "*="){
                binaryOp = "*";
            }
            else if(assignOp == "/="){
                binaryOp = "/";
            }
            else if(assignOp == "%="){
                binaryOp = "%";
            }
            else if(assignOp == "&="){
                binaryOp = "&";
            }
            else if(assignOp == "|="){
                binaryOp = "|";
            }
            else if(assignOp == "^="){
                binaryOp = "^";
            }
            else if(assignOp == "<<="){
                binaryOp = "<<";
            }
            else if(assignOp == ">>="){
                binaryOp = ">>";
            }
            else {
                return;
            }
            if(!binaryOpCodeFromSymbol(binaryOp,compoundBinaryCode)){
                return;
            }
        }

        auto emitAssignedValueExpr = [&](){
            if(!isCompound){
                consumeStmt(expr->rightExpr);
                return;
            }
            auto lhsType = inferFastType(expr->leftExpr);
            auto rhsType = inferFastType(expr->rightExpr);
            if(isFastNumericKind(lhsType.scalarKind) && isFastNumericKind(rhsType.scalarKind)){
                auto resultKind = promotedFastKind(lhsType.scalarKind,rhsType.scalarKind);
                RTTypedBinaryOp typedBinaryOp = RTTYPED_BINARY_ADD;
                std::string binarySymbol;
                if(compoundBinaryCode == BINARY_OP_PLUS){
                    binarySymbol = "+";
                }
                else if(compoundBinaryCode == BINARY_OP_MINUS){
                    binarySymbol = "-";
                }
                else if(compoundBinaryCode == BINARY_OP_MUL){
                    binarySymbol = "*";
                }
                else if(compoundBinaryCode == BINARY_OP_DIV){
                    binarySymbol = "/";
                }
                else if(compoundBinaryCode == BINARY_OP_MOD){
                    binarySymbol = "%";
                }
                if(!binarySymbol.empty() && typedBinaryOpFromSymbol(binarySymbol,typedBinaryOp)){
                    writeTypedBinaryHeader(genContext->out,resultKind,typedBinaryOp);
                    consumeStmt(expr->leftExpr);
                    consumeStmt(expr->rightExpr);
                    return;
                }
            }
            RTCode code = CODE_BINARY_OPERATOR;
            genContext->out.write((const char *)&code,sizeof(RTCode));
            genContext->out.write((const char *)&compoundBinaryCode,sizeof(compoundBinaryCode));
            consumeStmt(expr->leftExpr);
            consumeStmt(expr->rightExpr);
        };

        if(expr->leftExpr->type == ID_EXPR && expr->leftExpr->id && expr->leftExpr->id->type == ASTIdentifier::Var){
            uint32_t localSlot = 0;
            if(currentLocalSlotForName(expr->leftExpr->id->val,localSlot)){
                RTCode code = CODE_RTLOCAL_SET;
                genContext->out.write((const char *)&code,sizeof(RTCode));
                genContext->out.write((const char *)&localSlot,sizeof(localSlot));
            }
            else {
                RTCode code = CODE_RTVAR_SET;
                genContext->out.write((const char *)&code,sizeof(RTCode));
                RTID varId;
                ASTIdentifier_to_RTID(expr->leftExpr->id,varId);
                genContext->out << &varId;
            }
            emitAssignedValueExpr();
            return;
        }

        if(expr->leftExpr->type == MEMBER_EXPR){
            std::string memberName;
            if(!getMemberName(expr->leftExpr,memberName)){
                return;
            }
            uint32_t fieldSlot = 0;
            RTCode code = tryResolveDirectFieldSlot(expr->leftExpr,fieldSlot) ? CODE_RTMEMBER_SET_FIELD_SLOT : CODE_RTMEMBER_SET;
            genContext->out.write((const char *)&code,sizeof(RTCode));
            consumeStmt(expr->leftExpr->leftExpr);
            if(code == CODE_RTMEMBER_SET_FIELD_SLOT){
                genContext->out.write((const char *)&fieldSlot,sizeof(fieldSlot));
            }
            else {
                RTID memberId = makeOwnedRTID(memberName);
                genContext->out << &memberId;
            }
            emitAssignedValueExpr();
            return;
        }

        if(expr->leftExpr->type == INDEX_EXPR){
            auto baseType = inferFastType(expr->leftExpr->leftExpr);
            if(isFastNumericKind(baseType.arrayElementKind)){
                writeTypedIndexSetHeader(genContext->out,baseType.arrayElementKind);
            }
            else {
                RTCode code = CODE_RTINDEX_SET;
                genContext->out.write((const char *)&code,sizeof(RTCode));
            }
            consumeStmt(expr->leftExpr->leftExpr);
            consumeStmt(expr->leftExpr->rightExpr);
            emitAssignedValueExpr();
            return;
        }
    }
    else if(stmt->type == IVKE_EXPR){
        if(expr->isConstructorCall){
            std::string className;
            if(!expr->callee){
                return;
            }
            if(expr->callee->type == ID_EXPR && expr->callee->id){
                className = expr->callee->id->val;
            }
            else if(expr->callee->type == MEMBER_EXPR && expr->callee->isScopeAccess){
                className = emittedNameForScopeMember(genContext,expr->callee);
            }
            if(className.empty()){
                return;
            }
            RTCode code = CODE_RTNEWOBJ;
            genContext->out.write((const char *)&code,sizeof(RTCode));
            RTID classId = makeOwnedRTID(className);
            genContext->out << &classId;
            unsigned argCount = expr->exprArrayData.size();
            genContext->out.write((const char *)&argCount,sizeof(argCount));
            for(auto *arg : expr->exprArrayData){
                consumeStmt(arg);
            }
            return;
        }

        bool calleeIsTypeLike = expr->callee
            && ((expr->callee->type == ID_EXPR && expr->callee->id
                 && expr->callee->id->type == ASTIdentifier::Class)
                || (expr->callee->type == MEMBER_EXPR
                    && expr->callee->rightExpr && expr->callee->rightExpr->id
                    && expr->callee->rightExpr->id->type == ASTIdentifier::Class));
        if(expr->runtimeCastTargetName.has_value() && calleeIsTypeLike){
            if(expr->exprArrayData.size() != 1){
                return;
            }
            RTCode code = CODE_RTCAST;
            genContext->out.write((const char *)&code,sizeof(RTCode));
            consumeStmt(expr->exprArrayData.front());
            RTID targetTypeId = makeOwnedRTID(expr->runtimeCastTargetName.value());
            genContext->out << &targetTypeId;
            return;
        }

        if(expr->callee && expr->callee->type == MEMBER_EXPR && !expr->callee->isScopeAccess){
            std::string memberName;
            if(!getMemberName(expr->callee,memberName)){
                return;
            }
            RTBuiltinMemberId builtinMemberId = RTBUILTIN_MEMBER_INVALID;
            RTCode code = rtBuiltinMemberIdForName(memberName,builtinMemberId)
                ? CODE_RTCALL_BUILTIN_MEMBER
                : CODE_RTMEMBER_IVK;
            genContext->out.write((const char *)&code,sizeof(RTCode));
            consumeStmt(expr->callee->leftExpr);
            if(code == CODE_RTCALL_BUILTIN_MEMBER){
                genContext->out.write((const char *)&builtinMemberId,sizeof(builtinMemberId));
            }
            else {
                RTID memberId = makeOwnedRTID(memberName);
                genContext->out << &memberId;
            }
            unsigned argCount = expr->exprArrayData.size();
            genContext->out.write((const char *)&argCount,sizeof(argCount));
            for(auto *arg : expr->exprArrayData){
                consumeStmt(arg);
            }
            return;
        }

        auto calleeName = [&]() -> std::string {
            if(!expr->callee){
                return {};
            }
            if(expr->callee->type == ID_EXPR && expr->callee->id){
                return expr->callee->id->val;
            }
            if(expr->callee->type == MEMBER_EXPR){
                std::string memberName;
                if(getMemberName(expr->callee,memberName)){
                    return memberName;
                }
            }
            return {};
        }();
        bool canUseSqrtIntrinsic = false;
        if(calleeName == "sqrt" && expr->exprArrayData.size() == 1){
            if(expr->callee->type == ID_EXPR){
                canUseSqrtIntrinsic = !(genContext && genContext->tableContext
                                        && genContext->tableContext->findEntryByEmittedNoDiag("sqrt"));
            }
            else if(expr->callee->type == MEMBER_EXPR && expr->callee->isScopeAccess){
                canUseSqrtIntrinsic = true;
            }
        }
        if(canUseSqrtIntrinsic){
            auto argType = inferFastType(expr->exprArrayData.front());
            if(isFastNumericKind(argType.scalarKind)){
                writeTypedIntrinsicHeader(genContext->out,RTTYPED_NUM_DOUBLE,RTTYPED_INTRINSIC_SQRT);
                consumeStmt(expr->exprArrayData.front());
                return;
            }
        }

        auto directCalleeName = directCalleeRuntimeName(genContext,expr->callee);
        RTCode code = directCalleeName.empty() ? CODE_RTIVKFUNC : CODE_RTCALL_DIRECT;
        genContext->out.write((const char *)&code,sizeof(RTCode));
        if(directCalleeName.empty()){
            consumeStmt(expr->callee);
        }
        else {
            RTID funcId = makeOwnedRTID(directCalleeName);
            genContext->out << &funcId;
        }
        unsigned argCount = expr->exprArrayData.size();
        genContext->out.write((const char *)&argCount,sizeof(argCount));
        for(auto *arg : expr->exprArrayData){
            consumeStmt(arg);
        }
    }
    else {
        
    };
}


}
