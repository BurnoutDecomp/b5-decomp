// FX-RUMBLE3 (crash parity 2026-09-24, G10-D6): the road-surface rumble -- the PRODUCTION
// BrnGameState::RumbleManager::UpdateSurfaceRumble @0x82378AE0 and the three DWARF helpers it inlines on the
// X360 (PlayRumble / ChangeRumbleVolume / StopRumble), plus Construct / Prepare and the file-scope constants,
// extracted from BrnRumbleManager.cpp by run_fxrumble3_surface_rumble.py and compiled against the revision's
// own BrnRumbleManager.h, surface.h and rumblesurface.h, with the production
// RCEntityActiveRaceCarOutputInterface::GetRaceCarState and the real Attrib::StringToKey (attribhash64.cpp).
// The Attrib database is a FAKE with the console's resolution rules: a surface list whose "Surfaces" array
// holds RefSpecs to surface collections, each surface layout carrying a DebugRenderColor at +0x00 and its
// RumbleSurface ref at +0x28, each rumblesurface layout 0x3C bytes. Every rumblesurface lane holds
// (its layout offset / 100), so an event lane names the offset it was copied from.
// Expected values from the ARTIST asm (see the production banner for the addresses):
//   the re-bind       FindCollectionWithDefault(0x42C25F49_85B5C4F4, StringToKey("340654") ==
//                     0xB96A0FF96535775A -- SURFACELIST.BIN's mKey) + Change, every call (0x82378B08..)
//   the sanity check  surface 1's DebugRenderColor with no |lane| > FLT_EPSILON -> the :376 assert
//   the wheel pass    mbLineTestIsValid (+0x2B) && mbIsOnGround (+0x28); id = (tag low half >> 4) & 0x3F;
//                     RumblePriority >= 0 registers the id (first free slot / its own slot's count)
//   live rumbles      found + unpaused + not Picture Paradise + meDriverType PLAYER -> ChangeVolume, else
//                     Stop and the id -> -1
//   new rumbles       the same gates, the first -1 id slot, PlayRumble(id, volume, priority, envelope)
//   events            {miPlayer 0, miPort -1}; envelope low = +0x38 +0x34 +0x24 +0x2C +0x30 +0x28,
//                     high = +0x18 +0x14 +0x04 +0x0C +0x10 +0x08; priority +0x00
//   volume            (Clamp(|speed|, min +0x1C, max +0x20) - min) / Max(max - min, 1.0)
#include "GameSource/GameState/RumbleManager/BrnRumbleManager.h"
#include "GameSource/AttribSys/Generated/classes/surface.h"
#include "GameSource/AttribSys/Generated/classes/rumblesurface.h"
#include "GameShared/GameClasses/Development/Log/CgsLog.h"
#include <cstddef>
#include <cstdio>
#include <cstring>

static unsigned gAsserts = 0, gChecks = 0, gFailures = 0;
static char     gLastAssert[160];

namespace CgsDev
{
namespace Assert
{
    int   BeginAssert() { return 0; }
    int   FireAssert(const char* lpcMessage, const char*, int)
    {
        ++gAsserts;
        std::snprintf(gLastAssert, sizeof(gLastAssert), "%s", lpcMessage ? lpcMessage : "");
        return 0;
    }
    void* EndAssert() { return nullptr; }
}
namespace Log { DebugPrint* gpDebugPrint = nullptr; }   // the [DIAG] lines stay silent
}

// Harness-only: RaceCarState's default ctor calls Clear() (BrnVehicleEvents.cpp, not under test). Every
// field the body reads is set by the cases below.
void BrnPhysics::Vehicle::RaceCarState::Clear() { std::memset(this, 0, sizeof(*this)); }

// ---- the fake Attrib database --------------------------------------------------------------------------
namespace
{
    enum EFakeKind { E_FAKE_SURFACELIST = 1, E_FAKE_SURFACE = 2, E_FAKE_RUMBLESURFACE = 3 };
    struct FakeCollection { int miKind; u64 muKey; void* mpLayout; };

    const u64 KU_SURFACES_ATTRIBUTE_KEY = 0x0ADCE56EF3DA7F1Full;   // surfacelist "Surfaces"
    const int KI_NUM_SURFACES = 6;
    const int KI_NUM_RUMBLES  = 4;

