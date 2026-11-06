// ============================================================================
// CgsAemsFactory.cpp -- CgsSound::Playback::AemsFactory runtime bodies.
//
// Bodied from BURNOUT_X360_ARTIST.XEX:
//   AemsFactory::CsisPrint(const char*)        @ 0x8268A018
//   AemsFactory::FindPatchMonitor(const char*) @ 0x82689E98
//   AemsFactory::UpdateAEMSPatches(f32)        @ 0x826A2C30
//   AemsFactory::AddPatchMonitor(ClassData*)   @ 0x82689EF8 (no ARTIST export: DecFIGS 0x82E954)
//
// CsisPrint @ 0x8268A018:
//   if (CgsDev::Message::gxMessageFilterFlags & 1)
//       (*gpDebugPrint)[vslot 1](gpDebugPrint, str ? str : "<NULLSTRING>");
// The X360 calls the gpDebugPrint stream's second vtable slot, which is the
// committed DebugPrint::operator<<(const char*) (CgsLog.h). So the print routes
// through `*gpDebugPrint << text` exactly as the engine logging convention.
// DWARF (CgsAemsFactory.cpp:397) confirms this is `protected` and returns
// `void`; the tail-called vtable slot's own return value is not propagated.
//
// FindPatchMonitor @ 0x82689E98:
//   if (muPatchMonitorCount == 0) return 0;
//   for each maPatchMonitors[i] (i < count):
//       byte-compare maPatchMonitors[i].mpName against lpcName;  // strcmp
//       if equal -> return &maPatchMonitors[i];
//   return 0;
// The X360 inner do/while is a hand-inlined strcmp over the two C strings; the
// outer loop walks the table and bails out at the registered count. Called by
// AemsFactory::AddPatchMonitor.
// ============================================================================

#include "GameShared/GameClasses/Sound/Playback/AEMS/CgsAemsFactory.h"
#include "GameShared/GameClasses/Sound/Playback/AEMS/CgsAemsContent.h"
#include "GameShared/GameClasses/Sound/Playback/AEMS/CgsAemsPlayerVoice.h"
#include "GameShared/GameClasses/Sound/Playback/RWAC/CgsGenericRwacFactory.h"
#include "GameShared/GameClasses/Development/Log/CgsLog.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"
#include "GameShared/GameClasses/Development/PerfMon/Cpu/CgsPerfMonCpu.h"

#include "rw/rwcore_structs.h"      // rw::IResourceAllocator / BaseResourceDescriptors (the carve)
#include "SDKs/Csis/CsisSystem.h"   // Csis::System::IsInited
#include "SDKs/Csis/CsisClass.h"
#include "SDKs/EATech/include/snd/sndaems.h" // Snd9::Aems::SetSamplePlayerFactory

#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <new>      // placement new (the in-carve construct)

namespace CgsSound
{
namespace Playback
{

namespace
{
    // The interned factory name (console dword_83008664, written at static-init
    // by sub_82C65788 = Name::MakeHash("~AemsFactory::SK_NAME~")).
    const Name skAemsFactoryName("~AemsFactory::SK_NAME~");
    const Name skAemsContentType("~AemsContent::SK_CONTENT_TYPE~");
    const Name skCsisContentType("~CsisContent::SK_CONTENT_TYPE~");

