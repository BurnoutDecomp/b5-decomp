// FX-WATERSND (crash parity 2026-09-24): a player crash into water must play the
// crash-in-water sting through the COLLISION state manager's splice bank.
//
// The PRODUCTION bodies of BrnSound::Logic::FxEffect::Notify @0x826F7248, ::FindFreeVoice
// (the four-slot scan Notify inlines) and ::UpdateParams @0x826BC338, plus the file's
// round-robin selector block and CgsSound::Utils::IntClamp @0x826895D0, are extracted by
// run_fxwatersnd_notify.py into fxwatersnd_methods.inc and replayed here against a fixture
// effect. The console facts the checks encode:
//   UpdateParams: --miFrameCountBeforeRetrigger clamped to [0, 256] (0x826BC388 li r5,0x100);
//     nothing else unless the player car is active (0x826BC3A0..0x826BC3F0); DataPoint<bool>
//     update from HasCrashedIntoWater(player index) (0x826BC420); on (previous != current &&
//     current == true) and a zero counter: counter = 120 (0x826BC49C li r9,0x78), then sound
//     message id 4 / data 8 into Notify (vtable slot +0x24, 0x826BC4C8).
//   Notify case 8 (0x826F7474..0x826F74B4): GetSampleTag(1 SampleTagCollision, 5, dword_8300C79C++),
//     mixer output 3 (stb r28), bank = *(module + 0x2968) + 0x8228 ==
//     GetEnvironment().GetStateManager(5)->GetSplicerBank(E_COLLISION_SPLICE_BANK_COLLISION).
//   Every other arm keeps its console bank: 0/5/6 FX (+0xB0), 1/2/3/7/9 PRESENTATION (+0xBC);
//     4 (and anything > 9) plays nothing (the jump table's default, result = 0).
//   The common tail (0x826F7678..0x826F7760): Create({ module, dword_83008404 SplicerFactory,
//     "SplicerVoiceSpec", bank, 0, "~SplicerPlayerVoice::Slot~", "Send01", 1, 0 }), the
//     inlined Play(sample index), mafVolumes[slot] = tag volume.
// The fixture VoiceWrapper::Create fires the same assert the real one does
// (CgsVoiceWrapper.cpp:44 "mCreateParams.mpContent") when a no-content-spec create carries a
// null bank -- the pre-fix type-8 arm did exactly that, then faulted reading 0x8.
#include "GameSource/Sound/Global/BrnFxEffect.h"                  // FxMessage, BrnEffectObject::SampleTag, DataPoint
#include "GameSource/Sound/Collision/BrnCollisionStateManager.h"  // Collision::ECollisionSpliceBankType (+ the real accessor)
#include "GameShared/GameClasses/Sound/IO/CgsMessage.h"
#include "GameShared/GameClasses/Numeric/CgsBranchlessOperations.h"
#include "GameSource/BurnoutConstants.h"                           // EActiveRaceCarIndex

#include <cmath>
#include <cstdarg>
#include <cstdio>
#include <cstring>
#include <string>
#include <type_traits>

static unsigned guAsserts = 0;
static std::string gsLastAssert;

namespace CgsDev { namespace Assert {
int BeginAssert() { return 0; }
int FireAssert(const char* apcExpression, const char*, int) { ++guAsserts; gsLastAssert = apcExpression; return 0; }
void* EndAssert() { return nullptr; }
} }

// The real accessor the fixed arm names must keep the console's shape: a const
// Content& selected by the bank enum, on a class whose primary base chain reaches
// CgsSound::Logic::StateManager (what Environment::GetStateManager returns).
static_assert(std::is_same<decltype(&BrnSound::Logic::Collision::CollisionStateManager::GetSplicerBank),
                           const CgsSound::Logic::Content& (BrnSound::Logic::Collision::CollisionStateManager::*)(
                               BrnSound::Logic::Collision::ECollisionSpliceBankType) const>::value,
              "CollisionStateManager::GetSplicerBank(ECollisionSpliceBankType) const -> const Content&");
