// FX-TAILS-B (crash parity 2026-09-24, item 1): the discarded-contact leg of the collision sound,
// against the ARTIST machine code.
//
//   InputCollision (discarded contact)                         sub_826BDAE8   DWARF h:424
//   CollisionStateManager::ImportContactSpies<EventQueue<DiscardedContact,20>>   0x826DD1C0
//   ContactSpyInterface::GetDiscardedContacts (inlined)        0x826F93C4..0x826F9404   DWARF h:104
//   ContactSpyData::GetDiscardedContacts (inlined)             mpData + 0x193C0          DWARF h:138
//
// run_fxtailsb_discarded.py extracts the PRODUCTION builders region of BrnCollisionStateManager.cpp (the
// material / orientation mappers and the Splat / Dot3 helpers the builder calls), the discarded
// builder itself and the ImportContactSpies member template, plus TrafficClassToSize, GetRaceCar and
// GetTrafficEntityIndex, and compiles them here against a fixture manager / sound input. The
// InputCollision, contact, event-queue and ContactSpyData / ContactSpyInterface types are the real
// headers (shadowed by the revision's own text on a --rev run).
#include "types.hpp"
#include "BrnCommonTypes.h"
#include "GameSource/BurnoutConstants.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"
#include "GameSource/Sound/Collision/BrnCollisionDataStructures.h"
#include "GameSource/Sound/Collision/BrnRaceCarCache.h"
#include "GameSource/Physics/ContactSpies/BrnContactSpyEvents.h"
#include "GameSource/Physics/ContactSpies/BrnContactSpyInterface.h"
#include "GameSource/World/EntityModules/TrafficEntityModule/SharedIO/BrnTrafficSoundInterfaces.h"
#include "GameSource/AttribSys/Enums/eMaterialType.h"
#include "SharedClasses/Traffic/BrnTrafficVehicleType.h"

#include <cmath>
#include <cstdio>
#include <vector>

static unsigned guAsserts = 0;
namespace CgsDev { namespace Assert {
int BeginAssert() { return 0; }
int FireAssert(const char*, const char*, int) { ++guAsserts; return 0; }
void* EndAssert() { return nullptr; }
} }

// ---- the fixture sound input (only what the builders read) -----------------------------------------
namespace BrnWorld { namespace RaceCarEntityModuleIO {
struct VehicleInterfaceFixture
{
    s32 miPlayer = 0;
    mutable int miReads = 0;
    s32 GetPlayerActiveRaceCarIndex() const { ++miReads; return miPlayer; }
};
} }
namespace BrnSound { namespace Module { namespace Io {
struct RootInputBuffer
{
    BrnTraffic::BrnTrafficIO::TrafficSoundOutputInterface mTraffic = {};
    BrnWorld::RaceCarEntityModuleIO::VehicleInterfaceFixture mVehicle;
    const BrnTraffic::BrnTrafficIO::TrafficSoundOutputInterface& GetTrafficOutputInterface() const { return mTraffic; }
    const BrnWorld::RaceCarEntityModuleIO::VehicleInterfaceFixture* GetVehicleInterface() const { return &mVehicle; }
};
} } }

namespace BrnTraffic { namespace BrnTrafficIO {
#include "fxtailsb_get_traffic_entity_index.inc"
} }

namespace BrnSound { namespace Logic { namespace Traffic {
enum ETrafficSize { E_SMALL = 0, E_MEDIUM = 1, E_LARGE = 2, E_MAX_SIZES = 3 };
struct TrafficStateManager
{
#include "fxtailsb_traffic_class_to_size.inc"
};
} } }

namespace BrnSound { namespace Logic { namespace Collision {

#include "fxtailsb_get_race_car.inc"

typedef AttribSys::Enums::eMaterialType::eMaterialType EeMaterialType;

struct CameraInfo
{
    Matrix44Affine mTransform;
    f32 mfFieldOfView, mfCosineHalfFov, mfAspectRatio, mfZoom;
};

class CollisionStateManager
{
public:
    CameraInfo mCameraInfo = {};
    RaceCarCache mRaceCarCache;
    FrameInformation mFrameInformation;
    ScrapeInfo mHistoryHit;                 // what FindInScrapeHistory returns when armed
    bool mbHistoryArmed = false;
    mutable int miHistoryLookups = 0;
    std::vector<InputCollision> maAdded;    // AddInputCollision's record

