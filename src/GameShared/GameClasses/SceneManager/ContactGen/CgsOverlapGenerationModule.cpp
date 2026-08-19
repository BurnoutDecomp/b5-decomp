// ===========================================================================
// CgsSceneManager::OverlapGenerationModule -- broad-phase overlap-generation stage.
//
// Reconstructed from the BURNOUT_X360_ARTIST.XEX (Jan-2008 "Breaker" build). The ten
// functions owned by this TU (X360 address + the DWARF's own decl_line in the console's
// CgsOverlapGenerationModule.cpp, which the baked assert __LINE__s corroborate):
//   Construct                  @ 0x828D0460   .cpp:44
//   Prepare                    @ 0x828CB6B0   .cpp:77   (assert line 0x77 == 119)
//   Release                    @ 0x828CB798   .cpp:138
//   Update                     @ 0x828CB878   .cpp:215  (assert line 0xD9 == 217)   ⭐ NEW
//   GenerateOverlaps           @ 0x828D5C08   .cpp:244  (asserts 0xF6/0xF7 == 246/247) ⭐ NEW
//   ProcessAddBodyQueue        @ 0x828C1D18   .cpp:271
//   ProcessUpdateBodyQueue     @ 0x828C1DE8   .cpp:305
//   ProcessRemoveBodyQueue     @ 0x828C1E90   .cpp:336
//   ProcessForceNoPaddingQueue @ 0x828B56D8   .cpp:365  (asserts 0x16F/0x177 == 367/375) ⭐ NEW
//   OutputCollidingPairs       @ 0x828C1F58   .cpp:396  (assert 0x18E == 398)          ⭐ NEW
//
// Prepare/Release are resumable multi-stage state machines (advanced one stage per
// call via the DWARF post-increment operator++ on the EPrepareStage / EReleaseStage
// enums) so the module spreads its alloc/free work across frames. The Process*Queue
// passes replay the per-frame body mutations decoded from the input buffer into the
// SceneSweeper and keep the per-culling-group live-body counters in step; Update is the
// pass that drives all four of them under one read lock, and GenerateOverlaps is the
// per-frame broadphase itself (sweeper -> colliding pairs -> output buffer).
//
// ⭐ 2026-08-18 wave Q5 cluster E2 -- WHY THIS TU MATTERS. Everything downstream of it
// already exists (OverlapCullingModule, the prop bridges, PropEntityModule::
// ProcessPotentialContacts) and everything upstream now exists too (the sweeper's six
// bodies, the module-IO buffers, PropCellManager::AddPropToContactGeneration). Until this
// TU compiles and mounts, SceneSweeper::Update has no caller, so a car can never be found
// touching a smash gate. The [Q5-sweep] diagnostic in OutputCollidingPairs below is the
// one observable that says whether the broadphase sees the car and the props.
// ===========================================================================

#include "GameShared/GameClasses/SceneManager/ContactGen/CgsOverlapGenerationModule.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"  // CGS_ASSERT
#include "GameShared/GameClasses/SceneManager/CgsOverlappingPair.h"  // CgsSceneManager::OverlappingPair (output element)

// [DIAG] host-only, NOT in the X360 binary -- see the [Q5-sweep] block in OutputCollidingPairs.
#include "GameShared/GameClasses/Development/Log/CgsLog.h"  // CgsDev::Log::gpDebugPrint
#include <stdlib.h>                                         // getenv (BRN_PROP_DIAG)

