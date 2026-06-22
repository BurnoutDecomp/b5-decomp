// ============================================================================
// CgsEffectBase.cpp -- CgsSound::Logic effect-base runtime bodies.
//
// Bodied from BURNOUT_X360_ARTIST.XEX:
//   EffectBase::Attach()             @ 0x826A1138
//   EffectBase::Prepare(State*)      @ 0x8268CEC8
//   EffectControl::GetTypeName()     @ 0x8268CEB8
//   EffectObject::GetTypeName()      @ 0x8268CE98
//
// The GetTypeName() pair return interned type-name string literals (the X360 loads
// them straight from rodata: off_82F2FA74 == "EffectControl", off_82F2FA64 ==
// "EffectObject", per the disassembly's own string annotations). The canonical
// engine routes these through each class's static ClassTypeInfo<T>::typeName; that
// static descriptor's registration (sTypeInfo + ClassTypeInfoArray) lives with the
// per-class RTTI-registration TU and is DEFERRED. Here GetStaticTypeInfo() returns
// a function-local descriptor seeded with the same recovered type name, so the
// observable return (the type-name string) matches byte-for-byte.
// ============================================================================

#include "GameShared/GameClasses/Sound/Logic/CgsEffectBase.h"

namespace CgsSound
{
namespace Logic
{

// ---------------------------------------------------------------------------
// EffectBase::Attach()  @ 0x826A1138
//   v2 = mu16AttachCount; meDetachState = 0; mu16AttachCount = v2 + 1; return 1;
// (asm: lhz r10,0xE ; stw 0,0x24 ; sth r10+1,0xE ; li r3,1)
// ---------------------------------------------------------------------------
bool EffectBase::Attach()
{
    meDetachState = E_DETACH_STATE_NONE; // stw 0, 0x24(this)
    ++mu16AttachCount;                   // lhz/addi/sth 0xE(this)
    return true;                         // li r3, 1
}

// ---------------------------------------------------------------------------
// EffectBase::Prepare(State* apState)  @ 0x8268CEC8
//   mpState = apState;
//   mpLogicModule = *(*(apState + 0x24) + 0x2C);  // owner->mpLogicModule
//   return 1;
// (asm: stw r4,8 ; lwz r10,0x24(r4) ; lwz r10,0x2C(r10) ; stw r10,0x28 ; li r3,1)
// ---------------------------------------------------------------------------
bool EffectBase::Prepare(State* apState)
{
    mpState       = apState;                          // stw r4, 8(this)
    mpLogicModule = apState->GetOwner()->mpLogicModule; // 0x24 then 0x2C deref
    return true;                                      // li r3, 1
}

// ---------------------------------------------------------------------------
// EffectControl::GetTypeName()  @ 0x8268CEB8  -> "EffectControl"
// ---------------------------------------------------------------------------
ClassTypeInfo<EffectControl>* EffectControl::GetStaticTypeInfo()
{
    static ClassTypeInfo<EffectControl> sTypeInfo(0, "EffectControl", nullptr, nullptr);
    return &sTypeInfo;
}

const char* EffectControl::GetTypeName() const
{
    return GetStaticTypeInfo()->typeName;
}

// ---------------------------------------------------------------------------
// EffectObject::GetTypeName()  @ 0x8268CE98  -> "EffectObject"
// ---------------------------------------------------------------------------
ClassTypeInfo<EffectObject>* EffectObject::GetStaticTypeInfo()
{
    static ClassTypeInfo<EffectObject> sTypeInfo(0, "EffectObject", nullptr, nullptr);
    return &sTypeInfo;
}

const char* EffectObject::GetTypeName() const
{
    return GetStaticTypeInfo()->typeName;
}

} // namespace Logic
} // namespace CgsSound
