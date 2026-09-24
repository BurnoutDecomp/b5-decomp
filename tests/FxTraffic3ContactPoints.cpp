// FX-TRAFFIC3 (crash parity wave 5, 2026-09-24, CC-2): the PRODUCTION TrafficEntityModule::
// HandleContactPoints @0x827340C0, ProcessContactPoint @0x82720C68 and DEBUG_RenderContactPoint
// @0x827082B8, their four constants and the entity-id helpers, extracted from
// src/GameSource/World/EntityModules/TrafficEntityModule/BrnTrafficEntityModule.cpp (and
// InputBuffer_PostPhysics::GetContactSpyInterface @0x827119A0 from the getters TU) by
// run_fxtraffic3_contact_points.py, hosted on a fixture with the real member types. The accessors
// they call (GetVehicle / GetVehicleTransform / GetVehicleTypeRuntime /
// GetTrafficPhysicsInfoForVehicl) are the production bodies too; the debug renderer is a recorder.
//
// Expectations, read off the ARTIST asm (never off the reconstruction):
//   HandleContactPoints
//     0x8273416C  every USED slot (maTrafficPhysicsInfoListBits) only
//     0x827342AC  debounce -= mfSimTimeStep, stored ; `fcmpu ; bgt skip` -> flags = 0 unless the
//                 stored timer is > 0 (so exactly 0 clears, and NaN clears)
//     0x82734308  mpData == 0 -> no contact is looked at
//     0x82734370/0x82734380  ProcessContactPoint for &idA then &idB of every contact
//     0x82734384  DEBUG_RenderContactPoint only while mbDEBUGRenderContacts
//   ProcessContactPoint
//     owner byte == 2 only ; point = mPointOnA if id == idA (value) else mPointOnB ;
//     physical AND alive ; half = mBBoxHalfSize + {0.2 (flt_82004744), 0.05 (flt_820047C8), 0}
//     (unk_8300CBD0, CRT 0x82C663A0) ; local = point - row3 ; right = local.row0, dir = local.row2 ;
//     |right| <= half.x * 0.75 (unk_8300CC10 <- flt_82004018) else discard ;
//     dir >= half.z * 0.75 (unk_8300CCF0) -> FRONT (|= 1) ; dir <= half.z * -0.75 -> BACK (|= 2) ;
//     either -> debounce = 1.0 (flt_82001C98)
//   DEBUG_RenderContactPoint
//     arrow mPointOnA -> mNormal * 5.0 (flt_8200426C) + mPointOnA ; colour 0xFF00FF00 when
//     idB's owner byte is 0 (world), else 0xFF00FFFF
#include "GameSource/World/EntityModules/TrafficEntityModule/BrnTrafficEntityModule.h"
#include "GameSource/World/EntityModules/TrafficEntityModule/BrnTrafficEntityModuleIO.h"
#include "GameSource/World/BrnEntityTypes.h"
#include "GameShared/GameClasses/Development/DebugSystem/Interface/CgsDebugInterface.h"
#include "GameShared/GameClasses/Development/DebugSystem/Render/CgsDebugRender.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"
#include "GameShared/GameClasses/Development/Log/CgsLog.h"
#include "rw/math/vpu/vector3_operation.h"   // operator+/-/*, Dot (the production TU includes it at :63)
#include <cmath>
#include <cstdio>
#include <cstring>
#include <limits>
#include <vector>

static unsigned gAsserts = 0, gChecks = 0, gFailures = 0;

namespace CgsDev
{
namespace Assert
{
    int   BeginAssert() { return 0; }
    int   FireAssert(const char* lpcMessage, const char*, int)
    {
        ++gAsserts;
        std::fprintf(stderr, "ASSERT: %s\n", lpcMessage);
        return 0;
    }
    void* EndAssert() { return nullptr; }
}
namespace Log { DebugPrint* gpDebugPrint = nullptr; }             // the witness stream stays off
namespace Message { unsigned long long gxMessageFilterFlags = 0; }

    // ---- the debug-render seam, as recorders (not under test) ----------------------------
    struct ArrowRecord { Vector3 mFrom; Vector3 mTo; RGBA mColour; };
    static std::vector<ArrowRecord> gArrows;
    static int giDebugInterfaces = 0;
    alignas(64) static unsigned char gaRenderStorage[sizeof(DebugRender)];

