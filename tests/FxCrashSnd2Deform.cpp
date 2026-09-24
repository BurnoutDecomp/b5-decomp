// FX-CRASHSND2 (crash parity 2026-09-24, item 3): the deformation legs of the collision sound against
// the ARTIST machine code.
//
//   MapBodyPartEnumToMateral                            0x82688CF8     DWARF cpp:3677
//   InputCollision (glass)                              sub_826BE398   DWARF h:466
//   InputCollision (hinged part)                        sub_826BDF60   DWARF h:460
//   InputCollision (broken joint / detached part)       sub_826BE108 / sub_826BE250   DWARF h:442 / h:451
//   InputCollision (physical car-part contact)          sub_826BDCB8   DWARF h:433
//   CollisionStateManager::UpdateGlass                  0x826D4850     DWARF cpp:3458
//   CollisionStateManager::UpdateHingingBodyParts       0x826D44D0     DWARF cpp:3078
//   HingeStateCache::Update / FindInCache / Insert      0x826830D8 / (inlined) / 0x826831A8
//
// run_fxcrashsnd2_deform.py extracts the PRODUCTION builders region (item 1: the material / orientation
// mappers the new builders call) and the deformation region of BrnCollisionStateManager.cpp, the
// HingeStateCache bodies, TrafficClassToSize, GetRaceCar and GetTrafficEntityIndex, and compiles them
// against a fixture manager / sound input. The event, contact, InputCollision, HingeStateCache and
// EntityId types are the real headers.
#include "types.hpp"
#include "BrnCommonTypes.h"
#include "GameSource/BurnoutConstants.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"
#include "GameShared/GameClasses/SceneManager/CgsEntityId.h"
#include "GameSource/Sound/Collision/BrnCollisionDataStructures.h"
#include "GameSource/Sound/Collision/BrnRaceCarCache.h"
#include "GameSource/Sound/Collision/BrnHingeStateCache.h"
#include "GameSource/Physics/ContactSpies/BrnContactSpyEvents.h"
#include "GameSource/Physics/DeformationManager/SharedIO/BrnDeformationOutputInterface.h"
#include "GameSource/World/EntityModules/TrafficEntityModule/SharedIO/BrnTrafficSoundInterfaces.h"
#include "GameSource/World/BrnEntityTypes.h"
#include "GameSource/AttribSys/Enums/eMaterialType.h"
#include "SharedClasses/Traffic/BrnTrafficVehicleType.h"

#include <cmath>
#include <cstdio>
#include <limits>
#include <map>
#include <vector>

static unsigned guAsserts = 0;
namespace CgsDev { namespace Assert {
int BeginAssert() { return 0; }
int FireAssert(const char*, const char*, int) { ++guAsserts; return 0; }
void* EndAssert() { return nullptr; }
} }

// ---- the sound input the builders read ------------------------------------------------------------
namespace BrnWorld { namespace RaceCarEntityModuleIO {
struct VehicleInterfaceFixture
{
    s32 miPlayer = 0;
    s32 GetPlayerActiveRaceCarIndex() const { return miPlayer; }
};
struct RCEntityActiveRaceCarOutputInterface { };
} }
namespace BrnSound { namespace Module { namespace Io {
struct RootInputBuffer
{
    BrnTraffic::BrnTrafficIO::TrafficSoundOutputInterface mTraffic = {};
    BrnWorld::RaceCarEntityModuleIO::VehicleInterfaceFixture mVehicle;
    s32 miSoundPlayer = 0;   // RootInputBuffer::GetPlayerActiveRaceCarIndex @0x82694F28 (the input's own)
    const BrnTraffic::BrnTrafficIO::TrafficSoundOutputInterface& GetTrafficOutputInterface() const { return mTraffic; }
    const BrnWorld::RaceCarEntityModuleIO::VehicleInterfaceFixture* GetVehicleInterface() const { return &mVehicle; }
    EActiveRaceCarIndex GetPlayerActiveRaceCarIndex() const { return static_cast<EActiveRaceCarIndex>(miSoundPlayer); }
};
} } }

namespace BrnTraffic { namespace BrnTrafficIO {
#include "fxcrashsnd2_get_traffic_entity_index.inc"
} }

