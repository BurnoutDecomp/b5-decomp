// ============================================================================
// CgsEffectBase.cpp -- CgsSound::Logic effect-base runtime bodies.
//
// Bodied from BURNOUT_X360_ARTIST.XEX:
//   EffectBase::Attach()              @ 0x826A1138
//   EffectBase::Prepare(State*)       @ 0x8268CEC8
//   EffectBase::GetMixerOutputValue() @ 0x82680720
//   EffectBase::SetDMixIOPtr()        @ 0x826808D8
//   EffectControl::GetTypeName()      @ 0x8268CEB8
//   EffectObject::GetTypeName()       @ 0x8268CE98
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
#include "GameShared/GameClasses/Core/CgsAssert.h"
#include "GameShared/GameClasses/Sound/Logic/CgsState.h"        // the REAL State (mpStateManager @ X360 +0x24)
#include "GameShared/GameClasses/Sound/Logic/CgsStateManager.h" // the REAL StateManager (GetLogicModule / +0x2C)

namespace CgsSound
{
namespace Logic
{

static ClassTypeInfo<EffectControl>* gapEffectControlTypeInfo[EffectControl::KU_SIZEOF_CLASS_ARRAY] = { nullptr };
static ClassTypeInfo<EffectObject>*  gapEffectObjectTypeInfo[EffectObject::KU_SIZEOF_CLASS_ARRAY] = { nullptr };

// ---------------------------------------------------------------------------
// AddToClassTypeInfoArray scan helper.
//
// The X360 pair @ 0x8268DD48 (EffectControl, array dword_82FFBB10) and
// 0x8268DE28 (EffectObject, array dword_82FFBA10) are the same routine over a
// per-class static array of KU_SIZEOF_CLASS_ARRAY (64) descriptor slots:
//   scan from slot 0 for the first NULL slot (index counter held 16-bit);
//   if no NULL slot is found within the 0x40 cap and the counter is still < 0x100,
//   fall through WITHOUT storing (the array is full but under the hard limit);
//   only past 0x100 does it fire the "Too Many Class registations" assert
//   (CgsEffectBase.h:363). On finding a NULL slot it stores the descriptor there.
// Reproduced generically by member/array NAME; no raw-offset cast.
// ---------------------------------------------------------------------------
template <typename T>
static ClassTypeInfo<T>* RegisterClassTypeInfo(ClassTypeInfo<T>** apArray,
                                               ClassTypeInfo<T>* apTypeInfo)
{
    u32 lu32Index = 0;

    // Scan for the first empty slot, capped at the class array size (0x40).
    for (lu32Index = 0; lu32Index < 0x40u; ++lu32Index)
    {
        if (apArray[lu32Index] == nullptr)
        {
            apArray[lu32Index] = apTypeInfo;
            return apTypeInfo;
        }
    }

    // No empty slot within the cap. The X360 only asserts once the (16-bit) counter
    // reaches 0x100; between 0x40 and 0x100 it silently does nothing. Since the loop
    // above stops at the 0x40 cap, the counter here is 0x40 (< 0x100) so no store and
    // no assert -- matching the fall-through path.
    CGS_ASSERT(lu32Index < 0x100u, "Too Many Class registations. Increase KU_SIZEOF_CLASS_ARRAY");
    return apTypeInfo;
}

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
//   mpLogicModule = *(*(apState + 0x24) + 0x2C);
//   return 1;
// (asm: stw r4,8 ; lwz r10,0x24(r4) ; lwz r10,0x2C(r10) ; stw r10,0x28 ; li r3,1)
// The +0x24 -> +0x2C chain is State::mpStateManager -> StateManager::mpLogicModule
// (both real DWARF members) -- by name, 2026-08-25 wave 4; the former invented
// State::Owner intermediate is retired.
// ---------------------------------------------------------------------------
bool EffectBase::Prepare(State* apState)
{
    mpState       = apState;                                    // stw r4, 8(this)
    mpLogicModule = apState->mpStateManager->GetLogicModule();  // 0x24 then 0x2C, by name
    return true;                                                // li r3, 1
}

s32 EffectBase::GetStateId() const
{
    return mpState ? mpState->miStateInstType : -1;
}

s32 EffectBase::GetInstanceId() const
{
    return mpState ? mpState->miInstNum : -1;
}

// ---------------------------------------------------------------------------
// EffectBase::GetMixerOutputValue(slot, preset)  @ 0x82680720
//   if (!mpDynamicMixIo) return 0.0f;
//   return (f32)(s32)mpDynamicMixIo->GetDMixOutput(slot, preset);
// (asm: lwz r3,0x30(this); cmplwi; beq -> lfs f1, flt_82001CC0 [== 0.0]; else
//  bl GetDMixOutput; extsw r11,r3; std; lfd; fcfid; frsp f1 -- signed-int -> f32.)
// ---------------------------------------------------------------------------
f32 EffectBase::GetMixerOutputValue(int aiSlot, int aiPreset)
{
    if (mpDynamicMixIo == nullptr) // lwz r3,0x30(this); cmplwi cr6,r3,0; beq
    {
        return 0.0f;               // lfs f1, flt_82001CC0
    }

    // extsw r11,r3 -> the GetDMixOutput result is a SIGNED 32-bit value; fcfid/frsp
    // convert it to single precision.
    const s32 liOutput = mpDynamicMixIo->GetDMixOutput(aiSlot, aiPreset);
    return static_cast<f32>(liOutput);
}

// ---------------------------------------------------------------------------
// EffectBase::SetDMixIOPtr(apDmixIO)  @ 0x826808D8
//   if (!apDmixIO) <assert "lpDmixIO">;  mpDynamicMixIo = apDmixIO;
// (asm: the non-null guard (cmplwi cr6,r31,0; bne skip) fires the assert; the store
//  stw r31,0x30(this) [mpDynamicMixIo] runs unconditionally afterwards.)
// ---------------------------------------------------------------------------
void EffectBase::SetDMixIOPtr(Nicotine::DMixIO* apDmixIO)
{
    CGS_ASSERT(apDmixIO != nullptr, "lpDmixIO");
    mpDynamicMixIo = apDmixIO; // stw r31,0x30(this)
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

// EffectControl::AddToClassTypeInfoArray @ 0x8268DD48 (array dword_82FFBB10).
ClassTypeInfo<EffectControl>* EffectControl::AddToClassTypeInfoArray(ClassTypeInfo<EffectControl>* apTypeInfo)
{
    return RegisterClassTypeInfo<EffectControl>(gapEffectControlTypeInfo, apTypeInfo);
}

ClassTypeInfo<EffectControl>* EffectControl::GetRegisteredTypeInfo(u32 auIndex)
{
    return auIndex < KU_SIZEOF_CLASS_ARRAY ? gapEffectControlTypeInfo[auIndex] : nullptr;
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

// EffectObject::AddToClassTypeInfoArray @ 0x8268DE28 (array dword_82FFBA10 -- a
// separate static array from EffectControl's).
ClassTypeInfo<EffectObject>* EffectObject::AddToClassTypeInfoArray(ClassTypeInfo<EffectObject>* apTypeInfo)
{
    return RegisterClassTypeInfo<EffectObject>(gapEffectObjectTypeInfo, apTypeInfo);
}

ClassTypeInfo<EffectObject>* EffectObject::GetRegisteredTypeInfo(u32 auIndex)
{
    return auIndex < KU_SIZEOF_CLASS_ARRAY ? gapEffectObjectTypeInfo[auIndex] : nullptr;
}

} // namespace Logic
} // namespace CgsSound