static_assert(std::is_base_of<CgsSound::Logic::StateManager,
                              BrnSound::Logic::Collision::CollisionStateManager>::value,
              "CollisionStateManager derives StateManager");
static_assert(BrnSound::Logic::Collision::E_COLLISION_SPLICE_BANK_COLLISION == 0 &&
              BrnSound::Logic::Collision::E_COLLISION_SPLICE_CRASH_IN_WATER == 5,
              "bank 0 / crash-in-water tag index 5");

namespace CgsSound { namespace Utils {
#include "fxwatersnd_intclamp.inc"
} }

namespace BrnSound { namespace Logic {

// ---- fixtures --------------------------------------------------------------------------
struct ContentFixture { int miId; };

struct StateManagerFixture { s32 miSlot; };

struct CollisionStateManagerFixture : StateManagerFixture
{
    ContentFixture maSplicerBanks[Collision::E_COLLISION_SPLICE_BANK_MAX];
    mutable u32 muBankCalls = 0;
    mutable s32 miLastBank = -1;
    const ContentFixture& GetSplicerBank(Collision::ECollisionSpliceBankType aeBank) const
    {
        ++muBankCalls;
        miLastBank = aeBank;
        return maSplicerBanks[aeBank];
    }
};

// Every slot holds a distinct manager with its own bank, so a walk to the wrong slot
// hands Create the wrong address.
struct EnvironmentFixture
{
    CollisionStateManagerFixture maManagers[16];
    mutable u32 muCalls = 0;
    mutable s32 miLastId = -1;
    StateManagerFixture* GetStateManager(s32 aiId) const
    {
        ++muCalls;
        miLastId = aiId;
        return const_cast<CollisionStateManagerFixture*>(&maManagers[aiId]);
    }
};

struct VehicleInterfaceFixture
{
    bool mbPlayerActive = true;
    EActiveRaceCarIndex mePlayer = static_cast<EActiveRaceCarIndex>(3);
    bool mabInWater[8] = {};
    mutable u32 muWaterQueries = 0;
    mutable s32 miLastWaterIndex = -1;
    bool IsPlayerCarActive() const { return mbPlayerActive; }
    EActiveRaceCarIndex GetPlayerActiveRaceCarIndex() const { return mePlayer; }
    bool HasCrashedIntoWater(EActiveRaceCarIndex aeIndex) const
    {
        ++muWaterQueries;
        miLastWaterIndex = aeIndex;
        return mabInWater[aeIndex];
    }
};

struct InputFixture
{
    VehicleInterfaceFixture mVehicle;
    const VehicleInterfaceFixture* GetVehicleInterface() const { return &mVehicle; }
};

struct SoundLogicModuleFixture
{
    EnvironmentFixture mEnvironment;
    InputFixture mInput;
    EnvironmentFixture& GetEnvironment() { return mEnvironment; }
    InputFixture* GetBrnInputStructure() { return &mInput; }
};

struct GlobalStateManagerFixture
{
    ContentFixture mFxSpliceBank;
    ContentFixture mPresentationSpliceBank;
    const ContentFixture& GetFxSpliceBank() const { return mFxSpliceBank; }
    const ContentFixture& GetPresentationSpliceBank() const { return mPresentationSpliceBank; }
};

struct CreateParamsFixture
{
    CreateParamsFixture() { Clear(); }
    void Clear()
    {
        mpLogicModule = 0; mpOnPostInit = 0; mFactoryName = 0; mVoiceSpecName = 0; mpContent = 0;
        mContentSpecName = 0; mSlotName = 0; mSendName = 0; mSubMixVoiceID = 0;
        mReverbSendName = 0; mReverbSubMixVoiceID = 0; miSendIndex = -1;
    }
    SoundLogicModuleFixture* mpLogicModule;
    void* mpOnPostInit;
    u32 mFactoryName;
    u32 mVoiceSpecName;
    const ContentFixture* mpContent;
    u32 mContentSpecName;
    u32 mSlotName;
    u32 mSendName;
    u32 mSubMixVoiceID;
    u32 mReverbSendName;
    u32 mReverbSubMixVoiceID;
    s32 miSendIndex;
};

struct VoiceWrapperFixture
{
    s32 meStage = CgsSound::Logic::VoiceWrapper::E_UPDATE_STAGE_IDLE;
    u32 muCreates = 0;
    u32 muPlays = 0;
    u32 muPlayParam = 0;
    CreateParamsFixture mParams;
    s32 GetState() const { return meStage; }
    void Create(const CreateParamsFixture& arParams)
    {
        ++muCreates;
        mParams = arParams;
        if (arParams.mContentSpecName == 0)   // CgsVoiceWrapper.cpp's no-spec branch
            CGS_ASSERT(arParams.mpContent != 0, "mCreateParams.mpContent");
        meStage = CgsSound::Logic::VoiceWrapper::E_UPDATE_STAGE_CREATE;
    }
    void Play(u32 auParam) { ++muPlays; muPlayParam = auParam; }
};

static u32 MakeHashFixture(const char* apcName)     // FNV-1a: distinct, stable per name
{
    u32 luHash = 2166136261u;
    for (; *apcName; ++apcName)
        luHash = (luHash ^ static_cast<u8>(*apcName)) * 16777619u;
    return luHash;
}

static std::string gsWitness;
static u32 guWitnessLines = 0;
inline bool HudSoundDiagBudget(u32& aruCount) { ++aruCount; return true; }
inline void HudSoundDiagPrintf(const char* apcFormat, ...)
{
    char lacLine[512];
    va_list lArgs;
    va_start(lArgs, apcFormat);
    std::vsnprintf(lacLine, sizeof(lacLine), apcFormat, lArgs);
    va_end(lArgs);
    gsWitness = lacLine;
    ++guWitnessLines;
}

struct FxWaterSndFixture
{
    enum { KI_NUM_VOICES = 4 };