    alignas(16) unsigned char gaSurfaceLayout[KI_NUM_SURFACES][0x90];
    alignas(16) unsigned char gaRumbleLayout[KI_NUM_RUMBLES][0x3C];
    alignas(16) unsigned char gaDefaultArea[0x100];                // Attrib::DefaultDataArea: zeroed

    FakeCollection gSurfaceList = { E_FAKE_SURFACELIST, 0xB96A0FF96535775Aull, nullptr };
    FakeCollection gaSurface[KI_NUM_SURFACES];
    FakeCollection gaRumble[KI_NUM_RUMBLES];
    Attrib::RefSpec gaSurfaceRefs[KI_NUM_SURFACES];

    int gFindCalls = 0;
    u64 gFindClassKey = 0, gFindCollectionKey = 0;
    int gClassAsserts = 0;

    FakeCollection* Resolve(u64 luKey)
    {
        for (int li = 0; li < KI_NUM_SURFACES; ++li)
            if (gaSurface[li].muKey == luKey && luKey != 0) return &gaSurface[li];
        for (int li = 0; li < KI_NUM_RUMBLES; ++li)
            if (gaRumble[li].muKey == luKey && luKey != 0) return &gaRumble[li];
        return nullptr;
    }
    FakeCollection* Fake(const Attrib::Collection* lpCollection)
    {
        return reinterpret_cast<FakeCollection*>(const_cast<Attrib::Collection*>(lpCollection));
    }
}

namespace Attrib
{
    Instance::Instance(Collection* lpCollection, void* lpOwner)
        : mpCollection(lpCollection), mpAttributeData(lpCollection ? Fake(lpCollection)->mpLayout : nullptr),
          mpOwner(lpOwner), muFlags(0) {}
    Instance::Instance(const RefSpec& lrRefSpec, void* lpOwner)
        : mpCollection(reinterpret_cast<Collection*>(Resolve(lrRefSpec.mCollectionKey))),
          mpAttributeData(nullptr), mpOwner(lpOwner), muFlags(0)
    {
        if (mpCollection) mpAttributeData = Fake(mpCollection)->mpLayout;
    }
    Instance::~Instance() {}
    Collection* Instance::Change(Collection* lpNewCollection)
    {
        Collection* lpOld = mpCollection;
        mpCollection      = lpNewCollection;
        mpAttributeData   = lpNewCollection ? Fake(lpNewCollection)->mpLayout : mpAttributeData;
        return lpOld;
    }
    void* Instance::GetAttributePointer(u64 luAttributeKey, u32 luIndex) const
    {
        if (Fake(mpCollection) != &gSurfaceList || luAttributeKey != KU_SURFACES_ATTRIBUTE_KEY) return nullptr;
        return (luIndex < static_cast<u32>(KI_NUM_SURFACES)) ? &gaSurfaceRefs[luIndex] : nullptr;
    }
    int Instance::GetClass() const
    {
        const FakeCollection* lp = Fake(mpCollection);
        if (!lp) return 0;
        if (lp->miKind == E_FAKE_SURFACELIST)   return -2051685132;                  // 0x85B5C4F4
        if (lp->miKind == E_FAKE_SURFACE)       return 2016857936;                   // 0x7836CF50
        return static_cast<int>(0x14E37D72u);                                        // rumblesurface
    }
    u64 Instance::GetCollection() const { return mpCollection ? Fake(mpCollection)->muKey : 0; }
    const Collection* RefSpec::GetCollection() { return reinterpret_cast<const Collection*>(Resolve(mCollectionKey)); }
    void* DefaultDataArea(u32) { return gaDefaultArea; }
    void AssertOnClassCheck(int, int, u64) { ++gClassAsserts; }
    Collection* FindCollectionWithDefault(u64 luClassKey, u64 luCollectionKey)
    {
        ++gFindCalls;
        gFindClassKey      = luClassKey;
        gFindCollectionKey = luCollectionKey;
        return reinterpret_cast<Collection*>(&gSurfaceList);
    }
    // surfacelist::Num_Surfaces (the [DIAG] surface table's count; silent here: gpDebugPrint is null).
    void* Instance::Get(AttributeValue* pOut, int*, u64) { return pOut; }
    int   Attribute::GetLength() { return KI_NUM_SURFACES; }
}
namespace CgsSceneManager { namespace CgsCollision { void BaseCollisionGenerator_Destruct(void*) {} } }

