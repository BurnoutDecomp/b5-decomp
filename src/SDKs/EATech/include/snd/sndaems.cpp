// ============================================================================
// SDKs/EATech/include/snd/sndaems.cpp
//
// Snd9::Aems AEMS module-bank manager bodies. Reconstructed from
// BURNOUT_X360_ARTIST.XEX; the PowerPC asm is authoritative for every offset,
// width and side-effect. No Feb-2007 source and no DecFIGS DWARF exist for this TU.
//   Snd9::Aems::BeginRemoveModuleBank    @ 0x82B72250
//   Snd9::Aems::IsModuleBankRemoved      @ 0x82B71720
//   Snd9::Aems::SetSamplePlayerFactory   @ 0x82B6FCD0
//
// The `_savegprlr_23` / `_restgprlr_23` calls in the pseudocode are the compiler's
// register save/restore prologue/epilogue helpers -- not source-level calls -- so
// they are dropped. off_83271928 is the shared rwaudio System singleton (same
// global reached by Decoder.cpp / PlugIn.cpp), whose Lock/Unlock guard the registry
// walks and whose deferred-command ring receives the queued remove command.
// ============================================================================

#include "SDKs/EATech/include/snd/sndaems.h"

#include "SDKs/Csis/CsisClass.h"   // Csis::ClassHandle, Csis::ClassClientNode, Csis::Class::UnsubscribeConstructorFast
#include "SDKs/Csis/CsisFunction.h"
#include "SDKs/Csis/CsisFunctionHandle.h"
#include "SDKs/Csis/CsisGlobalVariable.h"
#include "SDKs/Csis/CsisGlobalVariableHandle.h"
#include "rw/audio/core/PlugIn.h"  // rw::audio::core::System (Lock / Unlock / Free + command ring)
#include "rw/audio/core/TimerHandle.h"
#include "rw/audio/core/TimerManager.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <limits>

// The shared rwaudio System singleton (off_83271928). Defined/owned by the System
// sub-system TU; declared here as the target of Lock/Unlock/Free and the command
// ring. Same declaration form Decoder.cpp uses.
extern "C" rw::audio::core::System* off_83271928;

// The C-facing AEMS instance layer's per-subscriber teardown (SNDAEMSI_updatedestroy,
// paired with Csis::Function::UnsubscribeFast). Its body lives in its own TU;
// declared here so BeginRemoveModuleBank's instance walk can call it. FLAGGED extern.
extern "C" int SNDAEMSI_updatedestroy(void* apSubscriber);
extern "C" void SNDAEMSI_CreateModuleInstance(Csis::Class* apClass,
                                                void* apParameters,
                                                void* apModule);