namespace CgsSceneManager
{

// ---------------------------------------------------------------------------------
// Post-increment the resumable state-machine stage enums (DWARF
// CgsOverlapGenerationModule.h:167/168 -- operator++(EStage&, int)). The X360 build
// emits these as out-of-line helpers (sub_828AA218 / sub_828AA278); each advances the
// stage by one and returns the pre-increment value.
// ---------------------------------------------------------------------------------
static OverlapGenerationModule::EPrepareStage operator++(OverlapGenerationModule::EPrepareStage& lreStage, int)
{
    const OverlapGenerationModule::EPrepareStage eOld = lreStage;
    lreStage = static_cast<OverlapGenerationModule::EPrepareStage>(static_cast<int>(lreStage) + 1);
    return eOld;
}

static OverlapGenerationModule::EReleaseStage operator++(OverlapGenerationModule::EReleaseStage& lreStage, int)
{
    const OverlapGenerationModule::EReleaseStage eOld = lreStage;
    lreStage = static_cast<OverlapGenerationModule::EReleaseStage>(static_cast<int>(lreStage) + 1);
    return eOld;
}

// ---------------------------------------------------------------------------------
// Construct @ 0x828D0460
// ---------------------------------------------------------------------------------
void OverlapGenerationModule::Construct()
{
    CgsModule::ModuleSingleBuffered::Construct();

    // Construct sets the release stage to DONE and the prepare stage to START -- the
    // module starts "released, not yet prepared".
    meReleaseStage = E_RELEASESTAGE_DONE;
    mePrepareStage = E_PREPARESTAGE_START;

    // Build + register + clear the sweep-and-prune broadphase (the X360 carries this
    // sweeper construction inlined: debug-component register, interval-pair queue
    // construct, defaults, Clear).
    mSweeper.Construct();

    // Reset the per-culling-group live-body counters.
    for (s32 liType = 0; liType < KI_NUM_ENTITY_TYPES; ++liType)
    {
        mau16NumEntitiesPerType[liType] = 0;
    }

    // Mark this as a freshly-constructed module (CgsModule::Module::mbIsNewModule).
    mbIsNewModule = true;
}

// ---------------------------------------------------------------------------------
// Prepare @ 0x828CB6B0 -- resumable, one stage per call.
//
// Both parameters are pass-through: the X360 body never touches them except to forward
// them, unchanged and in order, to SceneSweeper::Prepare
// (0x828CB734 `mr r5,r28` -> arg3, 0x828CB738 `mr r4,r29` -> arg2,
//  0x828CB73C `addi r3,r31,0x230` -> &mSweeper). So the sweeper's own signature pins
// theirs: `u8* lpaVolumeInstanceCullingGroup, rw::BitTable::Storage* lpCullingTable`,
// which is also exactly what the DWARF spells (`virtual bool Prepare(uint8_t*, BitTable*)`).
// ---------------------------------------------------------------------------------
bool OverlapGenerationModule::Prepare(u8* lpaVolumeInstanceCullingGroup,
                                      rw::BitTable::Storage* lpCullingTable)
{
    switch (mePrepareStage)
    {
    case E_PREPARESTAGE_DONE:
        // A re-prepare after a finished cycle restarts the machine.
        mePrepareStage = E_PREPARESTAGE_START;
        // fall through
    case E_PREPARESTAGE_START:
        mePrepareStage++;  // START -> MANAGER
        // fall through
    case E_PREPARESTAGE_MANAGER:
        if (!CgsModule::ModuleSingleBuffered::Prepare())
        {
            return false;
        }
        mePrepareStage++;  // MANAGER -> SWEEPER
        // fall through
    case E_PREPARESTAGE_SWEEPER:
        if (!mSweeper.Prepare(lpaVolumeInstanceCullingGroup, lpCullingTable))
        {
            return false;
        }
        mePrepareStage++;  // SWEEPER -> DONE
        meReleaseStage = E_RELEASESTAGE_START;
        return true;

    default:
        CGS_ASSERT(false, "Unknown prepare state");
        return false;
    }
}

// ---------------------------------------------------------------------------------
// Release @ 0x828CB798 -- resumable, one stage per call (mirror of Prepare).
// ---------------------------------------------------------------------------------
bool OverlapGenerationModule::Release()
{
    switch (meReleaseStage)
    {
    case E_RELEASESTAGE_START:
        meReleaseStage++;  // START -> SWEEPER
        // fall through
    case E_RELEASESTAGE_SWEEPER:
        mSweeper.Clear();
        meReleaseStage++;  // SWEEPER -> MANAGER
        // fall through
    case E_RELEASESTAGE_MANAGER:
        if (!CgsModule::ModuleSingleBuffered::Release())
        {
            return false;
        }
        meReleaseStage++;  // MANAGER -> DONE
        // fall through
    case E_RELEASESTAGE_DONE:
        mePrepareStage = E_PREPARESTAGE_START;
        return true;

    default:
        CGS_ASSERT(false, "Unrecognised release state\n");
        return false;
    }
}

// ---------------------------------------------------------------------------------
// Update @ 0x828CB878 (39 insns, .cpp:215) -- the per-frame input replay.
//
// The whole body, store for store:
//   0x828CB894  cmplwi r31,0 / bne          -- the null tripwire (assert line 0xD9 == 217)
//   0x828CB8C0  IOBuffer::LockForRead(buf)
//   0x828CB8CC  ProcessRemoveBodyQueue(buf)  ⚠ REMOVE FIRST
//   0x828CB8D8  ProcessAddBodyQueue(buf)
//   0x828CB8E4  ProcessUpdateBodyQueue(buf)
//   0x828CB8F0  ProcessForceNoPaddingQueue(buf)
//   0x828CB8F8  IOBuffer::UnlockForRead(buf)
//
// ⭐ REMOVE-BEFORE-ADD IS THE CONSOLE'S ORDER AND IT IS NOT THE QUEUES' DECLARATION ORDER
// (mAddBodyQueue is first in the buffer; the remove queue is third). The order above is
// ASM-ATTESTED, byte for byte, at the four `bl` sites listed -- do not "tidy" it into
// declaration order. WHY it is that order is NOT attested and is left as an open question:
// the obvious reading is that a sweeper object index freed this frame can then be re-issued
// within the same frame without SceneSweeper::AddObject tripping its duplicate-add tripwire
// (`mCollidingBodies.IsBitSet(luObjectIndex)`, 0x828B5D48), but nothing in these 39
// instructions says so. Treat the rationale as a hypothesis and the order as a fact.
//
// ✅ THE CALLER IS LIVE. SceneManagerModule::UpdateScene step 6 calls
// `mOverlapGenerator.Update(lpOverlapGenerationInput)` (CgsSceneManagerModule.cpp, the
// step-6 block after `lpOverlapGenerationInput->UnlockForWrite()` and before the step-8
// DestroyIOBuffer), matching the console at X360 0x828D4DBC..0x828D4DD0 (module vtable
// slot +0x44, the generator input buffer in r4). Step 3 -- pushing that
// OverlapGenerationIO::InputBuffer on the input stack and draining the scene update
// interface into it -- is live in the same function. There is nothing left to wire here.
//
// ⚠️ STANDING WARNING FOR ANYONE MOVING OR ADDING A CALL: the placement is not free. It
// must stay AFTER `lpOverlapGenerationInput->UnlockForWrite()` and BEFORE DestroyIOBuffer.
// This function takes the buffer's READ lock, and IOBuffer::LockForRead asserts
// `!eStatusLockedForWrite` ("Already locked for write", CgsIOBuffer.cpp:45) -- calling it
// while the drain still holds the write lock fires one assert per frame. That is the
// console's own order too. And do NOT add a second per-frame Update: a duplicate replay
// re-adds every body and trips SceneSweeper::AddObject's duplicate-add tripwire
// (`mCollidingBodies.IsBitSet(luObjectIndex)`, 0x828B5D48).
//
// Reading the [Q5-sweep] counter below: a 0 means the drain queued nothing (or the
// bridge's volume legs are still missing upstream), NOT "the sweeper is broken".
// ---------------------------------------------------------------------------------
void OverlapGenerationModule::Update(const OverlapGenerationIO::InputBuffer* lpInputBuffer)
{
    CGS_ASSERT(lpInputBuffer != NULL, "lpInputBuffer != NULL");

    lpInputBuffer->LockForRead();

    ProcessRemoveBodyQueue(lpInputBuffer);
    ProcessAddBodyQueue(lpInputBuffer);
    ProcessUpdateBodyQueue(lpInputBuffer);
    ProcessForceNoPaddingQueue(lpInputBuffer);

    lpInputBuffer->UnlockForRead();
}

// ---------------------------------------------------------------------------------
// GenerateOverlaps @ 0x828D5C08 (38 insns, .cpp:244) -- the per-frame broadphase.
//
// (Export hole: absent from identity.json and from the per-address JSON export; the asm
// used here is the scout's targeted headless-idat dump, scratchpad/waveQ5/q5_out.json,
// entry req == 0x828d5c08, start 0x828D5C08 end 0x828D5CA0.)
//
// The whole body, store for store:
//   0x828D5C24  cmplwi r31,0 / bne          -- "lpOutputBuffer != NULL"  (line 0xF6 == 246)
//   0x828D5C4C  cmplwi r28,0 / bne          -- "lpEntityManager != NULL" (line 0xF7 == 247)
//   0x828D5C74  addi r3,r29,0x230           -- &mSweeper (the module's +0x230 member)
//   0x828D5C78  SceneSweeper::Update(&mSweeper, lpEntityManager)
//   0x828D5C80  IOBuffer::LockForWrite(lpOutputBuffer)
//   0x828D5C8C  OutputCollidingPairs(this, lpOutputBuffer)
//   0x828D5C94  IOBuffer::UnlockForWrite(lpOutputBuffer)
//
// Note the write lock covers ONLY the publish, not the sweep: SceneSweeper::Update runs
// outside it, so the pair array is built unlocked and only the queue append is guarded.
// ---------------------------------------------------------------------------------
void OverlapGenerationModule::GenerateOverlaps(OverlapGenerationIO::OutputBuffer* lpOutputBuffer,
                                              const EntityManager* lpEntityManager)
{
    CGS_ASSERT(lpOutputBuffer != NULL,  "lpOutputBuffer != NULL");
    CGS_ASSERT(lpEntityManager != NULL, "lpEntityManager != NULL");

    // Sort -> sweep -> build the colliding-pair array (and auto-freeze whatever did not move).
    mSweeper.Update(lpEntityManager);

    lpOutputBuffer->LockForWrite();
    OutputCollidingPairs(lpOutputBuffer);
    lpOutputBuffer->UnlockForWrite();
}

// ---------------------------------------------------------------------------------
// ProcessAddBodyQueue @ 0x828C1D18 -- replay each queued add-body request into the
// sweeper and bump the owning culling group's live-body counter.
// ---------------------------------------------------------------------------------
void OverlapGenerationModule::ProcessAddBodyQueue(const OverlapGenerationIO::InputBuffer* lpInputBuffer)
{
    CGS_ASSERT(lpInputBuffer != NULL, "lpInputBuffer != NULL");

    // The X360 reads the add-body queue out of the input structure (sub_828AFEE8, the
    // const GetAddBodyQueue) and iterates its decoded events by GetLength()/GetEvent(index).
    const OverlapGenerationIO::InAddBodyQueue& lrQueue = *lpInputBuffer->GetAddBodyQueue();

    const s32 liNumEvents = lrQueue.GetLength();
    for (s32 liEvent = 0; liEvent < liNumEvents; ++liEvent)
    {
        const OverlapGenerationIO::InAddBody& lrEvent = lrQueue.GetEvent(liEvent);

        // X360 0x828C1D8C-0x828C1DA0: r4 = lwz 0x24 (muIndex), r5 = the EVENT POINTER
        // (mAabb is the event's first member, so &event == &event.mAabb -- `mr r5,r31`),
        // r6 = lwz 0x28 (meBodyState), r7 = ld 0x30 (mVolumeInstanceID).
        // rw::collision::AABBox is forward-declared only (the rw::math::vpu::Vector3 fork),
        // so the box pointer is spelled as the cast the console's register move performs.
        mSweeper.AddObject(lrEvent.muIndex,
                           reinterpret_cast<const rw::collision::AABBox*>(&lrEvent),
                           lrEvent.meBodyState,
                           lrEvent.mVolumeInstanceID);

        // The counter index is the OWNER byte of the packed volume-instance id
        // (X360 `ld r11,0x30 ; srdi 32 ; srwi 24` == VolumeInstanceId::GetEntityIDOwner).
        // ⚠️ NOT InAddBody::mCullGroup -- the console reads the id, not the +0x20 lane.
        const u32 luCullingGroup = lrEvent.mVolumeInstanceID.GetEntityIDOwner();
        ++mau16NumEntitiesPerType[luCullingGroup];
    }
}

// ---------------------------------------------------------------------------------
// ProcessUpdateBodyQueue @ 0x828C1DE8 -- replay each queued update-body request into
// the sweeper.
// ---------------------------------------------------------------------------------
void OverlapGenerationModule::ProcessUpdateBodyQueue(const OverlapGenerationIO::InputBuffer* lpInputBuffer)
{
    CGS_ASSERT(lpInputBuffer != NULL, "lpInputBuffer != NULL");

    const OverlapGenerationIO::InUpdateBodyQueue& lrQueue = *lpInputBuffer->GetUpdateBodyQueue();

    const s32 liNumEvents = lrQueue.GetLength();
    for (s32 liEvent = 0; liEvent < liNumEvents; ++liEvent)
    {
        const OverlapGenerationIO::InUpdateBody& lrEvent = lrQueue.GetEvent(liEvent);

        // X360 0x828C1E54-0x828C1E68: r4 = lwz 0x30 (muIndex), r5 = the EVENT POINTER
        // (== &event.mAabb), v1 = lvx128 event+0x20 (mPadding), r6 = ld 0x38
        // (mVolumeInstanceID). ⚠️ The vector lane is the sweeper PADDING, not a position,
        // and it is the THIRD argument -- the old spelling passed the id there.
        mSweeper.UpdateObject(lrEvent.muIndex,
                              reinterpret_cast<const rw::collision::AABBox*>(&lrEvent),
                              lrEvent.mPadding,
                              lrEvent.mVolumeInstanceID);
    }
}

// ---------------------------------------------------------------------------------
// ProcessRemoveBodyQueue @ 0x828C1E90 -- replay each queued remove-body request into
// the sweeper and decrement the owning culling group's live-body counter.
// ---------------------------------------------------------------------------------
void OverlapGenerationModule::ProcessRemoveBodyQueue(const OverlapGenerationIO::InputBuffer* lpInputBuffer)
{
    CGS_ASSERT(lpInputBuffer != NULL, "lpInputBuffer != NULL");

    const OverlapGenerationIO::InRemoveBodyQueue& lrQueue = *lpInputBuffer->GetRemoveBodyQueue();

    const s32 liNumEvents = lrQueue.GetLength();
    for (s32 liEvent = 0; liEvent < liNumEvents; ++liEvent)
    {
        const OverlapGenerationIO::InRemoveBody& lrEvent = lrQueue.GetEvent(liEvent);

        // X360 `RemoveObject(a1 + 560, v7[2])` -- word 2 of the 16-byte element == +0x08
        // == muIndex; then `--*(2 * (HIBYTE(*v7) + 451448) + a1)`, whose HIBYTE(*v7) is the
        // owner byte of mVolumeInstanceID (word 0 is the id's big-endian high dword).
        mSweeper.RemoveObject(lrEvent.muIndex);

        const u32 luCullingGroup = lrEvent.mVolumeInstanceID.GetEntityIDOwner();
        --mau16NumEntitiesPerType[luCullingGroup];
    }
}

// ---------------------------------------------------------------------------------
// ProcessForceNoPaddingQueue @ 0x828B56D8 (49 insns, .cpp:365) -- replay each queued
// "this body must be swept with NO padding this frame" request into the sweeper.
//
// The whole body, store for store:
//   0x828B56F0  cmplwi r29,0 / bne          -- "lpInputBuffer != NULL"   (line 0x16F == 367)
//   0x828B571C  li r31,0                    -- the loop index
//   0x828B5720  sub_828B00E0(buf)           -- InputBuffer::GetForceNoPaddingQueue() const
//   0x828B5724  lwz r11,8(r3) / cmpwi / ble -- BaseEventQueue::GetLength() > 0
//   0x828B5734  addi r28,r30,0x230          -- &mSweeper
//   0x828B5740  sub_828B00E0(buf)           -- the accessor AGAIN, per iteration
//   0x828B5748  sub_828AE028(queue, i)      -- BaseEventQueue<InForceNoPadding>::GetEvent(i)
//   0x828B574C  lwz r30,0(r3)               -- InForceNoPadding::muVolInstIndex (+0x00)
//   0x828B5750  cmplwi r30,0x13B8 / blt     -- the range tripwire (line 0x177 == 375)
//   0x828B5778  SceneSweeper::ForceNoPadding(&mSweeper, muVolInstIndex)
//   0x828B5784  sub_828B00E0(buf) / lwz 8   -- GetLength() re-read at the bottom of the loop
//
// ⚠ 0x13B8 == 5048 is KI_MAX_NUM_VOLUME_INSTANCES, NOT the sweeper's KU_MAX_NUM_OBJECTS
// (5051). The console's own assert text bakes the name, so the two ceilings really are
// different numbers and this one is the volume-instance ceiling.
//
// SHAPE NOTE: the console re-calls the queue accessor and re-reads GetLength() on every
// iteration (three calls per event, not one hoisted pair) -- reproduced literally here.
// The three sibling Process*BodyQueue bodies above hoist the accessor into a reference
// instead; their asm (e.g. ProcessAddBodyQueue 0x828C1D5C/0x828C1D7C/0x828C1DCC) has the
// same un-hoisted shape, so that hoist is a pre-existing cosmetic divergence in those
// three, not a contradiction with this one. Behaviourally identical either way -- nothing
// in the loop appends to the queue it is walking.
// ---------------------------------------------------------------------------------
void OverlapGenerationModule::ProcessForceNoPaddingQueue(const OverlapGenerationIO::InputBuffer* lpInputBuffer)
{
    CGS_ASSERT(lpInputBuffer != NULL, "lpInputBuffer != NULL");

    for (s32 liEvent = 0; liEvent < lpInputBuffer->GetForceNoPaddingQueue()->GetLength(); ++liEvent)
    {
        const OverlapGenerationIO::InForceNoPadding& lrEvent =
            lpInputBuffer->GetForceNoPaddingQueue()->GetEvent(liEvent);

        CGS_ASSERT(lrEvent.muVolInstIndex < static_cast<u32>(SceneSweeper::KI_MAX_NUM_VOLUME_INSTANCES),
                   "lEvent.muVolInstIndex < static_cast<uint32_t>( KI_MAX_NUM_VOLUME_INSTANCES )");

        mSweeper.ForceNoPadding(lrEvent.muVolInstIndex);
    }
}

// ---------------------------------------------------------------------------------
// OutputCollidingPairs @ 0x828C1F58 (41 insns, .cpp:396) -- publish the sweeper's
// colliding-pair array onto the module output buffer as OverlappingPair events.
//
// The caller (GenerateOverlaps) already holds the buffer's WRITE lock.
//
// The whole body, store for store:
//   0x828C1F6C  cmplwi r29,0 / bne     -- "lpOutputBuffer != NULL"  (line 0x18E == 398)
//   0x828C1F94  lwzx r30,r31,0xDC6E4   -- the pair count. MODULE-relative 0xDC6E4 minus the
//                                         mSweeper offset 0x230 == sweeper-relative 0xDC4B4
//                                         == SceneSweeper::muNumCollidingPairs
//                                         (CgsSceneSweeper.h:349). == GetNumCollidingPairs().
//   0x828C1FA8  addis/addi r31,0xD67F4 -- module-relative &maCollidingPairs[0] + 4:
//                                         0xD67F4 - 0x230 == 0xD65C4, and maCollidingPairs is
//                                         at sweeper-relative 0xD65C0 (CgsSceneSweeper.h:337).
//                                         The cursor is biased +4 so the two u16s are reached
//                                         at -4/-2 and the float at 0. Stride 12 (`addi 0xC`)
//                                         == sizeof(CgsSceneManager::CollidingPair).
//   per pair, into a 16-byte stack temp:
//     lhz -4(cur) -> stw +0x00   CollidingPair::mu16ObjectA -> OverlappingPair::muVolumeInstanceA
//     lhz -2(cur) -> stw +0x04   CollidingPair::mu16ObjectB -> OverlappingPair::muVolumeInstanceB
//     lfs  0(cur) -> stfs +0x08  CollidingPair::mfPadding   -> OverlappingPair::mfPadding
//     lbz  4(cur) -> stb  +0x0C  CollidingPair::mbCull      -> OverlappingPair::mbCull
//   0x828C1FD8  CgsSceneManager::OverlapGen(a2)  == OutputBuffer::GetOverlappingPairQueue()
//               (sub_828B04D0 -- the NON-const accessor, `this+4`, IDA truncated the symbol)
//   0x828C1FE0  BaseEventQueue<OverlappingPair>::AddEvent(queue, &temp)
//
// ⭐ THE TWO LEADING FIELDS WIDEN u16 -> u32 AND THEY ARE VOLUME-INSTANCE POOL INDICES,
// NOT VolumeInstanceIds. This producer is one of the three independent attestations behind
// the CgsOverlappingPair.h layout correction landed the same day by the culler cluster
// (E3a) -- see that header's banner, which quotes this exact lhz/stw + lfs/stfs + lbz/stb
// sequence: the retired two-VolumeInstanceId model could not have been produced by it (two
// u64 handles would be two `std`s), and a consumer decoding an object index as a packed
// entity handle would read a garbage owner/index out of it. AddEvent (not AddEventSafe) --
// the console does not range-check here; the sweeper's own KU_MAX_NUM_COLLIDING_PAIRS == 1024
// ceiling is enforced upstream in BuildCollidingPairs.
// ---------------------------------------------------------------------------------
void OverlapGenerationModule::OutputCollidingPairs(OverlapGenerationIO::OutputBuffer* lpOutputBuffer)
{
    CGS_ASSERT(lpOutputBuffer != NULL, "lpOutputBuffer != NULL");

    const s32 liNumCollidingPairs = static_cast<s32>(mSweeper.GetNumCollidingPairs());

    // ---------------------------------------------------------------------------------
    // [DIAG] NOT IN THE X360 BINARY. Wave-Q5 bring-up observable: this is the single point
    // in the whole chain that can answer "does the broadphase see the car and the props?".
    // Everything upstream (the drain, the sweeper) is silent, and everything downstream
    // (culler, bridges, PropEntityModule) only reports once a pair already exists -- so a
    // reader with no [Q5-sweep] line at all knows GenerateOverlaps is not being called,
    // a reader with "0" knows the sweeper has no bodies -- i.e. the drain queued nothing
    // (or the bridge's volume legs are still missing upstream); UpdateScene steps 3 and 6
    // are both live, see the banner on Update above -- and a non-zero count is the first
    // proof that a car box and a prop box overlapped.
    // Two lines, deliberately: a ONE-SHOT the moment the count first goes non-zero (that
    // frame is the interesting one and it scrolls past instantly), and a coarse periodic
    // sample so a reader can watch the count move while driving. The period is a call
    // counter, not a timer -- GenerateOverlaps runs once per scene update, so 60 is
    // "about once a second" at 60 Hz and degrades gracefully if the frame rate does.
    // The getenv latch is evaluated ONCE (a per-frame syscall is not acceptable here) and
    // the shipped path pays one predictable branch when the probe is off.
    // ---------------------------------------------------------------------------------
    {
        static const bool sbPropDiag = ( getenv( "BRN_PROP_DIAG" ) != 0 );
        if ( sbPropDiag && CgsDev::Log::gpDebugPrint != 0 )
        {
            static bool sbFirstPairReported = false;
            static u32  suDiagCallCount     = 0;

            ++suDiagCallCount;

            if ( !sbFirstPairReported && liNumCollidingPairs > 0 )
            {
                sbFirstPairReported = true;
                *CgsDev::Log::gpDebugPrint
                    << "[Q5-sweep] first colliding pair(s): " << liNumCollidingPairs << "\n";
            }

            if ( ( suDiagCallCount % 60u ) == 0u )
            {
                *CgsDev::Log::gpDebugPrint
                    << "[Q5-sweep] colliding pairs this frame: " << liNumCollidingPairs
                    << " (GenerateOverlaps call " << suDiagCallCount << ")\n";
            }
        }
    }

    for (s32 liPair = 0; liPair < liNumCollidingPairs; ++liPair)
    {
        // CollidingPair is a NAMESPACE-scope struct (CgsSceneSweeper.h:158), not nested in
        // SceneSweeper -- the DWARF declares it at CgsSceneSweeper.h:48, outside the class.
        const CollidingPair* lpCollidingPair =
            mSweeper.GetCollidingPair(static_cast<u32>(liPair));

        OverlappingPair lOverlappingPair;
        lOverlappingPair.muVolumeInstanceA = lpCollidingPair->GetObjectA();  // u16 -> u32 (lhz/stw)
        lOverlappingPair.muVolumeInstanceB = lpCollidingPair->GetObjectB();  // u16 -> u32 (lhz/stw)
        lOverlappingPair.mfPadding         = lpCollidingPair->GetPadding();  // f32     (lfs/stfs)
        lOverlappingPair.mbCull            = lpCollidingPair->GetCull();     // bool    (lbz/stb)

        lpOutputBuffer->GetOverlappingPairQueue()->AddEvent(lOverlappingPair);
    }
}

}