    VoiceWrapperFixture mVoiceWrappers[KI_NUM_VOICES];
    f32 mafVolumes[KI_NUM_VOICES] = { 1.0f, 1.0f, 1.0f, 1.0f };
    u8 mau8MixerOutputs[KI_NUM_VOICES] = {};
    GlobalStateManagerFixture* mpGlobalStateManager = 0;
    CgsSound::Utils::DataPoint<bool> mbHasCrashedIntoWater;
    s32 miFrameCountBeforeRetrigger = 0;
    SoundLogicModuleFixture* mpLogicModule = 0;

    // BrnEffectObject::GetSampleTag stand-in: records its arguments; the resolved index is
    // 7 + the selection so the round-robin value is visible in Play's argument.
    bool mbTagResolves = true;
    mutable u32 muTagCalls = 0;
    mutable u32 muTagType = 0, muTagIndex = 0, muTagSelection = 0;
    bool GetSampleTag(u32 auTag, u32 auIndex, u32 auSelection, BrnEffectObject::SampleTag& arTag) const
    {
        ++muTagCalls;
        muTagType = auTag;
        muTagIndex = auIndex;
        muTagSelection = auSelection;
        arTag.miSampleIndex = static_cast<s16>(7 + auSelection);
        arTag.mfVolume = 0.5f;
        return mbTagResolves;
    }
    SoundLogicModuleFixture* GetLogicModule() const { return mpLogicModule; }

    s32 FindFreeVoice() const;
    void Notify(const CgsSound::Io::MessageHeader* apMessageHeader);
    void UpdateParams(f32 af32DeltaTime);
};

#include "fxwatersnd_methods.inc"

} }

using namespace BrnSound::Logic;

static unsigned guChecks = 0, guFailures = 0;
static void Check(bool abPass, const char* apcName)
{
    ++guChecks;
    if (!abPass)
    {
        ++guFailures;
        std::fprintf(stderr, "FAIL: %s\n", apcName);
    }
}

