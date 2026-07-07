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
    // -----------------------------------------------------------------------
    // Virtual dispatch through the SpatialPartition base vtable by slot. The shared
    // SpatialPartition header declares only AddEntityToGraph as a virtual, so the base
    // vtable is not modelled slot-for-slot there; these bodies index the runtime vtable at
    // the DWARF-attested slots (matching the X360 asm byte offsets, slot*4):
    //   slot  0 (+0x00) Construct(ConstructParams*, IResourceAllocator*)
    //   slot  1 (+0x04) Destruct()
    //   slot  2 (+0x08) Prepare()                 -> bool
    //   slot  3 (+0x0C) Release()                 -> bool
    //   slot 11 (+0x2C) SetEntityPosition(id, pos)
    //   slot 12 (+0x30) SetEntityRadius(id, radius)
    //   slot 15 (+0x3C) RemoveEntityFromGraph(id)
    // -----------------------------------------------------------------------
    template <typename Pfn>
    inline Pfn PartitionVFn(const SpatialPartition* lpPartition, unsigned luSlot)
    {
        Pfn const* lpVTable = *reinterpret_cast<Pfn const* const*>(lpPartition);
        return lpVTable[luSlot];
    }

    inline void PartitionConstruct(SpatialPartition* lpP,
                                   SpatialPartitionConstructParams* lpParams,
                                   rw::IResourceAllocator* lpAllocator)
    {
        typedef void (*Pfn)(SpatialPartition*, SpatialPartitionConstructParams*, rw::IResourceAllocator*);
        PartitionVFn<Pfn>(lpP, 0)(lpP, lpParams, lpAllocator);
    }

    inline void PartitionDestruct(SpatialPartition* lpP)
    {
        typedef void (*Pfn)(SpatialPartition*);
        PartitionVFn<Pfn>(lpP, 1)(lpP);
    }

    inline bool PartitionPrepare(SpatialPartition* lpP)
    {
        typedef bool (*Pfn)(SpatialPartition*);
        return PartitionVFn<Pfn>(lpP, 2)(lpP);
    }

    inline bool PartitionRelease(SpatialPartition* lpP)
    {
        typedef bool (*Pfn)(SpatialPartition*);
        return PartitionVFn<Pfn>(lpP, 3)(lpP);
    }

    inline void PartitionSetEntityPosition(SpatialPartition* lpP, u16 lu16Id, Vector3 lPosition)
    {
        typedef void (*Pfn)(SpatialPartition*, u16, Vector3);
        PartitionVFn<Pfn>(lpP, 11)(lpP, lu16Id, lPosition);
    }

    inline void PartitionSetEntityRadius(SpatialPartition* lpP, u16 lu16Id, f32 lfRadius)
    {
        typedef void (*Pfn)(SpatialPartition*, u16, f32);
        PartitionVFn<Pfn>(lpP, 12)(lpP, lu16Id, lfRadius);
    }

    inline void PartitionRemoveEntityFromGraph(SpatialPartition* lpP, u16 lu16Id)
    {
        typedef void (*Pfn)(SpatialPartition*, u16);
        PartitionVFn<Pfn>(lpP, 15)(lpP, lu16Id);
    }

    // Inbound update-queue record type ids (the switch keys returned by the VEQ; the
    // producer InputBuffer_Update stages one of these per Add/Remove/SetPosition/SetRadius).
    enum ESpatialPartitionUpdateEventType
    {
        E_SPATIAL_PARTITION_UPDATE_ADD_ENTITY          = 0,
        E_SPATIAL_PARTITION_UPDATE_REMOVE_ENTITY       = 1,
        E_SPATIAL_PARTITION_UPDATE_SET_ENTITY_POSITION = 2,
        E_SPATIAL_PARTITION_UPDATE_SET_ENTITY_RADIUS   = 3,
    };
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

                CGS_ASSERT(mpSpatialPartition != NULL, "mpSpatialPartition != NULL");
                PartitionConstruct(mpSpatialPartition, lpConstructParams, lpSceneAllocator);
            }
            mePrepareStage++;   // -> SCENE_GRAPH
            // fall through
        case E_SCENE_GRAPH_PREPARE_SCENE_GRAPH:
            if (PartitionPrepare(mpSpatialPartition))
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
            if (!PartitionRelease(mpSpatialPartition))
            {
                PartitionDestruct(mpSpatialPartition);
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

    const CgsModule::Event* lpEvent = NULL;
    s32 liSize = 0;
    s32 liId = lpInputBuffer->GetSpatialPartitionUpdateQueue()->GetFirstEvent(&lpEvent, &liSize);
    while (liId >= 0)
    {
        const u8* lpBytes = reinterpret_cast<const u8*>(lpEvent);
        switch (liId)
        {
            case E_SPATIAL_PARTITION_UPDATE_ADD_ENTITY:
            {
                Vector3 lPosition   = *reinterpret_cast<const Vector3*>(lpBytes + 0x00);
                u16     lu16Id      = *reinterpret_cast<const u16*>(lpBytes + 0x10);
                u32     lxTypeFlags = *reinterpret_cast<const u32*>(lpBytes + 0x14);
                f32     lfRadius    = *reinterpret_cast<const f32*>(lpBytes + 0x18);
                lpPartition->AddEntity(lu16Id, lxTypeFlags, lPosition, lfRadius);
                break;
            }
            case E_SPATIAL_PARTITION_UPDATE_REMOVE_ENTITY:
            {
                u16 lu16Id = *reinterpret_cast<const u16*>(lpBytes + 0x00);
                PartitionRemoveEntityFromGraph(lpPartition, lu16Id);
                CGS_ASSERT(lu16Id < SpatialPartition::KI_MAX_NUM_ENTITIES, "lu16Index < KI_MAX_NUM_ENTITIES");
                break;
            }
            case E_SPATIAL_PARTITION_UPDATE_SET_ENTITY_POSITION:
            {
                Vector3 lPosition = *reinterpret_cast<const Vector3*>(lpBytes + 0x00);
                u16     lu16Id    = *reinterpret_cast<const u16*>(lpBytes + 0x10);
                PartitionSetEntityPosition(lpPartition, lu16Id, lPosition);
                break;
            }
            case E_SPATIAL_PARTITION_UPDATE_SET_ENTITY_RADIUS:
            {
                u16 lu16Id   = *reinterpret_cast<const u16*>(lpBytes + 0x00);
                f32 lfRadius = *reinterpret_cast<const f32*>(lpBytes + 0x04);
                PartitionSetEntityRadius(lpPartition, lu16Id, lfRadius);
                break;
            }
            default:
                CGS_ASSERT(false, "Unrecognised event in update queue");
                break;
        }
        liId = lpInputBuffer->GetSpatialPartitionUpdateQueue()->GetNextEvent(lpEvent, &lpEvent, &liSize);
    }
}
}
