// ============================================================================
// CgsEffectBase.cpp -- CgsSound::Logic effect-base runtime bodies.
//
// Bodied from BURNOUT_X360_ARTIST.XEX:
//   EffectBase::Attach()              @ 0x826A1138
//   EffectBase::Prepare(State*)       @ 0x8268CEC8
//   EffectBase::GetMixerOutputValue() @ 0x82680720
//   EffectBase::GetRWACMixerOutputValue() @ 0x82680778
//   EffectBase::SetMixerInputValue()   @ 0x82680610
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
#include "GameShared/GameClasses/Core/CgsAssertMessageAt.h"
#include "GameShared/GameClasses/Development/CgsStrStream.h"
#include "GameShared/GameClasses/Numeric/CgsBranchlessOperations.h"
#include "GameShared/GameClasses/Sound/Logic/CgsState.h"        // the REAL State (mpStateManager @ X360 +0x24)
#include "GameShared/GameClasses/Sound/Logic/CgsStateManager.h" // the REAL StateManager (GetLogicModule / +0x2C)

namespace CgsSound
{
namespace Logic
{

static ClassTypeInfo<EffectControl>* gapEffectControlTypeInfo[EffectControl::KU_SIZEOF_CLASS_ARRAY] = { 0 };
static ClassTypeInfo<EffectObject>*  gapEffectObjectTypeInfo[EffectObject::KU_SIZEOF_CLASS_ARRAY] = { 0 };


// ---------------------------------------------------------------------------
// EffectBase::~EffectBase()  (DWARF CgsEffectBase.cpp:80; no ARTIST copy of its own)
// Every effect destructor expands it (EffectControl 0x826963D8, Cgs3dEffectControl
// 0x826C3E70, Car3DControl 0x826CF838, ...): the detach state finishes, the loaded-data
// flag drops and the attach state clears (stw 3,0x24 / stb 0,0x2D / stw 0,0x20).
// ---------------------------------------------------------------------------
EffectBase::~EffectBase()
{
    meDetachState   = E_DETACH_STATE_FINISHED;
    mbHasLoadedData = false;
    meAttachState   = E_ATTACH_STATE_NONE;
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

// ---------------------------------------------------------------------------
// EffectBase::SetDMixIOPtr(apDmixIO)  @ 0x826808D8
//   if (!apDmixIO) <assert "lpDmixIO">;  mpDynamicMixIo = apDmixIO;
// (asm: the non-null guard (cmplwi cr6,r31,0; bne skip) fires the assert; the store
//  stw r31,0x30(this) [mpDynamicMixIo] runs unconditionally afterwards.)
// ---------------------------------------------------------------------------
void EffectBase::SetDMixIOPtr(Nicotine::DMixIO* apDmixIO)
{
    CGS_ASSERT_AT(apDmixIO != 0,
        "lpDmixIO",
            "..\\..\\..\\GameShared\\GameClasses\\Sound/Logic/CgsEffectBase.h", 916 /* ARTIST 0x82680904 */);

    mpDynamicMixIo = apDmixIO; // stw r31,0x30(this)
}

// ---------------------------------------------------------------------------
// EffectControl::GetTypeName()  @ 0x8268CEB8  -> "EffectControl"
// ---------------------------------------------------------------------------
// ClassTypeInfo<EffectControl>::AddToClassTypeInfoArray @ 0x8268DD48 (array dword_82FFBB10): the State routine
// (0x8268DF08) over this class's 64-slot array; the streamed assert compares the u16
// index with the array's byte size (CgsEffectBase.h:363).
template<>
void ClassTypeInfo<EffectControl>::AddToClassTypeInfoArray()
{
    // DWARF CgsEffectBase.h:353 `uint16_t lCnt`, scanned by a for loop; the assert (:363) follows
    // it. ARTIST inlines the streamed literal after the loop (D12, asserts lane).
    u16 lCnt;
    for (lCnt = 0; lCnt < EffectControl::KU_SIZEOF_CLASS_ARRAY; ++lCnt)
    {
        if (!gapEffectControlTypeInfo[lCnt])
        {
            gapEffectControlTypeInfo[lCnt] = this;
            return;
        }
    }

    CGS_ASSERT_STREAM_AT(lCnt < sizeof(gapEffectControlTypeInfo),
        "Too Many Class registations. Increase KU_SIZEOF_CLASS_ARRAY",
        "..\..\..\GameShared\GameClasses\Sound/Logic/CgsEffectBase.h", 363);
}

// ARTIST 0x82F2FA70.
ClassTypeInfo<EffectControl> EffectControl::sTypeInfo(-1, "EffectControl", 0, 0);

ClassTypeInfo<EffectControl>* EffectControl::GetStaticTypeInfo()
{
    return &sTypeInfo;
}

const char* EffectControl::GetTypeName() const
{
    return sTypeInfo.typeName; // ARTIST 0x8268CEB8
}

// ---------------------------------------------------------------------------
// EffectBase::SetMixerInputValue(slot, value) @ 0x82680610
//   assert 0 <= slot < KI_MAX_SFX_CTLS;
//   if (mpDynamicMixIo) mpDynamicMixIo->SetDMixInput(slot, clamp(value,0,0x7fff));
// ---------------------------------------------------------------------------
void EffectBase::SetMixerInputValue(int aiSlot, int aiValue)
{
    CGS_ASSERT_AT(aiSlot >= 0 && aiSlot < KI_MAX_SFX_CTLS,
        "lSlotIndex >= 0 && lSlotIndex < 32",
            "..\\..\\..\\GameShared\\GameClasses\\Sound/Logic/CgsEffectBase.h", 812);
    if (mpDynamicMixIo)
    {
        const s32 liClamped = CgsNumeric::Min(CgsNumeric::Max(aiValue, 0), 0x7fff);
        mpDynamicMixIo->SetDMixInput(aiSlot, liClamped);
    }
}

ClassTypeInfo<EffectControl>* EffectControl::GetRegisteredTypeInfo(u32 auIndex)
{
    return auIndex < KU_SIZEOF_CLASS_ARRAY ? gapEffectControlTypeInfo[auIndex] : 0;
}

// ---------------------------------------------------------------------------
// EffectObject::GetTypeName()  @ 0x8268CE98  -> "EffectObject"
// ---------------------------------------------------------------------------
// ClassTypeInfo<EffectObject>::AddToClassTypeInfoArray @ 0x8268DE28 (array dword_82FFBA10): the State routine
// (0x8268DF08) over this class's 64-slot array; the streamed assert compares the u16
// index with the array's byte size (CgsEffectBase.h:363).
template<>
void ClassTypeInfo<EffectObject>::AddToClassTypeInfoArray()
{
    // DWARF CgsEffectBase.h:353 `uint16_t lCnt`, scanned by a for loop; the assert (:363) follows
    // it. ARTIST inlines the streamed literal after the loop (D12, asserts lane).
    u16 lCnt;
    for (lCnt = 0; lCnt < EffectObject::KU_SIZEOF_CLASS_ARRAY; ++lCnt)
    {
        if (!gapEffectObjectTypeInfo[lCnt])
        {
            gapEffectObjectTypeInfo[lCnt] = this;
            return;
        }
    }

    CGS_ASSERT_STREAM_AT(lCnt < sizeof(gapEffectObjectTypeInfo),
        "Too Many Class registations. Increase KU_SIZEOF_CLASS_ARRAY",
        "..\..\..\GameShared\GameClasses\Sound/Logic/CgsEffectBase.h", 363);
}

// ARTIST 0x82F2FA60.
ClassTypeInfo<EffectObject> EffectObject::sTypeInfo(-1, "EffectObject", 0, 0);

ClassTypeInfo<EffectObject>* EffectObject::GetStaticTypeInfo()
{
    return &sTypeInfo;
}

const char* EffectObject::GetTypeName() const
{
    return sTypeInfo.typeName; // ARTIST 0x8268CE98
}

ClassTypeInfo<EffectObject>* EffectObject::GetRegisteredTypeInfo(u32 auIndex)
{
    return auIndex < KU_SIZEOF_CLASS_ARRAY ? gapEffectObjectTypeInfo[auIndex] : 0;
}

} // namespace Logic
} // namespace CgsSound