struct Rig
{
    SoundLogicModuleFixture mModule;
    GlobalStateManagerFixture mGlobal;
    FxWaterSndFixture mEffect;
    Rig()
    {
        for (s32 liSlot = 0; liSlot < 16; ++liSlot)
        {
            mModule.mEnvironment.maManagers[liSlot].miSlot = liSlot;
            mModule.mEnvironment.maManagers[liSlot].maSplicerBanks[0].miId = 100 + liSlot;
        }
        mGlobal.mFxSpliceBank.miId = 1;
        mGlobal.mPresentationSpliceBank.miId = 2;
        mEffect.mpGlobalStateManager = &mGlobal;
        mEffect.mpLogicModule = &mModule;
    }
    const ContentFixture* CollisionBank() const { return &mModule.mEnvironment.maManagers[5].maSplicerBanks[0]; }
    u32 Creates() const
    {
        u32 luCreates = 0;
        for (const VoiceWrapperFixture& lrVoice : mEffect.mVoiceWrappers)
            luCreates += lrVoice.muCreates;
        return luCreates;
    }
    void Post(s32 aiType)
    {
        CgsSound::Io::Message<s32> lMessage;
        lMessage.Construct(4);
        lMessage.mData = aiType;
        mEffect.Notify(&lMessage);
    }
};

static bool SameParams(const CreateParamsFixture& arParams, const SoundLogicModuleFixture* apModule)
{
    return arParams.mpLogicModule == apModule && arParams.mpOnPostInit == 0 &&
           arParams.mFactoryName == MakeHashFixture("~SplicerFactory::SK_NAME~") &&
           arParams.mVoiceSpecName == MakeHashFixture("SplicerVoiceSpec") &&
           arParams.mContentSpecName == 0 &&
           arParams.mSlotName == MakeHashFixture("~SplicerPlayerVoice::Slot~") &&
           arParams.mSendName == MakeHashFixture("Send01") &&
           arParams.mSubMixVoiceID == 1 && arParams.miSendIndex == 0 &&
           arParams.mReverbSendName == 0 && arParams.mReverbSubMixVoiceID == 0;
}