namespace Snd9
{
namespace Aems
{

// Result of BeginRemoveModuleBank when no registered bank matches the handle
// (X360: `li r31, -0xA`).
static const s32 KI_MODULE_BANK_NOT_FOUND = -10;

// The process-global AEMS module-bank registry head (off_832775F0). Holds the
// address of the first bank's embedded link (bank +0x50), not the bank base. Banks
// are pushed onto it by the SNDAEMSI create path (its own TU); the remove/query
// paths here only read it. Null until the first bank registers.
ModuleBankLink* gpModuleBankListHead = 0;

// Whether the AEMS module system is active (byte_83277864). While false the query
// path short-circuits (every bank reports removed). Set by AEMS init (its own TU).
bool gbAemsActive = false;

// The process-global AEMS sample-player factory (off_82F87DBC), installed by
// SetSamplePlayerFactory.
IAemsSamplePlayerFactory* gpSamplePlayerFactory = 0;

namespace
{
struct InstanceDestroySubscriber
{
    ModuleRecord* mpModule; // +0x00
    void* mpInstance;       // +0x08
    Csis::Class* mpClass;   // +0x10
    s32 miDestroyPending;   // +0x18
    u32 muPadding;
};

struct InstanceLink
{
    InstanceLink* mpNext;
    InstanceLink* mpPrev;
};

struct CsisRelocationRecord
{
    u32 muTargetOffset;
    u32 muInterfaceIdOffset;
    u8 muKind;
    u8 maPadding[3];
};

static_assert(sizeof(CsisRelocationRecord) == 12,
              "native AEMS CSIS relocation record");

InstanceLink* gpInstanceUpdateList = 0;

rw::audio::core::TimerHandle gAemsTimerHandle = {};
bool gbAemsTimerAdded = false;
f32 gfAemsTimerPeriod = -1.0f;
f32 gfAemsStepMilliseconds = 0.0f;
s32 giAemsUpdateInterval = 1;
s32 giAemsUpdateCountdown = 1;

void Unlink(InstanceLink*& arpHead, InstanceLink* apNode)
{
    if (arpHead == apNode)
        arpHead = apNode->mpNext;
    if (apNode->mpPrev)
        apNode->mpPrev->mpNext = apNode->mpNext;
    if (apNode->mpNext)
        apNode->mpNext->mpPrev = apNode->mpPrev;
}

void AemsDestructorCallback(Csis::Class*, void* apClientData)
{
    *reinterpret_cast<u32*>(reinterpret_cast<u8*>(apClientData) + 0x20) = 1;
}

void AemsGlobalCallback(CsisDef::Parameter* apValue, void* apClientData)
{
    *reinterpret_cast<u32*>(reinterpret_cast<u8*>(apClientData) + 0x30) =
        static_cast<u32>(apValue->intVal);
}

void AemsMemberDataCallback(void* apParameters, void* apClientData)
{
    u8* lpBlock = static_cast<u8*>(apClientData);
    const u8 luCount = lpBlock[0x20];
    if (luCount)
        std::memcpy(lpBlock + 0x28, apParameters, sizeof(u32) * luCount);
}

void AemsFunctionCallback(uintptr_t auParameters, void* apClientData)
{
    u8* lpBlock = static_cast<u8*>(apClientData);
    const u8 luCount = lpBlock[0x30];
    if (luCount && auParameters)
    {
        std::memcpy(lpBlock + 0x38, reinterpret_cast<const void*>(auParameters),
                    sizeof(u32) * luCount);
    }
    lpBlock[0x31] = 1;
}

void AemsConstructorCallback(Csis::Class* apClass, void* apParameters,
                             void* apClientData)
{
    SNDAEMSI_CreateModuleInstance(apClass, apParameters, apClientData);
}

bool ValidSpan(const ModuleBank* apBank, u32 auOffset, size_t auBytes)
{
    return auOffset <= apBank->muTotalSize &&
           auBytes <= static_cast<size_t>(apBank->muTotalSize - auOffset);
}

bool ValidResidentSpan(const ModuleBank* apBank, u32 auOffset, size_t auBytes)
{
    return auOffset <= apBank->muResidentSize &&
           auBytes <= static_cast<size_t>(apBank->muResidentSize - auOffset);
}

bool ApplyPointerRelocations(ModuleBank* apBank, const ModuleBank* apSource)
{
    if (!ValidSpan(apSource, apSource->muPointerRelocOffset, sizeof(u32)))
        return false;
    u8* lpBase = reinterpret_cast<u8*>(apBank);
    const u8* lpSource = reinterpret_cast<const u8*>(apSource);
    const u32* lpTable = reinterpret_cast<const u32*>(
        lpSource + apSource->muPointerRelocOffset);
    const u32 luCount = lpTable[0];
    if (!ValidSpan(apSource, apSource->muPointerRelocOffset + sizeof(u32),
                   sizeof(u32) * luCount))
        return false;
    for (u32 luIndex = 0; luIndex < luCount; ++luIndex)
    {
        const u32 luTarget = lpTable[luIndex + 1];
        if (!ValidResidentSpan(apBank, luTarget, sizeof(uintptr_t)))
            return false;
        uintptr_t& lruValue = *reinterpret_cast<uintptr_t*>(lpBase + luTarget);
        lruValue += reinterpret_cast<uintptr_t>(lpBase);
    }
    return true;
}

bool ApplyCsisRelocations(ModuleBank* apBank, const ModuleBank* apSource)
{
    if (!ValidSpan(apSource, apSource->muCsisRelocOffset, sizeof(u32)))
        return false;
    u8* lpBase = reinterpret_cast<u8*>(apBank);
    const u8* lpSource = reinterpret_cast<const u8*>(apSource);
    const u8* lpTable = lpSource + apSource->muCsisRelocOffset;
    const u32 luCount = *reinterpret_cast<const u32*>(lpTable);
    if (!ValidSpan(apSource, apSource->muCsisRelocOffset + sizeof(u32),
                   static_cast<size_t>(luCount) * 12u))
        return false;

    for (u32 luIndex = 0; luIndex < luCount; ++luIndex)
    {
        const CsisRelocationRecord& lrEntry =
            reinterpret_cast<const CsisRelocationRecord*>(lpTable + sizeof(u32))[luIndex];
        const u32 luTarget = lrEntry.muTargetOffset;
        const u32 luId = lrEntry.muInterfaceIdOffset;
        const u8 luKind = lrEntry.muKind;
        if (!ValidResidentSpan(apBank, luTarget, sizeof(Csis::ClassHandle)) ||
            !ValidSpan(apSource, luId, 5))
            return false;

        Csis::InterfaceId lId;
        lId.muSystemId = *reinterpret_cast<const u16*>(lpSource + luId + 0x00);
        lId.muInterfaceId = *reinterpret_cast<const u16*>(lpSource + luId + 0x02);
        lId.mpName = reinterpret_cast<const char*>(lpSource + luId + 0x04);

        if (luKind == 0)
            reinterpret_cast<Csis::ClassHandle*>(lpBase + luTarget)->SetFast(&lId);
        else if (luKind == 1)
            reinterpret_cast<Csis::FunctionHandle*>(lpBase + luTarget)->SetFast(&lId);
        else
            reinterpret_cast<Csis::GlobalVariableHandle*>(lpBase + luTarget)->SetFast(&lId);

        // XB1 sub_14096C510 dispatches each relocation and deliberately ignores
        // the resolver's return value.  CSIS content and AEMS banks are streamed
        // independently, so an interface may not be subscribed yet when the bank
        // first arrives; that is not a malformed-bank result at this layer.
    }
    return true;
}

inline s32 RoundAems(f32 afValue)
{
    return static_cast<s32>(afValue < 0.0f ? afValue - 0.5f : afValue + 0.5f);
}

struct ClassDestructorBlock
{
    Csis::ClassClientNode mClient;
    u32 muTriggered;
};

struct ClassDataBlock
{
    Csis::ClassClientNode mClient;
    u8 muParameterCount;
    u8 maPadding[7];
    s32 maParameters[1];
};

struct FunctionCallBlock
{
    Csis::Function mFunction;
    u8 mbEnabled;
    u8 muParameterCount;
    u8 maPadding[6];
    s32 maRangeAndRuntime[1];
};

struct CounterBlock
{
    s32 miMinimum;
    s32 miMaximum;
    s32 miCurrent;
    s32 miStep;
    s32 miEnabled;
    s32 miInput;
};

struct RandomBlock
{
    s32 miMinimum;
    s32 miRange;
    s32 miCurrent;
    s32 miTrigger;
};

struct RandomShuffleBlock
{
    u16 muTriggerOffset;
    u8  muValueWidth;
    u8  mbCycleComplete;
    s32 miBase;
    u16 muCursor;
    u16 muLimit;
    s32 miCurrent;
    u8  maValues[1];
};

struct RandomWeightedBlock
{
    const u8* mpWeights;
    s32 miBase;
    s32 miCount;
    s32 miCurrent;
    s32 miTrigger;
};

struct RangeTriggerBlock
{
    s32 miEnterMinimum;
    s32 miEnterMaximum;
    s32 miResetMinimum;
    s32 miResetMaximum;
    u8  mbLatched;
    u8  mbOutput;
    u8  maPadding[2];
    s32 miInput;
};

struct DelayTriggerBlock
{
    f32 mfElapsed;
    s32 miOutput;
    s32 miTrigger;
    s32 miDelayMilliseconds;
};

struct StateGeneratorBlock
{
    u16 muTriggerOffset;
    u16 muCurrent;
    u16 muCount;
    u16 muPadding;
    s32 maStates[1];
};

struct EnvelopeBlock
{
    u16 muControlOffset;
    u8  muPreviousControl;
    u8  muPoint;
    f32 mfTimeRemaining;
    f32 mfStep;
    f32 mfValue;
    u8  muPointCount;
    u8  muPadding;
    u16 muReleasePoint;
    f32 maPoints[1];
};

struct TableData
{
    u8 muElementSize;
    u8 muPadding;
    u16 muElementCount;
    s32 miMinimum;
    s32 miMaximum;
    f32 mfScale;
    u8 maValues[1];
};

struct TableBlock
{
    const TableData* mpTable;
    s32 miPreviousInput;
    s32 miOutput;
    s32 miInput;
};

struct DelayLineBlock
{
    u16 muInputOffset;
    u16 muCapacity;
    u16 muWriteIndex;
    u16 muReadIndex;
    s32 miPreviousDelay;
    s32 maSamples[1];
};

struct DemuxBlock
{
    u8 muOutputCount;
    u8 muPadding;
    u16 muPreviousSelection;
    s32 miSelection;
    s32 miInput;
    s32 maOutputs[1];
};

struct ScaleBlock
{
    u8 muInputCount;
    u8 maPadding[3];
    f32 mfScale;
    s32 maInputs[1];
};

u32 AemsRandom()
{
    // Exact six-word iSNDrandom accumulator.  Both ARTIST (iSNDrandom @
    // 0x82B74490) and the x64 build (sub_140929A30) use these same seed words.
    static u32 sauState[6] =
    {
        0xF22D0E56u, 0x883126E9u, 0xC624DD2Fu,
        0x0702C49Cu, 0x9E353F7Du, 0x6FDF3B64u
    };

    u64 luSum = static_cast<u64>(sauState[4]) + sauState[5];
    sauState[4] = static_cast<u32>(luSum);
    u32 luCarry = static_cast<u32>(luSum >> 32);
    for (s32 liWord = 3; liWord >= 0; --liWord)
    {
        luSum = static_cast<u64>(sauState[liWord]) +
                sauState[liWord + 1] + luCarry;
        sauState[liWord] = static_cast<u32>(luSum);
        luCarry = static_cast<u32>(luSum >> 32);
    }

    ++sauState[5];
    if (!sauState[5])
    {
        for (s32 liWord = 4; liWord >= 0; --liWord)
        {
            ++sauState[liWord];
            if (sauState[liWord])
                break;
        }
    }
    return sauState[0];
}

s32 UpdateClassDestructor(void* apData)
{
    ClassDestructorBlock* lpBlock = static_cast<ClassDestructorBlock*>(apData);
    const s32 liResult = static_cast<s32>(lpBlock->muTriggered);
    lpBlock->muTriggered = 0;
    return liResult;
}

s32 UpdateClassData(void* apData)
{
    return static_cast<ClassDataBlock*>(apData)->maParameters[0];
}

s32 UpdateClassDataTail(void* apData)
{
    ClassDataBlock* lpBlock = static_cast<ClassDataBlock*>(apData);
    return lpBlock->muParameterCount > 2 ? lpBlock->maParameters[2] : 0;
}

s32 UpdateCreate(void* apData)
{
    s32* lpValue = static_cast<s32*>(apData);
    const s32 liResult = *lpValue;
    *lpValue = 0;
    return liResult;
}

s32 UpdateDestroy(void* apData)
{
    return SNDAEMSI_updatedestroy(apData);
}

s32 UpdateCallFunction(void* apData)
{
    FunctionCallBlock* lpBlock = static_cast<FunctionCallBlock*>(apData);
    s32* lpRanges = lpBlock->maRangeAndRuntime;
    s32* lpRuntime = lpRanges + 2 * lpBlock->muParameterCount;
    s32* lpParameters = lpRuntime + 1;
    if (lpBlock->mbEnabled)
    {
        for (u8 luIndex = 0; luIndex < lpBlock->muParameterCount; ++luIndex)
        {
            lpParameters[luIndex] = (std::max)(lpRanges[2 * luIndex],
                (std::min)(lpRanges[2 * luIndex + 1], lpParameters[luIndex]));
        }
    }
    if (*lpRuntime)
        lpBlock->mFunction.CallFast(reinterpret_cast<uintptr_t>(lpParameters));
    return 0;
}

s32 UpdateCounter(void* apData)
{
    CounterBlock* lpBlock = static_cast<CounterBlock*>(apData);
    if (lpBlock->miInput >= lpBlock->miMinimum &&
        lpBlock->miInput <= lpBlock->miMaximum)
        return lpBlock->miInput;
    if (lpBlock->miEnabled > 0)
    {
        lpBlock->miCurrent += lpBlock->miStep;
        if (lpBlock->miCurrent > lpBlock->miMaximum)
            lpBlock->miCurrent = lpBlock->miMinimum;
        else if (lpBlock->miCurrent < lpBlock->miMinimum)
            lpBlock->miCurrent = lpBlock->miMaximum;
    }
    return lpBlock->miCurrent;
}

s32 UpdateRandom(void* apData)
{
    RandomBlock* lpBlock = static_cast<RandomBlock*>(apData);
    if (lpBlock->miTrigger && lpBlock->miRange > 0)
        lpBlock->miCurrent = lpBlock->miMinimum +
            static_cast<s32>(AemsRandom() % static_cast<u32>(lpBlock->miRange));
    return lpBlock->miCurrent;
}

s32 UpdateRandomShuffle(void* apData)
{
    RandomShuffleBlock* lpBlock = static_cast<RandomShuffleBlock*>(apData);
    const u8* lpBase = reinterpret_cast<const u8*>(lpBlock);
    if (!*reinterpret_cast<const s32*>(lpBase + lpBlock->muTriggerOffset))
        return lpBlock->miCurrent;

    const u16 luRemaining = static_cast<u16>(
        lpBlock->muLimit - lpBlock->mbCycleComplete - lpBlock->muCursor);
    if (!luRemaining)
        return lpBlock->miCurrent;
    const u16 luPick = static_cast<u16>(lpBlock->muCursor +
        AemsRandom() % luRemaining);
    s32 liResult;
    if (lpBlock->muValueWidth == 1)
    {
        u8* lpValues = lpBlock->maValues;
        liResult = lpValues[luPick];
        std::swap(lpValues[luPick], lpValues[lpBlock->muCursor]);
    }
    else
    {
        u16* lpValues = reinterpret_cast<u16*>(lpBlock->maValues);
        liResult = lpValues[luPick];
        std::swap(lpValues[luPick], lpValues[lpBlock->muCursor]);
    }
    liResult += lpBlock->miBase;
    lpBlock->miCurrent = liResult;
    ++lpBlock->muCursor;
    lpBlock->mbCycleComplete = lpBlock->muCursor >= lpBlock->muLimit;
    if (lpBlock->mbCycleComplete)
        lpBlock->muCursor = 0;
    return liResult;
}

s32 UpdateRandomWeighted(void* apData)
{
    RandomWeightedBlock* lpBlock = static_cast<RandomWeightedBlock*>(apData);
    if (!lpBlock->miTrigger || lpBlock->miCount <= 0)
        return lpBlock->miCurrent;
    const u32 luPick = AemsRandom() % 100u;
    u32 luTotal = 0;
    for (s32 liIndex = 0; liIndex < lpBlock->miCount; ++liIndex)
    {
        luTotal += lpBlock->mpWeights[16 + liIndex];
        if (luTotal > luPick)
        {
            lpBlock->miCurrent = lpBlock->miBase + liIndex;
            break;
        }
    }
    return lpBlock->miCurrent;
}

s32 UpdateRangeTrigger(void* apData)
{
    RangeTriggerBlock* lpBlock = static_cast<RangeTriggerBlock*>(apData);
    if (lpBlock->miInput >= lpBlock->miEnterMinimum &&
        lpBlock->miInput <= lpBlock->miEnterMaximum)
    {
        if (!lpBlock->mbLatched)
        {
            lpBlock->mbLatched = 1;
            lpBlock->mbOutput = 1;
            return 1;
        }
    }
    else if (lpBlock->miInput >= lpBlock->miResetMinimum &&
             lpBlock->miInput <= lpBlock->miResetMaximum)
    {
        lpBlock->mbLatched = 0;
    }
    lpBlock->mbOutput = 0;
    return 0;
}

s32 UpdateDelayTrigger(void* apData)
{
    DelayTriggerBlock* lpBlock = static_cast<DelayTriggerBlock*>(apData);
    if (lpBlock->miTrigger)
        lpBlock->mfElapsed = 0.0f;
    if (lpBlock->mfElapsed >= 0.0f)
    {
        if (lpBlock->mfElapsed >= static_cast<f32>(lpBlock->miDelayMilliseconds))
        {
            lpBlock->mfElapsed = -1.0f;
            lpBlock->miOutput = 1;
            return 1;
        }
        lpBlock->mfElapsed += gfAemsStepMilliseconds;
    }
    lpBlock->miOutput = 0;
    return 0;
}

s32 UpdateStateGenerator(void* apData)
{
    StateGeneratorBlock* lpBlock = static_cast<StateGeneratorBlock*>(apData);
    s32* lpTriggers = reinterpret_cast<s32*>(
        reinterpret_cast<u8*>(lpBlock) + lpBlock->muTriggerOffset);
    for (u16 luIndex = 0; luIndex < lpBlock->muCount; ++luIndex)
    {
        if (lpTriggers[luIndex])
        {
            lpBlock->muCurrent = static_cast<u16>(lpBlock->maStates[luIndex]);
            break;
        }
    }
    return lpBlock->muCurrent;
}

s32 UpdateMerge(void* apData)
{
    const u8* lpBlock = static_cast<const u8*>(apData);
    const s32* lpInputs = reinterpret_cast<const s32*>(lpBlock + 4);
    for (u8 luIndex = 0; luIndex < lpBlock[0]; ++luIndex)
    {
        if (lpInputs[luIndex])
            return 1;
    }
    return 0;
}

s32 UpdateEnvelope(void* apData)
{
    EnvelopeBlock* lpBlock = static_cast<EnvelopeBlock*>(apData);
    const s32 liControl = *reinterpret_cast<const s32*>(
        reinterpret_cast<const u8*>(lpBlock) + lpBlock->muControlOffset);
    f32* lpPoints = lpBlock->maPoints;

    if (liControl == 1 && lpBlock->muPreviousControl != 1)
    {
        lpBlock->mfValue = lpPoints[0];
        lpBlock->muPoint = 0;
        lpBlock->mfTimeRemaining = lpPoints[1];
        lpBlock->mfStep = lpBlock->mfTimeRemaining != 0.0f
            ? (lpPoints[2] - lpBlock->mfValue) /
              lpBlock->mfTimeRemaining * gfAemsStepMilliseconds
            : 0.0f;
    }
    else if (liControl == 3 && lpBlock->muPreviousControl != 3 &&
             lpBlock->muPoint < lpBlock->muReleasePoint)
    {
        lpBlock->muPoint = static_cast<u8>(lpBlock->muReleasePoint);
        lpBlock->mfTimeRemaining = lpPoints[1 + 2 * lpBlock->muPoint];
        lpBlock->mfStep = lpBlock->mfTimeRemaining != 0.0f
            ? (lpPoints[2 + 2 * lpBlock->muPoint] - lpBlock->mfValue) /
              lpBlock->mfTimeRemaining * gfAemsStepMilliseconds
            : 0.0f;
    }

    if ((liControl == 1 || liControl == 3) &&
        lpBlock->muPoint < lpBlock->muPointCount)
    {
        lpBlock->mfTimeRemaining -= gfAemsStepMilliseconds;
        if (lpBlock->mfTimeRemaining > 0.0f)
        {
            lpBlock->mfValue += lpBlock->mfStep;
        }
        else
        {
            lpBlock->mfValue = lpPoints[2 + 2 * lpBlock->muPoint];
            ++lpBlock->muPoint;
            if (lpBlock->muPoint < lpBlock->muPointCount)
            {
                lpBlock->mfTimeRemaining = lpPoints[1 + 2 * lpBlock->muPoint];
                lpBlock->mfStep = lpBlock->mfTimeRemaining != 0.0f
                    ? (lpPoints[2 + 2 * lpBlock->muPoint] - lpBlock->mfValue) /
                      lpBlock->mfTimeRemaining * gfAemsStepMilliseconds
                    : 0.0f;
            }
        }
    }
    else if (liControl != 2)
    {
        lpBlock->mfValue = 0.0f;
    }

    lpBlock->muPreviousControl = static_cast<u8>(liControl);
    return RoundAems(lpBlock->mfValue);
}

s32 ReadTableValue(const TableData* apTable, s32 aiIndex)
{
    if (apTable->muElementSize == 1)
        return reinterpret_cast<const s8*>(apTable->maValues)[aiIndex];
    if (apTable->muElementSize == 2)
        return reinterpret_cast<const s16*>(apTable->maValues)[aiIndex];
    return reinterpret_cast<const s32*>(apTable->maValues)[aiIndex];
}

s32 UpdateTable(void* apData)
{
    TableBlock* lpBlock = static_cast<TableBlock*>(apData);
    if (lpBlock->miInput == lpBlock->miPreviousInput)
        return lpBlock->miOutput;

    const TableData* lpTable = lpBlock->mpTable;
    lpBlock->miPreviousInput = lpBlock->miInput;
    const s32 liInput = (std::max)(lpTable->miMinimum,
        (std::min)(lpTable->miMaximum, lpBlock->miInput));
    if (lpTable->muElementCount == 0)
        return lpBlock->miOutput = 0;

    if (lpTable->mfScale == 1.0f)
    {
        const s32 liIndex = (std::max)(0, (std::min)(
            static_cast<s32>(lpTable->muElementCount) - 1,
            liInput - lpTable->miMinimum));
        return lpBlock->miOutput = ReadTableValue(lpTable, liIndex);
    }

    const f32 lfPosition = static_cast<f32>(liInput - lpTable->miMinimum) *
                           lpTable->mfScale;
    s32 liIndex = RoundAems(lfPosition - 0.5f);
    liIndex = (std::max)(0, (std::min)(
        static_cast<s32>(lpTable->muElementCount) - 1, liIndex));
    const s32 liNext = (std::min)(
        static_cast<s32>(lpTable->muElementCount) - 1, liIndex + 1);
    const f32 lfFraction = lfPosition - static_cast<f32>(liIndex);
    const f32 lfValue = static_cast<f32>(ReadTableValue(lpTable, liIndex)) +
        static_cast<f32>(ReadTableValue(lpTable, liNext) -
                         ReadTableValue(lpTable, liIndex)) * lfFraction;
    lpBlock->miOutput = RoundAems(lfValue);
    return lpBlock->miOutput;
}

s32 UpdateDelayLine(void* apData)
{
    DelayLineBlock* lpBlock = static_cast<DelayLineBlock*>(apData);
    s32* lpInput = reinterpret_cast<s32*>(
        reinterpret_cast<u8*>(lpBlock) + lpBlock->muInputOffset);
    if (lpInput[1] != lpBlock->miPreviousDelay)
    {
        lpBlock->miPreviousDelay = (std::max)(lpInput[1], 0);
        s32 liSamples = RoundAems(static_cast<f32>(lpBlock->miPreviousDelay) /
                                 gfAemsStepMilliseconds);
        liSamples = (std::min)(liSamples,
            static_cast<s32>(lpBlock->muCapacity) - 1);
        lpBlock->muWriteIndex = static_cast<u16>(lpBlock->muReadIndex + liSamples);
    }
    if (lpBlock->muWriteIndex >= lpBlock->muCapacity)
        lpBlock->muWriteIndex = static_cast<u16>(
            lpBlock->muWriteIndex - lpBlock->muCapacity);
    if (lpBlock->muReadIndex >= lpBlock->muCapacity)
        lpBlock->muReadIndex = 0;
    lpBlock->maSamples[lpBlock->muWriteIndex++] = lpInput[0];
    return lpBlock->maSamples[lpBlock->muReadIndex++];
}

s32 UpdateMux(void* apData)
{
    const u8* lpBlock = static_cast<const u8*>(apData);
    const s32* lpWords = reinterpret_cast<const s32*>(lpBlock + 4);
    const s32 liSelection = lpWords[0];
    if (liSelection <= 0 || liSelection > lpBlock[0])
        return 0;
    return lpWords[liSelection];
}

s32 UpdateDemux(void* apData)
{
    DemuxBlock* lpBlock = static_cast<DemuxBlock*>(apData);
    if (lpBlock->muPreviousSelection > 0 &&
        lpBlock->muPreviousSelection <= lpBlock->muOutputCount)
        lpBlock->maOutputs[lpBlock->muPreviousSelection - 1] = 0;
    const s32 liSelection = lpBlock->miSelection;
    if (liSelection > 0 && liSelection <= lpBlock->muOutputCount)
    {
        lpBlock->maOutputs[liSelection - 1] = lpBlock->miInput;
        lpBlock->muPreviousSelection = static_cast<u16>(liSelection);
    }
    return lpBlock->maOutputs[0];
}

s32 UpdateMinimum(void* apData)
{
    const u8* lpBlock = static_cast<const u8*>(apData);
    const s32* lpValues = reinterpret_cast<const s32*>(lpBlock + 4);
    if (!lpBlock[0])
        return 0;
    s32 liResult = lpValues[0];
    for (u8 luIndex = 1; luIndex < lpBlock[0]; ++luIndex)
        liResult = (std::min)(liResult, lpValues[luIndex]);
    return liResult;
}

s32 UpdateMaximum(void* apData)
{
    const u8* lpBlock = static_cast<const u8*>(apData);
    const s32* lpValues = reinterpret_cast<const s32*>(lpBlock + 4);
    if (!lpBlock[0])
        return 0;
    s32 liResult = lpValues[0];
    for (u8 luIndex = 1; luIndex < lpBlock[0]; ++luIndex)
        liResult = (std::max)(liResult, lpValues[luIndex]);
    return liResult;
}

s32 UpdateScale(void* apData)
{
    const ScaleBlock* lpBlock = static_cast<const ScaleBlock*>(apData);
    f32 lfValue = lpBlock->muInputCount
        ? static_cast<f32>(lpBlock->maInputs[0]) : 0.0f;
    for (u8 luIndex = 1; luIndex < lpBlock->muInputCount; ++luIndex)
        lfValue *= static_cast<f32>(lpBlock->maInputs[luIndex]);
    return RoundAems(lfValue * lpBlock->mfScale);
}

s32 UpdateAdd(void* apData)
{
    const u8* lpBlock = static_cast<const u8*>(apData);
    const s32* lpValues = reinterpret_cast<const s32*>(lpBlock + 4);
    s32 liResult = 0;
    for (u8 luIndex = 0; luIndex < lpBlock[0]; ++luIndex)
        liResult += lpValues[luIndex];
    return liResult;
}

s32 UpdateSubtract(void* apData)
{
    const s32* lpValues = static_cast<const s32*>(apData);
    return lpValues[0] - lpValues[1];
}

s32 UpdateMultiply(void* apData)
{
    const s32* lpValues = static_cast<const s32*>(apData);
    return lpValues[0] * lpValues[1];
}

s32 UpdateDivide(void* apData)
{
    const s32* lpValues = static_cast<const s32*>(apData);
    return lpValues[1] ? lpValues[0] / lpValues[1] : 0;
}

s32 UpdateModulo(void* apData)
{
    const s32* lpValues = static_cast<const s32*>(apData);
    return lpValues[1] ? lpValues[0] % lpValues[1] : 0;
}

struct AemsPlayerInput
{
    u8 muSelector;
    u8 maPadding[3];
    s32 miPrevious;
    s32 miValue;
};

struct AemsPlayerBlock
{
    ModuleBank* mpBank;
    const s32* mpSampleSelectorTable;
    IAemsSamplePlayer* mpPlayer;
    u8 muCurrentControl;
    u8 muPreviousControl;
    u8 muInputCount;
    u8 mbHasSampleOutputs;
    u8 maPadding1C[1];
    u8 muOutputCount;
    u8 maPadding1E[2];
    s32 miSampleIndex;
    s32 miDesiredControl;
    AemsPlayerInput maInputs[1];
};

static_assert(sizeof(AemsPlayerInput) == 12, "native AEMS player input");
static_assert(offsetof(AemsPlayerBlock, maInputs) == 0x28,
              "native AEMS player block");

// The native loader expands byte_140C3D3D8 into these 36 fixed-point values
// (sub_14096C510: each unsigned byte << 8) before the first bank is linked.
int gaLegacyAzimuths[36] =
{
    0x0000, 0x0100, 0x0100, 0x0100, 0x0100, 0x0100,
    0xE000, 0x2000, 0x0100, 0x0100, 0x0100, 0x0100,
    0x0100, 0x0100, 0x0100, 0x0100, 0x0100, 0x0100,
    0xE000, 0x2000, 0xA000, 0x6000, 0x0100, 0x0100,
    0x0100, 0x0100, 0x0100, 0x0100, 0x0100, 0x0100,
    0xE000, 0x0000, 0x2000, 0xA000, 0x6000, 0x0000
};

void SetPlayerInput(IAemsSamplePlayer* apPlayer, u8 auSelector, s32 aiValue)
{
    if (!apPlayer)
        return;
    if (auSelector == IAemsSamplePlayer::PLAYER_INPUT_AZIMUTH)
    {
        apPlayer->SetAzimuth(aiValue, gaLegacyAzimuths);
        return;
    }

    s32 liValue = aiValue;
    if (auSelector == IAemsSamplePlayer::PLAYER_INPUT_PITCHMULT ||
        auSelector == IAemsSamplePlayer::PLAYER_INPUT_LOWPASS ||
        auSelector == IAemsSamplePlayer::PLAYER_INPUT_HIGHPASS)
        liValue = (std::max)(0, (std::min)(0xFFFF, liValue));
    else if (auSelector == IAemsSamplePlayer::PLAYER_INPUT_VOL ||
             auSelector == IAemsSamplePlayer::PLAYER_INPUT_FXWET0 ||
             auSelector == IAemsSamplePlayer::PLAYER_INPUT_DRYLEVEL)
        liValue = (std::max)(0, (std::min)(0x7FFF, liValue));
    apPlayer->SetInput(static_cast<IAemsSamplePlayer::InputSelector>(auSelector),
                       liValue);
}

s32* PlayerOutputData(AemsPlayerBlock* apBlock)
{
    return reinterpret_cast<s32*>(apBlock->maInputs + apBlock->muInputCount);
}

void ClearPlayer(AemsPlayerBlock* apBlock)
{
    if (apBlock->mpPlayer)
    {
        apBlock->mpPlayer->Release();
        apBlock->mpPlayer = 0;
    }
    if (apBlock->mbHasSampleOutputs)
        PlayerOutputData(apBlock)[0] = 0;
}

IAemsSamplePlayer* CreateSamplePlayer(AemsPlayerBlock* apBlock,
                                      const u8* apSelection)
{
    if (!gpSamplePlayerFactory || !apBlock->mpBank ||
        !apBlock->mpBank->mpSampleBank)
    {
        ClearPlayer(apBlock);
        return 0;
    }

    const u8* lpSampleBank = static_cast<const u8*>(
        apBlock->mpBank->mpSampleBank);
    const u16 luSampleIndex = *reinterpret_cast<const u16*>(apSelection);
    u32 luEncodedOffset;
    std::memcpy(&luEncodedOffset,
                lpSampleBank + 12 + sizeof(u32) * luSampleIndex,
                sizeof(luEncodedOffset));
    const u32 luSampleOffset = ((luEncodedOffset & 0x000000FFu) << 24) |
                               ((luEncodedOffset & 0x0000FF00u) << 8)  |
                               ((luEncodedOffset & 0x00FF0000u) >> 8)  |
                               ((luEncodedOffset & 0xFF000000u) >> 24);

    int laiOutputs[6];
    for (u32 luOutput = 0; luOutput < 6; ++luOutput)
        laiOutputs[luOutput] = static_cast<int>(apSelection[3 + luOutput]) << 8;

    s32 liStreamOffset;
    std::memcpy(&liStreamOffset, apSelection + 8, sizeof(liStreamOffset));
    AemsPlayerInputAccessor lAccessor = {};
    lAccessor.muCount = apBlock->muInputCount;
    lAccessor.mpRecords = reinterpret_cast<AemsPlayerInputAccessor::Input*>(
        apBlock->maInputs);

    IAemsSamplePlayer* lpPlayer = gpSamplePlayerFactory->CreateInstance(
        const_cast<u8*>(lpSampleBank + luSampleOffset), apSelection[2],
        laiOutputs, static_cast<const char*>(apBlock->mpBank->mpAllocatedBlock),
        apBlock->mpBank->miStreamFileOffset + liStreamOffset, &lAccessor);
    if (!lpPlayer)
    {
        ClearPlayer(apBlock);
        return 0;
    }

    for (u8 luInput = 0; luInput < apBlock->muInputCount; ++luInput)
    {
        SetPlayerInput(lpPlayer, apBlock->maInputs[luInput].muSelector,
                       apBlock->maInputs[luInput].miValue);
        apBlock->maInputs[luInput].miPrevious =
            apBlock->maInputs[luInput].miValue;
    }
    return lpPlayer;
}

s32 RefreshSamplePlayer(AemsPlayerBlock* apBlock)
{
    for (u8 luInput = 0; luInput < apBlock->muInputCount; ++luInput)
    {
        AemsPlayerInput& lrInput = apBlock->maInputs[luInput];
        if (lrInput.miPrevious != lrInput.miValue)
        {
            SetPlayerInput(apBlock->mpPlayer, lrInput.muSelector, lrInput.miValue);
            lrInput.miPrevious = lrInput.miValue;
        }
    }

    int laiOutputs[12] = {};
    apBlock->mpPlayer->GetOutputs(11, laiOutputs);
    if (!laiOutputs[0])
    {
        ClearPlayer(apBlock);
        return 0;
    }

    s32* lpOutputs = PlayerOutputData(apBlock);
    if (apBlock->mbHasSampleOutputs)
    {
        lpOutputs[0] = laiOutputs[1];
        lpOutputs[1] = laiOutputs[2];
        lpOutputs += 2;
    }
    if (apBlock->muOutputCount)
    {
        for (u32 luOutput = 0; luOutput < 8; ++luOutput)
            lpOutputs[luOutput] = laiOutputs[3 + luOutput];
    }
    return 1;
}

s32 UpdatePlayer(void* apData)
{
    AemsPlayerBlock* lpBlock = static_cast<AemsPlayerBlock*>(apData);
    s32 liControl = (std::max)(0, (std::min)(2, lpBlock->miDesiredControl));
    if (liControl != lpBlock->muCurrentControl)
    {
        if (liControl == 0)
        {
            ClearPlayer(lpBlock);
        }
        else if (liControl == 1)
        {
            if (lpBlock->mpPlayer)
            {
                lpBlock->mpPlayer->Unpause();
            }
            else if (lpBlock->muCurrentControl != 2 ||
                     lpBlock->muPreviousControl != 1)
            {
                const s32* lpTable = lpBlock->mpSampleSelectorTable;
                s32 liIndex = lpBlock->miSampleIndex;
                const s32 liCount = lpTable ? lpTable[0] : 0;
                if (liIndex < 0)
                    liIndex = 0;
                else if (liIndex >= liCount)
                    liIndex = liCount - 1;
                const u8* lpSelection = liCount > 0
                    ? reinterpret_cast<const u8*>(lpTable + 1 + 3 * liIndex) : 0;
                if (!lpSelection ||
                    *reinterpret_cast<const u16*>(lpSelection) == 0xFFFFu)
                    ClearPlayer(lpBlock);
                else
                    lpBlock->mpPlayer = CreateSamplePlayer(lpBlock, lpSelection);
            }
        }
        else if (lpBlock->mpPlayer)
        {
            lpBlock->mpPlayer->Pause();
        }
        lpBlock->muPreviousControl = lpBlock->muCurrentControl;
        lpBlock->muCurrentControl = static_cast<u8>(liControl);
    }

    if (liControl == 1 && lpBlock->mpPlayer)
        return RefreshSamplePlayer(lpBlock);
    return lpBlock->mpPlayer ? liControl : 0;
}

struct OscillatorBlock
{
    u8 muWaveform;
    u8 maPadding[3];
    f32 mfPhase;
    s32 miFrequency;
    s32 miAmplitude;
};

s32 UpdateOscillator(void* apData)
{
    OscillatorBlock* lpBlock = static_cast<OscillatorBlock*>(apData);
    if (lpBlock->miFrequency <= 0)
        return 0;
    while (lpBlock->mfPhase >= 1.0f)
        lpBlock->mfPhase -= 1.0f;

    const f32 lfAmplitude = static_cast<f32>(lpBlock->miAmplitude);
    f32 lfValue;
    if (lpBlock->muWaveform == 0)
        lfValue = std::sin(lpBlock->mfPhase * 6.2831853071795864769f) * lfAmplitude;
    else if (lpBlock->muWaveform == 1)
        lfValue = lpBlock->mfPhase < 0.5f ? 0.0f : lfAmplitude;
    else if (lpBlock->muWaveform == 2)
        lfValue = lpBlock->mfPhase * lfAmplitude;
    else
        lfValue = (lpBlock->mfPhase < 0.5f ? lpBlock->mfPhase :
                   1.0f - lpBlock->mfPhase) * 2.0f * lfAmplitude;
    lpBlock->mfPhase += gfAemsStepMilliseconds /
                        static_cast<f32>(lpBlock->miFrequency);
    return RoundAems(lfValue);
}

struct RampBlock
{
    f32 mfCurrent;
    f32 mfDeltaPerTick;
    s32 miPreviousTarget;
    s32 miPreviousDuration;
    s32 miDuration;
    s32 miTicks;
    s32 miTarget;
};

s32 UpdateRamp(void* apData)
{
    RampBlock* lpBlock = static_cast<RampBlock*>(apData);
    const f32 lfTarget = static_cast<f32>(lpBlock->miTarget);
    if (lpBlock->mfCurrent == lfTarget)
        return lpBlock->miTarget;
    if (lpBlock->miPreviousTarget != lpBlock->miTarget ||
        lpBlock->miPreviousDuration != lpBlock->miDuration)
    {
        lpBlock->miPreviousTarget = lpBlock->miTarget;
        lpBlock->miPreviousDuration = lpBlock->miDuration;
        if (lpBlock->miDuration <= 0)
        {
            lpBlock->mfCurrent = lfTarget;
            return lpBlock->miTarget;
        }
        lpBlock->mfDeltaPerTick = (lfTarget - lpBlock->mfCurrent) *
            gfAemsStepMilliseconds / static_cast<f32>(lpBlock->miDuration) /
            4096.0f;
    }
    lpBlock->mfCurrent += static_cast<f32>(lpBlock->miTicks) *
                          lpBlock->mfDeltaPerTick;
    if ((lpBlock->mfDeltaPerTick >= 0.0f && lpBlock->mfCurrent > lfTarget) ||
        (lpBlock->mfDeltaPerTick < 0.0f && lpBlock->mfCurrent < lfTarget))
        lpBlock->mfCurrent = lfTarget;
    return RoundAems(lpBlock->mfCurrent);
}

s32 UpdateAddMaximum(void* apData)
{
    const u8* lpBlock = static_cast<const u8*>(apData);
    const s32* lpValues = reinterpret_cast<const s32*>(lpBlock + 4);
    s32 liValue = 0;
    for (u8 luIndex = 1; luIndex < lpBlock[0]; ++luIndex)
        liValue += lpValues[luIndex];
    return (std::min)(lpValues[0], liValue);
}

s32 UpdateSubtractMinimum(void* apData)
{
    const s32* lpValues = static_cast<const s32*>(apData);
    return (std::max)(lpValues[0], lpValues[1] - lpValues[2]);
}

s32 UpdateMultiplyMaximum(void* apData)
{
    const s32* lpValues = static_cast<const s32*>(apData);
    return (std::min)(lpValues[0], lpValues[1] * lpValues[2]);
}

s32 UpdateMinimum2(void* apData)
{
    const s32* lpValues = static_cast<const s32*>(apData);
    return (std::min)(lpValues[0], lpValues[1]);
}

s32 UpdateMaximum2(void* apData)
{
    const s32* lpValues = static_cast<const s32*>(apData);
    return (std::max)(lpValues[0], lpValues[1]);
}

s32 UpdateScale2(void* apData)
{
    const f32* lpScale = static_cast<const f32*>(apData);
    const s32* lpValues = reinterpret_cast<const s32*>(lpScale + 1);
    return RoundAems(lpScale[0] * static_cast<f32>(lpValues[0]) *
                     static_cast<f32>(lpValues[1]));
}

s32 UpdateAdd2(void* apData)
{
    const s32* lpValues = static_cast<const s32*>(apData);
    return lpValues[0] + lpValues[1];
}

s32 UpdateFunction(void* apData)
{
    u8* lpBytes = static_cast<u8*>(apData);
    const s32 liResult = lpBytes[49];
    lpBytes[49] = 0;
    return liResult;
}

struct ControlClassBlock
{
    Csis::ClassHandle mClassHandle;
    Csis::Class* mpClass;
    u8 mbHasParameters;
    u8 muParameterCount;
    u8 maPadding[6];
    s32 maRangeAndRuntime[1];
};

s32 UpdateControlClass(void* apData)
{
    ControlClassBlock* lpBlock = static_cast<ControlClassBlock*>(apData);
    s32* lpRanges = lpBlock->maRangeAndRuntime;
    s32* lpRuntime = lpRanges + 2 * lpBlock->muParameterCount;
    s32* lpParameters = lpRuntime + 2;
    if (lpRuntime[1])
    {
        if (lpBlock->mpClass)
        {
            lpBlock->mpClass->Release();
            lpBlock->mpClass = 0;
        }
    }
    else if (lpRuntime[0])
    {
        if (!lpBlock->mpClass)
        {
            if (lpBlock->mbHasParameters)
            {
                for (u8 luIndex = 0; luIndex < lpBlock->muParameterCount; ++luIndex)
                    lpParameters[luIndex] = (std::max)(lpRanges[2 * luIndex],
                        (std::min)(lpRanges[2 * luIndex + 1], lpParameters[luIndex]));
            }
            Csis::Class::CreateInstanceFast(&lpBlock->mClassHandle,
                                            lpParameters, &lpBlock->mpClass);
        }
    }
    else if (lpBlock->mpClass)
    {
        if (lpBlock->mbHasParameters)
        {
            for (u8 luIndex = 0; luIndex < lpBlock->muParameterCount; ++luIndex)
                lpParameters[luIndex] = (std::max)(lpRanges[2 * luIndex],
                    (std::min)(lpRanges[2 * luIndex + 1], lpParameters[luIndex]));
        }
        lpBlock->mpClass->SetMemberDataFast(lpParameters);
    }
    s32 liRefCount = 0;
    if (lpBlock->mpClass)
        lpBlock->mpClass->GetRefCount(&liRefCount);
    return liRefCount;
}

struct SetGlobalBlock
{
    Csis::GlobalVariableHandle mHandle;
    s32 miMinimum;
    s32 miMaximum;
    s32 miPrevious;
    s32 miPadding;
    s32 miInput;
};

s32 UpdateSetGlobalVariable(void* apData)
{
    SetGlobalBlock* lpBlock = static_cast<SetGlobalBlock*>(apData);
    if (lpBlock->miPrevious != lpBlock->miInput)
    {
        lpBlock->miPrevious = lpBlock->miInput;
        CsisDef::Parameter lValue = {};
        lValue.intVal = (std::max)(lpBlock->miMinimum,
            (std::min)(lpBlock->miMaximum, lpBlock->miInput));
        Csis::GlobalVariable::SetFast(&lpBlock->mHandle, &lValue);
    }
    return 0;
}

typedef s32 (*AemsUpdateFunction)(void*);

AemsUpdateFunction const gapAemsUpdateFunctions[40] =
{
    &UpdateClassDestructor, &UpdateClassData, &UpdateClassDataTail,
    &UpdateCreate, &UpdateDestroy, &UpdateCallFunction, &UpdateCounter,
    &UpdateRandom, &UpdateRandomShuffle, &UpdateRandomWeighted,
    &UpdateRangeTrigger, &UpdateDelayTrigger, &UpdateStateGenerator,
    &UpdateMerge, &UpdateEnvelope, &UpdateTable, &UpdateDelayLine,
    &UpdateMux, &UpdateDemux, &UpdateMinimum, &UpdateMaximum, &UpdateScale,
    &UpdateAdd, &UpdateSubtract, &UpdateMultiply, &UpdateDivide, &UpdateModulo,
    &UpdatePlayer, &UpdateOscillator, &UpdateRamp, &UpdateAddMaximum,
    &UpdateSubtractMinimum, &UpdateMultiplyMaximum, &UpdateMinimum2,
    &UpdateMaximum2, &UpdateScale2, &UpdateAdd2, &UpdateFunction,
    &UpdateControlClass, &UpdateSetGlobalVariable
};

struct ModuleInstanceHeader
{
    InstanceLink mModuleLink;
    InstanceLink mUpdateLink;
    const u8* mpProgram;
    u8* mpData;
};

struct BytecodeAssignment
{
    s32 miSourceOffset;
    s32 miDestinationOffset;
};

void InterpretModule(ModuleInstanceHeader* apInstance)
{
    const u8* lpProgram = apInstance->mpProgram;
    u8* lpData = apInstance->mpData;
    while (lpProgram && lpProgram[0] != 0xFFu)
    {
        const u8 luOpcode = lpProgram[0];
        const u8 luAssignmentCount = lpProgram[1];
        if (luOpcode >= 40)
            return;
        const s32 liResult = gapAemsUpdateFunctions[luOpcode](lpData);
        const BytecodeAssignment* lpAssignments =
            reinterpret_cast<const BytecodeAssignment*>(lpProgram + 4);
        for (u8 luAssignment = 0; luAssignment < luAssignmentCount;
             ++luAssignment)
        {
            const BytecodeAssignment& lrAssignment = lpAssignments[luAssignment];
            s32* lpDestination = reinterpret_cast<s32*>(
                lpData + lrAssignment.miDestinationOffset);
            *lpDestination = lrAssignment.miSourceOffset == -1
                ? liResult
                : *reinterpret_cast<const s32*>(
                    lpData + lrAssignment.miSourceOffset);
        }
        const s32* lpDataDisplacement = reinterpret_cast<const s32*>(
            lpAssignments + luAssignmentCount);
        lpData += *lpDataDisplacement;
        lpProgram = reinterpret_cast<const u8*>(lpDataDisplacement + 1);
    }
}

void AemsTimerCallback(void*, f32 afPeriod)
{
    if (afPeriod <= 0.0f)
        afPeriod = 256.0f / 48000.0f;
    if (afPeriod != gfAemsTimerPeriod)
    {
        gfAemsTimerPeriod = afPeriod;
        const f32 lfTargetPeriod = 1.0f / 41.599998474121094f;
        giAemsUpdateInterval = (std::max)(1,
            static_cast<s32>(std::ceil(lfTargetPeriod / afPeriod)) - 1);
        giAemsUpdateCountdown = giAemsUpdateInterval;
        gfAemsStepMilliseconds = static_cast<f32>(giAemsUpdateInterval) *
                                 afPeriod * 1000.0f;
    }
    if (--giAemsUpdateCountdown != 0)
        return;
    giAemsUpdateCountdown = giAemsUpdateInterval;

    InstanceLink* lpLink = gpInstanceUpdateList;
    while (lpLink)
    {
        InstanceLink* lpNext = lpLink->mpNext;
        ModuleInstanceHeader* lpInstance = reinterpret_cast<ModuleInstanceHeader*>(
            reinterpret_cast<u8*>(lpLink) - offsetof(ModuleInstanceHeader, mUpdateLink));
        InterpretModule(lpInstance);
        lpLink = lpNext;
    }
}

bool AddAemsTimer()
{
    if (gbAemsTimerAdded)
        return true;
    if (rw::audio::core::TimerManager::AddTimer(
            &off_83271928->mTimerManager, &gAemsTimerHandle,
            &AemsTimerCallback, 0, "Aems Modules", 0, 0) != 0)
        return false;
    gbAemsTimerAdded = true;
    return true;
}

void RemoveAemsTimer()
{
    if (!gbAemsTimerAdded)
        return;
    rw::audio::core::TimerManager::RemoveTimer(
        &off_83271928->mTimerManager, &gAemsTimerHandle);
    gbAemsTimerAdded = false;
    gfAemsTimerPeriod = -1.0f;
}
}

// One record slot queued into the audio System's deferred-command ring by
// BeginRemoveModuleBank; replayed on the audio thread by Snd9::RemoveModuleBankHandler.
// The X360 lays it out in 12 bytes (4-byte handler pointer); on the host it is 16.
// Offsets in the member comments are the console ones the asm encodes.
struct RemoveModuleBankCommand
{
    int (*mpfnHandler)(void* apCommand); // +0x00  &Snd9::RemoveModuleBankHandler
    u64   muReserved;                    // +0x08  always 0
    s32   miBankHandle;                  // +0x10  the removed bank's handle
    u32   muPadding;
};

static_assert(sizeof(RemoveModuleBankCommand) == 0x18,
              "native AEMS remove command");

// ----------------------------------------------------------------------------
// Snd9::Aems::BeginRemoveModuleBank @ 0x82B72250
// ----------------------------------------------------------------------------
s32 BeginRemoveModuleBank(s32 aiBankHandle)
{
    rw::audio::core::System* lpSystem = off_83271928;
    rw::audio::core::System::Lock(lpSystem);

    // Locate the registered bank whose handle matches (registry is threaded through
    // each bank's embedded +0x50 link, so recover the owning bank with container-of).
    ModuleBank* lpBank = 0;
    for (ModuleBankLink* lpLink = gpModuleBankListHead; lpLink; lpLink = lpLink->mpNext)
    {
        ModuleBank* lpCandidate = ModuleBankFromLink(lpLink);
        if (lpCandidate->miBankHandle == aiBankHandle)
        {
            lpBank = lpCandidate;
            break;
        }
    }

    s32 liResult;
    if (!lpBank)
    {
        liResult = KI_MODULE_BANK_NOT_FOUND;
    }
    else
    {
        ModuleRecord* lpRecord = reinterpret_cast<ModuleRecord*>(
            reinterpret_cast<u8*>(lpBank) + lpBank->muModuleTableOffset);
        const u32 luModuleCount = lpBank->muModuleCount;
        for (u32 luModule = 0; luModule < luModuleCount; ++luModule)
        {
            Csis::Class::UnsubscribeConstructorFast(
                &lpRecord->mClassHandle, &lpRecord->mConstructorNode);

            u8* lpInstance = static_cast<u8*>(lpRecord->mpInstanceList);
            while (lpInstance)
            {
                u8* lpNext = *reinterpret_cast<u8**>(lpInstance);
                InstanceDestroySubscriber* lpSubscriber =
                    reinterpret_cast<InstanceDestroySubscriber*>(
                        lpInstance + lpRecord->miDestroyDataOffset);
                lpSubscriber->miDestroyPending = 1;
                SNDAEMSI_updatedestroy(lpSubscriber);
                lpInstance = lpNext;
            }
            lpRecord = NextModuleRecord(lpRecord);
        }

        // Free the bank's auxiliary allocation, mark it removed, and queue a deferred
        // RemoveModuleBankHandler command onto the System's command ring (one
        // RemoveModuleBankCommand slot -- 12 bytes on the X360).
        if (lpBank->mpAllocatedBlock)
        {
            rw::audio::core::System::Free(lpSystem, lpBank->mpAllocatedBlock, 0);
        }
        lpBank->miRemoved = 1;

        const u32 luCursor = lpSystem->muDeferredRingCursor;
        RemoveModuleBankCommand* lpCmd = reinterpret_cast<RemoveModuleBankCommand*>(
            lpSystem->mpDeferredRingBase + luCursor);
        // RECORD STRIDE (X360-literal trap): the asm's `addi r10,r10,0xC` @0x82B72358 IS
        // the console sizeof(RemoveModuleBankCommand) ({int(*)(void*), u32, s32} = 4+4+4).
        // The x64 ABI arbiter lays the widened command out as 24 bytes with the bank
        // handle at +0x10. The ring consumer advances by the handler's return, so the
        // enqueue stride is the native host sizeof rather than the literal console 12.
        lpSystem->muDeferredRingCursor =
            luCursor + static_cast<u32>(sizeof(RemoveModuleBankCommand)); // X360: +0xC
        lpCmd->mpfnHandler  = &Snd9::RemoveModuleBankHandler;
        lpCmd->muReserved   = 0;
        lpCmd->miBankHandle = aiBankHandle;
        lpCmd->muPadding    = 0;
        liResult = 0;
    }

    rw::audio::core::System::Unlock(lpSystem);
    return liResult;
}

// ----------------------------------------------------------------------------
// Snd9::Aems::IsModuleBankRemoved @ 0x82B71720
// ----------------------------------------------------------------------------
bool IsModuleBankRemoved(s32 aiBankHandle)
{
    // While the AEMS system is inactive there are no live banks: report removed
    // without touching the registry lock.
    if (!gbAemsActive)
    {
        return true;
    }

    rw::audio::core::System* lpSystem = off_83271928;
    rw::audio::core::System::Lock(lpSystem);

    bool lbRemoved = true;
    for (ModuleBankLink* lpLink = gpModuleBankListHead; lpLink; lpLink = lpLink->mpNext)
    {
        if (ModuleBankFromLink(lpLink)->miBankHandle == aiBankHandle)
        {
            lbRemoved = false;
            break;
        }
    }

    rw::audio::core::System::Unlock(lpSystem);
    return lbRemoved;
}

// ----------------------------------------------------------------------------
// Snd9::Aems::SetSamplePlayerFactory @ 0x82B6FCD0
// ----------------------------------------------------------------------------
IAemsSamplePlayerFactory* SetSamplePlayerFactory(IAemsSamplePlayerFactory* apFactory)
{
    gpSamplePlayerFactory = apFactory;
    return apFactory;
}

} // namespace Aems

// ----------------------------------------------------------------------------
// Snd9::RemoveModuleBankHandler @0x82B71650 / XB1 native-8 sub_14096C1F0.
// ----------------------------------------------------------------------------
int RemoveModuleBankHandler(void* apCommand)
{
    Aems::RemoveModuleBankCommand* lpCommand =
        static_cast<Aems::RemoveModuleBankCommand*>(apCommand);

    for (Aems::ModuleBankLink* lpLink = Aems::gpModuleBankListHead;
         lpLink; lpLink = lpLink->mpNext)
    {
        Aems::ModuleBank* lpBank = Aems::ModuleBankFromLink(lpLink);
        if (lpBank->miBankHandle != lpCommand->miBankHandle)
            continue;

        if (lpLink->mppPrev)
            *lpLink->mppPrev = lpLink->mpNext;
        if (lpLink->mpNext)
            lpLink->mpNext->mppPrev = lpLink->mppPrev;
        break;
    }
    if (!Aems::gpModuleBankListHead)
    {
        Aems::RemoveAemsTimer();
        Aems::gbAemsActive = false;
    }
    return static_cast<int>(sizeof(Aems::RemoveModuleBankCommand));
}

} // namespace Snd9