    const RaceCarCache& GetRaceCarCache() const { return mRaceCarCache; }
    const FrameInformation& GetFrameInformation() const { return mFrameInformation; }
    ScrapeInfo* FindInScrapeHistory(const ScrapeInfo&) { ++miHistoryLookups; return mbHistoryArmed ? &mHistoryHit : nullptr; }
    bool MapPropTypeToMaterial(u16, u64&) const { return false; }
    void AddInputCollision(const InputCollision& lrCollision) { maAdded.push_back(lrCollision); }

    template <typename SpyQueue>
    void ImportContactSpies(const SpyQueue& lSpyQueue, const LogicInputBuffer& lInputBuffer,
                            f32 lfTimeStamp, f32 lfTimeStep);
};

EeMaterialType MapEntityIdToMaterial(EntityId lEntityId, s32 liPlayerIndex, const LogicInputBuffer& lInput);
AttribSys::Enums::eOrientation::eOrientation MapPositionToOrientationUsingBox(
    Vector3 lPosition, Vector3 lNormal, Matrix44Affine lTransform, Vector3 lUnused,
    Vector3 lComOffset, Vector3 lMin, Vector3 lMax);
bool MapPositionToOrientation(Matrix44Affine lTransform, Vector3 lPosition, Vector3 lNormal,
                              EntityId lVehicleIdA, EntityId lVehicleIdB,
                              const CollisionStateManager& lMgr,
                              AttribSys::Enums::eOrientation::eOrientation& leOrientation);

#include "fxtailsb_builders_region.inc"

#include "fxtailsb_discarded_builder.inc"

#include "fxtailsb_import_contact_spies.inc"

} } }

// ---- checks ----------------------------------------------------------------------------------------
using namespace BrnSound::Logic;
using namespace BrnSound::Logic::Collision;
using BrnPhysics::ContactSpy::DiscardedContact;
namespace eO = AttribSys::Enums::eOrientation;

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
static bool Near(f32 a, f32 b, f32 tol = 1e-5f) { return std::fabs(a - b) <= tol; }

static EntityId Id(u32 luOwner, u32 luIndex) { EntityId l; l.muValue = (luOwner << 24) | (luIndex << 10); return l; }
static Vector3 V(f32 x, f32 y, f32 z) { Vector3 l = { x, y, z, 0.0f }; return l; }
static Matrix44Affine Frame(Vector3 lRight, Vector3 lUp, Vector3 lAt, Vector3 lPos)
{
    Matrix44Affine l; l.xAxis = lRight; l.yAxis = lUp; l.zAxis = lAt; l.wAxis = lPos; return l;
}
static Matrix44Affine Identity() { return Frame(V(1, 0, 0), V(0, 1, 0), V(0, 0, 1), V(0, 0, 0)); }

// A race-car cache node, previous frame = the one MapPositionToOrientation reads (+0x11 / +0x60).
static void ArmCar(CollisionStateManager& lrMgr, u32 luIndex)
{
    RaceCarCache::RaceCarCacheNode& lrNode = lrMgr.mRaceCarCache.maRaceCars[luIndex];
    lrNode.mbActive.mCurrentValue = true;
    lrNode.mbActive.mPreviousValue = true;
    lrNode.mTransform.mPreviousValue = Identity();
    lrNode.mTransform.mCurrentValue = Identity();
    lrNode.mComOffset = V(0, 0, 0);
    lrNode.mMin = V(-1.0f, -0.5f, -2.0f);
    lrNode.mMax = V(1.0f, 0.5f, 2.0f);
}