namespace BrnSound { namespace Logic { namespace Traffic {
enum ETrafficSize { E_SMALL = 0, E_MEDIUM = 1, E_LARGE = 2, E_MAX_SIZES = 3 };
struct TrafficStateManager
{
#include "fxcrashsnd2_traffic_class_to_size.inc"
};
} } }

namespace CgsSound { namespace Logic { class Module { public: virtual ~Module() {} }; } }
namespace BrnSound { namespace Module {
class SoundLogicModule : public CgsSound::Logic::Module
{
public:
    Io::RootInputBuffer* mpInput = nullptr;
    Io::RootInputBuffer* GetBrnInputStructure() { return mpInput; }
};
} }

namespace BrnPhysics { namespace Vehicle { struct RaceCarState { Matrix44Affine mTransform; Vector3 mComOffset; EntityId mEntityId; }; } }
namespace BrnPhysics { namespace Deformation {
struct CarState { Vector3 mDeformedBBoxMin; Vector3 mDeformedBBoxMax; };
struct DeformationState { const CarState* GetCarStateF(u32) const { return nullptr; } };
} }

namespace BrnSound { namespace Logic { namespace Collision {

#include "fxcrashsnd2_hinge_cache_bodies.inc"

#include "fxcrashsnd2_get_race_car.inc"

typedef AttribSys::Enums::eMaterialType::eMaterialType EeMaterialType;

struct CameraInfo
{
    Matrix44Affine mTransform;
    f32 mfFieldOfView, mfCosineHalfFov, mfAspectRatio, mfZoom;
};

#include "fxcrashsnd2_generic_entity.inc"

class CollisionStateManager
{
public:
    // ---- the production members the regions reach ----
    f32 mfCurrentTime = 0.0f;
    CameraInfo mCameraInfo;
    RaceCarCache mRaceCarCache;
    HingeStateCache mHingeCache;
    FrameInformation mFrameInformation;
    CgsSound::Logic::Module* mpLogicModule = nullptr;
    CgsSound::Logic::Module* GetLogicModule() const { return mpLogicModule; }
    const RaceCarCache& GetRaceCarCache() const { return mRaceCarCache; }
    const FrameInformation& GetFrameInformation() const { return mFrameInformation; }
    ScrapeInfo* FindInScrapeHistory(const ScrapeInfo&) { return nullptr; }
    bool MapPropTypeToMaterial(u16, u64&) const { return false; }
    void UpdateGlass(const BrnPhysics::Deformation::DeformationOutputInterface& lrDeformation);
    void UpdateHingingBodyParts(const BrnPhysics::Deformation::DeformationOutputInterface::JointedPartStateQueue* lpQueue);

    // ---- recording fakes ----
    std::vector<InputCollision> maAdded;
    void AddInputCollision(const InputCollision& lrCollision) { maAdded.push_back(lrCollision); }
    std::map<u32, Vector3> mKnown;      // FindEntity answers: the vehicles it knows, at these positions
    mutable int miFindEntityCalls = 0;
    bool FindEntity(const EntityId& lEntityId, GenericEntity& lEntity) const
    {
        ++miFindEntityCalls;
        std::map<u32, Vector3>::const_iterator it = mKnown.find(lEntityId.muValue);
        if (it == mKnown.end())
            return false;
        lEntity.mPosition = it->second;
        return true;
    }
};

EeMaterialType MapEntityIdToMaterial(EntityId lEntityId, s32 liPlayerIndex, const LogicInputBuffer& lInput);
EeMaterialType MapBodyPartEnumToMateral(BrnPhysics::Deformation::EBodyParts leBodyPart);
AttribSys::Enums::eOrientation::eOrientation MapPositionToOrientationUsingBox(
    Vector3 lPosition, Vector3 lNormal, Matrix44Affine lTransform, Vector3 lUnused,
    Vector3 lComOffset, Vector3 lMin, Vector3 lMax);
bool MapPositionToOrientation(Matrix44Affine lTransform, Vector3 lPosition, Vector3 lNormal,
                              EntityId lVehicleIdA, EntityId lVehicleIdB,
                              const CollisionStateManager& lMgr,
                              AttribSys::Enums::eOrientation::eOrientation& leOrientation);

#include "fxcrashsnd2_builders_region.inc"

#include "fxcrashsnd2_deform_region.inc"

} } }