namespace
{
    s32 gNextModuleBankHandle = 0;
}

extern "C" s32 SNDAEMS_addmodulebank(void* apBank, s32 aiStreamFileOffset,
                                     s32 /*aiFlags*/,
                                     SNDAEMSModuleBankAllocator apAllocator)
{
    if (apBank == 0 || apAllocator == 0)
        return -6;

    Snd9::Aems::ModuleBank* lpSource =
        static_cast<Snd9::Aems::ModuleBank*>(apBank);
    if (std::memcmp(lpSource->maFileHeader, "ABKC", 4) != 0 ||
        lpSource->maFileHeader[8] != 10 ||
        lpSource->muTotalSize < sizeof(Snd9::Aems::ModuleBank) ||
        lpSource->muResidentSize < sizeof(Snd9::Aems::ModuleBank) ||
        lpSource->muResidentSize > lpSource->muTotalSize ||
        lpSource->muModuleTableOffset < sizeof(Snd9::Aems::ModuleBank) ||
        lpSource->muModuleTableOffset >= lpSource->muResidentSize)
        return -6;

    s32 liHandle = ++gNextModuleBankHandle;
    if (liHandle < 0)
    {
        gNextModuleBankHandle = 1;
        liHandle = 1;
    }
    lpSource->miBankHandle = liHandle;

    // The callback sizes and resident/sample copy are the exact add-bank contract.
    // The native-8 layout keeps these serialized scalar offsets in their ARTIST
    // positions; pointer-bearing runtime fields widen beginning at +0x40.
    const s32 liTotalSize = static_cast<s32>(lpSource->muTotalSize);
    const s32 liResidentSize = static_cast<s32>(lpSource->muResidentSize);
    const s32 liSampleOffset = static_cast<s32>(lpSource->muSampleOffset);
    const s32 liSampleSize = static_cast<s32>(lpSource->muSampleSize);

    if (liResidentSize <= 0 || liTotalSize < liResidentSize ||
        liSampleOffset < 0 || liSampleSize < 0)
        return -6;

    const u8* lpSourceSample = liSampleOffset != 0
        ? static_cast<const u8*>(apBank) + liSampleOffset : 0;
    lpSource->mpSampleBank = const_cast<u8*>(lpSourceSample);

    Snd9::Aems::ModuleBank* lpBank = static_cast<Snd9::Aems::ModuleBank*>(
        apAllocator(apBank, liResidentSize + liSampleSize, liTotalSize));
    if (lpBank == 0)
        return -6;

    if (lpBank != lpSource)
    {
        std::memcpy(lpBank, lpSource, static_cast<size_t>(liResidentSize));
        if (liSampleOffset != 0)
        {
            std::memcpy(static_cast<u8*>(static_cast<void*>(lpBank)) + liResidentSize,
                        lpSourceSample, static_cast<size_t>(liSampleSize));
            lpBank->mpSampleBank =
                static_cast<u8*>(static_cast<void*>(lpBank)) + liSampleOffset;
        }
    }

    const u8* lpSourceBase = reinterpret_cast<const u8*>(lpSource);
    if (!Snd9::Aems::ValidSpan(lpSource, lpSource->muFunctionRelocOffset, sizeof(u32)) ||
        *reinterpret_cast<const u32*>(lpSourceBase + lpSource->muFunctionRelocOffset) != 0 ||
        !Snd9::Aems::ApplyPointerRelocations(lpBank, lpSource) ||
        !Snd9::Aems::ApplyCsisRelocations(lpBank, lpSource))
        return -6;

    lpBank->miRemoved = 0;
    lpBank->mpAllocatedBlock = 0;
    lpBank->miStreamFileOffset = aiStreamFileOffset;

    Snd9::Aems::ModuleRecord* lpRecord =
        reinterpret_cast<Snd9::Aems::ModuleRecord*>(
            reinterpret_cast<u8*>(lpBank) + lpBank->muModuleTableOffset);
    u16 luSubscribedModules = 0;
    const auto RollbackConstructors = [&]()
    {
        Snd9::Aems::ModuleRecord* lpRollback =
            reinterpret_cast<Snd9::Aems::ModuleRecord*>(
                reinterpret_cast<u8*>(lpBank) + lpBank->muModuleTableOffset);
        for (u16 luModule = 0; luModule < luSubscribedModules; ++luModule)
        {
            Csis::Class::UnsubscribeConstructorFast(
                &lpRollback->mClassHandle, &lpRollback->mConstructorNode);
            lpRollback = Snd9::Aems::NextModuleRecord(lpRollback);
        }
        luSubscribedModules = 0;
    };
    for (u16 luModule = 0; luModule < lpBank->muModuleCount; ++luModule)
    {
        const size_t luRecordOffset = reinterpret_cast<u8*>(lpRecord) -
                                      reinterpret_cast<u8*>(lpBank);
        const size_t luRecordSize = sizeof(Snd9::Aems::ModuleRecord) +
            sizeof(s32) * (lpRecord->muSamplePlayerCount + lpRecord->muAlternateCount);
        if (luRecordOffset > lpBank->muResidentSize ||
            luRecordSize > lpBank->muResidentSize - luRecordOffset)
        {
            RollbackConstructors();
            return -6;
        }

        if (lpRecord->mpProgram)
            lpRecord->mpProgram = reinterpret_cast<u8*>(lpBank) +
                reinterpret_cast<uintptr_t>(lpRecord->mpProgram);
        if (lpRecord->mpInstanceTemplate)
            lpRecord->mpInstanceTemplate = reinterpret_cast<u8*>(lpBank) +
                reinterpret_cast<uintptr_t>(lpRecord->mpInstanceTemplate);

        lpRecord->mConstructorNode.mpfnConstructor = &Snd9::Aems::AemsConstructorCallback;
        lpRecord->mConstructorNode.mpClientData = lpRecord;
        // XB1 sub_14096C510 calls the constructor subscription and continues
        // without inspecting its result.  Keep that ordering: bank admission is
        // independent of whether its CSIS class has arrived in the registry yet.
        Csis::Class::SubscribeConstructorFast(
            &lpRecord->mClassHandle, &lpRecord->mConstructorNode);
        ++luSubscribedModules;

        s32* lpOffsets = Snd9::Aems::ModuleRecordOffsets(lpRecord);
        for (u8 luPlayer = 0; luPlayer < lpRecord->muSamplePlayerCount; ++luPlayer)
        {
            const s32 liOffset = lpOffsets[luPlayer];
            if (liOffset < 0 || static_cast<u32>(liOffset) + sizeof(void*) >
                    lpRecord->muInstanceSize)
            {
                RollbackConstructors();
                return -6;
            }
            *reinterpret_cast<void**>(
                reinterpret_cast<u8*>(lpRecord->mpInstanceTemplate) + liOffset) = lpBank;
        }
        lpRecord = Snd9::Aems::NextModuleRecord(lpRecord);
    }

    lpBank->mLink.mpNext = Snd9::Aems::gpModuleBankListHead;
    lpBank->mLink.mppPrev = &Snd9::Aems::gpModuleBankListHead;
    if (lpBank->mLink.mpNext)
        lpBank->mLink.mpNext->mppPrev = &lpBank->mLink.mpNext;
    Snd9::Aems::gpModuleBankListHead = &lpBank->mLink;
    // Native sub_14096C510 returns the timer-manager status to its caller, but
    // SNDAEMS_addmodulebank (sub_14096C960) ignores that value and returns the
    // bank handle.  Do not turn timer registration into a bank-format failure.
    Snd9::Aems::AddAemsTimer();
    Snd9::Aems::gbAemsActive = true;
    return lpBank->miBankHandle;
}

