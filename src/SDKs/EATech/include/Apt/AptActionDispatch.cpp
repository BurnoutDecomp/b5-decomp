// ===========================================================================
// EATech Apt -- AptActionInterpreter::sGlobalTable, the ActionScript opcode ->
// handler dispatch table that runStream indexes by bytecode byte.
//
// The opcode->handler MAP was EXTRACTED from the X360 ARTIST decrypted XEX: the
// static table lives at vaddr 0x82F73068 (the dispatch in runStream @0x82ADD440
// does `r28 = &off_82F73068; r11 = table[opcode]; bctrl`). Read from
// IDA Files/BURNOUT_X360_ARTIST_Decrypted_Uncompressed.xex at
//   file_off = 0x3000 + (vaddr - 0x82000000)
// as big-endian 4-byte function pointers, then each entry resolved to its handler
// via the named X360 exports. Valid range: opcodes 0x00..0xB8 (data past 0xB8 is
// the following StringPool constants, not table entries). 0x82AD5078 = the engine's
// shared no-op STUB (used for genuinely-unhandled opcodes).
//
// FULL EXTRACTED MAP (opcode = handler; (*) = handler reconstructed + wired below,
// the rest are the no-op stub until built):
//   0x00 Return            0x04 NextFrame         0x05 PrevFrame
//   0x06 Play              0x07 Stop              0x08/09 STUB
//   0x0A Add(*)            0x0B Subtract(*)       0x0C Multiply(*)
//   0x0D Divide(*)         0x0E Equals(*)         0x0F LessThan(*)
//   0x10 And(*)            0x11 Or(*)             0x12 Not(*)
//   0x13 StringEquals      0x14 StringLength      0x15 SubString
//   0x17 Pop(*)            0x18 ToInteger(*)      0x1C GetVariable
//   0x1D SetVariable       0x20 SetTarget2        0x21 StringAdd
//   0x22 GetProperty       0x23 SetProperty       0x24 CloneSprite
//   0x25 RemoveSprite      0x26 Trace             0x27 StartDragMovie
//   0x28 StopDragMovie     0x2A Throw             0x2B CastOp
//   0x2C ImplementsOp      0x30 Random            0x33 AsciiToChar
//   0x34 GetTimer          0x3A Delete            0x3B Delete2
//   0x3C DefineLocal       0x3D CallFunction      0x3E Return
//   0x3F Modulo(*)         0x40 NewObject         0x41 DefineLocal2
//   0x42 InitArray         0x43 InitObject        0x44 TypeOf
//   0x45 TargetPath        0x46 Enumerate         0x47 Add2
//   0x48 LessThan2         0x49 Equals2           0x4A ToNumber
//   0x4B ToString          0x4C PushDuplicate(*)  0x4D StackSwap(*)
//   0x4E GetMember         0x4F SetMember         0x50 Increment(*)
//   0x51 Decrement(*)      0x52 CallMethod        0x53 NewMethod
//   0x55 Enumerate         0x56 PushThis          0x58 PushGlobal
//   0x59 Push0(*)          0x5A Push1             0x5B CallFuncAndPop
//   0x5C CallFuncSetVar    0x5D CallMethodPop     0x5E CallMethodSetVar
//   0x60 BitAnd(*)         0x61 BitOr(*)          0x62 BitXor(*)
//   0x63 BitLShift(*)      0x64 BitRShift(*)      0x67 Greater(*)
//   0x69 Extends           0x70 PushThisVariable  0x71 PushGlobalVariable
//   0x72 PushZeroSetVar    0x73 PushTrue(*)       0x74 PushFalse(*)
//   0x75 PushUndefined(*)  0x76 PushNULL(*)       0x77 TraceStart
//   0x81 GotoFrame         0x83 GetUrl            0x87 StoreRegister
//   0x88 DefineDictionary  0x8B SetTarget         0x8C GotoLabel
//   0x8E DefineFunction2   0x8F Try               0x96 Push
//   0x99 BranchAlways(*)   0x9A GetUrl2           0x9B DefineFunction
//   0x9D BranchIfTrue(*)   0x9E CallFrame         0x9F GotoFrame2
//   0xA2 PushStringDictByte 0xA3 PushStringDictWord
//   0xA4 PushStringGetVar  0xA5 PushStringGetMember
//   0xA6 PushStringSetVar  0xA7 PushStringSetMember
//   0xAE StringDictByteGetVar  0xAF StringDictByteGetMember
//   0xB0 DictCallFuncPop   0xB1 DictCallFuncSetVar 0xB2 DictCallMethodPop
//   0xB3 DictCallMethodSetVar  0xB4 PushFloat     0xB5 PushByte
//   0xB6 PushWord          0xB7 PushDWord         0xB8 BranchIfFalse(*)
// (0x94 With, 0x9C..., and a few others map to handlers not yet placed.)
//
// EA SDK identifiers kept verbatim (CXX_NAMING_CONVENTIONS external-API exception).
// ===========================================================================