// ---- checks ----------------------------------------------------------------------------------------
using namespace BrnSound::Logic;
using namespace BrnSound::Logic::Collision;
namespace eM = AttribSys::Enums::eMaterialType;
namespace eA = AttribSys::Enums::eAction;
namespace eO = AttribSys::Enums::eOrientation;
namespace Def = BrnPhysics::Deformation;

static int giChecks = 0;
static int giFailures = 0;
static void Check(bool lbCondition, const char* lpcLabel)
{
    ++giChecks;
    if (!lbCondition)
    {
        ++giFailures;
        std::printf("FAIL  %s\n", lpcLabel);
    }
}
static bool Near(f32 a, f32 b) { return std::fabs(a - b) <= 1e-4f * std::max(1.0f, std::fabs(b)); }
static const f32 KF_NAN = std::numeric_limits<f32>::quiet_NaN();

static EntityId Id(u32 luOwner, u32 luIndex, u32 luPart = 0)
{
    EntityId l; l.muValue = (luOwner << 24) | (luIndex << 10) | luPart; return l;
}
static Vector3 V(f32 x, f32 y, f32 z) { Vector3 l = { x, y, z, 0.0f }; return l; }

struct Rig
{
    BrnSound::Module::Io::RootInputBuffer mInput;
    BrnSound::Module::SoundLogicModule mModule;
    CollisionStateManager mMgr;
    Rig()
    {
        mModule.mpInput = &mInput;
        mMgr.mpLogicModule = &mModule;
        // The camera at the origin looking down +z; the player's car is race car 0 for both sources.
        Matrix44Affine& lrT = mMgr.mCameraInfo.mTransform;
        lrT.xAxis = V(1, 0, 0); lrT.yAxis = V(0, 1, 0); lrT.zAxis = V(0, 0, 1); lrT.wAxis = V(0, 0, 0);
        mMgr.mFrameInformation.mPlayerTransform = lrT;
    }
};

static Def::GlassSmashOrCrackEvent Glass(EntityId lVehicle, s32 liPart, Def::EGlassState leState, bool lbDontPlay)
{
    Def::GlassSmashOrCrackEvent l = {};
    l.mVehicleEntityId = lVehicle;
    l.meGlassPart = static_cast<Def::EBodyParts>(liPart);
    l.meNewState = leState;
    l.mbDontPlaySmashEffect = lbDontPlay;
    return l;
}

static Def::JointedPartStateEvent Hinge(EntityId lVehicle, s32 liPart, f32 lfOrientation, f32 lfVelocity)
{
    Def::JointedPartStateEvent l = {};
    l.mVehicleId = lVehicle;
    l.meType = static_cast<Def::EBodyParts>(liPart);
    l.mfCurrentOrientation = lfOrientation;
    l.mfHingeVelocity = lfVelocity;
    return l;
}