extern "C" void SNDAEMSI_CreateModuleInstance(Csis::Class* apClass,
                                                void* /*apParameters*/,
                                                void* apModule)
{
    using namespace Snd9::Aems;
    ModuleRecord* lpModule = static_cast<ModuleRecord*>(apModule);
    rw::audio::core::System* lpSystem = off_83271928;
    rw::audio::core::System::Lock(lpSystem);

    if (!lpModule || lpModule->muCurrentInstances >= lpModule->muMaxInstances ||
        !lpModule->mpInstanceTemplate || lpModule->muInstanceSize < 0x30)
    {
        rw::audio::core::System::Unlock(lpSystem);
        return;
    }

    u8* lpInstance = static_cast<u8*>(rw::audio::core::System::Alloc(
        lpSystem, lpModule->muInstanceSize, "AEMS Module Instance", 16, 0));
    if (!lpInstance)
    {
        rw::audio::core::System::Unlock(lpSystem);
        return;
    }
    std::memcpy(lpInstance, lpModule->mpInstanceTemplate, lpModule->muInstanceSize);

    InstanceDestroySubscriber* lpDestroy =
        reinterpret_cast<InstanceDestroySubscriber*>(
            lpInstance + lpModule->miDestroyDataOffset);
    lpDestroy->mpModule = lpModule;
    lpDestroy->mpInstance = lpInstance;
    lpDestroy->mpClass = apClass;

    ModuleInstanceHeader* lpHeader =
        reinterpret_cast<ModuleInstanceHeader*>(lpInstance);
    lpHeader->mModuleLink.mpNext =
        static_cast<InstanceLink*>(lpModule->mpInstanceList);
    lpHeader->mModuleLink.mpPrev = 0;
    if (lpHeader->mModuleLink.mpNext)
        lpHeader->mModuleLink.mpNext->mpPrev = &lpHeader->mModuleLink;
    lpModule->mpInstanceList = &lpHeader->mModuleLink;

    InstanceLink* lpDestroyLink = &lpHeader->mUpdateLink;
    lpDestroyLink->mpNext = gpInstanceUpdateList;
    lpDestroyLink->mpPrev = 0;
    if (gpInstanceUpdateList)
        gpInstanceUpdateList->mpPrev = lpDestroyLink;
    gpInstanceUpdateList = lpDestroyLink;

    lpHeader->mpProgram = static_cast<const u8*>(lpModule->mpProgram);
    lpHeader->mpData = lpInstance + sizeof(ModuleInstanceHeader);

    u8* lpCursor = lpHeader->mpData;
    if (lpModule->mbHasDestructor)
    {
        Csis::ClassClientNode* lpNode =
            reinterpret_cast<Csis::ClassClientNode*>(lpCursor);
        lpNode->mpfnDestructor = &AemsDestructorCallback;
        lpNode->mpClientData = lpCursor;
        apClass->SubscribeDestructorFast(lpNode);
        lpCursor += 0x28;
    }

    for (u16 luGlobal = 0; luGlobal < lpModule->muGlobalCount; ++luGlobal)
    {
        Csis::GlobalVariableHandle* lpHandle =
            reinterpret_cast<Csis::GlobalVariableHandle*>(lpCursor);
        Csis::GlobalVariableSubscriber* lpNode =
            reinterpret_cast<Csis::GlobalVariableSubscriber*>(lpCursor + 0x10);
        lpNode->pfnCallback = &AemsGlobalCallback;
        lpNode->pUserData = lpCursor;
        Csis::GlobalVariable::SubscribeFast(lpHandle, lpNode);
        lpCursor += 0x38;
    }

    if (lpModule->mbHasClassData)
    {
        Csis::ClassClientNode* lpNode =
            reinterpret_cast<Csis::ClassClientNode*>(lpCursor);
        lpNode->mpfnMemberData = &AemsMemberDataCallback;
        lpNode->mpClientData = lpCursor;
        apClass->SubscribeMemberDataFast(lpNode);
        lpCursor += 0x28 + sizeof(u32) * lpCursor[0x20];
    }

    lpCursor = reinterpret_cast<u8*>((reinterpret_cast<uintptr_t>(lpCursor) + 7u) & ~uintptr_t(7));
    for (u16 luFunction = 0; luFunction < lpModule->muFunctionCount; ++luFunction)
    {
        Csis::Function* lpFunction = reinterpret_cast<Csis::Function*>(lpCursor);
        Csis::Function::Subscriber* lpNode =
            reinterpret_cast<Csis::Function::Subscriber*>(lpCursor + 0x10);
        lpNode->mpfnCallback = &AemsFunctionCallback;
        lpNode->mpContext = lpCursor;
        lpFunction->SubscribeFast(lpNode);
        lpCursor += 0x38 + sizeof(u32) * lpCursor[0x30];
        lpCursor = reinterpret_cast<u8*>((reinterpret_cast<uintptr_t>(lpCursor) + 7u) & ~uintptr_t(7));
    }

    ++lpModule->muCurrentInstances;
    rw::audio::core::System::Unlock(lpSystem);
}

