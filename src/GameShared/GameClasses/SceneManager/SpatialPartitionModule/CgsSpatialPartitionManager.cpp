// ===========================================================================
// GameShared/GameClasses/SceneManager/SpatialPartitionModule/
//   CgsSpatialPartitionManager.cpp
//
// CgsSceneManager::SpatialPartitionManager -- the SceneManager's coarse (broad-phase)
// spatial partition wrapper. Reconstructed from BURNOUT_X360_ARTIST.XEX, store-for-store:
//   Prepare            @ 0x828CFFA8  -- staged carve/Construct/Prepare of the octree
//   Release            @ 0x828AA860  -- staged Release/Destruct of the octree
//   ProcessUpdateQueue @ 0x828BAE50  -- drain the inbound update queue into the partition
//
// The active partition is stored as a SpatialPartition* (the concrete kind selected in
// Prepare is a LooseOctree). SpatialPartition's base vtable is intentionally modelled
// minimal in the shared header, so the virtual dispatches these bodies issue go through the
// X360 vtable slots directly (see PartitionVFn below) -- faithful to the asm byte offsets.
// ===========================================================================

#include "GameShared/GameClasses/SceneManager/SpatialPartitionModule/CgsSpatialPartitionManager.h"

#include <new>   // placement new (carve the octree into the allocator block)

#include "types.hpp"
#include "BrnCommonTypes.h"                                                                 // Vector3
#include "GameShared/GameClasses/Core/CgsAssert.h"                                          // CGS_ASSERT
#include "GameShared/GameClasses/Module/CgsVariableEventQueue.h"                            // CgsModule::Event / VariableEventQueue
#include "GameShared/GameClasses/SceneManager/SpatialPartitionModule/CgsSpatialPartitionManagerIO.h"          // SpatialPartitionIO::InputBuffer_Update
#include "GameShared/GameClasses/SceneManager/SpatialPartitionModule/SpatialPartitions/CgsSpatialPartition.h"  // SpatialPartition
#include "GameShared/GameClasses/SceneManager/SpatialPartitionModule/SpatialPartitions/CgsLooseOctree.h"       // LooseOctree

namespace CgsSceneManager
{
namespace
{
    // (The raw vtable-SLOT dispatch helpers this TU used are RETIRED 2026-07-28:
    //  CgsSpatialPartition.h now declares the partition's virtuals by name --
    //  Construct/Destruct/Prepare/Release @ slots 0..3, Update @ 10,
    //  SetEntityPosition/SetEntityRadius @ 11/12, AddEntityToGraph/
    //  RemoveEntityFromGraph @ 14/15 -- so the bodies below dispatch through the
    //  language. Indexing a hand-built slot table was only ever correct while the
    //  base had no modelled vtable, and it cannot be on the x64 target.)