int main()
{
    const EntityId kPlayer = Id(1, 0), kRival = Id(1, 1), kUnknown = Id(1, 5);

    // ---- MapBodyPartEnumToMateral (0x82688CF8) ------------------------------------------------------
    {
        // The image's switch, value by value (0x82688CF8's cases; everything else Nothing).
        std::map<s32, u32> lTable;
        for (s32 v : { 0, 3, 4, 8, 10, 12, 13, 14, 15 }) lTable[v] = eM::BodyPartLarge;
        for (s32 v : { 1, 2, 5, 6, 7, 24, 25, 26, 27, 28, 29 }) lTable[v] = eM::BodyPartSmall;
        for (s32 v : { 9, 11, 103 }) lTable[v] = eM::Mirrors;
        for (s32 v : { 16, 17, 18, 19 }) lTable[v] = eM::GlassSmall;
        for (s32 v : { 20, 21, 22, 23 }) lTable[v] = eM::GlassLarge;
        for (s32 v : { 38, 39 }) lTable[v] = eM::NumberPlate;
        for (s32 v : { 40, 41, 42, 43, 44, 121, 122 }) lTable[v] = eM::Lights;
        for (s32 v : { 45, 46, 47 }) lTable[v] = eM::Body;
        for (s32 v : { 48, 49, 50, 51, 52, 53 }) lTable[v] = eM::Suspension;
        for (s32 v : { 84, 85 }) lTable[v] = eM::Exhaust;
        for (s32 v : { 87, 88, 89, 90, 91, 94, 95, 118, 132 }) lTable[v] = eM::Wheels;
        for (s32 v : { 96, 97, 98, 99, 100, 101 }) lTable[v] = eM::Seats;
        lTable[106] = eM::Extinguisher;
        for (s32 v : { 107, 116, 117 }) lTable[v] = eM::RoofRacks;
        lTable[120] = eM::Ladder;
        lTable[127] = eM::Crane;
        lTable[128] = eM::Mixer;
        lTable[129] = eM::Tipper;
        int liWrong = 0;
        for (s32 v = -2; v < 140; ++v)
        {
            const u32 luWant = lTable.count(v) ? lTable[v] : static_cast<u32>(eM::Nothing);
            if (static_cast<u32>(MapBodyPartEnumToMateral(static_cast<Def::EBodyParts>(v))) != luWant)
                ++liWrong;
        }
        Check(liWrong == 0, "MapBodyPartEnumToMateral: every EBodyParts value maps as the image's switch table");
    }

    // ---- the glass builder (sub_826BE398) and UpdateGlass (0x826D4850) ------------------------------
    {
        Rig r;
        r.mInput.miSoundPlayer = 0;
        const GenericEntity lEntity = [] { GenericEntity l; l.mPosition = V(0, 3, 4); return l; }();
        InputCollision lGlass(r.mMgr, Glass(kPlayer, 20, Def::E_GLASS_STATE_SMASHED, false), lEntity);
        Check(!lGlass.mbCull && lGlass.meAction == eA::Collision && lGlass.meOrientation == eO::Front &&
                  lGlass.mePipeline == InputCollision::E_REGULAR && !lGlass.mScrapeInfo.mbValid &&
                  lGlass.mfPriorityAddition == 0.0f && lGlass.mPosition.y == 3.0f,
              "Glass: a player's smash is a regular Collision at the vehicle's position, not culled");
        Check(lGlass.maMaterial[0] == static_cast<u64>(eM::PlayerCar) && lGlass.maMaterial[1] == static_cast<u64>(eM::GlassLarge),
              "Glass: materials = the vehicle (PlayerCar) and the pane (windscreen -> GlassLarge), no extra bits");
        Check(lGlass.maEntityID[0].muValue == (kPlayer.muValue | 2u) && lGlass.maEntityID[1].muValue == kPlayer.muValue,
              "Glass: maEntityID[0] = the vehicle with meNewState (+0xA8) as its part index, [1] = the vehicle");
        Check(lGlass.maParameter[0].x == 3.0f && lGlass.maParameter[0].w == 3.0f,
              "Glass: the impulse lane is KF_MAX_IMPULSE (splat 3.0, unk_82FFBF60)");
        Check(lGlass.maParameter[1].x == 25.0f && lGlass.maParameter[2].x == 4.0f,
              "Glass: distance squared to the camera, and the RAW offset dotted with the camera's At (not normalised)");

        Check(InputCollision(r.mMgr, Glass(kPlayer, 20, Def::E_GLASS_STATE_CRACKED, false), lEntity).mbCull,
              "Glass: a crack is culled (only a smash sounds)");
        Check(InputCollision(r.mMgr, Glass(kPlayer, 20, Def::E_GLASS_STATE_SMASHED, true), lEntity).mbCull,
              "Glass: a smash that asked for no effect is culled");
        const InputCollision lAi(r.mMgr, Glass(kRival, 16, Def::E_GLASS_STATE_SMASHED, false), lEntity);
        Check(lAi.mbCull && lAi.maMaterial[0] == static_cast<u64>(eM::AiCar) && lAi.maMaterial[1] == static_cast<u64>(eM::GlassSmall),
              "Glass: an AI car's glass is culled (`cmpldi 4`)");
        r.mInput.miSoundPlayer = 1;
        r.mInput.mVehicle.miPlayer = 0;
        Check(InputCollision(r.mMgr, Glass(kRival, 16, Def::E_GLASS_STATE_SMASHED, false), lEntity).maMaterial[0] ==
                  static_cast<u64>(eM::PlayerCar),
              "Glass: the player index is the module input's own (RootInputBuffer::GetPlayerActiveRaceCarIndex)");

        // UpdateGlass walks the queue, looking a vehicle up only when it changes.
        Rig u;
        Def::DeformationOutputInterface lOut = {};
        lOut.mGlassSmashOrCrackQueue.Construct();
        lOut.mGlassSmashOrCrackQueue.AddEventSafe(Glass(kPlayer, 20, Def::E_GLASS_STATE_SMASHED, false));
        lOut.mGlassSmashOrCrackQueue.AddEventSafe(Glass(kPlayer, 21, Def::E_GLASS_STATE_SMASHED, false));
        lOut.mGlassSmashOrCrackQueue.AddEventSafe(Glass(kUnknown, 20, Def::E_GLASS_STATE_SMASHED, false));
        lOut.mGlassSmashOrCrackQueue.AddEventSafe(Glass(kPlayer, 16, Def::E_GLASS_STATE_CRACKED, false));
        u.mMgr.mKnown[kPlayer.muValue] = V(7, 0, 0);
        u.mMgr.UpdateGlass(lOut);
        Check(u.mMgr.maAdded.size() == 3 && u.mMgr.maAdded[0].mPosition.x == 7.0f &&
                  u.mMgr.maAdded[1].maMaterial[1] == static_cast<u64>(eM::GlassLarge) && u.mMgr.maAdded[2].mbCull,
              "UpdateGlass: every event of a vehicle FindEntity knows becomes a collision (the crack added culled)");
        Check(u.mMgr.miFindEntityCalls == 2,
              "UpdateGlass: FindEntity only when the vehicle differs from the last one FOUND (2 lookups for 4 events: "
              "the unknown vehicle does not replace the cached one)");
    }

    // ---- the hinge builder (sub_826BDF60) -----------------------------------------------------------
    {
        Rig r;
        GenericEntity lEntity; lEntity.mPosition = V(0, 3, 4);
        const InputCollision lHinge(r.mMgr, static_cast<Def::EBodyParts>(8), kPlayer, lEntity, eA::HingeOpen, -7.0f);
        Check(lHinge.meAction == eA::HingeOpen && lHinge.meOrientation == eO::Front && !lHinge.mbCull &&
                  lHinge.mePipeline == InputCollision::E_REGULAR && lHinge.mPosition.z == 4.0f,
              "Hinge: the given action, Front, regular, at the vehicle's position");
        Check(lHinge.maMaterial[0] == static_cast<u64>(eM::BodyPartLarge) && lHinge.maMaterial[1] == static_cast<u64>(eM::PlayerCar),
              "Hinge: materials = the part (door -> BodyPartLarge) then the vehicle");
        Check(lHinge.maEntityID[0].muValue == (kPlayer.muValue | 8u) && lHinge.maEntityID[1].muValue == kPlayer.muValue,
              "Hinge: maEntityID[0] = the vehicle with the part as its part index");
        Check(lHinge.maParameter[0].x == 7.0f && lHinge.maParameter[1].x == 25.0f && lHinge.maParameter[2].x == 4.0f,
              "Hinge: |velocity| in the impulse lane; the camera lanes not normalised");
    }

    // ---- UpdateHingingBodyParts (0x826D44D0) + HingeStateCache ---------------------------------------
    {
        Rig r;
        r.mMgr.mKnown[kPlayer.muValue] = V(1, 2, 3);
        Def::DeformationOutputInterface lOut = {};
        auto Frame = [&](f32 lfTime, std::initializer_list<Def::JointedPartStateEvent> laEvents) {
            r.mMgr.mfCurrentTime = lfTime;
            r.mMgr.maAdded.clear();
            lOut.mJointedPartStateQueue.Construct();
            for (const Def::JointedPartStateEvent& e : laEvents)
                lOut.mJointedPartStateQueue.AddEventSafe(e);
            r.mMgr.UpdateHingingBodyParts(&lOut.mJointedPartStateQueue);
        };

        Frame(1.0f, { Hinge(kPlayer, 8, 0.5f, 1.0f) });
        const HingeStateCache::CacheNode& lrNode = r.mMgr.mHingeCache.maEvents[0];
        Check(r.mMgr.maAdded.empty() && lrNode.mbValid && lrNode.mEvent.mfCurrentOrientation == 0.5f &&
                  !lrNode.mbHingeOpen && !lrNode.mbHingeClose && lrNode.mfTimeLastSeen == 1.0f,
              "Hinging: a part the cache does not hold is inserted (event, flags, clock) and makes no sound");

        Frame(1.05f, { Hinge(kPlayer, 8, 1.0f, -2.0f) });
        Check(r.mMgr.maAdded.size() == 1 && r.mMgr.maAdded[0].meAction == eA::HingeOpen &&
                  r.mMgr.maAdded[0].maParameter[0].x == 2.0f && r.mMgr.maAdded[0].mPosition.x == 1.0f &&
                  lrNode.mbHingeOpen,
              "Hinging: the orientation reaching 1 is a HingeOpen with the new velocity, at the vehicle");

        Frame(1.1f, { Hinge(kPlayer, 8, 1.3f, -2.0f) });
        Check(r.mMgr.maAdded.empty() && lrNode.mbHingeOpen,
              "Hinging: still open (1.3 clamped to 1) -- no second HingeOpen");

        Frame(1.15f, { Hinge(kPlayer, 8, -0.2f, 3.0f) });
        Check(r.mMgr.maAdded.size() == 1 && r.mMgr.maAdded[0].meAction == eA::HingeClose && lrNode.mbHingeClose &&
                  !lrNode.mbHingeOpen,
              "Hinging: the orientation reaching 0 (-0.2 clamped) is a HingeClose");

        Frame(1.2f, { Hinge(kPlayer, 8, 0.5f, 3.05f) });
        Check(r.mMgr.maAdded.empty(), "Hinging: a small velocity change (0.05 <= KF_HINGING_VELOCITY_DELTA_TOLERANCE) is quiet");
        Frame(1.25f, { Hinge(kPlayer, 8, 0.5f, 5.5f) });
        Check(r.mMgr.maAdded.empty(), "Hinging: a big change with both velocities under KF_HINGING_VELOCITY_MIN_TOLERANCE (6) is quiet");
        Frame(1.3f, { Hinge(kPlayer, 8, 0.5f, -6.5f) });
        Check(r.mMgr.maAdded.size() == 1 && r.mMgr.maAdded[0].meAction == eA::Hinging &&
                  r.mMgr.maAdded[0].maParameter[0].x == 6.5f,
              "Hinging: a swing (change > 0.1, one velocity > 6) is Hinging with the larger magnitude");
        Frame(1.35f, { Hinge(kPlayer, 8, 0.5f, 1.0f) });
        Check(r.mMgr.maAdded.size() == 1 && r.mMgr.maAdded[0].maParameter[0].x == 6.5f,
              "Hinging: slowing from -6.5 to 1 sounds with the OLD magnitude (the fsel picks the larger)");

        Frame(1.4f, { Hinge(kPlayer, 8, KF_NAN, 1.0f) });
        Check(lrNode.mbHingeOpen && r.mMgr.maAdded.size() == 1 && r.mMgr.maAdded[0].meAction == eA::HingeOpen,
              "Hinging: a NaN orientation clamps to 1 (both fsel tests fail) -- open");

        // A vehicle FindEntity does not know: no sound, but the node still takes the event.
        const EntityId kGhost = Id(2, 9);
        Frame(1.45f, { Hinge(kGhost, 3, 0.5f, 0.0f) });
        Frame(1.5f, { Hinge(kGhost, 3, 1.0f, 9.0f) });
        const HingeStateCache::CacheNode* lpGhost = r.mMgr.mHingeCache.FindInCache(Hinge(kGhost, 3, 0.0f, 0.0f));
        Check(r.mMgr.maAdded.empty() && lpGhost && lpGhost->mbHingeOpen && lpGhost->mEvent.mfHingeVelocity == 9.0f &&
                  lpGhost->mfTimeLastSeen == 1.5f,
              "Hinging: a part of a vehicle FindEntity cannot find is silent but its node is updated");

        // Aging: a node not seen for 0.1 s is dropped by HingeStateCache::Update and re-inserted.
        Frame(1.7f, { Hinge(kPlayer, 8, 0.0f, 20.0f) });
        Check(r.mMgr.maAdded.empty() && lrNode.mbValid && lrNode.mfTimeLastSeen == 1.7f,
              "Hinging: a node 0.25 s stale (>= 0.1, flt_820ABB14) is aged out and the part re-inserted silently");

        // FindInCache / Insert.
        HingeStateCache lCache;
        Check(lCache.FindInCache(Hinge(kPlayer, 8, 0, 0)) == nullptr, "HingeStateCache: an empty cache holds nothing");
        HingeStateCache::CacheNode* lp0 = lCache.Insert(Hinge(kPlayer, 8, 0.25f, 1.5f));
        Check(lp0 == &lCache.maEvents[0] && lp0->mbValid && lp0->mEvent.mfHingeVelocity == 1.5f,
              "HingeStateCache::Insert: the first free node takes the event, valid");
        Check(lCache.FindInCache(Hinge(kPlayer, 8, 0, 0)) == lp0 && !lCache.FindInCache(Hinge(kPlayer, 9, 0, 0)) &&
                  !lCache.FindInCache(Hinge(kRival, 8, 0, 0)),
              "HingeStateCache::FindInCache: same vehicle AND same part type");
        for (int i = 1; i < 32; ++i)
            lCache.Insert(Hinge(Id(1, 2, 0), i, 0, 0));
        Check(lCache.Insert(Hinge(kRival, 1, 0, 0)) == nullptr, "HingeStateCache::Insert: a full cache takes nothing");
    }

    // ---- the broken-joint / detached-part builders (sub_826BE108 / sub_826BE250) ----------------------
    {
        Rig r;
        r.mInput.miSoundPlayer = 1;   // the module input says car 1 is the player's
        r.mInput.mVehicle.miPlayer = 0;
        CameraInfo lCamera = r.mMgr.mCameraInfo;
        lCamera.mTransform.zAxis = V(0.6f, 0.8f, 0.0f);
        Def::BrokenJointNotificationEvent lBroken = {};
        lBroken.mPointOnA = V(3, 4, 0); lBroken.mVehicleId = kRival; lBroken.meType = static_cast<Def::EBodyParts>(87);
        const InputCollision lJoint(lCamera, r.mMgr, lBroken, r.mInput, 9.0f, 0.5f);
        Check(lJoint.meAction == eA::Detach && lJoint.meOrientation == eO::Front && !lJoint.mbCull &&
                  lJoint.mPosition.y == 4.0f && lJoint.maEntityID[0].muValue == kRival.muValue && lJoint.maEntityID[1].muValue == 0u,
              "BrokenJoint: a Detach at the point, maEntityID = (the vehicle, 0)");
        Check(lJoint.maMaterial[0] == static_cast<u64>(eM::PlayerCar) && lJoint.maMaterial[1] == static_cast<u64>(eM::Wheels),
              "BrokenJoint: the vehicle's material with the MODULE input's player index, then the part's (no 0x3000000000 bit)");
        Check(lJoint.maParameter[0].x == 3.0f && lJoint.maParameter[1].x == 25.0f && Near(lJoint.maParameter[2].x, 1.0f),
              "BrokenJoint: KF_MAX_IMPULSE, distance squared, and the NORMALISED offset dotted with the camera's At");

        Def::DetachedPartNotificationEvent lDetached = {};
        lDetached.mPointOnA = V(3, 4, 0); lDetached.mVehicleId = kPlayer; lDetached.meType = static_cast<Def::EBodyParts>(3);
        const InputCollision lPart(lCamera, r.mMgr, lDetached, r.mInput, 9.0f, 0.5f);
        Check(lPart.meAction == eA::Detach && lPart.maMaterial[0] == static_cast<u64>(eM::AiCar) &&
                  lPart.maMaterial[1] == static_cast<u64>(eM::BodyPartLarge) && lPart.maParameter[0].x == 3.0f &&
                  Near(lPart.maParameter[2].x, 1.0f),
              "DetachedPart: the same builder (bonnet -> BodyPartLarge)");
    }

    // ---- the physical car-part contact builder (sub_826BDCB8) ---------------------------------------
    {
        Rig r;
        r.mInput.miSoundPlayer = 3;
        r.mInput.mVehicle.miPlayer = 0;   // the car-part builder reads the VEHICLE interface's index
        BrnPhysics::ContactSpy::PhysicalCarPartContact lContact = {};
        lContact.mEntityIdA = Id(BrnWorld::E_ENTITYTYPE_RACECAR_DEFORMABLE_PART, 0, 4);
        lContact.mEntityIdB = Id(0, 0);
        lContact.mNormalStress = V(0, 640000.0f, 0);   // |stress| 640000 -> 640000 / (320000 * 0.5) = 4
        lContact.mNormal = V(0, 1, 0);
        lContact.mPointOnA = V(0, 3, 4);
        lContact.meType = static_cast<BrnPhysics::ContactSpy::EBodyParts>(1);
        const InputCollision lHit(r.mMgr.mCameraInfo, r.mMgr, lContact, r.mInput, 9.0f, 0.5f);
        Check(!lHit.mbCull && lHit.meAction == eA::Collision && lHit.meOrientation == eO::Front &&
                  lHit.maEntityID[0].muValue == lContact.mEntityIdA.muValue && lHit.mPosition.z == 4.0f,
              "CarPart: a Collision at the point on A, orientation from MapPositionToOrientation (A not a car: Front)");
        Check(lHit.maMaterial[0] == static_cast<u64>(eM::BodyPartSmall) && lHit.maMaterial[1] == static_cast<u64>(eM::World),
              "CarPart: the part's material, then the OTHER side's (B -- the world)");
        Check(Near(lHit.maParameter[0].x, 4.0f) && lHit.maParameter[1].x == 25.0f && Near(lHit.maParameter[2].x, 0.8f),
              "CarPart: the normalised impulse (|stress| / (KF_BIGGEST_COLLISION_IN_SECOND * dt)) and normalised facing");

        BrnPhysics::ContactSpy::PhysicalCarPartContact lWheel = lContact;
        lWheel.mEntityIdA = kPlayer;
        lWheel.mEntityIdB = Id(BrnWorld::E_ENTITYTYPE_DETACHED_RACECAR_WHEEL, 0);
        lWheel.meType = static_cast<BrnPhysics::ContactSpy::EBodyParts>(87);
        const InputCollision lWheelHit(r.mMgr.mCameraInfo, r.mMgr, lWheel, r.mInput, 9.0f, 0.5f);
        Check(lWheelHit.maMaterial[0] == static_cast<u64>(eM::Wheels) && lWheelHit.maMaterial[1] == static_cast<u64>(eM::PlayerCar),
              "CarPart: B the part -> the other side is A, mapped with the vehicle interface's player index");

        BrnPhysics::ContactSpy::PhysicalCarPartContact lHinged = lContact;
        lHinged.mbIsHinged = true;
        const InputCollision lHingedHit(r.mMgr.mCameraInfo, r.mMgr, lHinged, r.mInput, 9.0f, 0.5f);
        Check(lHingedHit.mbCull && lHingedHit.maMaterial[0] == 0u, "CarPart: a hinged part's contact is culled at once");
        Check(guAsserts == 0, "CarPart: no assert when one side is a deformable part");
        BrnPhysics::ContactSpy::PhysicalCarPartContact lNeither = lContact;
        lNeither.mEntityIdA = Id(0, 0);
        (void)InputCollision(r.mMgr.mCameraInfo, r.mMgr, lNeither, r.mInput, 9.0f, 0.5f);
        Check(guAsserts == 1, "CarPart: neither side a part or detached wheel trips the cpp:1320 assert");
        guAsserts = 0;
    }

    Check(guAsserts == 0, "no stray assert");
    std::printf("FxCrashSnd2Deform: %d checks, %d failures\n", giChecks, giFailures);
    return giFailures ? 1 : 0;
}