    DebugInterface::DebugInterface() : mpDebugManager(nullptr), mbIsAutomaticClass(true) { ++giDebugInterfaces; }
    DebugInterface::~DebugInterface() {}
    DebugRender& DebugInterface::GetRender() { return *reinterpret_cast<DebugRender*>(gaRenderStorage); }
    void DebugRender::DrawArrow(Vector3 lv3From, Vector3 lv3To, RGBA lColour)
    {
        gArrows.push_back(ArrowRecord{ lv3From, lv3To, lColour });
    }
}

namespace BrnTraffic
{
    // The TU's BRN_TRAFFIC_DIAG stream, off.
    CgsDev::Log::DebugPrint* TrafficDiagStream() { return nullptr; }

    struct ContactFixture
    {
        typedef TrafficEntityModule M;

        decltype(M::maVehicles)                   maVehicles;
        decltype(M::maVehicleTransforms)          maVehicleTransforms;
        decltype(M::maVehicleTypeRuntime)         maVehicleTypeRuntime;
        decltype(M::maTrafficPhysicsInfoList)     maTrafficPhysicsInfoList;
        decltype(M::maTrafficPhysicsInfoListBits) maTrafficPhysicsInfoListBits;
        decltype(M::mfSimTimeStep)                mfSimTimeStep;
        decltype(M::mbDEBUGRenderContacts)        mbDEBUGRenderContacts;

        Vehicle*                  GetVehicle(u32 luIndex);
        Matrix44Affine            GetVehicleTransform(u32 luIndex) const;
        const VehicleTypeRuntime* GetVehicleTypeRuntime(u32 luVehicleType) const;
        TrafficPhysicsInfo*       GetTrafficPhysicsInfoForVehicl(u32 luVehicle);

        void HandleContactPoints(const BrnTrafficIO::InputBuffer_PostPhysics* lpInput);
        void ProcessContactPoint(const BrnPhysics::ContactSpy::TrafficContact* lpContact,
                                 const EntityId& lEntityId);
        void DEBUG_RenderContactPoint(const BrnPhysics::ContactSpy::TrafficContact* lpContact);
    };
}

// The production bodies under test.
#include "contact_points.inc"

using namespace BrnTraffic;
typedef ContactFixture Fixture;
typedef BrnPhysics::ContactSpy::TrafficContact Contact;

static void Check(bool lbPass, const char* lpcName)
{
    ++gChecks;
    if (!lbPass)
    {
        ++gFailures;
        std::fprintf(stderr, "FAIL: %s\n", lpcName);
    }
}

// The ARTIST values the expectations are built from (NOT the production constants).
static const f32 KF_ADD_X     = 0.2f;    // flt_82004744
static const f32 KF_ADD_Z     = 0.0f;    // flt_82001CC0
static const f32 KF_FACTOR    = 0.75f;   // flt_82004018
static const f32 KF_DEBOUNCE  = 1.0f;    // flt_82001C98
static const f32 KF_ARROW     = 5.0f;    // flt_8200426C

static const u32 KU_CAR       = 10;      // vehicle index
static const u32 KU_SLOT      = 3;       // its TrafficPhysicsInfo slot
static const u32 KU_OTHER_CAR = 11;      // a second physical car
static const u32 KU_OTHER_SLOT = 5;
static const u32 KU_UNUSED_SLOT = 7;
static const f32 KF_HALF_X    = 1.0f;
static const f32 KF_HALF_Z    = 4.0f;    // widened z-half 4.0 -> limits exactly +-3.0

alignas(64) static unsigned char gaStorage[sizeof(Fixture)];
alignas(64) static unsigned char gaInputStorage[sizeof(BrnTrafficIO::InputBuffer_PostPhysics)];
alignas(64) static unsigned char gaSpyStorage[sizeof(BrnPhysics::ContactSpy::ContactSpyData)];

