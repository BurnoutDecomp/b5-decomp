// ============================================================================
// BrnWorld::TriggerEntityModule -- WORLD/physics-side trigger detector.
// Reconstructed from BURNOUT_X360_ARTIST.XEX:
//   Construct                 0x822D8ED0   Destruct                  0x822C42F8
//   Prepare                   0x822A8E10   Release                   0x822A8EC0
//   ProcessAddTriggerEvents   0x822D8F48   ProcessTriggerQueryEvents 0x82306BB0
//   ProcessSceneQueryResults  0x822EEC78   ProcessLineTestFineResult 0x822D9FF8
//
// Behaviour is from the X360 pseudocode (de-inlined into logical calls); shape is from
// the DecFIGS DWARF. Asserts reproduce the X360 file/line VERBATIM (explicit
// BeginAssert/FireAssert/EndAssert, no baked __FILE__/__LINE__).
// ============================================================================
#include "BrnTriggerEntityModule.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"                              // CgsDev::Assert::Begin/Fire/EndAssert
#include "GameShared/GameClasses/Development/CgsStrStream.h"                     // CgsDev::StrStream
#include "GameShared/GameClasses/Module/CgsVariableEventQueue.h"                // CgsModule::VariableEventQueue
#include "GameShared/GameClasses/SceneManager/CgsEntityId.h"                    // CgsSceneManager::EntityId
#include "GameShared/GameClasses/SceneManager/CgsSceneManagerIO_SceneUpdate.h"  // InSceneUpdateInterface (Add* producers)
#include "GameShared/GameClasses/SceneManager/CgsSceneManagerIO_EventLineTest.h"// InEventLineTestFine
#include "GameShared/GameClasses/SceneManager/CgsSceneManagerIO_LineTestFineResult.hpp" // OutEventLineTestFineResult
#include "GameSource/World/EntityModules/TriggerEntityModule/SharedIO/BrnTriggerEntityModuleInputInterface.h" // InAddTriggerEvent / InTriggerQueryEvent / KU_TRIGGER_QUERY_EXCLUDE_ENTITY_ID
#include "vendor/renderware/collision/CollisionVolume.hpp"                      // rw::collision::SphereVolume / BoxVolume
#include "rw/rwcore_structs.h"                                                  // rw::Resource (the Initialize block descriptor)

#include <cstddef>   // offsetof
#include <cmath>     // sqrtf

namespace BrnWorld
{
namespace
{
    // The X360-baked source path stamped into every assert in this TU (preserved verbatim).
    const char* const KAC_FILE =
        "d:\\p4\\b5_main\\burnout\\main\\code\\gamesource\\unity\\"
        "../World/EntityModules/TriggerEntityModule/BrnTriggerEntityModule.cpp";

    // E_ENTITYTYPE_TRIGGER -- the EntityId OWNER stamped on every trigger's scene entity.
    // X360 ProcessLineTestFineResult asserts each hit's owner == 4. FLAG: value pinned == 4 from
    // the asm constant; the named enum (BrnEntityTypes.h E_ENTITYTYPE_TRIGGER) is not homed here.
    const u32 KU_ENTITYTYPE_TRIGGER = 4;

    // The entity-type-FLAG (a 1<<type bitmask lane, distinct from the EntityId owner) the X360
    // stamps for trigger entities in both ProcessAddTriggerEvents (AddEntity 3rd arg) and
    // ProcessTriggerQueryEvents (InEventLineTestFine.mx32EntityTypeFlags). Both are the literal
    // 32 == 1<<5. FLAG: value pinned == 32 from the asm (li r25,0x20); named home unknown.
    const u32 KU_ENTITYTYPEFLAG_TRIGGER = 32;