    // (the inbound update-queue record type ids + payload records now live in their
    //  IO home, CgsSpatialPartitionManagerIO.h, beside the buffer that carries them)
}

// ===========================================================================
// SpatialPartitionManager::Construct
//
// Park the staged handshake and the (not yet carved) partition. The X360 body is the
// compiler-emitted member init the SceneManagerModule's Construct cascade runs; the
// partition itself is created lazily by Prepare out of the scene resource allocator.
// ===========================================================================
void SpatialPartitionManager::Construct()
{
    mePrepareStage         = E_SCENE_GRAPH_PREPARE_START;
    meReleaseStage         = E_SCENE_GRAPH_RELEASE_START;
    miTextX                = 0;
    miTextY                = 0;
    mpSpatialPartition     = NULL;
    meSpatialPartitionType = E_SPATIAL_PARTITION_TYPE_LOOSE_OCTREE;
}

// ===========================================================================
// SpatialPartitionManager::Prepare @ 0x828CFFA8
//
// Staged (resumable) prepare handshake. First entry (stage START) advances through
// MANAGER, carves + Constructs the concrete partition (only a LooseOctree is supported;
// a sphere tree / unknown type asserts), then drives the partition's own Prepare(). The
// partition Prepare() may need several frames -- while it returns false the manager stays
// at the SCENE_GRAPH stage and re-enters here; once it succeeds the stage advances to DONE,
// the release stage is reset and true is returned. De-gotoed into a fall-through switch
// (each step advances mePrepareStage), matching the X360 jump-table fall-through.
// ===========================================================================
bool SpatialPartitionManager::Prepare(SpatialPartitionConstructParams* lpConstructParams,
                                      rw::IResourceAllocator*          lpSceneAllocator)
{
    switch (mePrepareStage)
    {
        case E_SCENE_GRAPH_PREPARE_DONE:
            mePrepareStage = E_SCENE_GRAPH_PREPARE_START;
            // fall through
        case E_SCENE_GRAPH_PREPARE_START:
            mePrepareStage++;   // -> MANAGER
            // fall through
        case E_SCENE_GRAPH_PREPARE_MANAGER:
            mePrepareStage++;   // -> CONSTRUCT_SCENE_GRAPH
            // fall through
        case E_SCENE_GRAPH_PREPARE_CONSTRUCT_SCENE_GRAPH:
            if (mpSpatialPartition == NULL)
            {
                meSpatialPartitionType = lpConstructParams->meType;
                if (meSpatialPartitionType == E_SPATIAL_PARTITION_TYPE_SPHERE_TREE)
                {
                    CGS_ASSERT(false,
                        "Sphere tree is not currently supported - needs updating to suit new scene stuff\n");
                }
                else if (meSpatialPartitionType != E_SPATIAL_PARTITION_TYPE_LOOSE_OCTREE)
                {
                    CGS_ASSERT(false, "Scene graph type is unsupported");
                }
                else
                {
                    void* lpMemory = LooseOctree::operator new(sizeof(LooseOctree), lpSceneAllocator);
                    if (lpMemory != NULL)
                        mpSpatialPartition = ::new (lpMemory) LooseOctree();
                    else
                        mpSpatialPartition = NULL;
                }

                // ⭐ THE ASSERT IS THE CONSOLE'S ONLY TEST (:142, `li r5, 142` @0x828D00D8).
                // ⛔ AN INVENTED `if (mpSpatialPartition == NULL) return false;` USED TO SIT HERE
                // and was deleted 2026-08-16. Read from the asm: 0x828D00C4 loads the member,
                // 0x828D00CC is `bne cr6, 0x828D00EC` -- i.e. NOT-null SKIPS the assert block --
                // and the null path FALLS THROUGH into 0x828D00EC, which loads the same member and
                // dispatches Construct through its vtable. There is no early return on this arm.
                // (A sibling invention in Release is on record as having caused a real bug; an
                // invented guard turns a shipped crash into a silently un-built partition.)
                CGS_ASSERT(mpSpatialPartition != NULL, "mpSpatialPartition != NULL");
                mpSpatialPartition->Construct(lpConstructParams, lpSceneAllocator);
            }
            mePrepareStage++;   // -> SCENE_GRAPH
            // fall through
        case E_SCENE_GRAPH_PREPARE_SCENE_GRAPH:
            // ⛔ `mpSpatialPartition != NULL &&` USED TO GUARD THIS CALL and was deleted
            // 2026-08-16 for the same reason as the one above: the console's arm at 0x828D0108
            // runs the stage helper, then `lwz r3, 16(r29)` / `lwz r11, 0(r3)` / `lwz r11, 8(r11)`
            // / `bctrl` -- an unconditional virtual dispatch with no null test anywhere.
            if (mpSpatialPartition->Prepare())
            {
                mePrepareStage++;   // -> DONE
                meReleaseStage = E_SCENE_GRAPH_RELEASE_START;
                return true;
            }
            return false;

        default:
            CGS_ASSERT(false, "Unrecognised release state");
            return false;
    }
}

// ===========================================================================
// SpatialPartitionManager::Release @ 0x828AA860
//
// Staged (resumable) release handshake. Drives the partition's own Release() (which may
// span frames); while it returns false the manager falls back to Destruct() and returns
// false. Once Release() succeeds the stage advances through MANAGER to DONE, the prepare
// stage is reset and true is returned. De-gotoed into a fall-through switch, matching the
// X360 jump-table fall-through.
// ===========================================================================
bool SpatialPartitionManager::Release()
{
    switch (meReleaseStage)
    {
        case E_SCENE_GRAPH_RELEASE_START:
            meReleaseStage++;   // -> SCENE_GRAPH
            // fall through
        case E_SCENE_GRAPH_RELEASE_SCENE_GRAPH:
            // ⛔ NO null guard here, and that is FAITHFUL: @0x828AA8BC the console loads
            // mpSpatialPartition (+0x10), loads its vtable and calls slot +0xC with no test
            // whatsoever. A `if (mpSpatialPartition == NULL) { meReleaseStage++; break; }`
            // early-out lived here from cc48d4f2 until 2026-08-16; it was an INVENTED arm,
            // and because it `break`s out of the switch it also fell off the end of a
            // bool-returning function -- the ONE C4715 in the whole 1,210-TU build. The
            // caller (SceneManagerModule::Release @0x828C7220) reads that return as
            // "partition finished releasing?", so the garbage in `al` decided, per frame and
            // at random, whether the module advanced past a partition it had not released.
            if (!mpSpatialPartition->Release())
            {
                mpSpatialPartition->Destruct();
                return false;
            }
            meReleaseStage++;   // -> MANAGER
            // fall through
        case E_SCENE_GRAPH_RELEASE_MANAGER:
            meReleaseStage++;   // -> DONE
            // fall through
        case E_SCENE_GRAPH_RELEASE_DONE:
            mePrepareStage = E_SCENE_GRAPH_PREPARE_START;
            return true;

        default:
            CGS_ASSERT(false, "Unrecognised release state");
            return false;
    }
}

// ===========================================================================
// SpatialPartitionManager::ProcessUpdateQueue @ 0x828BAE50
//
// Walk the inbound spatial-partition update queue (a VariableEventQueue<135168,16> handed
// out by the read-locked InputBuffer_Update) and apply each record to the active partition:
//   0 AddEntity            -> SpatialPartition::AddEntity (non-virtual)
//   1 RemoveEntity         -> RemoveEntityFromGraph (vtable), then bounds-assert the id
//   2 SetEntityPosition    -> SetEntityPosition (vtable)
//   3 SetEntityRadius      -> SetEntityRadius (vtable)
// The record payloads are read by their X360 byte offsets. Each record's leading 16-byte
// lane is a Vector3 (position); the trailing scalars are id/flags/radius.
// ===========================================================================
void SpatialPartitionManager::ProcessUpdateQueue(const SpatialPartitionIO::InputBuffer_Update* lpInputBuffer)
{
    CGS_ASSERT(lpInputBuffer != NULL, "lpInputBuffer != NULL");

    SpatialPartition* lpPartition = mpSpatialPartition;
    if (lpPartition == NULL)
    {
        return;
    }

    const CgsModule::Event* lpEvent = NULL;
    s32 liSize = 0;
    s32 liId = lpInputBuffer->GetSpatialPartitionUpdateQueue()->GetFirstEvent(&lpEvent, &liSize);
    while (liId >= 0)
    {
        switch (liId)
        {
            case SpatialPartitionIO::E_SPATIAL_PARTITION_UPDATE_ADD_ENTITY:
            {
                const SpatialPartitionIO::InEventAddEntity& lrEvent =
                    *static_cast<const SpatialPartitionIO::InEventAddEntity*>(lpEvent);
                lpPartition->AddEntity(lrEvent.mu16EntityId, lrEvent.mx32TypeFlags,
                                       lrEvent.mPosition, lrEvent.mfRadius);
                break;
            }
            case SpatialPartitionIO::E_SPATIAL_PARTITION_UPDATE_REMOVE_ENTITY:
            {
                const SpatialPartitionIO::InEventRemoveEntity& lrEvent =
                    *static_cast<const SpatialPartitionIO::InEventRemoveEntity*>(lpEvent);
                lpPartition->RemoveEntityFromGraph(lrEvent.mu16EntityId);
                CGS_ASSERT(lrEvent.mu16EntityId < SpatialPartition::KI_MAX_NUM_ENTITIES,
                           "lu16Index < KI_MAX_NUM_ENTITIES");
                break;
            }
            case SpatialPartitionIO::E_SPATIAL_PARTITION_UPDATE_SET_ENTITY_POSITION:
            {
                const SpatialPartitionIO::InEventSetEntityPosition& lrEvent =
                    *static_cast<const SpatialPartitionIO::InEventSetEntityPosition*>(lpEvent);
                lpPartition->SetEntityPosition(lrEvent.mu16EntityId, lrEvent.mPosition);
                break;
            }
            case SpatialPartitionIO::E_SPATIAL_PARTITION_UPDATE_SET_ENTITY_RADIUS:
            {
                const SpatialPartitionIO::InEventSetEntityRadius& lrEvent =
                    *static_cast<const SpatialPartitionIO::InEventSetEntityRadius*>(lpEvent);
                lpPartition->SetEntityRadius(lrEvent.mu16EntityId, lrEvent.mfRadius);
                break;
            }
            default:
                CGS_ASSERT(false, "Unrecognised event in update queue");
                break;
        }
        liId = lpInputBuffer->GetSpatialPartitionUpdateQueue()->GetNextEvent(lpEvent, &lpEvent, &liSize);
    }
}

// ===========================================================================
// SpatialPartitionManager::UpdateScene @ 0x828C9948
//
//   assert(lpInputBuffer != NULL);
//   IOBuffer::LockForRead(lpInputBuffer);
//   ProcessUpdateQueue(lpInputBuffer);
//   mpSpatialPartition->Update();          // vtable slot 10 (asm `(**(a1+16) + 40)`)
//   IOBuffer::UnlockForRead(lpInputBuffer);
// ===========================================================================
void SpatialPartitionManager::UpdateScene(SpatialPartitionIO::InputBuffer_Update* lpInputBuffer)
{
    CGS_ASSERT(lpInputBuffer != NULL, "lpInputBuffer != NULL");
    if (lpInputBuffer == NULL)
    {
        return;
    }

    lpInputBuffer->LockForRead();
    ProcessUpdateQueue(lpInputBuffer);
    if (mpSpatialPartition != NULL)
    {
        mpSpatialPartition->Update();
    }
    lpInputBuffer->UnlockForRead();
}
}