static Fixture& F() { return *reinterpret_cast<Fixture*>(gaStorage); }
static BrnTrafficIO::InputBuffer_PostPhysics& Input() { return *reinterpret_cast<BrnTrafficIO::InputBuffer_PostPhysics*>(gaInputStorage); }
static BrnPhysics::ContactSpy::ContactSpyData& Spy() { return *reinterpret_cast<BrnPhysics::ContactSpy::ContactSpyData*>(gaSpyStorage); }

static EntityId Id(u32 luOwner, u32 luIndex)
{
    EntityId lId;
    lId.muValue = (luOwner << 24) | (luIndex << 10);
    return lId;
}

static void MakeCar(Fixture& lr, u32 luVehicle, u32 luSlot, u8 luFlags)
{
    Vehicle& lrVehicle = lr.maVehicles[luVehicle];
    lrVehicle.mxFlags = luFlags;
    lrVehicle.muVehicleType = 0;
    lrVehicle.miPhysicalPartsIndex = static_cast<s8>(luSlot);
    lr.maTrafficPhysicsInfoListBits.SetBit(luSlot);
    lr.maTrafficPhysicsInfoList[luSlot].muOwningVehicleIndex = static_cast<u16>(luVehicle);
    lr.maTrafficPhysicsInfoList[luSlot].mfStuckTimerDebounce = 0.0f;
    lr.maTrafficPhysicsInfoList[luSlot].muContactSideFlags = 0;
    lr.maVehicleTransforms[luVehicle].SetIdentity();
    lr.maVehicleTransforms[luVehicle].wAxis = { 0.0f, 0.0f, 0.0f, 1.0f };   // at the origin
}

static Fixture& Fresh()
{
    std::memset(gaStorage, 0, sizeof(gaStorage));
    Fixture& lr = F();
    for (u32 luVehicle = 0; luVehicle < KU_MAX_TOTAL_TRAFFIC; ++luVehicle)
    {
        lr.maVehicles[luVehicle].miPhysicalPartsIndex = -1;
        lr.maVehicleTransforms[luVehicle].SetIdentity();
    }
    lr.maVehicleTypeRuntime[0].mBBoxHalfSize = { KF_HALF_X, 0.5f, KF_HALF_Z, 0.0f };
    MakeCar(lr, KU_CAR, KU_SLOT, Vehicle::E_FLAG_ALIVE | Vehicle::E_FLAG_PHYSICAL);
    lr.mfSimTimeStep = 0.25f;
    lr.mbDEBUGRenderContacts = false;
    CgsDev::gArrows.clear();
    return lr;
}

static void ResetInput(bool lbBound)
{
    std::memset(gaInputStorage, 0, sizeof(gaInputStorage));
    std::memset(gaSpyStorage, 0, sizeof(gaSpyStorage));
    Spy().mTrafficContactQueue.Construct(BrnWorld::E_ENTITYTYPE_TRAFFIC_VEHICLE);
    Input().mxStatusFlags.SetBit(CgsModule::IOBuffer::eStatusLockedForRead);
    Input().mContactSpyInterface.mpData = lbBound ? &Spy() : nullptr;
}

static Contact MakeContact(EntityId lIdA, EntityId lIdB, Vector3 lPointA, Vector3 lPointB)
{
    Contact lContact;
    std::memset(&lContact, 0, sizeof(lContact));
    lContact.mEntityIdA = lIdA;
    lContact.mEntityIdB = lIdB;
    lContact.mPointOnA  = lPointA;
    lContact.mPointOnB  = lPointB;
    lContact.mNormal    = { 0.0f, 1.0f, 0.0f, 0.0f };
    return lContact;
}

static void Push(const Contact& lrContact) { Spy().mTrafficContactQueue.AddEvent(lrContact); }

static TrafficPhysicsInfo& Info(u32 luSlot) { return F().maTrafficPhysicsInfoList[luSlot]; }

// One ProcessContactPoint on idB (traffic) of a world contact, with the point on B.
static void TouchB(Vector3 lPoint)
{
    const Contact lContact = MakeContact(Id(BrnWorld::E_ENTITYTYPE_WORLD, 0), Id(2, KU_CAR),
                                         Vector3{ 9999.0f, 0.0f, 9999.0f, 0.0f }, lPoint);
    F().ProcessContactPoint(&lContact, lContact.mEntityIdB);
}