static DiscardedContact Discarded(EntityId a, EntityId b, f32 lfClosing, Vector3 onA, Vector3 onB, Vector3 normal)
{
    DiscardedContact l = {};
    l.mEntityIdA = a; l.mEntityIdB = b; l.mfClosingVelocity = lfClosing;
    l.mNormal = normal; l.mPointOnA = onA; l.mPointOnB = onB;
    return l;
}

// The console's ContactSpyData the leg reads from (static: 0x1DEB0 bytes).
static BrnPhysics::ContactSpy::ContactSpyData gData;

int main()
{
    BrnSound::Module::Io::RootInputBuffer lInput;
    lInput.mVehicle.miPlayer = 1;
    lInput.mTraffic.mu16EntityCount = 1;
    lInput.mTraffic.maActiveEntityList[0].mu16EntityIndex = 9;
    lInput.mTraffic.maActiveEntityList[0].muVehicleClass = BrnTraffic::E_VEHICLECLASS_BUS;

    CameraInfo lCamera = {};
    lCamera.mTransform = Frame(V(1, 0, 0), V(0, 1, 0), V(0, 0, 1), V(0, 0, -10));
    const f32 kfDt = 1.0f / 60.0f;

    // ---- the builder, sub_826BDAE8 --------------------------------------------------------------
    {
        // Car 3 against car 1: the regular builder would order them (1, 3); this one keeps (A, B).
        CollisionStateManager lMgr;
        ArmCar(lMgr, 3);
        const DiscardedContact lContact = Discarded(Id(1, 3), Id(1, 1), 12.5f, V(0, 0, 1.9f), V(7, 7, 7), V(0, 1, 0));
        InputCollision lIn(lCamera, lMgr, lContact, lInput, 7.0f, kfDt);
        Check(lIn.maEntityID[0].muValue == Id(1, 3).muValue && lIn.maEntityID[1].muValue == Id(1, 1).muValue,
              "discarded: maEntityID = (A, B) as recorded (+0x80 / +0x84) -- no race-car ordering");
        Check(Near(lIn.mPosition.x, 0.0f) && Near(lIn.mPosition.z, 1.9f),
              "discarded: mPosition = the point on A (spy+0x20 -> +0x60)");
        Check(lIn.maParameter[0].x == 12.5f && lIn.maParameter[0].y == 12.5f &&
                  lIn.maParameter[0].z == 12.5f && lIn.maParameter[0].w == 12.5f,
              "discarded: maParameter[0] = splat(closing velocity) exactly (lfs 8(r30) ; vspltw 0 -> +0x30)");
        Check(lIn.maMaterial[0] == 0x4 && lIn.maMaterial[1] == 0x2,
              "discarded: materials = MapEntityIdToMaterial(A), (B) -- AiCar / PlayerCar");
        Check(lIn.meOrientation == eO::Front,
              "discarded: orientation from A's side at A's point (Front face of car 3)");
        Check(lIn.mePipeline == InputCollision::E_REGULAR && lIn.meAction == AttribSys::Enums::eAction::Collision &&
                  !lIn.mbCull && lIn.mfPriorityAddition == 0.0f && !lIn.mScrapeInfo.mbValid,
              "discarded: pipeline REGULAR (+0x94), action Collision (+0x8C), not culled (+0x98), priority 0.0 "
              "(+0x88), scrape invalid (+0x29)");
        Check(lIn.mScrapeInfo.mEntityIdA.muValue == 0 && lIn.mScrapeInfo.mfTimeStamp == 0.0f &&
                  lIn.mScrapeInfo.mfIntensity == 0.0f,
              "discarded: no scrape entry is written (only mbValid = 0)");
        Check(lInput.mVehicle.miReads == 2,
              "discarded: the player index is read through the vehicle interface once per material (two I() calls)");
    }
    {
        // A negative closing velocity passes as recorded: no fabs, no clamp, no normalisation.
        CollisionStateManager lMgr;
        const DiscardedContact lContact = Discarded(Id(1, 1), Id(0, 0), -3.25f, V(0, 0, 0), V(0, 0, 0), V(0, 1, 0));
        InputCollision lIn(lCamera, lMgr, lContact, lInput, 0.0f, kfDt);
        Check(lIn.maParameter[0].x == -3.25f && lIn.maParameter[0].w == -3.25f,
              "discarded: a negative closing velocity is stored as is (no fabs / clamp)");
        InputCollision lIn0(lCamera, lMgr, lContact, lInput, 0.0f, 0.0f);
        Check(lIn0.maParameter[0].x == -3.25f,
              "discarded: dt is never read (dt = 0 leaves the impulse finite; f2 unused)");
        Check(Near(lIn.maParameter[1].x, 100.0f) && Near(lIn.maParameter[1].w, 100.0f),
              "discarded: maParameter[1] = |position - camera row 3|^2 splatted (vmsum3fp128 -> +0x40)");
        Check(Near(lIn.maParameter[2].x, 1.0f),
              "discarded: maParameter[2] = normalize(position - camera) . camera row 2 (At) (-> +0x50)");
        CameraInfo lSideways = lCamera;
        lSideways.mTransform.zAxis = V(1, 0, 0);
        InputCollision lIn2(lSideways, lMgr, lContact, lInput, 0.0f, kfDt);
        Check(Near(lIn2.maParameter[2].x, 0.0f),
              "discarded: facing follows the camera's At row, not the contact normal (perpendicular -> 0)");
        CameraInfo lOffset = lCamera;
        lOffset.mTransform.wAxis = V(3, 0, -4);
        InputCollision lIn3(lOffset, lMgr, lContact, lInput, 0.0f, kfDt);
        Check(Near(lIn3.maParameter[1].x, 25.0f) && Near(lIn3.maParameter[2].x, 0.8f),
              "discarded: an off-axis camera -> distance^2 25, facing 4/5");
    }
    {
        // A car's contact with a prop: B's material is Nothing (1) exactly -- no 0x2000000000 bit
        // (Hex-Rays prints one; 0x826BDC0C is `extsw r9,r3`, 0x826BDC28 `std r9,0x78(r31)`).
        CollisionStateManager lMgr;
        ArmCar(lMgr, 1);
        const DiscardedContact lContact = Discarded(Id(1, 1), Id(3, 20), 1.0f, V(0.95f, 0, 0), V(0, 0, 0), V(1, 0, 0));
        InputCollision lIn(lCamera, lMgr, lContact, lInput, 0.0f, kfDt);
        Check(lIn.maMaterial[0] == 0x2 && lIn.maMaterial[1] == 1ull,
              "discarded: car vs prop -> (PlayerCar, Nothing == 1), no 0x2000000000 bit");
        Check(lIn.meOrientation == eO::Side,
              "discarded: A's side face against a prop stays Side");
        const DiscardedContact lTraffic = Discarded(Id(1, 1), Id(2, 9), 1.0f, V(0.95f, 0, 0), V(0, 0, 0), V(1, 0, 0));
        InputCollision lInT(lCamera, lMgr, lTraffic, lInput, 0.0f, kfDt);
        Check(lInT.maMaterial[1] == 0x1000000,
              "discarded: traffic B by its size class (a bus -> TrafficCarLarge)");
        const DiscardedContact lCars = Discarded(Id(1, 1), Id(1, 4), 1.0f, V(0.95f, 0, 0), V(0, 0, 0), V(1, 0, 0));
        InputCollision lInC(lCamera, lMgr, lCars, lInput, 0.0f, kfDt);
        Check(lInC.meOrientation == eO::Front,
              "discarded: Side against another race car becomes Front (MapPositionToOrientation)");
    }
    {
        // A not an active race car: MapPositionToOrientation says Front and the pair is NOT reversed
        // (its result is never tested, 0x826BDB6C -> 0x826BDB70).
        CollisionStateManager lMgr;
        ArmCar(lMgr, 2);
        const DiscardedContact lContact = Discarded(Id(0, 0), Id(1, 2), 1.0f, V(5, 5, 5), V(0.95f, 0, 0), V(1, 0, 0));
        InputCollision lIn(lCamera, lMgr, lContact, lInput, 0.0f, kfDt);
        Check(lIn.maEntityID[0].muValue == 0 && lIn.maEntityID[1].muValue == Id(1, 2).muValue &&
                  lIn.meOrientation == eO::Front && lIn.maMaterial[0] == 0x10 && lIn.maMaterial[1] == 0x4,
              "discarded: world vs car stays (world, car), Front, (World, AiCar) -- the regular builder would reverse it");
        Check(Near(lIn.mPosition.x, 5.0f), "discarded: mPosition stays A's point when A is not a car");
    }
    {
        // No SloMoCrash culling: impact time on and a fresh history entry do not cull, and the
        // history is never consulted.
        CollisionStateManager lMgr;
        lMgr.mbHistoryArmed = true;
        lMgr.mHistoryHit.mfTimeStamp = 6.9f;
        lMgr.mFrameInformation.meImpactTime.mCurrentValue = AttribSys::Enums::eImpactTime::True;
        const DiscardedContact lContact = Discarded(Id(1, 1), Id(0, 0), 1.0f, V(0, 0, 0), V(0, 0, 0), V(0, 1, 0));
        InputCollision lIn(lCamera, lMgr, lContact, lInput, 7.0f, kfDt);
        Check(!lIn.mbCull && lMgr.miHistoryLookups == 0,
              "discarded: no SloMoCrash culling and no scrape-history lookup");
    }

    // ---- ImportContactSpies<EventQueue<DiscardedContact,20>> 0x826DD1C0, through the inlined
    //      ContactSpyInterface::GetDiscardedContacts (mpData + 0x193C0) ------------------------------
    {
        gData.mDiscardedContactQueue.Construct();
        BrnPhysics::ContactSpy::ContactSpyInterface lInterface;
        lInterface.SetData(&gData);
        const unsigned luAssertsBefore = guAsserts;
        Check(lInterface.GetDiscardedContacts() == &gData.mDiscardedContactQueue && guAsserts == luAssertsBefore,
              "interface: GetDiscardedContacts() is mpData's discarded queue (+0x193C0), no assert when bound");
        Check(gData.GetDiscardedContacts() == &gData.mDiscardedContactQueue,
              "data: ContactSpyData::GetDiscardedContacts() == &mDiscardedContactQueue");

        CollisionStateManager lMgr;
        lMgr.mCameraInfo = lCamera;
        lMgr.ImportContactSpies(*lInterface.GetDiscardedContacts(), lInput, 1.0f, kfDt);
        Check(lMgr.maAdded.empty(), "import: an empty discarded queue adds nothing (the console's every frame)");

        for (int i = 0; i < 3; ++i)
            gData.mDiscardedContactQueue.AddEventSafe(
                Discarded(Id(1, 5 + i), Id(0, 0), 1.0f + i, V(0, 0, static_cast<f32>(i)), V(0, 0, 0), V(0, 1, 0)));
        lMgr.ImportContactSpies(*lInterface.GetDiscardedContacts(), lInput, 1.0f, kfDt);
        bool lbOrdered = lMgr.maAdded.size() == 3u;
        for (u32 i = 0; lbOrdered && i < 3u; ++i)
        {
            const InputCollision& lr = lMgr.maAdded[i];
            lbOrdered = lr.maEntityID[0].muValue == Id(1, 5 + i).muValue && lr.maParameter[0].x == 1.0f + i &&
                        Near(lr.mPosition.z, static_cast<f32>(i)) && !lr.mScrapeInfo.mbValid;
        }
        Check(lbOrdered, "import: one discarded-builder input per record, in queue order (GetEvent(i) -> AddInputCollision)");
        Check(lMgr.maAdded.size() == 3u && Near(lMgr.maAdded[2].maParameter[1].x, 144.0f),
              "import: the builder reads the manager's own camera info (mgr + 0x8140)");
    }

    std::printf("FxTailsBDiscarded: %d checks, %d failures\n", giChecks, giFailures);
    return giFailures ? 1 : 0;
}
