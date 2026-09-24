// FX-EMITTER (crash parity 2026-09-24): the world-emitter attach gate -- EmitterEffect::Attach
// against the ARTIST machine code.
//
//   BrnSound::Logic::World::EmitterEffect::Attach   0x826F5740   (DWARF BrnEmitterEffect.cpp:231..287)
//
// The dev assert `luEmitter < static_cast< uint32_t >( lWorldEmitters.mNumWorldEmitters() )` paused six
// lanes' live runs. The console gates the entity's type on the world-emitter list's SCALAR attribute
// mNumWorldEmitters (`lwz r11,0x4B8(r11)` at 0x826F5804 for the assert and 0x826F5838 for the gate);
// the PC read the array header's length (Num_mWorldEmitters) instead, which is 50 where the scalar
// is 38 -- so a type in [38, 50) walked into an empty RefSpec slot without the assert.
//
// run_fxemitter_attach.py extracts the PRODUCTION Attach body from BrnEmitterEffect.cpp and compiles
// it here as a member of a fixture EmitterEffect. The attribute classes (worldemitterlist.h,
// worldemitter.h) and StaticSoundEntity (BrnStaticSoundMap.h) are the REAL headers -- with --rev the
// runner shadows them with that revision's text -- over a list layout built byte-for-byte like the
// shipped SOUND/BURNOUTGLOBALDATA.BIN collection 23D8BE2C59CFEBB0:
//   Attrib::Array header {alloc 50, count 50, elementSize 24, typeInfo 0} @+0
//   RefSpec[50] @+8: slots 0..37 name the 38 world emitters, 38..49 are empty (collection key 0)
//   mNumWorldEmitters (Int32) = 38 @+1208 (0x4B8)          -- ARTIST embedded schema, layout 1216
// The Attrib runtime's out-of-line bodies are replaced by recording doubles below.
//
// Second commit (FX-EMITTER invented arms): the console's Attach has no null tests of its own --
// the 3D control is called unconditionally (0x826F5774..0x826F5790), the body runs on after the
// "lpLogicModule" assert, and a non-streamed emitter's EmitterName is hashed as it stands
// (0x826F5890); AttachController reads the controller id without a null test (0x826866B4). The
// nameless-slot case below pins the last one; the runner's wiring checks pin the rest.
#include "types.hpp"
#include "BrnCommonTypes.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"
#include "SharedClasses/Sound/World/BrnStaticSoundMap.h"
#include "GameSource/AttribSys/Generated/classes/worldemitterlist.h"
#include "GameSource/AttribSys/Generated/classes/worldemitter.h"

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <map>
#include <new>
#include <string>
#include <utility>
#include <vector>

static unsigned guAsserts = 0;
static std::vector<std::string> gaAssertTexts;
static std::vector<std::string> gaCallOrder;   // Detach: base detach / voice release / 3D control, in order
namespace CgsDev { namespace Assert {
int BeginAssert() { return 0; }
int FireAssert(const char* lpcExpression, const char*, int) { ++guAsserts; gaAssertTexts.push_back(lpcExpression); return 0; }
void* EndAssert() { return nullptr; }
} }
// The body's [DIAG] witness (BRN_EMITTER_DIAG, unset here) streams into gpDebugPrint.
namespace CgsDev { namespace Log {
struct DebugPrintFixture
{
    template <typename T> DebugPrintFixture& operator<<(const T&) { return *this; }
};
DebugPrintFixture* gpDebugPrint = nullptr;
} }

// ---- Attrib runtime doubles: collections are fixture records {key, layout}; a RefSpec resolves by
// its collection key; Change(null) leaves the data pointer alone (as Instance::Change does).
namespace
{
struct FixtureCollection
{
    u64 muKey;
    void* mpLayout;
};
std::map<u64, FixtureCollection> gCollections;
std::vector<u64> gaResolvedKeys;   // every RefSpec::GetCollectionWithDefault, in order
alignas(16) u8 gauZeroArea[64] = {};

Attrib::Collection* Lookup(u64 luKey)
{
    std::map<u64, FixtureCollection>::iterator lIt = gCollections.find(luKey);
    return lIt == gCollections.end() ? nullptr : reinterpret_cast<Attrib::Collection*>(&lIt->second);
}
void* LayoutOf(Attrib::Collection* lpCollection)
{
    return reinterpret_cast<FixtureCollection*>(lpCollection)->mpLayout;
}
} // namespace