    // The five-pair allocator-descriptor inline (the RWAC/Module TU-local convention).
    void* AllocateMemoryResource(rw::IResourceAllocator* lpAllocator, size_t luSize,
                                 u32 luAlignment, const char* lpcName)
    {
        rw::BaseResourceDescriptors<5> lDescriptor;
        for (u32 luEntry = 0u; luEntry < 5u; ++luEntry)
        {
            lDescriptor.m_baseResourceDescriptors[luEntry].m_size      = 0u;
            lDescriptor.m_baseResourceDescriptors[luEntry].m_alignment = 1u;
        }
        lDescriptor.m_baseResourceDescriptors[0].m_size      = static_cast<u32>(luSize);
        lDescriptor.m_baseResourceDescriptors[0].m_alignment = luAlignment;

        rw::Resource lResource = lpAllocator->DoAllocate(
            reinterpret_cast<const rw::ResourceDescriptor&>(lDescriptor), lpcName);
        return lResource.m_baseResources[0];
    }
}

// ---------------------------------------------------------------------------
// AemsFactory::Create @ 0x826DAC28  (AEMS-cascade slice 2; full decode
// progress/scratch_dossiers/aems_factory_cascade_codex.md). Ref-spec ABI;
// console carve 4*(entityCount+0x162)+data+strings (0x162 words == the fixed
// head through the +0x56C registry header) at host widths; the returned
// handle's ref rides the Factory-subobject refcount (console overall +0x08 --
// the MI proof), which on the host is the same Object::Acquire through the
// Factory base.
// ---------------------------------------------------------------------------
Handle<AemsFactory> AemsFactory::Create(Environment& arEnvironment,
                                        const AemsFactorySpec& akrSpec)
{
    const size_t luBytes = sizeof(AemsFactory)
                         + sizeof(Registry)
                         + sizeof(void*) * akrSpec.mu32EntityCount
                         + akrSpec.mu32DataSize
                         + akrSpec.mu32StringTableSize;

    void* lpMemory = AllocateMemoryResource(arEnvironment.GetAllocator(), luBytes,
                                            4, "AemsFactory");
    if (lpMemory == 0)
    {
        return Handle<AemsFactory>();
    }

    AemsFactory* lpFactory = ::new (lpMemory) AemsFactory(arEnvironment, akrSpec);

    // The returned handle's owned ref (console `stw ptr` + the +0x08 increment;
    // interim plain-store Handle model -- the explicit Acquire beside the store).
    lpFactory->Acquire();
    return Handle<AemsFactory>(lpFactory);
}

// ---------------------------------------------------------------------------
// AemsFactory ctor @ 0x826DAAD0. Console store order after the AemsRW base:
// the final vptr pair (host: compiler-emitted), the retained RWAC handle
// (+0x64 + the pointee refcount increment), the two command-queue control
// words, the registry pointer + the in-place Registry (+0x56C), the CSIS
// checks, and the global sample-player-factory install.
// ---------------------------------------------------------------------------
AemsFactory::AemsFactory(Environment& arEnvironment, const AemsFactorySpec& akrSpec)
    : AemsRWSampleFactory(skAemsFactoryName, arEnvironment)
{
    // +0x64: the retained RWAC factory handle (raw pointer + explicit retain --
    // the console's `old count + 1` on the pointee when non-null).
    mpRwacFactory = static_cast<GenericRwacFactory*>(akrSpec.mpRwacFactory);
    if (mpRwacFactory != 0)
    {
        mpRwacFactory->Acquire();
    }

    // +0x60 -> the in-place Registry immediately after the fixed head (console +0x56C).
    RegistrySpec lRegistrySpec;
    lRegistrySpec.mu32EntityCount   = akrSpec.mu32EntityCount;
    lRegistrySpec.muDataSize        = akrSpec.mu32DataSize;
    lRegistrySpec.muStringTableSize = akrSpec.mu32StringTableSize;
    mpRegistry = ::new (reinterpret_cast<u8*>(this) + sizeof(AemsFactory))
        Registry(lRegistrySpec);

    // The CSIS integration tail:
    //  1. Csis::System::IsInited @0x82B10040, asserted (console fires when the
    //     low byte is zero).
    CGS_ASSERT(Csis::System::IsInited() != 0, "Csis::System::IsInited()");
    //  2. The console materializes &CsisPrint (the static one-arg callback --
    //     the ABI-corrected shape) and calls 0x82B0F1B8 with it. That callee's
    //     own dossier is a two-instruction `li r3,0; blr` leaf: it neither
    //     reads the callback nor writes state, so there is NO registration to
    //     reproduce -- promoting it into an invented CSIS callback registry is
    //     exactly what the decode warns against. The address expression is kept
    //     here so the callback's shape stays load-bearing.
    (void)&AemsFactory::CsisPrint;
    //  3. Snd9::Aems::SetSamplePlayerFactory(this) @0x82B6FCD0 -- the process
    //     global (off_82F87DBC) now points at this factory's IAems interface.
    Snd9::Aems::SetSamplePlayerFactory(this);

    // +0x5C := 0 -- LAST, after the CSIS check (console order 9).
    muPatchMonitorCount = 0;
}

// The per-factory registry accessor (CgsSoundPlaybackModule.h:100 -- the console
// reads AemsFactory+0x60). The generic Factory* arrives as the Factory SUBOBJECT
// pointer (the environment table stores that, per the MI decode), so the cast
// walks down through the real hierarchy.
Registry* GetAemsFactoryRegistry(Factory* lpAemsFactory)
{
    return static_cast<AemsFactory*>(lpAemsFactory)->GetRegistry();
}

const Name& AemsFactorySkName()
{
    return skAemsFactoryName;
}

// ARTIST @0x826E9AC8. AEMS only accepts player voices. The constructed voice
// immediately queues its CSIS class binding and delegates the render graph to
// the retained GenericRwacFactory.
bool AemsFactory::DoCreateVoice(const VoiceSpec& akrSpec,
                                Handle<Voice>& arHandleOut,
                                u32 au32Ident)
{
    arHandleOut.SetObject(0);
    CGS_ASSERT(akrSpec.mu8VoiceType == E_PLAYER_VOICE,
               "E_PLAYER_VOICE == akrSpec.GetVoiceType()");
    if (akrSpec.mu8VoiceType != E_PLAYER_VOICE)
        return false;

    AemsPlayerVoice* lpVoice = new (*this, akrSpec)
        AemsPlayerVoice(*this, akrSpec, au32Ident);
    if (!lpVoice || !lpVoice->IsCreated())
        return false;

    lpVoice->Acquire();
    arHandleOut.SetObject(lpVoice);
    return true;
}

// ARTIST @0x826E9B98. The AEMS factory owns exactly two concrete content
// classes: native module banks and CSIS class/interface descriptors.
bool AemsFactory::DoCreateContent(const ContentSpec& akrSpec,
                                  Handle<Content>& arHandleOut,
                                  u32 au32Ident)
{
    const Name& lkrContentTypeName = akrSpec.GetContentType().GetName();
    Content* lpContent = 0;

    if (lkrContentTypeName == skCsisContentType)
    {
        lpContent = new (*this, akrSpec) CsisContent(*this, akrSpec, au32Ident);
    }
    else if (lkrContentTypeName == skAemsContentType)
    {
        lpContent = new (*this, akrSpec) AemsContent(*this, akrSpec, au32Ident);
    }

    if (lpContent)
        lpContent->Acquire();
    arHandleOut.SetObject(lpContent);
    return lpContent != 0;
}

// [FLAG PC witness] [aems] NOT IN THE X360 BINARY. BRN_AEMS_DIAG=1, capped; off by default.
namespace
{
    bool AemsDiagEnabled()
    {
        static const bool sbEnabled = std::getenv("BRN_AEMS_DIAG") != 0;
        return sbEnabled;
    }