// The production bodies (and GetRaceCarState), extracted verbatim.
#include "fxrumble3_surface_rumble.inc"

using namespace BrnGameState;
using CgsInput::InputIO::JoltEffect;

static void Check(bool lbPass, const char* lpcName)
{
    ++gChecks;
    if (!lbPass)
    {
        ++gFailures;
        std::printf("  FAIL %s\n", lpcName);
    }
    else
    {
        std::printf("  ok   %s\n", lpcName);
    }
}

// RefSpec's assignment is declaration-only in the SDK header (@0x8280DFB0 lives in the AttribSys runtime), so
// the fake's refs are written field by field (the access macros make the members reachable).
static void SetRef(Attrib::RefSpec& lrRef, u64 luClassKey, u64 luCollectionKey)
{
    lrRef.mClassKey       = luClassKey;
    lrRef.mCollectionKey  = luCollectionKey;
    lrRef.mpCollectionPtr = nullptr;
}

// Surface i: collection key 0x5000 + i; its RumbleSurface ref -> rumble key (0x7000 + r) or 0 (unresolved).
// Rumble r: collection key 0x7000 + r; every f32 lane = offset / 100 + r (so rumbles differ); the priority
// and the speed band are set per case.
static void BuildDatabase()
{
    std::memset(gaSurfaceLayout, 0, sizeof(gaSurfaceLayout));
    std::memset(gaRumbleLayout, 0, sizeof(gaRumbleLayout));
    std::memset(gaDefaultArea, 0, sizeof(gaDefaultArea));
    const int kaiRumbleOfSurface[KI_NUM_SURFACES] = { -1, 0, 1, 2, -1, 3 };   // surface 4: no rumble ref
    for (int li = 0; li < KI_NUM_SURFACES; ++li)
    {
        gaSurface[li] = { E_FAKE_SURFACE, 0x5000ull + li, gaSurfaceLayout[li] };
        SetRef(gaSurfaceRefs[li], 0x68428A3C7836CF50ull, 0x5000ull + li);
        Attrib::Gen::surface::_LayoutStruct* lpLayout = reinterpret_cast<Attrib::Gen::surface::_LayoutStruct*>(gaSurfaceLayout[li]);
        lpLayout->mafDebugRenderColor[0] = 0.25f;                                    // a real colour
        SetRef(lpLayout->mRumbleSurface, 0x540C6D1714E37D72ull,
               kaiRumbleOfSurface[li] >= 0 ? 0x7000ull + kaiRumbleOfSurface[li] : 0ull);
    }
    for (int lr = 0; lr < KI_NUM_RUMBLES; ++lr)
    {
        gaRumble[lr] = { E_FAKE_RUMBLESURFACE, 0x7000ull + lr, gaRumbleLayout[lr] };
        f32* lpfLanes = reinterpret_cast<f32*>(gaRumbleLayout[lr]);
        for (int lo = 4; lo < 0x3C; lo += 4)
            lpfLanes[lo / 4] = static_cast<f32>(lo) / 100.0f + static_cast<f32>(lr);
    }
}

static Attrib::Gen::rumblesurface::_LayoutStruct& Rumble(int lr)
{
    return *reinterpret_cast<Attrib::Gen::rumblesurface::_LayoutStruct*>(gaRumbleLayout[lr]);
}

static BrnWorld::RaceCarEntityModuleIO::RCEntityActiveRaceCarOutputInterface gInterface;
static RumbleManager gManager;
static const EActiveRaceCarIndex KE_PLAYER = E_ACTIVE_RACE_CAR_INDEX_2;

static BrnPhysics::Vehicle::RaceCarState& Player() { return gInterface.maRaceCarStates[KE_PLAYER]; }

static void PutWheel(int liWheel, int liSurface, bool lbValid = true, bool lbOnGround = true)
{
    BrnPhysics::Vehicle::Wheel::RoadContact& lrContact = Player().maWheels[liWheel].mRoadContact;
    // Bits 4..9 of the LOW half carry the id; the rest of the word is noise the body must mask off.
    lrContact.mCollisionTag.muValue = 0xABCD0000u | (0x3u << 10) | (static_cast<u32>(liSurface) << 4) | 0xFu;
    lrContact.mbLineTestIsValid = lbValid;
    lrContact.mbIsOnGround      = lbOnGround;
}