namespace Attrib
{
Instance::Instance(Collection* lpCollection, void* lpOwner)
    : mpCollection(lpCollection), mpAttributeData(lpCollection ? LayoutOf(lpCollection) : nullptr),
      mpOwner(lpOwner), muFlags(0) {}
Instance::Instance(const RefSpec& lrRefSpec, void* lpOwner)
    : Instance(Lookup(lrRefSpec.GetCollectionKey()), lpOwner) {}
Instance::~Instance() {}
Collection* Instance::Change(Collection* lpNewCollection)
{
    Collection* lpOld = mpCollection;
    if (lpNewCollection != mpCollection)
    {
        mpCollection = lpNewCollection;
        if (lpNewCollection)
            mpAttributeData = LayoutOf(lpNewCollection);
    }
    return lpOld;
}
int Instance::GetClass() const { return 0; }
u64 Instance::GetCollection() const { return 0; }
void AssertOnClassCheck(int, int, u64) {}
void* DefaultDataArea(u32) { return gauZeroArea; }
const Collection* RefSpec::GetCollectionWithDefault()
{
    gaResolvedKeys.push_back(mCollectionKey);
    return Lookup(mCollectionKey);
}
unsigned int Private::GetLength() const   // @0x82803558 `lhz r3, 2(r3)` -- the Array element count
{
    u16 lu16Count;
    std::memcpy(&lu16Count, mData + 2, sizeof(lu16Count));
    return lu16Count;
}
} // namespace Attrib

// ---- playback names: a stand-in intern hash (FNV-1a) and fixed factory/slot names. Like the real
// Name::MakeHash (CgsCommon.cpp), a null or empty string hashes to 0.
namespace CgsSound { namespace Playback {
struct Name
{
    static uintptr_t MakeHash(const char* lpcText)
    {
        if (!lpcText || !*lpcText)
            return 0;
        u32 lu = 2166136261u;
        for (; *lpcText; ++lpcText)
            lu = (lu ^ static_cast<u8>(*lpcText)) * 16777619u;
        return lu;
    }
    u32 mHash;
    uintptr_t GetValue() const { return mHash; }
};
const Name& GenericRwacFactorySkName() { static const Name l = { 0x6E4E0001u }; return l; }
struct PlayerVoice { static const Name SK_PLAYER_SLOT_NAME; };
const Name PlayerVoice::SK_PLAYER_SLOT_NAME = { 0x51070001u };
} }

namespace CgsSound { namespace Logic {
class Module { public: virtual ~Module() {} };

struct VoiceWrapper
{
    struct CreateParams
    {
        Module* mpLogicModule = nullptr;
        u32 mFactoryName = 0;
        u32 mVoiceSpecName = 0;
        u32 mContentSpecName = 0;
        u32 mSlotName = 0;
        u32 mSendName = 0;
        u32 mSubMixVoiceID = 0;
        u32 mReverbSendName = 0;
        u32 mReverbSubMixVoiceID = 0;
        s32 miSendIndex = -1;
    };
    CreateParams mLast;
    int miCreates = 0;
    int miPlays = 0;
    std::vector<std::pair<s32, f32>> maParameters;
    void Create(const CreateParams& lrParams) { mLast = lrParams; ++miCreates; }
    void Play(u32) { ++miPlays; }
    void SetParameter(s32 liIndex, f32 lfValue, const u32*) { maParameters.push_back(std::make_pair(liIndex, lfValue)); }
    void Release() { gaCallOrder.push_back("voice-release"); }
};

struct EffectBase
{
    enum EDetachState
    {
        E_DETACH_STATE_NONE     = 0,
        E_DETACH_STATE_BEGIN    = 1,
        E_DETACH_STATE_UPDATING = 2,
        E_DETACH_STATE_FINISHED = 3,
    };
    u16 mu16AttachCount = 0;
    EDetachState meDetachState = static_cast<EDetachState>(7);
    Module* mpLogicModule = nullptr;
    bool Attach() { meDetachState = E_DETACH_STATE_NONE; ++mu16AttachCount; return true; }   // EffectBase::Attach @0x826A1138
};
} }