    inline f32 VectorMagnitude(f32 lfX, f32 lfY, f32 lfZ)
    {
        return sqrtf(lfX * lfX + lfY * lfY + lfZ * lfZ);
    }
}

// offsetof tripwire: pins the layout facts this TU controls (private members, complete class).
// NOTE: the absolute X360 GUEST offsets (mePrepareStage @552, mTriggers @560, mUsedTriggerList
// @66096, mTriggerEntityModuleDebugComponent @66160) are documented in the class header; they are
// NOT static_asserted here because they depend on the size of the CgsModule::ModuleSingleBuffered
// base (552 guest bytes), which is pointer-width-dependent on the PC build. What IS asserted is
// the layout this record owns and that the X360 stride/relative math depends on:
//   * the Trigger record stride is exactly 128 bytes (the array index << 7),
//   * mTriggers precedes mUsedTriggerList by exactly 512 * 128 == 65536 bytes,
//   * the gameplay handle sits at Trigger +8 (the (idx<<7)+568 read-back).
void TriggerEntityModule::_AssertLayout()
{
    static_assert(sizeof(Trigger) == 128, "Trigger record stride must be 128 (array index << 7)");
    static_assert(offsetof(TriggerEntityModule, mUsedTriggerList)
                  - offsetof(TriggerEntityModule, mTriggers) == 512 * 128,
                  "mUsedTriggerList must follow mTriggers[512] (stride 128)");
    static_assert(offsetof(Trigger, mHandle) == 8, "Trigger gameplay handle must be at +8");
    static_assert(offsetof(Trigger, meType) == 12, "Trigger meType must be at +12");
}

// ---------------------------------------------------------------------------
// Construct -- X360 0x822D8ED0.
// ---------------------------------------------------------------------------
void TriggerEntityModule::Construct()
{
    ModuleSingleBuffered::Construct();

    meReleaseStage = E_RELEASESTAGE_DONE;   // *(this+556) = 2
    mePrepareStage = E_PREPARESTAGE_START;  // *(this+552) = 0

    mTriggerEntityModuleDebugComponent.Construct(this);

    // X360 zeroes the 512-bit used-trigger set with an unrolled 8 * std-qword store loop.
    mUsedTriggerList.UnSetAll();

    // X360: *(this+4) = 1 -- the base ModuleSingleBuffered "new module" flag (mbIsNewModule),
    // set true so Prepare()/Release() skip the (un)allocation of input/output data structures
    // (this module owns no DataStructure-backed buffers).
    mbIsNewModule = true;
}

// ---------------------------------------------------------------------------
// Destruct -- X360 0x822C42F8.
//   base Destruct; debug component Destruct (the X360 asserts the debug component's
//   back-pointer != NULL, clears it, then tail-calls its BaseCollisionGenerator::Destruct).
// ---------------------------------------------------------------------------
void TriggerEntityModule::Destruct()
{
    ModuleSingleBuffered::Destruct();
    mTriggerEntityModuleDebugComponent.Destruct();
}

// ---------------------------------------------------------------------------
// Prepare -- X360 0x822A8E10. Resumable two-stage machine.
//   mePrepareStage < DONE(2)  -> run the stage (register debug component, base Prepare).
//   mePrepareStage == DONE(2) -> success no-op (the asm gotos the success tail without
//                                re-running base::Prepare).
//   mePrepareStage  > DONE(2) -> corrupt resume point -> assert "Invalid Stage"@110, return false.
// ---------------------------------------------------------------------------
bool TriggerEntityModule::Prepare()
{
    switch (mePrepareStage)
    {
        case E_PREPARESTAGE_START:
            mTriggerEntityModuleDebugComponent.Register();
            // fall through
        case E_PREPARESTAGE_MANAGER:
            // X360 sets the stage to MANAGER(1) before the base Prepare so a partial Prepare
            // resumes here without re-registering the debug component.
            mePrepareStage = E_PREPARESTAGE_MANAGER;
            if (!ModuleSingleBuffered::Prepare())
                return false;
            break;

        case E_PREPARESTAGE_DONE:
            // Already prepared: success no-op (skip base::Prepare, fall through to the success
            // tail). The X360 gotos the tail for stage == DONE.
            break;

        default:
            // Only stage values > DONE(2) are a corrupt resume point.
            CgsDev::Assert::BeginAssert();
            CgsDev::Assert::FireAssert("Invalid Stage\n", KAC_FILE, 110);
            CgsDev::Assert::EndAssert();
            return false;
    }

    mePrepareStage = E_PREPARESTAGE_MANAGER;
    meReleaseStage = E_RELEASESTAGE_START;
    return true;
}

// ---------------------------------------------------------------------------
// Release -- X360 0x822A8EC0. Resumable machine (mirror of Prepare).
//   meReleaseStage < DONE(2)  -> run the stage (base Release).
//   meReleaseStage == DONE(2) -> success no-op (the asm falls through to the success tail
//                                WITHOUT running base::Release). This IS reachable: Construct
//                                sets meReleaseStage = DONE, so a Release() before any Prepare()
//                                (or after a full cycle) must succeed as a no-op, not assert.
//   meReleaseStage  > DONE(2) -> corrupt resume point -> assert "Invalid Stage"@161, return false.
// ---------------------------------------------------------------------------
bool TriggerEntityModule::Release()
{
    if (meReleaseStage < E_RELEASESTAGE_DONE)
    {
        meReleaseStage = E_RELEASESTAGE_MANAGER;
        if (!ModuleSingleBuffered::Release())
            return false;
    }
    else if (meReleaseStage > E_RELEASESTAGE_DONE)
    {
        // meReleaseStage > DONE(2) -> invalid resume point.
        CgsDev::Assert::BeginAssert();
        CgsDev::Assert::FireAssert("Invalid Stage\n", KAC_FILE, 161);
        CgsDev::Assert::EndAssert();
        return false;
    }
    // meReleaseStage == DONE falls through here as a success no-op.

    meReleaseStage = E_RELEASESTAGE_MANAGER;
    mePrepareStage = E_PREPARESTAGE_START;
    return true;
}

// ---------------------------------------------------------------------------
// ProcessAddTriggerEvents -- X360 0x822D8F48.
// (See the header + the findings doc for the full per-step narrative.)
//
// For each add-trigger event: allocate the first free slot (first CLEAR bit in
// mUsedTriggerList), fill the Trigger record from the event, build the matching rw::collision
// volume into a 128-byte stage image, and register it with the scene manager.
// Type discriminator (the queue event type id): 0 == plane segment, 1 == sphere, 2 == box.
// The entity-type FLAG passed to AddDynamicVolume is a (1<<type) bitmask: plane 1, sphere 2,
// box 4 (NOT type+1 -- box must be 4).
// ---------------------------------------------------------------------------
void TriggerEntityModule::ProcessAddTriggerEvents(
    const TriggerEntityModuleIO::TriggerManagementInputInterface* lpInputInterface,
    OutputBuffer_PreScene::SceneInputInterface* lpOutSceneInputInterface)
{
    if (!lpInputInterface)
    {
        CgsDev::Assert::BeginAssert();
        CgsDev::Assert::FireAssert("lpInputInterface", KAC_FILE, 221);
        CgsDev::Assert::EndAssert();
    }
    if (!lpOutSceneInputInterface)
    {
        CgsDev::Assert::BeginAssert();
        CgsDev::Assert::FireAssert("lpOutSceneInputInterface", KAC_FILE, 222);
        CgsDev::Assert::EndAssert();
    }

    typedef CgsModule::VariableEventQueue<131072, 16> AddTriggerQueue;
    const AddTriggerQueue& lrQueue = lpInputInterface->GetAddTriggerEventQueue();

    const TriggerEntityModuleIO::InAddTriggerEvent* lpEvent = 0;
    s32 liSize = 0;
    s32 liType = lrQueue.GetFirstEvent(
        reinterpret_cast<const CgsModule::Event**>(&lpEvent), &liSize);

    while (lpEvent)
    {
        // ---- 1/2. allocate the first free trigger slot (first CLEAR bit) ------------------
        s32 liTriggerIndex = mUsedTriggerList.GetFirstClearBit();
        if (liTriggerIndex < 0 || static_cast<u16>(liTriggerIndex) >= KU_MAX_TRIGGERS)
        {
            CgsDev::Assert::BeginAssert();
            CgsDev::Assert::FireAssert("All available triggers used.", KAC_FILE, 260);
            CgsDev::Assert::EndAssert();
            break;
        }
        mUsedTriggerList.SetBit(static_cast<u32>(liTriggerIndex));

        Trigger& lrTrigger = mTriggers[liTriggerIndex];

        // ---- 3. fill the Trigger record from the event ------------------------------------
        lrTrigger.meType = static_cast<ETriggerTypeID>(liType);
        // The event's gameplay handle (event @+64/+68) is copied into the Trigger record (@+8).
        lrTrigger.SetHandle(lpEvent->GetTriggerHandle());
        // The event's world transform becomes the trigger's transform (position == W column).
        lrTrigger.mTransform = lpEvent->GetTransform();

        // The scene EntityId stamped for this trigger: owner = E_ENTITYTYPE_TRIGGER, index = slot.
        CgsSceneManager::EntityId lEntityId;
        lEntityId.Set(KU_ENTITYTYPE_TRIGGER, static_cast<u32>(liTriggerIndex), 0);

        f32  lfEntityBoundingSphereRadius = 0.0f;
        u8   lu8TriggerTypeFlag           = 0;
        u8   laVolumeStorage[128];   // staged rw::collision volume image (block-copied by AddDynamicVolume)
        bool lbVolumeOk                   = false;

        switch (liType)
        {
            case E_TRIGGERTYPE_PLANE_SEGMENT:   // 0
            {
                const f32 lfDimX = lpEvent->GetDimensionX();
                const f32 lfDimY = lpEvent->GetDimensionY();
                if (!(lfDimX > 0.0f) || !(lfDimY > 0.0f))
                {
                    CgsDev::Assert::BeginAssert();
                    CgsDev::Assert::FireAssert("Invalid plane segment trigger dimensions", KAC_FILE, 282);
                    CgsDev::Assert::EndAssert();
                }
                // A plane segment is a very thin box. The X360 feeds BoxVolume::Initialize the
                // FULL X/Y dims and a literal Z half-extent of 0.0099999998 (== KF_PLANE_SEGMENT_
                // TRIGGER_DEPTH, used directly -- NO x0.5 anywhere in the plane branch).
                //
                // The console stages a zeroed rw::Resource whose first word is the volume block
                // and hands THAT to the (static) Initialize -- 0x822D98DC..0x822D9900:
                //   stw r30(=0), 0..0x10(var_2A0) ; stw &var_1D0, 0(var_2A0)
                //   addi r3, r1, var_2A0 ; bl rw::collision::BoxVolume::Initialize
                // The result IS the volume pointer, and it is the console's own
                // `lpVolume` assert operand below.
                rw::Resource lResource = {};
                lResource.m_baseResources[0] = laVolumeStorage;
                rw::collision::BoxVolume* lpVolume = rw::collision::BoxVolume::Initialize(
                        lResource, lfDimX, lfDimY, KF_PLANE_SEGMENT_TRIGGER_DEPTH);
                lbVolumeOk = (lpVolume != 0);
                // Bounding sphere radius = 0.5 * sqrt(dimX^2 + dimY^2) over the FULL dims (the asm
                // vmsum is over X^2 + Y^2 only -- Z is dropped -- then x0.5).
                lfEntityBoundingSphereRadius = 0.5f * sqrtf(lfDimX * lfDimX + lfDimY * lfDimY);
                lu8TriggerTypeFlag = 1;   // (1 << E_TRIGGERTYPE_PLANE_SEGMENT) == 1
                break;
            }

            case E_TRIGGERTYPE_SPHERE:          // 1
            {
                const f32 lfRadius = lpEvent->GetRadius();
                if (!(lfRadius > 0.0f))
                {
                    CgsDev::Assert::BeginAssert();
                    CgsDev::Assert::FireAssert("Invalid sphere trigger radius", KAC_FILE, 327);
                    CgsDev::Assert::EndAssert();
                }
                // Same staged rw::Resource as the two box branches (0x822D96C4..0x822D96EC).
                rw::Resource lResource = {};
                lResource.m_baseResources[0] = laVolumeStorage;
                rw::collision::SphereVolume* lpVolume =
                    rw::collision::SphereVolume::Initialize(lResource, lfRadius);
                lbVolumeOk = (lpVolume != 0);
                lfEntityBoundingSphereRadius = lfRadius;
                lu8TriggerTypeFlag = 2;   // (1 << E_TRIGGERTYPE_SPHERE) == 2
                break;
            }

            case E_TRIGGERTYPE_BOX:             // 2
            {
                // Box: BoxVolume::Initialize gets the HALVED dims (the asm vmulfp128 by 0.5).
                const f32 lfHX = lpEvent->GetDimensionX() * 0.5f;
                const f32 lfHY = lpEvent->GetDimensionY() * 0.5f;
                const f32 lfHZ = lpEvent->GetDimensionZ() * 0.5f;
                // Staged rw::Resource, 0x822D9520..0x822D9554 (this is the branch whose
                // `vmulfp128 v1, v0, 0.5` builds the halved-extent Vector3 in v1).
                rw::Resource lResource = {};
                lResource.m_baseResources[0] = laVolumeStorage;
                rw::collision::BoxVolume* lpVolume =
                    rw::collision::BoxVolume::Initialize(lResource, lfHX, lfHY, lfHZ);
                lbVolumeOk = (lpVolume != 0);
                // Bounding sphere radius = 0.5 * sqrt(fullX^2 + fullY^2 + fullZ^2) (the asm vmsum3
                // is over the full dims, then x0.5). == magnitude of the halved-extent vector.
                lfEntityBoundingSphereRadius = VectorMagnitude(lfHX, lfHY, lfHZ);
                lu8TriggerTypeFlag = 4;   // (1 << E_TRIGGERTYPE_BOX) == 4 (NOT 3)
                break;
            }

            default:
                CgsDev::Assert::BeginAssert();
                CgsDev::Assert::FireAssert("Unknown event type.", KAC_FILE, 394);
                CgsDev::Assert::EndAssert();
                break;
        }

        if (!lbVolumeOk)
        {
            CgsDev::Assert::BeginAssert();
            CgsDev::Assert::FireAssert("lpVolume", KAC_FILE, 398);
            CgsDev::Assert::EndAssert();
        }
        if (!(lfEntityBoundingSphereRadius > 0.0f))
        {
            CgsDev::Assert::BeginAssert();
            CgsDev::Assert::FireAssert("lfEntityBoundingSphereRadius > 0.0f", KAC_FILE, 399);
            CgsDev::Assert::EndAssert();
        }
        if (lu8TriggerTypeFlag == 0)
        {
            CgsDev::Assert::BeginAssert();
            CgsDev::Assert::FireAssert("lu8TriggerTypeFlag > 0", KAC_FILE, 400);
            CgsDev::Assert::EndAssert();
        }

        // ---- 4. register the trigger volume + entity with the scene manager ----------------
        // The trigger type FLAG is the (1<<type) bitmask; the entity-type FLAG fed to AddEntity is
        // the literal 32 (== 1<<5 == KU_ENTITYTYPEFLAG_TRIGGER), NOT the EntityId owner 4.
        lpOutSceneInputInterface->AddDynamicVolume(lEntityId, laVolumeStorage, lu8TriggerTypeFlag);
        lpOutSceneInputInterface->AddEntity(lEntityId, KU_ENTITYTYPEFLAG_TRIGGER,
                                            lfEntityBoundingSphereRadius);
        lpOutSceneInputInterface->AddVolumeInstance(lEntityId, lrTrigger.mTransform);

        // advance
        liType = lrQueue.GetNextEvent(reinterpret_cast<const CgsModule::Event*>(lpEvent),
                                      reinterpret_cast<const CgsModule::Event**>(&lpEvent), &liSize);
    }
}

// ---------------------------------------------------------------------------
// ProcessRemoveTriggerEvents -- SEPARATE ledger entry, not bodied in this TU.
// ---------------------------------------------------------------------------

// ---------------------------------------------------------------------------
// ProcessTriggerQueryEvents -- X360 0x82306BB0.
//
// Drains the post-scene trigger-query input queue (one car line query per event) and, for each
// E_TRIGGERQUERY_LINETEST (type == 3), pushes a fine line-test request into the scene manager's
// fine-line-test queue (AddEvent type id 5). InEventLineTestFine field mapping (asm-verified):
//   mLineStart          = query event @ +16 (lvx128 r30,r19  r19=16)
//   mLineEnd            = query event @ +32 (lvx128 r30,r25  r25=32)
//   mQueryId            = query event @ +0  (lwz   r31,0(r30))
//   mx32EntityTypeFlags = constant 32       (li r25,0x20 -> KU_ENTITYTYPEFLAG_TRIGGER)
//   mExcludeEntityId    = dword_82CDB5A0    (the global exclude-entity id)
//   meExclusionMode     = 0                 (E_EXCLUDE_ENTITY_ONLY)
//   mxVolumeTypeFlags   = query event @ +4 byte (lbz r29,4(r30))
// ---------------------------------------------------------------------------
void TriggerEntityModule::ProcessTriggerQueryEvents(const InputBuffer_PostScene* lpInput,
                                                    OutputBuffer_PostScene* lpOutput)
{
    if (!lpInput)
    {
        CgsDev::Assert::BeginAssert();
        CgsDev::Assert::FireAssert("lpInput", KAC_FILE, 512);
        CgsDev::Assert::EndAssert();
    }
    if (!lpOutput)
    {
        CgsDev::Assert::BeginAssert();
        CgsDev::Assert::FireAssert("lpOutput", KAC_FILE, 513);
        CgsDev::Assert::EndAssert();
    }

    typedef CgsModule::VariableEventQueue<4096, 16> TriggerQueryQueue;
    typedef CgsModule::VariableEventQueue<2048, 16> FineQueryQueue;

    const TriggerQueryQueue* lpQueryQueue = lpInput->GetQueryInputInterface();
    FineQueryQueue*          lpFineQueue  = lpOutput->GetSceneFineQueryQueue();

    const TriggerEntityModuleIO::InTriggerQueryEvent* lpEvent = 0;
    s32 liSize = 0;
    s32 liType = lpQueryQueue->GetFirstEvent(
        reinterpret_cast<const CgsModule::Event**>(&lpEvent), &liSize);

    while (lpEvent)
    {
        // E_TRIGGERQUERY_LINETEST == 3 (the only valid trigger-query type).
        if (liType == 3)
        {
            CgsSceneManager::SceneManagerIO::InEventLineTestFine lFine;
            lFine.mLineStart          = lpEvent->GetLineStart();      // event @ +16
            lFine.mLineEnd            = lpEvent->GetLineEnd();        // event @ +32
            lFine.mQueryId            = lpEvent->GetQueryId();        // event @ +0
            // The entity-type-flags lane is the literal constant 32 (1<<5), NOT the 0x82CDB5A0
            // global.
            lFine.mx32EntityTypeFlags = KU_ENTITYTYPEFLAG_TRIGGER;
            // The 0x82CDB5A0 global is the EXCLUDE-entity id (so the querying car can't self-hit).
            lFine.mExcludeEntityId    = CgsSceneManager::EntityId(
                TriggerEntityModuleIO::KU_TRIGGER_QUERY_EXCLUDE_ENTITY_ID);
            lFine.meExclusionMode     = CgsSceneManager::SceneManagerIO::E_EXCLUDE_ENTITY_ONLY;
            // The query event's +4 byte carries the volume-type-flags filter.
            lFine.mxVolumeTypeFlags   = lpEvent->GetVolumeTypeFlags();

            // event type 5 == InEventLineTestFine's queue id.
            lpFineQueue->AddEvent<CgsSceneManager::SceneManagerIO::InEventLineTestFine>(&lFine, 5);
        }
        else
        {
            char lacMessageBuffer[CgsDev::Assert::KI_MESSAGEBUFFERSIZE];
            CgsDev::StrStream lStrStream(lacMessageBuffer, CgsDev::Assert::KI_MESSAGEBUFFERSIZE);
            lStrStream << "Unknown trigger query type " << liType;
            CgsDev::Assert::BeginAssert();
            CgsDev::Assert::FireAssert(lStrStream.GetBuffer(), KAC_FILE, 544);
            CgsDev::Assert::EndAssert();
        }

        liType = lpQueryQueue->GetNextEvent(reinterpret_cast<const CgsModule::Event*>(lpEvent),
                                            reinterpret_cast<const CgsModule::Event**>(&lpEvent), &liSize);
    }
}

// ---------------------------------------------------------------------------
// ProcessSceneQueryResults -- X360 0x822EEC78.
// ---------------------------------------------------------------------------
void TriggerEntityModule::ProcessSceneQueryResults(const InputBuffer_PrePhysics* lpInput,
                                                   OutputBuffer_PrePhysics* lpOutput)
{
    if (!lpInput)
    {
        CgsDev::Assert::BeginAssert();
        CgsDev::Assert::FireAssert("lpInput", KAC_FILE, 587);
        CgsDev::Assert::EndAssert();
    }
    if (!lpOutput)
    {
        CgsDev::Assert::BeginAssert();
        CgsDev::Assert::FireAssert("lpOutput", KAC_FILE, 588);
        CgsDev::Assert::EndAssert();
    }

    typedef CgsModule::VariableEventQueue<32768, 16> SceneResultQueue;
    const SceneResultQueue* lpResultQueue = lpInput->GetSceneResultQueue();

    const CgsSceneManager::SceneManagerIO::OutEventLineTestFineResult* lpResult = 0;
    s32 liSize = 0;
    s32 liType = lpResultQueue->GetFirstEvent(
        reinterpret_cast<const CgsModule::Event**>(&lpResult), &liSize);

    while (lpResult)
    {
        // E_OUTEVENT_LINETESTFINERESULT == 1.
        if (liType == 1)
            ProcessLineTestFineResult(lpResult, lpOutput);

        liType = lpResultQueue->GetNextEvent(reinterpret_cast<const CgsModule::Event*>(lpResult),
                                             reinterpret_cast<const CgsModule::Event**>(&lpResult), &liSize);
    }
}

// ---------------------------------------------------------------------------
// ProcessLineTestFineResult -- X360 0x822D9FF8.
// ---------------------------------------------------------------------------
void TriggerEntityModule::ProcessLineTestFineResult(
    const CgsSceneManager::SceneManagerIO::OutEventLineTestFineResult* lpLineTestFineResult,
    OutputBuffer_PrePhysics* lpOutput)
{
    if (!lpLineTestFineResult)
    {
        CgsDev::Assert::BeginAssert();
        CgsDev::Assert::FireAssert("lpLineTestFineResult", KAC_FILE, 629);
        CgsDev::Assert::EndAssert();
    }
    if (!lpOutput)
    {
        CgsDev::Assert::BeginAssert();
        CgsDev::Assert::FireAssert("lpOutput", KAC_FILE, 630);
        CgsDev::Assert::EndAssert();
    }

    const s32 liNumIntersections = lpLineTestFineResult->GetNumIntersections();
    if (liNumIntersections == 0)
        return;

    const CgsSceneManager::SceneQueryId lQueryId = lpLineTestFineResult->GetQueryId();

    // Validate every intersection is a trigger entity in range (X360 asserts, lines 650/651).
    // Producer (EntityId::Set) and consumer (GetEntityIndex) both use the committed 8/12/12
    // EntityId convention, so the slot round-trips self-consistently regardless of the X360
    // build's 14/10 bit positions.
    for (s32 liIndex = 0; liIndex < liNumIntersections; ++liIndex)
    {
        const CgsSceneManager::EntityId lEntityId =
            lpLineTestFineResult->GetIntersection(liIndex).mEntityId;

        if (!(lEntityId.GetOwner() == KU_ENTITYTYPE_TRIGGER))
        {
            CgsDev::Assert::BeginAssert();
            CgsDev::Assert::FireAssert(
                "lpIntersections[liIndex].mEntityId.GetOwner() == E_ENTITYTYPE_TRIGGER",
                KAC_FILE, 650);
            CgsDev::Assert::EndAssert();
        }
        if (!(lEntityId.GetEntityIndex() < KU_MAX_TRIGGERS))
        {
            CgsDev::Assert::BeginAssert();
            CgsDev::Assert::FireAssert(
                "lpIntersections[liIndex].mEntityId.GetEntityIndex() < KU_MAX_TRIGGERS",
                KAC_FILE, 651);
            CgsDev::Assert::EndAssert();
        }
    }

    // Allocate the output event: 2 header words (queryId, count) + count trigger handles.
    typedef CgsModule::VariableEventQueue<1024, 16> TriggerOutputQueue;
    TriggerOutputQueue* lpOutputQueue = lpOutput->GetOutputInterface();

    // event type 1, payload = 4 * (count + 2) bytes.
    u32* lpPayload = static_cast<u32*>(
        lpOutputQueue->AllocateEvent(1, static_cast<s32>(sizeof(u32)) * (liNumIntersections + 2)));

    lpPayload[0] = lQueryId.mId;                          // result[0] = query id
    lpPayload[1] = static_cast<u32>(liNumIntersections);  // result[1] = count
    u32* lpHandle = lpPayload + 2;

    for (s32 liIndex = 0; liIndex < liNumIntersections; ++liIndex)
    {
        const CgsSceneManager::EntityId lEntityId =
            lpLineTestFineResult->GetIntersection(liIndex).mEntityId;
        const u32 luTriggerIndex = lEntityId.GetEntityIndex();
        *lpHandle++ = mTriggers[luTriggerIndex].GetHandle();   // mTriggers[idx] field @+8
    }
}
}