    void AemsDiagPrintf(const char* lpcFormat, ...)
    {
        static u32 suLines = 0;
        if (!AemsDiagEnabled() || suLines >= 400u)
            return;
        ++suLines;
        char lacMsg[256];
        va_list lArgs;
        va_start(lArgs, lpcFormat);
        std::vsnprintf(lacMsg, sizeof(lacMsg), lpcFormat, lArgs);
        va_end(lArgs);
        CgsDev::Log::WriteToLog(lacMsg);
    }
}

// ARTIST @0x826C2358. Drain the fixed CSIS command ring on the playback thread, then run
// the live AEMS instances.
void AemsFactory::DoUpdate(f32 af32DeltaTime)
{
    CpuMonitors& lrMonitors = GetEnvironment().GetCpuMonitors();
    CgsDev::PerfMonCpu::StartMonitor(lrMonitors.miAemsFactoryUpdate);
    CgsDev::PerfMonCpu::StartMonitor(lrMonitors.miAemsFactoryUpdate2);

    uintptr_t laWords[16];
    for (;;)
    {
        u32 luCount = 16;
        if (!mCommandQueue.GetCommand(luCount, laWords))
            break;

        CGS_ASSERT(luCount != 0 && luCount <= 16, "luCommandCount");
        if (luCount == 0 || luCount > 16)
            continue;

        switch (static_cast<ECsisCommandType>(laWords[0]))
        {
        case E_CSIS_COMMAND_SET_CLASS_HANDLE:
        {
            CGS_ASSERT(luCount == 5, "SetClassHandle command size");
            Csis::ClassHandle* lpHandle =
                reinterpret_cast<Csis::ClassHandle*>(laWords[1]);
            Csis::InterfaceId lId;
            lId.mpName = reinterpret_cast<const char*>(laWords[2]);
            CGS_ASSERT(lId.mpName != 0, "lCommand.mpName");
            CGS_ASSERT(lpHandle != 0, "lCommand.mpHandle");
            lId.muSystemId = static_cast<u16>(laWords[3]);
            lId.muInterfaceId = static_cast<u16>(laWords[4]);
            const Csis::Result leResult = lpHandle->SetFast(&lId);
            CGS_ASSERT(static_cast<s32>(leResult) == 0, "Csis::RESULT_OK == lResult");
            break;
        }
        case E_CSIS_COMMAND_CREATE:
        {
            CGS_ASSERT(luCount == 4, "Create command size");
            Csis::ClassHandle* lpHandle =
                reinterpret_cast<Csis::ClassHandle*>(laWords[1]);
            void* lpParameters = reinterpret_cast<void*>(laWords[2]);
            Csis::Class** lppClass = reinterpret_cast<Csis::Class**>(laWords[3]);
            CGS_ASSERT(lpHandle != 0, "lCommand.mpHandle");
            CGS_ASSERT(lpParameters != 0, "lCommand.mpData");
            CGS_ASSERT(lppClass != 0, "lCommand.mppClass");
            const int liResult = Csis::Class::CreateInstanceFast(
                lpHandle, lpParameters, lppClass);
            CGS_ASSERT(liResult == 0, "Csis::RESULT_OK == lResult");
            AddPatchMonitor(*lppClass);    // 0x826C2578: right after the create, no guard
            break;
        }
        case E_CSIS_COMMAND_RELEASE:
        {
            CGS_ASSERT(luCount == 2, "Release command size");
            Csis::Class* lpClass = reinterpret_cast<Csis::Class*>(laWords[1]);
            CGS_ASSERT(lpClass != 0, "lCommand.mpClass");
            const int liResult = lpClass->ReleaseFast();
            CGS_ASSERT(liResult == 0, "Csis::RESULT_OK == lResult");
            break;
        }
        case E_CSIS_COMMAND_UPDATE:
        {
            CGS_ASSERT(luCount == 3, "Update command size");
            Csis::Class* lpClass = reinterpret_cast<Csis::Class*>(laWords[1]);
            void* lpData = reinterpret_cast<void*>(laWords[2]);
            CGS_ASSERT(lpClass != 0, "lCommand.mpClass");
            CGS_ASSERT(lpData != 0, "lCommand.mpData");
            const int liResult = lpClass->SetMemberDataFast(lpData);
            CGS_ASSERT(liResult == 0, "Csis::RESULT_OK == lResult");
            break;
        }
        default:
            CGS_ASSERT(false, "Invalid Command");
            break;
        }
    }

    UpdateAEMSPatches(af32DeltaTime);

    CgsDev::PerfMonCpu::StopMonitor(lrMonitors.miAemsFactoryUpdate);
    CgsDev::PerfMonCpu::StopMonitor(lrMonitors.miAemsFactoryUpdate2);
}

// ---------------------------------------------------------------------------
// AemsFactory::UpdateAEMSPatches(f32 af32TimeStep)  @ 0x826A2C30
//   flt_83277644 = step * 1000 (ms) for the walk, 10.0f after it. For each timer client
//   of off_832775EC (next read first: a tick may destroy its own instance): its monitor
//   (the inlined FindPatchMonitor(TIMERCLIENTFN*)), assert "lpPatchMonitor" (:282, NOT a
//   guard), Start/InterpretModule(fn, data)/Stop. ARTIST has no other caller of
//   InterpretModule and no AEMS timer: AddTimer's callers are the rwaudio plug-ins and
//   Snd9Service only. Runs under the System lock Environment::Update @0x826D7500 holds.
// ---------------------------------------------------------------------------
void AemsFactory::UpdateAEMSPatches(f32 af32TimeStep)
{
    Snd9::gVariableTimerPeriod = af32TimeStep * 1000.0f;

    u32 luInstances = 0;
    Snd9::Aems::InstanceLink* lpNode = Snd9::Aems::gpInstanceUpdateList;
    while (lpNode)
    {
        Snd9::Aems::InstanceLink* lpNext = lpNode->mpNext;
        const Snd9::Aems::TimerClient* lpClient =
            reinterpret_cast<const Snd9::Aems::TimerClient*>(lpNode);

        PatchMonitor* lpPatchMonitor = FindPatchMonitor(lpClient->mpClientFn);
        CGS_ASSERT(lpPatchMonitor != 0, "lpPatchMonitor");

        CgsDev::PerfMonCpu::StartMonitor(lpPatchMonitor->miPerfmon);
        Snd9::InterpretModule(lpClient->mpClientFn, lpClient->mpClientData);
        CgsDev::PerfMonCpu::StopMonitor(lpPatchMonitor->miPerfmon);

        lpNode = lpNext;
        ++luInstances;
    }

    if (AemsDiagEnabled())
    {
        static u32 suTicks = 0;
        if (suTicks < 5u || (luInstances != 0u && suTicks % 600u == 0u))
            AemsDiagPrintf("[FLAG PC witness] [aems] tick=%u stepMs=%.3f instances=%u monitors=%u\n",
                           suTicks, Snd9::gVariableTimerPeriod, luInstances, muPatchMonitorCount);
        ++suTicks;
    }

    Snd9::gVariableTimerPeriod = 10.0f;
}

// ---------------------------------------------------------------------------
// AemsFactory::AddPatchMonitor(ClassData*)  @ 0x82689EF8 (absent from the ARTIST export;
// DecFIGS @0x82E954, its only caller the same DoUpdate create arm as ARTIST @0x826C2358).
//   One monitor per class name. A class met again only refreshes the bytecode pointer
//   (a re-registered bank's new image); a new one fills the next slot and registers a
//   perf monitor: page 0x13 (the "Aems Update" page Module::Prepare registers), budget
//   0.25, scaled. The asserts are not guards.
//
// FLAG (native-64 record): the console reads the module and the name out of the 32-bit
// class descriptor. In the native record the name is muTargetOrOffset (the field
// CsisSystem.cpp's handle lookup compares with strcmp) and the module is the head constructor
// client's mpClientData (the only place SNDAEMS_addmodulebank stores it).
// ---------------------------------------------------------------------------
void AemsFactory::AddPatchMonitor(Csis::Class* apClassData)
{
    CGS_ASSERT(apClassData != 0, "lpClassData");

    const Csis::SystemClient24* lpDescriptor =
        reinterpret_cast<const Csis::SystemClient24*>(apClassData->mpClassDesc);
    const Csis::ClassClientNode* lpClient =
        reinterpret_cast<const Csis::ClassClientNode*>(lpDescriptor->muRuntimeLink);
    const Snd9::Aems::ModuleRecord* lpModule =
        static_cast<const Snd9::Aems::ModuleRecord*>(lpClient->mpClientData);
    CGS_ASSERT(lpModule != 0, "lpModule");

    const char* lpcName = reinterpret_cast<const char*>(lpDescriptor->muTargetOrOffset);
    PatchMonitor* lpPatchMonitor = FindPatchMonitor(lpcName);
    if (lpPatchMonitor != 0)
    {
        lpPatchMonitor->mpClientFunc = lpModule->mpProgram;
        return;
    }

    CGS_ASSERT(muPatchMonitorCount < KU_MAX_PATCH_MONITORS,
               "muPatchMonitorCount < KU_MAX_PATCH_MONITORS");

    PatchMonitor& lrMonitor = maPatchMonitors[muPatchMonitorCount];
    lrMonitor.mpName = lpcName;
    lrMonitor.mpClientFunc = lpModule->mpProgram;
    lrMonitor.mpClientData = 0;
    lrMonitor.miPerfmon = CgsDev::PerfMonCpu::AddMonitor(
        lpcName, static_cast<CgsDev::PerfMonCpuPage>(0x13), false, 0.25f, true);
    ++muPatchMonitorCount;
    AemsDiagPrintf("[FLAG PC witness] [aems] monitor %u name=%s\n",
                   muPatchMonitorCount, lpcName);
}

// DWARF h:235 (DecFIGS @0x825DAC), inlined into UpdateAEMSPatches on ARTIST.
PatchMonitor* AemsFactory::FindPatchMonitor(const u8* apClientFunc)
{
    for (u32 luI = 0; luI < muPatchMonitorCount; ++luI)
    {
        if (maPatchMonitors[luI].mpClientFunc == apClientFunc)
            return &maPatchMonitors[luI];
    }
    return nullptr;
}

// ---------------------------------------------------------------------------
// AemsFactory::CsisPrint(const char* lpcText)  @ 0x8268A018
// ---------------------------------------------------------------------------
void AemsFactory::CsisPrint(const char* lpcText)
{
    if ((CgsDev::Message::gxMessageFilterFlags & 1u) != 0)
    {
        *CgsDev::Log::gpDebugPrint << (lpcText ? lpcText : "<NULLSTRING>");
    }
}

// ---------------------------------------------------------------------------
// AemsFactory::FindPatchMonitor(const char* lpcName)  @ 0x82689E98
// ---------------------------------------------------------------------------
PatchMonitor* AemsFactory::FindPatchMonitor(const char* lpcName)
{
    if (muPatchMonitorCount == 0)
    {
        return nullptr; // cmplwi count,0 ; beq -> li r3,0
    }

    for (u32 luI = 0; luI < muPatchMonitorCount; ++luI)
    {
        // Hand-inlined strcmp over the C strings (the X360 do/while loop).
        if (std::strcmp(maPatchMonitors[luI].mpName, lpcName) == 0)
        {
            return &maPatchMonitors[luI];
        }
    }
    return nullptr;
}

} // namespace Playback
} // namespace CgsSound