// BrnEffectObject::Detach @0x826EBF88 always completes on the console (drop requests, attach state
// 0, detach state FINISHED, return 1); the double can also refuse, to pin what the caller does then.
namespace BrnSound { namespace Logic {
struct BrnEffectObject : public CgsSound::Logic::EffectBase
{
    bool mbDetachResult = true;
    bool Detach()
    {
        gaCallOrder.push_back("base-detach");
        if (mbDetachResult)
            meDetachState = E_DETACH_STATE_FINISHED;
        return mbDetachResult;
    }
};
} }

// ---- the logic module: GetGlobalData().WorldEmitterList() is the burnoutglobaldata RefSpec @+0x488.
namespace
{
const u64 KU_LIST_COLLECTION = 0x23D8BE2C59CFEBB0ull;   // the one worldemitterlist collection
const u64 KU_EMITTER_CLASS = 0x475A643D99976E71ull;     // the RefSpecs' class key (vault)
}
namespace BrnSound { namespace Module {
struct GlobalDataFixture
{
    Attrib::RefSpec mWorldEmitterList;
    const Attrib::RefSpec& WorldEmitterList() const { return mWorldEmitterList; }
};
struct SoundLogicModule : public CgsSound::Logic::Module
{
    GlobalDataFixture mGlobalData;
    GlobalDataFixture& GetGlobalData() { return mGlobalData; }
};
} }

namespace BrnSound { namespace Logic { namespace World {
struct Emitter3dControl
{
    const Vector3* mpPosition = nullptr;
    int miCalls = 0;
    void AttachEmitterPosition(const Vector3* lpPosition)
    {
        mpPosition = lpPosition;
        ++miCalls;
        gaCallOrder.push_back("3d-position");
    }
};

struct EmitterEffect : public BrnSound::Logic::BrnEffectObject
{
    CgsSound::Logic::VoiceWrapper mVoice;
    Vector3 mPos;
    Emitter3dControl* mp3dControl = nullptr;
    s16 mi16PitchOutput = 0;

    BrnSound::World::StaticSoundEntity mEntity;   // what the attached EmitterState hands back
    const BrnSound::World::StaticSoundEntity& GetSoundEntity() const { return mEntity; }

    bool Attach();
    bool Detach();
};

#include "fxemitter_attach_body.inc"
} } }

