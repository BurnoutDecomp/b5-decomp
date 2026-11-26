// ============================================================================
// CgsEnvironment.cpp -- CgsSound::Logic::Environment runtime bodies.
//
// Bodied from BURNOUT_X360_ARTIST.XEX:
//   Environment::AddStateManager(StateManager*)  @ 0x82680D60
//   Environment::GetStateManager(s32)            @ 0x8268D1C0
//   Environment::Construct(const EnvironmentSpec&) @ 0x8268D050   (phase B2)
//   Environment::Update(f32, f32)                @ 0x826C3F78     (phase B2)
//
// (2026-08-25, faithful-audio-engine phase B2): the old "[type + 1] reserved
// slot" indexing convention is RETIRED -- the Construct asm proves the word at
// Environment+0 is mpAllocator, so the console's (type+1)*4 displacement lands
// in mapStateManagers[type] of the real layout (see the header banner). The
// Add/Get bodies below index the map directly; the byte math is unchanged.
// ============================================================================

#include "GameShared/GameClasses/Sound/Logic/CgsEnvironment.h"
#include "rw/rwcore_structs.h"
#include "GameShared/GameClasses/Core/CgsAssertMessageAt.h"
#include "GameShared/GameClasses/Development/PerfMon/Cpu/CgsPerfMonCpu.h" // AddMonitor / Start/StopMonitor
#include "SDKs/EATech/include/NFSMix/MixerAllocator.hpp"                  // Nicotine mixer-allocator face
#include "GameShared/GameClasses/Sound/IO/CgsMessage.h"
#include "GameShared/GameClasses/Sound/Logic/CgsState.h"
#include "GameShared/GameClasses/Sound/Logic/CgsEffectBase.h"

// ARTIST's asserts in this file name it through the GameShared include path.
#define CGS_ENVIRONMENT_CPP_FILE "..\\..\\..\\GameShared\\GameClasses\\Sound/Logic/CgsEnvironment.cpp"