int main()
{
    const f32 lfSideLimit  = (KF_HALF_X + KF_ADD_X) * KF_FACTOR;   // 0.9 (float)
    const f32 lfFrontLimit = (KF_HALF_Z + KF_ADD_Z) * KF_FACTOR;   // 3.0 exactly
    const Vector3 KV_FRONT = { 0.5f, 0.3f, 3.5f, 0.0f };
    const Vector3 KV_BACK  = { -0.5f, 0.0f, -3.5f, 0.0f };

    // ---- ProcessContactPoint -----------------------------------------------------------------
    Fresh();
    TouchB(KV_FRONT);
    Check(Info(KU_SLOT).muContactSideFlags == 1u, "P1 a point ahead of 0.75 * half.z sets E_CONTACT_SIDE_FRONT (|= 1)");
    Check(Info(KU_SLOT).mfStuckTimerDebounce == KF_DEBOUNCE, "P2 a flagged contact re-arms the debounce to 1.0");

    Fresh();
    TouchB(KV_BACK);
    Check(Info(KU_SLOT).muContactSideFlags == 2u, "P3 a point behind -0.75 * half.z sets E_CONTACT_SIDE_BACK (|= 2)");

    Fresh();
    TouchB(KV_FRONT);
    TouchB(KV_BACK);
    Check(Info(KU_SLOT).muContactSideFlags == 3u, "P4 front then back in one frame ORs both bits");

    Fresh();
    TouchB(Vector3{ lfFrontLimit * 0.0f, 0.0f, lfFrontLimit, 0.0f });
    Check(Info(KU_SLOT).muContactSideFlags == 1u, "P5 dir == 0.75 * half.z exactly is FRONT (vcmpgefp: >=)");

    Fresh();
    TouchB(Vector3{ 0.0f, 0.0f, -lfFrontLimit, 0.0f });
    Check(Info(KU_SLOT).muContactSideFlags == 2u, "P6 dir == -0.75 * half.z exactly is BACK (<=)");

    Fresh();
    TouchB(Vector3{ 0.2f, 0.0f, 2.9f, 0.0f });
    Check(Info(KU_SLOT).muContactSideFlags == 0u && Info(KU_SLOT).mfStuckTimerDebounce == 0.0f,
          "P7 a point between the two bands sets nothing and leaves the debounce alone");

    Fresh();
    TouchB(Vector3{ lfSideLimit, 0.0f, 3.5f, 0.0f });
    Check(Info(KU_SLOT).muContactSideFlags == 1u, "P8 |right| == 0.75 * (half.x + 0.2) exactly is still inside (<=)");

    Fresh();
    TouchB(Vector3{ -(lfSideLimit + 0.01f), 0.0f, 3.5f, 0.0f });
    Check(Info(KU_SLOT).muContactSideFlags == 0u, "P9 a point outside the widened side (|right| > 0.9) is discarded");

    Fresh();
    TouchB(Vector3{ 0.95f, 0.0f, 3.5f, 0.0f });
    Check(Info(KU_SLOT).muContactSideFlags == 0u,
          "P10 the side is widened by 0.2 BEFORE the 0.75 factor (0.95 > (1.0+0.2)*0.75)");

    Fresh();
    TouchB(Vector3{ 0.85f, 0.0f, 3.5f, 0.0f });
    Check(Info(KU_SLOT).muContactSideFlags == 1u,
          "P11 0.85 < (1.0+0.2)*0.75 -- the widening is applied (0.85 > 1.0*0.75 would discard)");

    Fresh();
    TouchB(Vector3{ std::numeric_limits<f32>::quiet_NaN(), 0.0f, 3.5f, 0.0f });
    Check(Info(KU_SLOT).muContactSideFlags == 0u, "P12 a NaN point fails the side test (all-lanes vcmpgefp) and is discarded");

    // A car turned 90 degrees: right = row 0 = (0,0,-1), dir = row 2 = (1,0,0), at (10,0,20).
    Fresh();
    F().maVehicleTransforms[KU_CAR].xAxis = { 0.0f, 0.0f, -1.0f, 0.0f };
    F().maVehicleTransforms[KU_CAR].yAxis = { 0.0f, 1.0f, 0.0f, 0.0f };
    F().maVehicleTransforms[KU_CAR].zAxis = { 1.0f, 0.0f, 0.0f, 0.0f };
    F().maVehicleTransforms[KU_CAR].wAxis = { 10.0f, 0.0f, 20.0f, 1.0f };
    TouchB(Vector3{ 13.5f, 0.0f, 20.2f, 0.0f });   // local: dir 3.5, right -0.2
    Check(Info(KU_SLOT).muContactSideFlags == 1u,
          "P13 the local frame is point - row3, dir = . row2, right = . row0 (a turned car's nose)");

    // The id picks the point: idA -> mPointOnA, otherwise mPointOnB.
    Fresh();
    {
        const Contact lContact = MakeContact(Id(2, KU_CAR), Id(BrnWorld::E_ENTITYTYPE_WORLD, 0), KV_BACK, KV_FRONT);
        F().ProcessContactPoint(&lContact, lContact.mEntityIdA);
        Check(Info(KU_SLOT).muContactSideFlags == 2u, "P14 the idA end reads mPointOnA");
    }
    Fresh();
    {
        const Contact lContact = MakeContact(Id(1, 0), Id(2, KU_CAR), KV_BACK, KV_FRONT);
        F().ProcessContactPoint(&lContact, lContact.mEntityIdA);
        Check(Info(KU_SLOT).muContactSideFlags == 0u, "P15 a race-car end (owner 1) is ignored");
        F().ProcessContactPoint(&lContact, lContact.mEntityIdB);
        Check(Info(KU_SLOT).muContactSideFlags == 1u, "P16 the idB end reads mPointOnB");
    }

    // Physical AND alive.
    Fresh();
    F().maVehicles[KU_CAR].mxFlags = Vehicle::E_FLAG_PHYSICAL;          // dead
    TouchB(KV_FRONT);
    Check(Info(KU_SLOT).muContactSideFlags == 0u, "P17 a dead (physical-flagged) car is not looked at");
    Fresh();
    F().maVehicles[KU_CAR].mxFlags = Vehicle::E_FLAG_ALIVE;             // alive, not physical
    TouchB(KV_FRONT);
    Check(Info(KU_SLOT).muContactSideFlags == 0u, "P18 an alive car that is not physical is not looked at");

    // ---- HandleContactPoints -----------------------------------------------------------------
    Fresh();
    ResetInput(true);
    Info(KU_SLOT).mfStuckTimerDebounce = 1.0f;
    Info(KU_SLOT).muContactSideFlags   = 1;
    MakeCar(F(), KU_OTHER_CAR, KU_OTHER_SLOT, Vehicle::E_FLAG_ALIVE | Vehicle::E_FLAG_PHYSICAL);
    Info(KU_OTHER_SLOT).mfStuckTimerDebounce = std::numeric_limits<f32>::quiet_NaN();
    Info(KU_OTHER_SLOT).muContactSideFlags   = 2;
    Info(KU_UNUSED_SLOT).mfStuckTimerDebounce = 0.5f;            // NOT in the used-slot bitset
    Info(KU_UNUSED_SLOT).muContactSideFlags   = 2;
    for (int i = 0; i < 3; ++i) { F().HandleContactPoints(&Input()); }
    Check(Info(KU_SLOT).mfStuckTimerDebounce == 0.25f && Info(KU_SLOT).muContactSideFlags == 1u,
          "H1 the debounce runs down by mfSimTimeStep and the flags hold while it is > 0");
    Check(Info(KU_OTHER_SLOT).muContactSideFlags == 0u, "H2 a NaN debounce clears the flags (`bgt` skip only)");
    Check(Info(KU_UNUSED_SLOT).mfStuckTimerDebounce == 0.5f && Info(KU_UNUSED_SLOT).muContactSideFlags == 2u,
          "H3 a slot outside maTrafficPhysicsInfoListBits is untouched");
    F().HandleContactPoints(&Input());
    Check(Info(KU_SLOT).mfStuckTimerDebounce == 0.0f && Info(KU_SLOT).muContactSideFlags == 0u,
          "H4 at exactly 0 the flags clear (!(timer > 0))");

    // The contact walk: both ends of every contact, then the debug arrow only when flagged on.
    Fresh();
    ResetInput(true);
    MakeCar(F(), KU_OTHER_CAR, KU_OTHER_SLOT, Vehicle::E_FLAG_ALIVE | Vehicle::E_FLAG_PHYSICAL);
    Push(MakeContact(Id(2, KU_CAR), Id(2, KU_OTHER_CAR), KV_FRONT, KV_BACK));   // car 10 nose into car 11 tail
    Push(MakeContact(Id(BrnWorld::E_ENTITYTYPE_WORLD, 0), Id(2, KU_CAR), KV_FRONT, KV_BACK));
    F().HandleContactPoints(&Input());
    Check(Info(KU_SLOT).muContactSideFlags == 3u && Info(KU_OTHER_SLOT).muContactSideFlags == 2u,
          "H5 every contact is processed on BOTH ends (A: nose of 10 ; B: tail of 11 ; 2nd contact B: tail of 10)");
    Check(Info(KU_SLOT).mfStuckTimerDebounce == KF_DEBOUNCE && Info(KU_OTHER_SLOT).mfStuckTimerDebounce == KF_DEBOUNCE,
          "H6 the timers are re-armed after the decay pass of the same call");
    Check(CgsDev::gArrows.empty(), "H7 no debug arrow while mbDEBUGRenderContacts is false");

    F().mbDEBUGRenderContacts = true;
    CgsDev::gArrows.clear();
    F().HandleContactPoints(&Input());
    Check(CgsDev::gArrows.size() == 2u, "H8 one debug arrow per contact while mbDEBUGRenderContacts is set");
    if (CgsDev::gArrows.size() == 2u)
    {
        const CgsDev::ArrowRecord& lrFirst  = CgsDev::gArrows[0];
        const CgsDev::ArrowRecord& lrSecond = CgsDev::gArrows[1];
        Check(lrFirst.mFrom.x == KV_FRONT.x && lrFirst.mFrom.z == KV_FRONT.z
              && lrFirst.mTo.y == KV_FRONT.y + KF_ARROW * 1.0f && lrFirst.mTo.z == KV_FRONT.z,
              "D1 the arrow runs from mPointOnA to mPointOnA + mNormal * 5.0");
        Check(lrFirst.mColour == 0xFF00FFFFu, "D2 a traffic-vs-traffic contact draws 0xFF00FFFF (idB owner != 0)");
        Check(lrSecond.mColour == 0xFF00FFFFu,
              "D3 the colour tests idB's owner: world on A, traffic on B -> 0xFF00FFFF");
    }
    else
    {
        Check(false, "D1 (no arrows)"); Check(false, "D2 (no arrows)"); Check(false, "D3 (no arrows)");
    }
    CgsDev::gArrows.clear();
    {
        const Contact lWorldB = MakeContact(Id(2, KU_CAR), Id(BrnWorld::E_ENTITYTYPE_WORLD, 0), KV_FRONT, KV_BACK);
        F().DEBUG_RenderContactPoint(&lWorldB);
        Check(CgsDev::gArrows.size() == 1u && CgsDev::gArrows[0].mColour == 0xFF00FF00u,
              "D4 a contact whose idB is the world (owner 0) draws 0xFF00FF00");
    }

    // An unbound spy (mpData == 0): the decay still runs, no contact is looked at.
    Fresh();
    ResetInput(false);
    Info(KU_SLOT).mfStuckTimerDebounce = 0.5f;
    Info(KU_SLOT).muContactSideFlags   = 1;
    Push(MakeContact(Id(BrnWorld::E_ENTITYTYPE_WORLD, 0), Id(2, KU_CAR), KV_BACK, KV_BACK));
    F().HandleContactPoints(&Input());
    Check(Info(KU_SLOT).mfStuckTimerDebounce == 0.25f && Info(KU_SLOT).muContactSideFlags == 1u,
          "H9 an unbound contact spy (IsValid false) skips the walk but not the decay");

    Check(gAsserts == 0, "A1 no assert fired on any path");

    std::printf("FxTraffic3ContactPoints: %u checks, %u failures\n", gChecks, gFailures);
    return gFailures == 0 ? 0 : 1;
}