// ---------------------------------------------------------------------------------------------
namespace
{
unsigned guChecks = 0;
unsigned guFailures = 0;

void Check(bool lbPass, const char* lpcLabel)
{
    ++guChecks;
    if (!lbPass)
    {
        ++guFailures;
        std::printf("FAIL  %s\n", lpcLabel);
    }
    else
    {
        std::printf("ok    %s\n", lpcLabel);
    }
}

// The list layout: 1216 bytes, built at the ARTIST schema offsets (the header pins them).
alignas(16) u8 gauListLayout[1216];
struct EmitterLayout { u32 muEmitterName; bool mbIsStream; bool mbAffectedByDoppler; u8 mau8Padding[2]; };
EmitterLayout gaEmitterLayouts[38];
const char* const kapcNames[38] = {
    "AI_Exotic_music1", "AI_Muscle_music1", "AI_Sedan_music1", "AI_Truck_music1", "AI_Tuner_muisc1",
    "AI_Super_muisc1", "Fountain2", "Digger1", "Cranes1", "Construction1", "Construction2",
    "Construction3", "Fan", "Junkyard1", "Pier", "Helicopter", "NightClubMUS", "MALLClub1",
    "FerrisWLmus", "HudsonDAM", "PowerSTN", "RivalCar01", "RivalCar01F", "ShakeShckMUS", "lumber2",
    "MusicAmphitheatre", "MusicTrailerPark", "MusicDINER", "JacobArc", "TrainStation1", "RivalCar02",
    "RivalCar02F", "RivalCar03", "RivalCar03F", "Toll", "waterfallSmall", "waterMill", "windmillNEW" };
// AffectedByDoppler per slot (vault): 1 for slots 6..15, 19, 20, 24, 28, 34..37.
bool Doppler(int liSlot)
{
    return (liSlot >= 6 && liSlot <= 15) || liSlot == 19 || liSlot == 20 || liSlot == 24 ||
           liSlot == 28 || liSlot >= 34;
}
// worldemitter::EmitterName() widens a u32 Text slot (Vault::Initialize resolves the vault's
// strings into the low-4-GB resource arena), so the fixture keeps its names in a page below 4 GB.
extern "C" __declspec(dllimport) void* __stdcall VirtualAlloc(void*, size_t, unsigned long, unsigned long);
const char* gapcNames[38] = {};

void PlaceNamesBelow4Gb()
{
    char* lpcPage = nullptr;
    for (uintptr_t luHint = 0x10000000u; !lpcPage && luHint < 0xF0000000u; luHint += 0x01000000u)
        lpcPage = static_cast<char*>(VirtualAlloc(reinterpret_cast<void*>(luHint), 0x4000,
                                                  0x1000 | 0x2000 /* MEM_COMMIT|MEM_RESERVE */,
                                                  0x04 /* PAGE_READWRITE */));
    if (!lpcPage)
        return;
    char* lpcCursor = lpcPage;
    for (int liSlot = 0; liSlot < 38; ++liSlot)
    {
        const int liWritten = std::snprintf(lpcCursor, 256, "gamedb://burnout5/Burnout/Sound/World/Source/%s.wav",
                                            kapcNames[liSlot]);
        gapcNames[liSlot] = lpcCursor;
        lpcCursor += liWritten + 1;
    }
}

u64 EmitterKey(int liSlot) { return 0x1000000000000000ull + static_cast<u64>(liSlot); }

void BuildWorld()
{
    std::memset(gauListLayout, 0, sizeof(gauListLayout));
    const u16 lau16Header[4] = { 50, 50, 24, 0 };   // alloc, count, elementSize, typeInfo
    std::memcpy(gauListLayout, lau16Header, sizeof(lau16Header));
    for (int liSlot = 0; liSlot < 50; ++liSlot)
    {
        const u64 luCollection = liSlot < 38 ? EmitterKey(liSlot) : 0;
        new (gauListLayout + 8 + 24 * liSlot) Attrib::RefSpec(KU_EMITTER_CLASS, luCollection);
    }
    const s32 liNumWorldEmitters = 38;
    std::memcpy(gauListLayout + 1208, &liNumWorldEmitters, sizeof(liNumWorldEmitters));
    gCollections[KU_LIST_COLLECTION] = FixtureCollection{ KU_LIST_COLLECTION, gauListLayout };

    PlaceNamesBelow4Gb();
    for (int liSlot = 0; liSlot < 38; ++liSlot)
    {
        const u32 luName = static_cast<u32>(reinterpret_cast<uintptr_t>(gapcNames[liSlot]));
        gaEmitterLayouts[liSlot] = EmitterLayout{ luName, false, Doppler(liSlot), { 0, 0 } };
        gCollections[EmitterKey(liSlot)] = FixtureCollection{ EmitterKey(liSlot), &gaEmitterLayouts[liSlot] };
    }
}

struct Result
{
    bool mbReturned;
    unsigned muAsserts;
    std::vector<std::string> maAssertTexts;
    std::vector<u64> maResolved;
    int miCreates;
    int miPlays;
    s16 mi16PitchOutput;
    u32 muContentSpec;
    u16 mu16AttachCount;
    s32 meDetachState;
    int mi3dCalls;
    bool mbPositionBound;
    f32 mafPos[3];
    std::vector<std::pair<s32, f32>> maParameters;
};

// Attach one emitter whose packed W lane holds lu32PackedW (read as ONE u32: type << 16 | radius).
Result AttachEntity(u32 lu32PackedW)
{
    BrnSound::Module::SoundLogicModule lModule;
    new (&lModule.mGlobalData.mWorldEmitterList) Attrib::RefSpec(0x764C301B793F44D6ull, KU_LIST_COLLECTION);

    BrnSound::Logic::World::Emitter3dControl l3dControl;
    BrnSound::Logic::World::EmitterEffect lEffect;
    lEffect.mpLogicModule = &lModule;
    lEffect.mp3dControl = &l3dControl;
    const f32 lafPos[3] = { 2851.2f, -6.7f, -1403.3f };   // TRK_UNIT146 emitter entity 0
    lEffect.mEntity.mPosPlus.x = lafPos[0];
    lEffect.mEntity.mPosPlus.y = lafPos[1];
    lEffect.mEntity.mPosPlus.z = lafPos[2];
    std::memcpy(&lEffect.mEntity.mPosPlus.w, &lu32PackedW, sizeof(lu32PackedW));

    guAsserts = 0;
    gaAssertTexts.clear();
    gaResolvedKeys.clear();
    Result lResult;
    lResult.mbReturned = lEffect.Attach();
    lResult.muAsserts = guAsserts;
    lResult.maAssertTexts = gaAssertTexts;
    lResult.maResolved = gaResolvedKeys;
    lResult.miCreates = lEffect.mVoice.miCreates;
    lResult.miPlays = lEffect.mVoice.miPlays;
    lResult.mi16PitchOutput = lEffect.mi16PitchOutput;
    lResult.muContentSpec = lEffect.mVoice.mLast.mContentSpecName;
    lResult.mu16AttachCount = lEffect.mu16AttachCount;
    lResult.meDetachState = lEffect.meDetachState;
    lResult.mi3dCalls = l3dControl.miCalls;
    lResult.mbPositionBound = l3dControl.mpPosition == &lEffect.mPos;
    lResult.mafPos[0] = lEffect.mPos.x;
    lResult.mafPos[1] = lEffect.mPos.y;
    lResult.mafPos[2] = lEffect.mPos.z;
    lResult.maParameters = lEffect.mVoice.maParameters;
    return lResult;
}

struct DetachResult
{
    bool mbReturned;
    std::vector<std::string> maOrder;
    s32 meFinalState;
    bool mbPositionKept;
};

// Detach an effect whose meDetachState starts at leStartState; the base Detach completes or refuses.
DetachResult DetachFrom(s32 leStartState, bool lbBaseCompletes)
{
    BrnSound::Logic::World::Emitter3dControl l3dControl;
    BrnSound::Logic::World::EmitterEffect lEffect;
    lEffect.mp3dControl = &l3dControl;
    l3dControl.mpPosition = &lEffect.mPos;
    lEffect.meDetachState = static_cast<CgsSound::Logic::EffectBase::EDetachState>(leStartState);
    lEffect.mbDetachResult = lbBaseCompletes;
    gaCallOrder.clear();
    DetachResult lResult;
    lResult.mbReturned = lEffect.Detach();
    lResult.maOrder = gaCallOrder;
    lResult.meFinalState = lEffect.meDetachState;
    lResult.mbPositionKept = l3dControl.mpPosition == &lEffect.mPos && l3dControl.miCalls == 0;
    return lResult;
}

bool Order(const DetachResult& lrResult, const std::vector<std::string>& laExpected)
{
    return lrResult.maOrder == laExpected;
}

bool ResolvedList(const Result& lrResult, const std::vector<u64>& laExpected)
{
    return lrResult.maResolved == laExpected;
}

bool SameParameters(const Result& lrResult)
{
    const std::pair<s32, f32> kaExpected[4] = { { 2, 0.85f }, { 3, 1.0f }, { 4, 1.0f }, { 5, 0.0f } };
    if (lrResult.maParameters.size() != 4)
        return false;
    for (int li = 0; li < 4; ++li)
        if (lrResult.maParameters[li].first != kaExpected[li].first ||
            lrResult.maParameters[li].second != kaExpected[li].second)
            return false;
    return true;
}
} // namespace