int main()
{
    // ---- A. one E_CRASH_IN_WATER message on a fresh effect ---------------------------------
    {
        Rig lRig;
        guAsserts = 0;
        guFxSelectCrashInWater = 0;
        lRig.Post(FxMessage::E_CRASH_IN_WATER);
        const VoiceWrapperFixture& lrVoice = lRig.mEffect.mVoiceWrappers[3];   // the LAST free slot
        Check(lRig.Creates() == 1 && lrVoice.muCreates == 1, "A: type 8 creates exactly one voice, on slot 3");
        Check(lrVoice.mParams.mpContent != 0, "A: type 8 hands Create a bank (the old arm passed NULL)");
        Check(lrVoice.mParams.mpContent == lRig.CollisionBank(),
              "A: the bank is the slot-5 (collision) manager's splice bank");
        Check(lRig.mModule.mEnvironment.muCalls == 1 && lRig.mModule.mEnvironment.miLastId == 5,
              "A: Environment::GetStateManager(5) once (module+0x2968)");
        Check(lRig.mModule.mEnvironment.maManagers[5].muBankCalls == 1 &&
                  lRig.mModule.mEnvironment.maManagers[5].miLastBank == Collision::E_COLLISION_SPLICE_BANK_COLLISION,
              "A: GetSplicerBank(E_COLLISION_SPLICE_BANK_COLLISION) once (+0x8228)");
        Check(lRig.mEffect.muTagCalls == 1 && lRig.mEffect.muTagType == 1 && lRig.mEffect.muTagIndex == 5 &&
                  lRig.mEffect.muTagSelection == 0 && guFxSelectCrashInWater == 1,
              "A: GetSampleTag(1, 5, dword_8300C79C++)");
        Check(lRig.mEffect.mau8MixerOutputs[3] == 3, "A: mixer output 3 on the slot");
        Check(lrVoice.muPlays == 1 && lrVoice.muPlayParam == 7, "A: Play(sample index)");
        Check(std::fabs(lRig.mEffect.mafVolumes[3] - 0.5f) < 1e-6f, "A: mafVolumes[slot] = tag volume");
        Check(SameParams(lrVoice.mParams, &lRig.mModule), "A: splicer create params (factory/spec/slot/send/submix 1/send 0)");
        Check(guAsserts == 0, "A: no assert (CgsVoiceWrapper.cpp:44 mCreateParams.mpContent)");
        Check(gsWitness.find("type=8") != std::string::npos && gsWitness.find("PLAY") != std::string::npos &&
                  gsWitness.find("bank=1") != std::string::npos,
              "A: [fx-sound-msg] witness reports type=8 PLAY bank=1");
    }

    // ---- B. the selector is per-type and post-increments; the next free slot is 2 ----------
    {
        Rig lRig;
        guAsserts = 0;
        guFxSelectCrashInWater = 0;
        lRig.Post(FxMessage::E_CRASH_IN_WATER);
        lRig.Post(FxMessage::E_CRASH_IN_WATER);
        const VoiceWrapperFixture& lrVoice = lRig.mEffect.mVoiceWrappers[2];
        Check(lrVoice.muCreates == 1 && lrVoice.mParams.mpContent == lRig.CollisionBank() &&
                  lRig.mEffect.muTagSelection == 1 && lrVoice.muPlayParam == 8 && guFxSelectCrashInWater == 2,
              "B: second type-8 message -> slot 2, selection 1, same collision bank");
        Check(guAsserts == 0, "B: no assert");
    }

    // ---- C. an unresolved tag still resolves the bank and stamps the mixer, plays nothing ---
    {
        Rig lRig;
        guAsserts = 0;
        lRig.mEffect.mbTagResolves = false;
        lRig.Post(FxMessage::E_CRASH_IN_WATER);
        Check(lRig.Creates() == 0 && lRig.mEffect.mau8MixerOutputs[3] == 3 &&
                  lRig.mModule.mEnvironment.muCalls == 1 && guAsserts == 0,
              "C: unresolved tag -> no voice, mixer still 3 (stb precedes the result test)");
        Check(gsWitness.find("IGNORED") != std::string::npos && gsWitness.find("bank=1") != std::string::npos,
              "C: witness reports IGNORED with the bank resolved");
    }

    // ---- D. every other arm keeps its console bank and tag; none walks to a state manager --
    {
        struct Arm { s32 miType; u32 muTag; u32 muIndex; s32 miBank; bool mbTagCall; };
        const Arm kaArms[] = {
            { FxMessage::E_WINDOW_SMASH,       0,  0, 1, false },
            { FxMessage::E_CAMERA_CUT,         4,  2, 2, true  },
            { FxMessage::E_STUNT_SMASH,        4,  0, 2, true  },
            { FxMessage::E_STUNT_STUNT,        4,  1, 2, true  },
            { FxMessage::E_RESET_ON_TRACK,     2,  3, 1, true  },
            { FxMessage::E_CAMERA_PHOTO,       2,  6, 1, true  },
            { FxMessage::E_QUIT_EVENT,         4,  8, 2, true  },
            { FxMessage::E_ONLINE_RIVAL_SWEEP, 4, 23, 2, true  },
        };
        for (const Arm& lrArm : kaArms)
        {
            Rig lRig;
            guAsserts = 0;
            lRig.Post(lrArm.miType);
            const VoiceWrapperFixture& lrVoice = lRig.mEffect.mVoiceWrappers[3];
            const ContentFixture* lpExpected = lrArm.miBank == 1 ? &lRig.mGlobal.mFxSpliceBank
                                                                 : &lRig.mGlobal.mPresentationSpliceBank;
            char lacName[160];
            std::snprintf(lacName, sizeof(lacName),
                          "D: type %d -> tag (%u, %u), bank %s, mixer 3, no state-manager walk",
                          lrArm.miType, lrArm.muTag, lrArm.muIndex, lrArm.miBank == 1 ? "FX" : "PRESENTATION");
            const bool lbTag = lrArm.mbTagCall
                ? (lRig.mEffect.muTagCalls == 1 && lRig.mEffect.muTagType == lrArm.muTag &&
                   lRig.mEffect.muTagIndex == lrArm.muIndex)
                : lRig.mEffect.muTagCalls == 0;
            Check(lbTag && lrVoice.muCreates == 1 && lrVoice.mParams.mpContent == lpExpected &&
                      lRig.mEffect.mau8MixerOutputs[3] == 3 && lRig.mModule.mEnvironment.muCalls == 0 &&
                      SameParams(lrVoice.mParams, &lRig.mModule) && guAsserts == 0,
                  lacName);
        }
        for (s32 liType : { static_cast<s32>(FxMessage::E_STUNT_JUMP), 10 })
        {
            Rig lRig;
            guAsserts = 0;
            lRig.Post(liType);
            Check(lRig.Creates() == 0 && lRig.mEffect.mau8MixerOutputs[3] == 0 && lRig.mEffect.muTagCalls == 0,
                  "D: type 4 / out-of-range -> the jump table's default: nothing played, no mixer stamp");
        }
    }

    // ---- E. the producer: UpdateParams on the player's water edge -------------------------
    {
        Rig lRig;
        guAsserts = 0;
        VehicleInterfaceFixture& lrCar = lRig.mModule.mInput.mVehicle;

        lrCar.mbPlayerActive = false;
        lrCar.mabInWater[3] = true;
        lRig.mEffect.UpdateParams(1.0f / 60.0f);
        Check(lRig.Creates() == 0 && lrCar.muWaterQueries == 0 &&
                  !lRig.mEffect.mbHasCrashedIntoWater.GetCurrent() && lRig.mEffect.miFrameCountBeforeRetrigger == 0,
              "E: inactive player -> no query, no DataPoint update, counter clamped at 0");

        lrCar.mbPlayerActive = true;
        lrCar.mabInWater[3] = false;
        lRig.mEffect.UpdateParams(1.0f / 60.0f);
        Check(lRig.Creates() == 0 && lrCar.muWaterQueries == 1 && lrCar.miLastWaterIndex == 3,
              "E: dry player frame queries HasCrashedIntoWater(player index 3), posts nothing");

        lrCar.mabInWater[3] = true;
        lRig.mEffect.UpdateParams(1.0f / 60.0f);
        const VoiceWrapperFixture& lrVoice = lRig.mEffect.mVoiceWrappers[3];
        Check(lRig.Creates() == 1 && lrVoice.mParams.mpContent == lRig.CollisionBank() && guAsserts == 0,
              "E: the rising water edge posts type 8 -> voice created on the collision splice bank, no assert");
        Check(lRig.mEffect.miFrameCountBeforeRetrigger == 120, "E: retrigger counter = 120");

        lRig.mEffect.UpdateParams(1.0f / 60.0f);
        Check(lRig.Creates() == 1 && lRig.mEffect.miFrameCountBeforeRetrigger == 119,
              "E: still in water -> no second post; counter counts down");

        lrCar.mabInWater[3] = false;
        lRig.mEffect.UpdateParams(1.0f / 60.0f);
        lrCar.mabInWater[3] = true;
        lRig.mEffect.UpdateParams(1.0f / 60.0f);
        Check(lRig.Creates() == 1 && lRig.mEffect.miFrameCountBeforeRetrigger == 117,
              "E: a new edge inside the 120-frame window posts nothing");

        lrCar.mabInWater[3] = false;
        for (s32 liFrame = 0; liFrame < 117; ++liFrame)
            lRig.mEffect.UpdateParams(1.0f / 60.0f);
        Check(lRig.mEffect.miFrameCountBeforeRetrigger == 0, "E: the window runs out");
        lrCar.mabInWater[3] = true;
        lRig.mEffect.UpdateParams(1.0f / 60.0f);
        Check(lRig.Creates() == 2 && lRig.mEffect.mVoiceWrappers[2].mParams.mpContent == lRig.CollisionBank() &&
                  guAsserts == 0,
              "E: the next edge after the window posts again, collision bank again");

        lRig.mEffect.miFrameCountBeforeRetrigger = 1000;
        lrCar.mbPlayerActive = false;
        lRig.mEffect.UpdateParams(1.0f / 60.0f);
        Check(lRig.mEffect.miFrameCountBeforeRetrigger == 256, "E: counter clamped to 256 (li r5, 0x100)");
    }

    std::printf("FxWaterSndNotify: %u checks, %u failures\n", guChecks, guFailures);
    return guFailures ? 1 : 0;
}