#include "SDKs/EATech/include/Apt/AptActionInterpreter.h"

// The table itself (zero-init; filled by InitDispatchTable at startup). The console
// ships it as static data; building it at startup yields the identical map.
AptActionInterpreter::AptActionHandler AptActionInterpreter::sGlobalTable[256] = { 0 };

// The shared no-op handler for unhandled/unused opcodes (the engine's own STUB
// at X360 0x82AD5078). Leaves the stack untouched.
void AptActionInterpreter::_FunctionAptActionStub(AptActionInterpreter*, LocalContextT*)
{
}

// Fill sGlobalTable from the extracted opcode->handler map. Every slot defaults to
// the stub; the reconstructed handlers are wired at their real opcodes. As more
// handlers are built they are added here.  FLAG: called from AptInit (the console's
// equivalent is the static initialiser of the data table).
void AptActionInterpreter::InitDispatchTable()
{
    for (int i = 0; i < 256; ++i)
        sGlobalTable[i] = &_FunctionAptActionStub;

    // -- arithmetic --
    sGlobalTable[0x0A] = &_FunctionAptActionAdd;
    sGlobalTable[0x0B] = &_FunctionAptActionSubtract;
    sGlobalTable[0x0C] = &_FunctionAptActionMultiply;
    sGlobalTable[0x0D] = &_FunctionAptActionDivide;
    sGlobalTable[0x3F] = &_FunctionAptActionModulo;

    // -- comparison --
    sGlobalTable[0x0E] = &_FunctionAptActionEquals;
    sGlobalTable[0x0F] = &_FunctionAptActionLessThan;
    sGlobalTable[0x67] = &_FunctionAptActionGreater;

    // -- logic --
    sGlobalTable[0x10] = &_FunctionAptActionAnd;
    sGlobalTable[0x11] = &_FunctionAptActionOr;
    sGlobalTable[0x12] = &_FunctionAptActionNot;

    // -- bitwise --
    sGlobalTable[0x60] = &_FunctionAptActionBitAnd;
    sGlobalTable[0x61] = &_FunctionAptActionBitOr;
    sGlobalTable[0x62] = &_FunctionAptActionBitXor;
    sGlobalTable[0x63] = &_FunctionAptActionBitLShift;
    sGlobalTable[0x64] = &_FunctionAptActionBitRShift;

    // -- unary numeric --
    sGlobalTable[0x50] = &_FunctionAptActionIncrement;
    sGlobalTable[0x51] = &_FunctionAptActionDecrement;
    sGlobalTable[0x18] = &_FunctionAptActionToInteger;

    // -- branch --
    sGlobalTable[0x99] = &_FunctionAptActionBranchAlways;
    sGlobalTable[0x9D] = &_FunctionAptActionBranchIfTrue;
    sGlobalTable[0xB8] = &_FunctionAptActionBranchIfFalse;

    // -- stack / constant push --
    sGlobalTable[0x17] = &_FunctionAptActionPop;
    sGlobalTable[0x4C] = &_FunctionAptActionPushDuplicate;
    sGlobalTable[0x4D] = &_FunctionAptActionStackSwap;
    sGlobalTable[0x59] = &_FunctionAptActionPush0;
    sGlobalTable[0x73] = &_FunctionAptActionPushTrue;
    sGlobalTable[0x74] = &_FunctionAptActionPushFalse;
    sGlobalTable[0x75] = &_FunctionAptActionPushUndefined;
    sGlobalTable[0x76] = &_FunctionAptActionPushNULL;   // X360 folds NULL/Undefined to one fn

    // -- immediate pushes (inline operands) --
    sGlobalTable[0x5A] = &_FunctionAptActionPush1;
    sGlobalTable[0xB4] = &_FunctionAptActionPushFloat;
    sGlobalTable[0xB5] = &_FunctionAptActionPushByte;
    sGlobalTable[0xB6] = &_FunctionAptActionPushWord;
    sGlobalTable[0xB7] = &_FunctionAptActionPushDWord;

    // -- variable access (resolve a name against the run scope/target) --
    sGlobalTable[0x1C] = &_FunctionAptActionGetVariable;
    sGlobalTable[0x1D] = &_FunctionAptActionSetVariable;

    // -- member access (object[key] get/set) --
    sGlobalTable[0x4E] = &_FunctionAptActionGetMember;
    sGlobalTable[0x4F] = &_FunctionAptActionSetMember;

    // -- special value / register / control --
    sGlobalTable[0x34] = &_FunctionAptActionGetTimer;
    sGlobalTable[0x3E] = &_FunctionAptActionReturn;
    sGlobalTable[0x56] = &_FunctionAptActionPushThis;
    sGlobalTable[0x58] = &_FunctionAptActionPushGlobal;
    sGlobalTable[0x70] = &_FunctionAptActionPushThisVariable;
    sGlobalTable[0x71] = &_FunctionAptActionPushGlobalVariable;
    sGlobalTable[0x87] = &_FunctionAptActionStoreRegister;

    // -- value output / control --
    sGlobalTable[0x26] = &_FunctionAptActionTrace;
    sGlobalTable[0x2A] = &_FunctionAptActionThrow;
    sGlobalTable[0x4B] = &_FunctionAptActionToString;

    // -- delete --
    sGlobalTable[0x3B] = &_FunctionAptActionDelete2;

    // -- value / string / array utility --
    sGlobalTable[0x14] = &_FunctionAptActionStringLength;
    sGlobalTable[0x21] = &_FunctionAptActionStringAdd;
    sGlobalTable[0x30] = &_FunctionAptActionRandom;
    sGlobalTable[0x42] = &_FunctionAptActionInitArray;
    sGlobalTable[0x72] = &_FunctionAptActionPushZeroSetVar;

    // -- ECMA value ops --
    sGlobalTable[0x13] = &_FunctionAptActionStringEquals;
    sGlobalTable[0x47] = &_FunctionAptActionAdd2;

    // -- class / prototype ops --
    sGlobalTable[0x69] = &_FunctionAptActionExtends;
    sGlobalTable[0x2C] = &_FunctionAptActionImplementsOp;

    // -- string conversion --
    sGlobalTable[0x15] = &_FunctionAptActionSubString;
    sGlobalTable[0x4A] = &_FunctionAptActionToNumber;

    // -- local-variable definition --
    sGlobalTable[0x3C] = &_FunctionAptActionDefineLocal;
    sGlobalTable[0x41] = &_FunctionAptActionDefineLocal2;

    // =======================================================================
    // WIRED 2026-07-01: the 47 handlers homed by the later interpreter waves
    // (call/member/variable/timeline/proto/dict families) were never added to
    // this table -- every one below dispatched to the no-op stub even though
    // its faithful body existed. Wired per the extracted X360 map (0x82F73068).
    // Still on the no-op stub (genuinely unbuilt): 0x94 With (PS3 body exists;
    // the last one).
    // =======================================================================

    // -- run control / timeline --
    sGlobalTable[0x00] = &_FunctionAptActionReturn;
    sGlobalTable[0x04] = &_FunctionAptActionNextFrame;
    sGlobalTable[0x05] = &_FunctionAptActionPrevFrame;
    sGlobalTable[0x06] = &_FunctionAptActionPlay;
    sGlobalTable[0x07] = &_FunctionAptActionStop;
    sGlobalTable[0x81] = &_FunctionAptActionGotoFrame;
    sGlobalTable[0x8C] = &_FunctionAptActionGotoLabel;
    sGlobalTable[0x9E] = &_FunctionAptActionCallFrame;
    sGlobalTable[0x9F] = &_FunctionAptActionGotoFrame2;

    // -- target / sprite --
    sGlobalTable[0x20] = &_FunctionAptActionSetTarget2;
    sGlobalTable[0x24] = &_FunctionAptActionCloneSprite;
    sGlobalTable[0x25] = &_FunctionAptActionRemoveSprite;
    sGlobalTable[0x27] = &_FunctionAptActionStartDragMovie;
    sGlobalTable[0x28] = &_FunctionAptActionStopDragMovie;
    sGlobalTable[0x45] = &_FunctionAptActionTargetPath;
    sGlobalTable[0x8B] = &_FunctionAptActionSetTarget;

    // -- property / member / type --
    sGlobalTable[0x22] = &_FunctionAptActionGetProperty;
    sGlobalTable[0x23] = &_FunctionAptActionSetProperty;
    sGlobalTable[0x2B] = &_FunctionAptActionCastOp;
    sGlobalTable[0x3A] = &_FunctionAptActionDelete;
    sGlobalTable[0x48] = &_FunctionAptActionLessThan2;
    sGlobalTable[0x44] = &_FunctionAptActionTypeOf;
    sGlobalTable[0x46] = &_FunctionAptActionEnumerate;
    sGlobalTable[0x49] = &_FunctionAptActionEquals2;
    sGlobalTable[0x55] = &_FunctionAptActionEnumerate2;

    // -- calls / construction --
    sGlobalTable[0x3D] = &_FunctionAptActionCallFunction;
    sGlobalTable[0x40] = &_FunctionAptActionNewObject;
    sGlobalTable[0x43] = &_FunctionAptActionInitObject;
    sGlobalTable[0x52] = &_FunctionAptActionCallMethod;
    sGlobalTable[0x53] = &_FunctionAptActionNewMethod;
    sGlobalTable[0x5B] = &_FunctionAptActionCallFuncAndPop;
    sGlobalTable[0x5C] = &_FunctionAptActionCallFuncSetVar;
    sGlobalTable[0x5D] = &_FunctionAptActionCallMethodPop;
    sGlobalTable[0x5E] = &_FunctionAptActionCallMethodSetVar;

    // -- functions / exception / misc --
    sGlobalTable[0x33] = &_FunctionAptActionAsciiToChar;
    sGlobalTable[0x77] = &_FunctionAptActionTraceStart;
    sGlobalTable[0x83] = &_FunctionAptActionGetUrl;
    sGlobalTable[0x88] = &_FunctionAptActionDefineDictionary;
    sGlobalTable[0x8E] = &_FunctionAptActionDefineFunction2;
    sGlobalTable[0x8F] = &_FunctionAptActionTry;
    sGlobalTable[0x96] = &_FunctionAptActionPush;
    sGlobalTable[0x9A] = &_FunctionAptActionGetUrl2;
    sGlobalTable[0x9B] = &_FunctionAptActionDefineFunction;

    // -- string-dictionary push / fused forms --
    sGlobalTable[0xA2] = &_FunctionAptActionPushStringDictByte;
    sGlobalTable[0xA3] = &_FunctionAptActionPushStringDictWord;
    sGlobalTable[0xA4] = &_FunctionAptActionPushStringGetVar;
    sGlobalTable[0xA5] = &_FunctionAptActionPushStringGetMember;
    sGlobalTable[0xA6] = &_FunctionAptActionPushStringSetVar;
    sGlobalTable[0xA7] = &_FunctionAptActionPushStringSetMember;
    sGlobalTable[0xAE] = &_FunctionAptActionStringDictByteGetVar;
    sGlobalTable[0xAF] = &_FunctionAptActionStringDictByteGetMember;
    sGlobalTable[0xB0] = &_FunctionAptActionDictCallFuncPop;
    sGlobalTable[0xB1] = &_FunctionAptActionDictCallFuncSetVar;
    sGlobalTable[0xB2] = &_FunctionAptActionDictCallMethodPop;
    sGlobalTable[0xB3] = &_FunctionAptActionDictCallMethodSetVar;
}