static void Fresh()
{
    BuildDatabase();
    // Rumble 0 (surface 1): priority 5, band [10, 50]. Rumble 1 (surface 2): priority 3, band [20, 20.25]
    // (narrower than 1 -> the Max(.., 1.0) floor). Rumble 2 (surface 3): priority -1 (no rumble).
    // Rumble 3 (surface 5): priority 0 (registered: the gate is >= 0).
    Rumble(0).miRumblePriority = 5;  Rumble(0).mfMinSpeedForRumble = 10.0f; Rumble(0).mfMaxSpeedForRumble = 50.0f;
    Rumble(1).miRumblePriority = 3;  Rumble(1).mfMinSpeedForRumble = 20.0f; Rumble(1).mfMaxSpeedForRumble = 20.25f;
    Rumble(2).miRumblePriority = -1;
    Rumble(3).miRumblePriority = 0;  Rumble(3).mfMinSpeedForRumble = 0.0f;  Rumble(3).mfMaxSpeedForRumble = 100.0f;

    for (int li = 0; li < 8; ++li) gInterface.maRaceCarStates[li].Clear();
    Player().meDriverType = BrnPhysics::Vehicle::E_DRIVER_TYPE_PLAYER;
    Player().mfSpeedMPH   = -30.0f;                                                    // |speed| is used
    for (int lw = 0; lw < 4; ++lw) PutWheel(lw, 0, false, false);                    // nothing on the ground

    gManager.Construct();
    gManager.Prepare();                                                                // ids -1, slots 0xFF / 0
    gFindCalls = 0; gClassAsserts = 0; gAsserts = 0;
}

static void Tick() { gManager.UpdateSurfaceRumble(&gInterface, KE_PLAYER); }

static int Plays()   { return gManager.mPlayRumbleEffectEventQueue.GetLength(); }
static int Volumes() { return gManager.mChangeVolumeRumbleEffectEventQueue.GetLength(); }
static int Stops()   { return gManager.mStopRumbleEffectEventQueue.GetLength(); }
static void Drain()
{
    gManager.mPlayRumbleEffectEventQueue.Clear();
    gManager.mChangeVolumeRumbleEffectEventQueue.Clear();
    gManager.mStopRumbleEffectEventQueue.Clear();
    gManager.mPlayJoltEffectEventQueue.Clear();
}

// The envelope the console builds from rumble r: low = +0x38 +0x34 +0x24 +0x2C +0x30 +0x28,
// high = +0x18 +0x14 +0x04 +0x0C +0x10 +0x08 (each lane = offset / 100 + r).
static bool EnvelopeIs(const JoltEffect& lrEffect, int lr)
{
    const int kaiOffsets[12] = { 0x38, 0x34, 0x24, 0x2C, 0x30, 0x28, 0x18, 0x14, 0x04, 0x0C, 0x10, 0x08 };
    const f32* lpf = &lrEffect.mLowFreqJoltData.mfAttackTime;
    for (int li = 0; li < 12; ++li)
    {
        if (lpf[li] != static_cast<f32>(kaiOffsets[li]) / 100.0f + static_cast<f32>(lr)) return false;
    }
    return true;
}