extern "C" int SNDAEMSI_updatedestroy(void* apSubscriber)
{
    using namespace Snd9::Aems;
    InstanceDestroySubscriber* lpDestroy =
        static_cast<InstanceDestroySubscriber*>(apSubscriber);
    if (!lpDestroy || lpDestroy->miDestroyPending == 0)
        return 0;

    ModuleRecord* lpModule = lpDestroy->mpModule;
    u8* lpInstance = static_cast<u8*>(lpDestroy->mpInstance);
    Csis::Class* lpClass = lpDestroy->mpClass;
    if (!lpModule || !lpInstance)
        return 0;

    InstanceLink* lpModuleHead = static_cast<InstanceLink*>(lpModule->mpInstanceList);
    Unlink(lpModuleHead, reinterpret_cast<InstanceLink*>(lpInstance));
    lpModule->mpInstanceList = lpModuleHead;
    Unlink(gpInstanceUpdateList, reinterpret_cast<InstanceLink*>(lpInstance + 0x10));

    u8* lpCursor = lpInstance + 0x30;
    if (lpModule->mbHasDestructor)
    {
        lpClass->UnsubscribeDestructorFast(
            reinterpret_cast<Csis::ClassClientNode*>(lpCursor));
        lpCursor += 0x28;
    }
    for (u16 luGlobal = 0; luGlobal < lpModule->muGlobalCount; ++luGlobal)
    {
        Csis::GlobalVariable::UnsubscribeFast(
            reinterpret_cast<Csis::GlobalVariableHandle*>(lpCursor),
            reinterpret_cast<Csis::GlobalVariableSubscriber*>(lpCursor + 0x10));
        lpCursor += 0x38;
    }
    if (lpModule->mbHasClassData)
    {
        lpClass->UnsubscribeMemberDataFast(
            reinterpret_cast<Csis::ClassClientNode*>(lpCursor));
        lpCursor += 0x28 + sizeof(u32) * lpCursor[0x20];
    }
    lpCursor = reinterpret_cast<u8*>((reinterpret_cast<uintptr_t>(lpCursor) + 7u) & ~uintptr_t(7));
    for (u16 luFunction = 0; luFunction < lpModule->muFunctionCount; ++luFunction)
    {
        Csis::Function* lpFunction = reinterpret_cast<Csis::Function*>(lpCursor);
        lpFunction->UnsubscribeFast(
            reinterpret_cast<Csis::Function::Subscriber*>(lpCursor + 0x10));
        lpCursor += 0x38 + sizeof(u32) * lpCursor[0x30];
        lpCursor = reinterpret_cast<u8*>((reinterpret_cast<uintptr_t>(lpCursor) + 7u) & ~uintptr_t(7));
    }

    s32* lpOffsets = ModuleRecordOffsets(lpModule);
    for (u8 luPlayer = 0; luPlayer < lpModule->muSamplePlayerCount; ++luPlayer)
    {
        u8* lpSlot = lpInstance + lpOffsets[luPlayer];
        Snd9::IAemsSamplePlayer*& lrpPlayer =
            reinterpret_cast<AemsPlayerBlock*>(lpSlot)->mpPlayer;
        if (lrpPlayer)
        {
            lrpPlayer->Release();
            lrpPlayer = 0;
        }
    }
    for (u8 luAlternate = 0; luAlternate < lpModule->muAlternateCount; ++luAlternate)
    {
        u8* lpSlot = lpInstance + lpOffsets[lpModule->muSamplePlayerCount + luAlternate];
        Csis::Class*& lrpAlternate =
            reinterpret_cast<ControlClassBlock*>(lpSlot)->mpClass;
        if (lrpAlternate)
        {
            lrpAlternate->Release();
            lrpAlternate = 0;
        }
    }

    if (lpModule->muCurrentInstances)
        --lpModule->muCurrentInstances;
    rw::audio::core::System::Free(off_83271928, lpInstance, 0);
    return 0;
}