namespace CgsSound
{
namespace Logic
{

// ARTIST 0x826C3F00: clear the manager map, destroy the mixer base, and
// return the Environment resource through the allocator captured before teardown.
void Environment::Destruct()
{
    rw::IResourceAllocator* lpAllocator = mpAllocator;
    for (s32 liSlot = 0; liSlot < KI_MAX_NUMBER_OF_STATES; ++liSlot)
        mapStateManagers[liSlot] = 0;
    mDynamicMixer.Nicotine::IDynamicMixer::~IDynamicMixer();
    rw::Resource lResource;
    lResource.m_baseResources[0] = this;
    lpAllocator->DoFree(lResource);
}

bool Environment::AddStateManager(StateManager* apStateManager)
{
    const char* const lpcAssertFile = "..\\..\\..\\GameShared\\GameClasses\\Sound/Logic/CgsEnvironment.h";
    CGS_ASSERT(apStateManager, CGS_ASSERT_MESSAGE_AT("lpStateManager", lpcAssertFile, 475));
    CGS_ASSERT(apStateManager->GetStateType() < KI_MAX_NUMBER_OF_STATES,
        CGS_ASSERT_MESSAGE_AT("lpStateManager->GetStateType() < KI_MAX_NUMBER_OF_STATES", lpcAssertFile, 476));
    CGS_ASSERT(mapStateManagers[apStateManager->GetStateType()] == 0,
        CGS_ASSERT_MESSAGE_AT("mapStateManagers[lpStateManager->GetStateType()] == NULL", lpcAssertFile, 477));

    mapStateManagers[apStateManager->GetStateType()] = apStateManager;
    return true;
}

// ---------------------------------------------------------------------------
// Environment::GetStateManager(s32 liStateManId)  @ 0x8268D1C0
//
//   if (liStateManId >= 16) assert("liStateManId < KI_MAX_NUMBER_OF_STATES"); // :481
//   return mapStateManagers[liStateManId];   ((liStateManId+1)*4 + this == the
//                                             map slot past the +0 allocator word)
// The X360 asserts only the upper bound (>= 16); a negative id is not guarded there,
// so it is not guarded here either (faithful).
// ---------------------------------------------------------------------------
StateManager* Environment::GetStateManager(s32 liStateManId) const
{
    CGS_ASSERT(liStateManId < KI_MAX_NUMBER_OF_STATES,
        CGS_ASSERT_MESSAGE_AT(
            "liStateManId < KI_MAX_NUMBER_OF_STATES",
            "..\\..\\..\\GameShared\\GameClasses\\Sound/Logic/CgsEnvironment.cpp", 481 /* ARTIST 0x8268D1EC */));

    return mapStateManagers[liStateManId];
}

// ---------------------------------------------------------------------------
// Environment::Construct(const EnvironmentSpec&)  @ 0x8268D050
//
// Asm store map (byte offsets):
//   assert(spec.mpAllocator, "lEnvironmentSpec.mpAllocator", cpp:66)
//   +0x000 = spec.mpAllocator          +0x314 mfGameTime = 0.0
//   +0x04C = spec.mu32StateManagerCount
//   zero the 16 map slots (+0x004..)
//   +0x300 mDynamicMixer.mpEnvironment = this
//   Nicotine::IDynamicMixer::CreateInstance(&mDynamicMixer,
//       {0, spec.mu32StateManagerCount, spec.mpAllocator})
//   the four monitors seeded -1 then registered (page 14, budget 1.0):
//     +0x310 "Logic Environment"  +0x304 "Dynamic Mixer"
//     +0x308 "Process Update"     +0x30C "Update Params"
// ---------------------------------------------------------------------------
void Environment::Construct(const EnvironmentSpec& akrSpec)
{
    CGS_ASSERT_AT(akrSpec.mpAllocator != 0, "lEnvironmentSpec.mpAllocator", CGS_ENVIRONMENT_CPP_FILE, 66);

    // ARTIST reads the spec's count before it stores the game time (0x8268D0B4 / 0x8268D0BC).
    mpAllocator = akrSpec.mpAllocator;
    mu32StateManagerCount = akrSpec.mu32StateManagerCount;
    mfGameTime  = 0.0f;

    for (s32 lCnt = 0; lCnt < KI_MAX_NUMBER_OF_STATES; ++lCnt)   // DWARF lCnt (cpp:72)
        mapStateManagers[lCnt] = 0;

    // The mixer's creation descriptor (DWARF lCreateParams, cpp:81): the count comes from the
    // spec before DynamicMixer::Construct stores the back-pointer, the allocator after it.
    Nicotine::IDynamicMixer::MAP_CREATE_PARAMS lCreateParams;
    lCreateParams.mPrintSelect   = 0;
    lCreateParams.NumMixStates   = static_cast<int>(akrSpec.mu32StateManagerCount);
    mDynamicMixer.Construct(this);
    lCreateParams.MixerAllocator = mpAllocator;
    mDynamicMixer.CreateInstance(&lCreateParams);

    // DWARF: GetCpuMonitors() then CpuMonitors::Construct() (both inline).
    GetCpuMonitors().Construct();
    mCpuMonitors.miLogicEnvironment = CgsDev::PerfMonCpu::AddMonitor(
        "Logic Environment", (CgsDev::PerfMonCpuPage)14, false, 1.0f, true);
    mCpuMonitors.miDynamicMixer = CgsDev::PerfMonCpu::AddMonitor(
        "Dynamic Mixer", (CgsDev::PerfMonCpuPage)14, false, 1.0f, true);
    mCpuMonitors.miProcessUpdate = CgsDev::PerfMonCpu::AddMonitor(
        "Process Update", (CgsDev::PerfMonCpuPage)14, false, 1.0f, true);
    mCpuMonitors.miUpdateParams = CgsDev::PerfMonCpu::AddMonitor(
        "Update Params", (CgsDev::PerfMonCpuPage)14, false, 1.0f, true);
}

// ---------------------------------------------------------------------------
// Environment::Update(f32 gameDt, f32 simDt)  @ 0x826C3F78
//
//   StartMonitor(miEnvironmentUpdate)
//   mfGameTime += gameDt
//   MicrophoneSystem::UpdateMicrophones(&mMicrophoneSystem, gameDt)
//   StartMonitor(miUpdateParams)
//   for each registered manager slot:                    (BY NAME: the setters
//     mgr->mfTimeStepGame = gameDt (+0x0C)                inline these stores)
//     mgr->mfTimeStepSimulation = simDt (+0x10)
//     mgr->mfCurrentTime = mfGameTime (+0x04)
//     virtual mgr->UpdateParams(gameDt)   (vtbl +0x18)
//   StopMonitor(miUpdateParams)
//   StartMonitor(miDynamicMixer)
//   Nicotine::IDynamicMixer::ProcessMixMap(&mDynamicMixer, gameDt)
//   StopMonitor(miDynamicMixer)
//   StartMonitor(miProcessUpdate)
//   for each registered manager slot: virtual mgr->ProcessUpdate() (vtbl +0x1C)
//   StopMonitor(miProcessUpdate)
//   StopMonitor(miEnvironmentUpdate)
// ---------------------------------------------------------------------------
void Environment::Update(f32 af32GameDt, f32 af32SimDt)
{
    CgsDev::PerfMonCpu::StartMonitor(mCpuMonitors.miLogicEnvironment);

    mfGameTime += af32GameDt;
    mMicrophoneSystem.UpdateMicrophones(af32GameDt);

    CgsDev::PerfMonCpu::StartMonitor(mCpuMonitors.miUpdateParams);
    // DWARF uint32_t luIndex (cpp:177). ARTIST re-reads the slot for every setter (0x826C3FEC /
    // 0x826C3FF4): the original indexes the map each time rather than holding a local.
    for (u32 luIndex = 0; luIndex < mu32StateManagerCount; ++luIndex)
    {
        if (mapStateManagers[luIndex])
        {
            mapStateManagers[luIndex]->SetTimeStepGame(af32GameDt);
            mapStateManagers[luIndex]->SetTimeStepSimulation(af32SimDt);
            mapStateManagers[luIndex]->SetCurrentTime(mfGameTime);
            mapStateManagers[luIndex]->UpdateParams(af32GameDt);
        }
    }
    CgsDev::PerfMonCpu::StopMonitor(mCpuMonitors.miUpdateParams);

    CgsDev::PerfMonCpu::StartMonitor(mCpuMonitors.miDynamicMixer);
    mDynamicMixer.ProcessMixMap(af32GameDt);
    CgsDev::PerfMonCpu::StopMonitor(mCpuMonitors.miDynamicMixer);

    CgsDev::PerfMonCpu::StartMonitor(mCpuMonitors.miProcessUpdate);
    for (u32 luIndex = 0; luIndex < mu32StateManagerCount; ++luIndex)
    {
        if (mapStateManagers[luIndex])
            mapStateManagers[luIndex]->ProcessUpdate();
    }
    CgsDev::PerfMonCpu::StopMonitor(mCpuMonitors.miProcessUpdate);

    CgsDev::PerfMonCpu::StopMonitor(mCpuMonitors.miLogicEnvironment);
}

static void NotifyStateAndEffects(State* apState,
                                  const CgsSound::Io::MessageHeader* apkMessage)
{
    apState->Notify(apkMessage);
    const u16 luEffectId = apkMessage->GetEffectId();
    if (luEffectId == CgsSound::Io::MessageHeader::KU16_NO_DESTINATION)
        return;

    EffectBase* lpHead = 0;
    if (apkMessage->GetEffectType() == CgsSound::Io::MessageHeader::E_EFFECT_TYPE_OBJECT)
        lpHead = apState->GetHeadEffectObject();
    else if (apkMessage->GetEffectType() == CgsSound::Io::MessageHeader::E_EFFECT_TYPE_CONTROL)
        lpHead = apState->GetHeadEffectControl();
    else
    {
        CGS_ASSERT(false, "Unknown sound effect type\n");
        return;
    }

    if (luEffectId == CgsSound::Io::MessageHeader::KU16_ALL_DESTINATIONS)
    {
        for (EffectBase* lpEffect = lpHead; lpEffect; lpEffect = lpEffect->mpNextEffectBase)
            lpEffect->Notify(apkMessage);
        return;
    }

    for (EffectBase* lpEffect = lpHead; lpEffect; lpEffect = lpEffect->mpNextEffectBase)
    {
        if (lpEffect->GetEffectID() == luEffectId)
        {
            lpEffect->Notify(apkMessage);
            return;
        }
    }
}

void Environment::Notify(const CgsSound::Io::MessageHeader* apkMessage) const
{
    CGS_ASSERT(apkMessage != 0, "lpMessageHeader");
    if (!apkMessage)
        return;

    const u16 luManagerId = apkMessage->GetStateManagerId();
    const u16 luInstanceId = apkMessage->GetInstanceId();
    const u32 luBegin = (luManagerId == CgsSound::Io::MessageHeader::KU16_NO_DESTINATION)
                      ? 0u : static_cast<u32>(luManagerId);
    const u32 luEnd = (luManagerId == CgsSound::Io::MessageHeader::KU16_NO_DESTINATION)
                    ? mu32StateManagerCount : luBegin + 1u;

    for (u32 luManager = luBegin; luManager < luEnd; ++luManager)
    {
        StateManager* lpManager = GetStateManager(static_cast<s32>(luManager));
        if (!lpManager)
            continue;
        lpManager->Notify(apkMessage);

        for (State* lpState = lpManager->GetHeadState(); lpState; lpState = lpState->GetNextState())
        {
            if (luInstanceId == CgsSound::Io::MessageHeader::KU16_NO_DESTINATION ||
                lpState->GetInstanceID() == static_cast<s32>(luInstanceId))
                NotifyStateAndEffects(lpState, apkMessage);
        }
    }
}

// The one rodata ModuleParams record (X360 unk_820AA480, XEX-recovered big-endian
// {0x0010, 0x0010, 0x0010}): 16 voice proxies / 16 content proxies / 16 state
// managers -- the constant BrnSound SoundLogicModule::Prepare @0x82703C18 hands the
// engine base Prepare. Modeled as the DWARF-declared ModuleParams::DEFAULT (h:153).
const ModuleParams ModuleParams::DEFAULT = { 16, 16, 16 };

} // namespace Logic
} // namespace CgsSound