int main()
{
    static_assert(offsetof(Attrib::Gen::rumblesurface::_LayoutStruct, miRumblePriority) == 0x00, "priority +0x00");
    static_assert(offsetof(Attrib::Gen::rumblesurface::_LayoutStruct, mfRightMotorSustainTime) == 0x04, "+0x04");
    static_assert(offsetof(Attrib::Gen::rumblesurface::_LayoutStruct, mfLeftMotorSustainSpeed) == 0x28, "+0x28");
    static_assert(offsetof(Attrib::Gen::surface::_LayoutStruct, mRumbleSurface) == 0x28, "RumbleSurface +0x28");

    // ---- an invalid player car index: nothing at all ------------------------------------------------
    Fresh();
    gManager.UpdateSurfaceRumble(&gInterface, E_ACTIVE_RACE_CAR_INDEX_INVALID);
    Check(gFindCalls == 0 && Plays() == 0 && Volumes() == 0 && Stops() == 0,
          "lePlayerCarIndex == -1: returns before the re-bind (0x82378B00)");

    // ---- the re-bind and the sanity check -------------------------------------------------------------
    Fresh();
    Tick();
    Check(gFindCalls == 1 && gFindClassKey == 0x42C25F4985B5C4F4ull,
          "the surface list is re-bound every call: FindCollectionWithDefault(surfacelist 0x42C25F4985B5C4F4, ..)");
    Check(gFindCollectionKey == 0xB96A0FF96535775Aull,
          "...with the collection key StringToKey(\"340654\") == 0xB96A0FF96535775A (SURFACELIST.BIN's mKey)");
    Check(gAsserts == 0 && gClassAsserts == 0, "a surface 1 with a real DebugRenderColor passes the sanity check quietly");
    Check(Plays() == 0 && Volumes() == 0 && Stops() == 0, "no wheel on the ground: no rumble requests");
    reinterpret_cast<Attrib::Gen::surface::_LayoutStruct*>(gaSurfaceLayout[1])->mafDebugRenderColor[0] = 1.0e-8f;
    Tick();
    Check(gAsserts == 1 && std::strcmp(gLastAssert, "Surface list appears to be corrupt") == 0,
          "surface 1's DebugRenderColor with no |lane| > FLT_EPSILON (unk_82029BA4) asserts :376");

    // ---- the wheel pass and the first plays ---------------------------------------------------------
    Fresh();
    PutWheel(0, 1); PutWheel(1, 1); PutWheel(2, 2); PutWheel(3, 3);                  // surface 3: priority -1
    Tick();
    Check(gManager.mau8SurfaceID[0] == 1 && gManager.mau8NumWheelsOnSurface[0] == 2
          && gManager.mau8SurfaceID[1] == 2 && gManager.mau8NumWheelsOnSurface[1] == 1
          && gManager.mau8SurfaceID[2] == 0xFF && gManager.mau8NumWheelsOnSurface[2] == 0,
          "wheels 0/1 on surface 1 share slot 0 (count 2), wheel 2 takes slot 1; surface 3 (priority -1) is not listed");
    Check(Plays() == 2 && Volumes() == 0 && Stops() == 0, "two new surfaces -> two PlayRumble requests, nothing else");
    Check(gManager.manRumbleID[0] == 1 && gManager.manRumbleID[1] == 2 && gManager.manRumbleID[2] == -1,
          "the new rumbles take the first free id slots in surface order: ids {1, 2, -1, -1}");
    if (Plays() == 2)
    {
        const CgsInput::InputIO::PlayRumbleEffectEvent& lr0 = gManager.mPlayRumbleEffectEventQueue.GetEvent(0);
        const CgsInput::InputIO::PlayRumbleEffectEvent& lr1 = gManager.mPlayRumbleEffectEventQueue.GetEvent(1);
        Check(lr0.miPlayer == 0 && lr0.miPort == -1 && lr0.miRumblePriority == 5 && lr0.miRumbleId == 1,
              "play 0: {player 0, port -1, priority 5 (+0x00), id 1}");
        Check(EnvelopeIs(lr0.mJoltEffect, 0), "play 0: the envelope lanes come from +0x38/34/24/2C/30/28 | +0x18/14/04/0C/10/08");
        Check(lr0.mfRumbleVolume == 0.5f, "play 0: volume (Clamp(|-30|, 10, 50) - 10) / Max(40, 1) = 0.5");
        Check(lr1.miRumblePriority == 3 && lr1.miRumbleId == 2 && EnvelopeIs(lr1.mJoltEffect, 1),
              "play 1: surface 2's rumble -- priority 3, id 2, its own envelope");
        Check(lr1.mfRumbleVolume == 0.25f, "play 1: a band narrower than 1 is floored: (20.25 - 20) / Max(0.25, 1.0) = 0.25");
    }
    else
    {
        Check(false, "play 0: {player 0, port -1, priority 5 (+0x00), id 1}");
        Check(false, "play 0: the envelope lanes come from +0x38/34/24/2C/30/28 | +0x18/14/04/0C/10/08");
        Check(false, "play 0: volume (Clamp(|-30|, 10, 50) - 10) / Max(40, 1) = 0.5");
        Check(false, "play 1: surface 2's rumble -- priority 3, id 2, its own envelope");
        Check(false, "play 1: a band narrower than 1 is floored: (20.25 - 20) / Max(0.25, 1.0) = 0.25");
    }

    // ---- the next frame: both live rumbles are refreshed --------------------------------------------
    Drain();
    Player().mfSpeedMPH = 60.0f;                                                         // above rumble 0's max
    Tick();
    Check(Plays() == 0 && Volumes() == 2 && Stops() == 0, "same surfaces next frame: two ChangeVolume requests, no plays");
    if (Volumes() == 2)
    {
        const CgsInput::InputIO::ChangeVolumeRumbleEffectEvent& lr0 = gManager.mChangeVolumeRumbleEffectEventQueue.GetEvent(0);
        Check(lr0.miPlayer == 0 && lr0.miPort == -1 && lr0.miRumbleId == 1 && EnvelopeIs(lr0.mJoltEffect, 0)
              && lr0.mfRumbleVolume == 1.0f,
              "volume 0: {0, -1, envelope, id 1, (Clamp(60, 10, 50) - 10) / 40 = 1.0}");
    }
    else
    {
        Check(false, "volume 0: {0, -1, envelope, id 1, (Clamp(60, 10, 50) - 10) / 40 = 1.0}");
    }

    // ---- a surface left behind is stopped -----------------------------------------------------------
    Drain();
    PutWheel(2, 1);                                                                      // all driven wheels on surface 1
    Tick();
    Check(Stops() == 1 && gManager.mStopRumbleEffectEventQueue.GetEvent(0).miRumbleId == 2
          && gManager.mStopRumbleEffectEventQueue.GetEvent(0).miPlayer == 0
          && gManager.mStopRumbleEffectEventQueue.GetEvent(0).miPort == -1,
          "surface 2 left: StopRumble {0, -1, id 2}");
    Check(gManager.manRumbleID[1] == -1 && Volumes() == 1 && Plays() == 0,
          "...its id slot goes back to -1; surface 1 is refreshed, nothing new plays");

    // ---- the three gates stop every live rumble and start none --------------------------------------
    for (int liGate = 0; liGate < 3; ++liGate)
    {
        Fresh();
        PutWheel(0, 1); PutWheel(1, 2);
        Tick();                                                                          // two rumbles live
        Drain();
        if (liGate == 0) gManager.mbRumblePaused = true;
        if (liGate == 1) gManager.mbInPictureParadise = true;
        if (liGate == 2) Player().meDriverType = static_cast<BrnPhysics::Vehicle::E_DRIVER_TYPE>(1);
        Tick();
        const char* kapcNames[3] = { "paused (+0x399): both live rumbles stopped, none played",
                                     "Picture Paradise (+0x39B): both live rumbles stopped, none played",
                                     "not the player driving (meDriverType +0x458 != 0): both stopped, none played" };
        Check(Stops() == 2 && Volumes() == 0 && Plays() == 0 && gManager.manRumbleID[0] == -1 && gManager.manRumbleID[1] == -1,
              kapcNames[liGate]);
    }

    // ---- the wheel filters ----------------------------------------------------------------------------
    Fresh();
    PutWheel(0, 1, false, true);                                                        // line test invalid
    PutWheel(1, 2, true, false);                                                        // not on the ground
    Tick();
    Check(Plays() == 0 && gManager.mau8NumWheelsOnSurface[0] == 0,
          "a wheel needs BOTH mbLineTestIsValid (+0x2B) and mbIsOnGround (+0x28)");

    Fresh();
    PutWheel(0, 5);                                                                      // rumble priority 0
    PutWheel(1, 4);                                                                      // no rumble ref -> default layout
    Tick();
    Check(gManager.mau8SurfaceID[0] == 5 && gManager.mau8SurfaceID[1] == 4 && Plays() == 2,
          "priority 0 and an unresolved rumble ref (the zeroed default layout: priority 0) both register -- the gate is >= 0");

    std::printf("FxRumble3SurfaceRumble: %u checks, %u failures\n", gChecks, gFailures);
    return gFailures == 0 ? 0 : 1;
}