int main()
{
    BuildWorld();

    // TRK_UNIT146 emitter entity 0, 29 m from the assert site on the junction-480886 route. Its
    // X360 W bytes are 00 1d 00 ad: ONE big-endian u32 0x001D00AD -> type 29 (TrainStation1),
    // radius 173 m. (The build/game copy converted before parent b81ce4f9 flipped the two u16
    // halves separately -> 0x00AD001D -> type 173: the live trigger, a DATA defect.)
    Check(gapcNames[29] != nullptr, "fixture: emitter names placed below 4 GB (the vault arena contract)");
    if (gapcNames[29] == nullptr)
    {
        std::printf("FxEmitterAttach: %u checks, %u failures\n", guChecks, guFailures);
        return 1;
    }

    const Result lStation = AttachEntity(0x001D00ADu);
    Check(lStation.mbReturned, "type 29: Attach returns true (li r3,1)");
    Check(lStation.muAsserts == 0, "type 29 (< mNumWorldEmitters 38): no assert");
    Check(ResolvedList(lStation, { EmitterKey(29) }), "type 29: resolves list slot 29 (TrainStation1)");
    Check(lStation.miCreates == 1 && lStation.miPlays == 1, "type 29: voice created and played once");
    Check(lStation.muContentSpec == static_cast<u32>(CgsSound::Playback::Name::MakeHash(gapcNames[29])),
          "type 29: content spec = MakeHash(TrainStation1 EmitterName)");
    Check(lStation.mi16PitchOutput == 1, "type 29 (AffectedByDoppler 0): pitch output 1");
    Check(SameParameters(lStation), "type 29: SetParameter (2,0.85) (3,1) (4,1) (5,0)");
    Check(lStation.mu16AttachCount == 1 && lStation.meDetachState == 0,
          "EffectBase::Attach ran first (count+1, detach state 0)");
    Check(lStation.mi3dCalls == 1 && lStation.mbPositionBound, "3D control bound to &mPos");
    Check(lStation.mafPos[0] == 2851.2f && lStation.mafPos[1] == -6.7f && lStation.mafPos[2] == -1403.3f,
          "mPos = the entity position");

    const Result lFountain = AttachEntity((6u << 16) | 40u);
    Check(lFountain.muAsserts == 0 && lFountain.miCreates == 1, "type 6 (Fountain2): attaches");
    Check(lFountain.mi16PitchOutput == 2, "type 6 (AffectedByDoppler 1): pitch output 2 (sth r27=2,0xA0)");

    const Result lLast = AttachEntity((37u << 16) | 37u);
    Check(lLast.muAsserts == 0 && lLast.miCreates == 1 && ResolvedList(lLast, { EmitterKey(37) }),
          "type 37, the last live slot: attaches (37 < 38)");

    // [38, 50): past the SCALAR count, inside the array header's count. The console asserts and
    // skips the list (`bge cr6, loc_826F59F8` jumps straight to ~Instance).
    const unsigned kauEmpty[2] = { 38u, 45u };
    for (unsigned luType : kauEmpty)
    {
        const Result lEmpty = AttachEntity((luType << 16) | 12u);
        char lacLabel[160];
        std::snprintf(lacLabel, sizeof(lacLabel),
                      "type %u (>= mNumWorldEmitters 38, < array count 50): the assert fires", luType);
        Check(lEmpty.muAsserts == 1 &&
              lEmpty.maAssertTexts.size() == 1 &&
              lEmpty.maAssertTexts[0] == "luEmitter < static_cast< uint32_t >( lWorldEmitters.mNumWorldEmitters() )",
              lacLabel);
        std::snprintf(lacLabel, sizeof(lacLabel),
                      "type %u: the list is never consulted (no mWorldEmitters / ChangeWithDefault)", luType);
        Check(lEmpty.maResolved.empty(), lacLabel);
        std::snprintf(lacLabel, sizeof(lacLabel), "type %u: no voice, pitch output 1, returns true", luType);
        Check(lEmpty.miCreates == 0 && lEmpty.mi16PitchOutput == 1 && lEmpty.mbReturned, lacLabel);
    }

    // A live slot whose EmitterName slot is empty: the console hashes the name as it stands
    // (0x826F5890 `lwz r3,0(r11)` ; `bl MakeHash`, no null test) and still creates the voice.
    const u32 luSavedName = gaEmitterLayouts[12].muEmitterName;
    gaEmitterLayouts[12].muEmitterName = 0u;
    const Result lNameless = AttachEntity((12u << 16) | 30u);
    gaEmitterLayouts[12].muEmitterName = luSavedName;
    Check(lNameless.muAsserts == 0 && lNameless.miCreates == 1 && lNameless.miPlays == 1,
          "type 12 with an empty EmitterName: the voice is still created (no invented null skip)");
    Check(lNameless.muContentSpec == 0u, "type 12 with an empty EmitterName: content spec = MakeHash(null) = 0");

    // The stale build/game bytes of the same entity: the console gate refuses them as well --
    // the assert the lanes saw is the console's own verdict on the wrong data.
    const Result lStale = AttachEntity(0x00AD001Du);
    Check(lStale.muAsserts == 1 && lStale.maResolved.empty() && lStale.miCreates == 0,
          "stale per-u16 flip 0x00AD001D (type 173): asserts, attaches nothing");

    // ---- Detach @0x826F5A10 (the export hole after Attach): the staged meDetachState switch.
    const DetachResult lFresh = DetachFrom(0, true);
    Check(lFresh.mbReturned && Order(lFresh, { "base-detach", "voice-release" }) && lFresh.meFinalState == 3,
          "Detach from NONE: BrnEffectObject::Detach first, then the voice release; FINISHED, returns 1");
    Check(lFresh.mbPositionKept, "Detach leaves the 3D control's position pointer alone (no null hand-off)");
    const DetachResult lBegin = DetachFrom(1, true);
    Check(lBegin.mbReturned && Order(lBegin, { "base-detach", "voice-release" }),
          "Detach from BEGIN: same path as NONE (jump-table cases 0 and 1 share 0x826F5A58)");
    const DetachResult lUpdating = DetachFrom(2, true);
    Check(lUpdating.mbReturned && Order(lUpdating, { "base-detach", "voice-release" }),
          "Detach from UPDATING: base detach, then the voice release");
    const DetachResult lFinished = DetachFrom(3, true);
    Check(lFinished.mbReturned && Order(lFinished, { "voice-release" }) && lFinished.meFinalState == 3,
          "Detach from FINISHED: only the voice release (case 3 @0x826F5A7C), returns 1");
    const DetachResult lRefused = DetachFrom(0, false);
    Check(!lRefused.mbReturned && Order(lRefused, { "base-detach" }) && lRefused.meFinalState == 2,
          "base detach refuses: returns 0 in UPDATING, the voice is NOT released yet");
    const DetachResult lBadState = DetachFrom(5, true);
    Check(!lBadState.mbReturned && lBadState.maOrder.empty(),
          "meDetachState > FINISHED: returns 0 and touches nothing (`cmplwi 3 ; bgt`)");

    std::printf("FxEmitterAttach: %u checks, %u failures\n", guChecks, guFailures);
    return guFailures == 0 ? 0 : 1;
}
