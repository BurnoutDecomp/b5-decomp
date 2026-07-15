// ===== Owning header: GameSource/World/EntityModules/TriggerEntityModule/SharedIO/BrnTriggerEntityModuleInputInterface.h =====
// DWARF home: BrnTriggerEntityModuleInputInterface.h:92.
// Homes the trigger entity module's pre-scene/post-scene INPUT element + aggregate types:
//   * InRemoveTriggerEvent          -- the element of the remove-trigger input queue (committed).
//   * InAddTriggerEvent             -- the element of the add-trigger input queue (Process* TU grow).
//   * InTriggerQueryEvent           -- the element of the trigger-query input queue (Process* TU grow).
//   * TriggerManagementInputInterface -- the add+remove queue aggregate (Process* TU grow).
//   * KU_TRIGGER_QUERY_EXCLUDE_ENTITY_ID -- the exclude-entity id pushed into the fine line test.
// The sibling LineTest events and the TriggerQueryInputInterface aggregate that also live in this
// DWARF header are NOT reconstructed here (declared-only / left for their own TUs).
#pragma once

#include "BrnCommonTypes.h"                                       // u32, Vector3, Matrix44Affine
#include "GameShared/GameClasses/Module/CgsVariableEventQueue.h"  // CgsModule::Event / VariableEventQueue
#include "GameShared/GameClasses/Module/CgsEventQueue.h"          // CgsModule::EventQueue (remove queue)
#include "GameShared/GameClasses/SceneManager/CgsSceneQueryId.h"  // CgsSceneManager::SceneQueryId

// The add-box producer (AddTriggerRegion) takes a trigger region by pointer and reads its
// leading BoxRegion + GenericRegion category byte. Forward-declared here (full layouts are
// pulled by the .cpp) to keep the header light.
namespace BrnTrigger { struct TriggerRegion; struct GenericRegion; }

namespace BrnWorld
{
namespace TriggerEntityModuleIO
{
    // 32-bit trigger handle (DWARF spells it `TriggerId`, home BrnTriggerTypes.h). No committed
    // `TriggerId` typedef exists yet; the X360 element stride is exactly 4 bytes and the
    // LightTriggerId==u32 precedent (BrnGameModeParams.h) fixes the width. Provisional minimal
    // slice -- promote to its BrnTriggerTypes.h home when that TU lands.
    typedef u32 TriggerId;

    // The exclude-entity id pushed into each fine line test (the X360 dword_82CDB5A0 global).
    // X360 ProcessTriggerQueryEvents writes this into InEventLineTestFine.mExcludeEntityId (the
    // entity the line test must NOT report a hit against, i.e. the querying car itself); it is the
    // 3rd field stored after the type-flags constant, NOT the entity-type-flags lane (that lane is
    // the literal 32 == 1<<5). FLAG: the exact id is the X360 dword at 0x82CDB5A0 (not in the
    // exports); modelled as the invalid id (0xFFFFFFFF) so no real entity is wrongly excluded.
    // Replace with the real value when 0x82CDB5A0 is recovered.
    static const u32 KU_TRIGGER_QUERY_EXCLUDE_ENTITY_ID = 0xFFFFFFFFu; // FLAG: rdata @0x82CDB5A0 not in exports

    // BrnTriggerEntityModuleInputInterface.h:92 (DecFIGS DWARF). Element record of the module's
    // "remove trigger" input queue. Derives from the empty CgsModule::Event base (EBO -> sizeof ==
    // 4, matching the X360 4-byte element stride). Owning queue capacity == 256.
    struct InRemoveTriggerEvent : public CgsModule::Event
    {
        TriggerId mTriggerID;   // BrnTriggerEntityModuleInputInterface.h:94
    };

    // ADDITIVE GROW (Process* TU): element record of the module's "add trigger" input queue
    // (CgsModule::VariableEventQueue<131072,16>). X360 ProcessAddTriggerEvents (0x822D8F48) reads:
    //   meType    = the queue event TYPE id (0/1/2), not a payload field
    //   mHandle   = the gameplay handle  (event @+64/+68 -> copied to Trigger@+8)
    //   mTransform= the world transform  (event @+0..+63; W column == position)
    //   mfRadius / dimensions @ event +80 (sphere radius / box dimensions)
    // FLAG: the precise interior offsets of the add-event payload are reconstructed from the asm
    // vector loads; the field set below is the load-bearing subset the producer reads.
    struct InAddTriggerEvent : public CgsModule::Event
    {
        Matrix44Affine mTransform;     // +0x00  world transform (position == W column)
        u32            mHandle;        // +0x40  gameplay trigger handle (event @+64)
        u32            mHandleHi;      // +0x44  handle high word (event @+68)
        // +0x50 dimension/radius lanes (the X360 reads *(event+80) as the sphere radius / the box
        // & plane dimensions). Modelled as three explicit floats so each case reads a distinct
        // lane without depending on rw::Vector3 lane accessors. FLAG: the per-lane mapping
        // (radius == X lane; box dims == X/Y/Z) is reconstructed from the asm; not byte-verified.
        f32            mfDimX;         // +0x50  (sphere radius / box+plane X dimension)
        f32            mfDimY;         // +0x54  (box+plane Y dimension)
        f32            mfDimZ;         // +0x58  (box Z dimension)

        Matrix44Affine GetTransform() const     { return mTransform; }
        u32            GetTriggerHandle() const  { return mHandle; }
        f32            GetRadius() const         { return mfDimX; }   // sphere radius == X lane
        f32            GetDimensionX() const     { return mfDimX; }
        f32            GetDimensionY() const     { return mfDimY; }
        f32            GetDimensionZ() const     { return mfDimZ; }
    };

    // ADDITIVE GROW (this TU -- BrnWorld::TriggerEntityModuleIO::TriggerManagementInputInterface::
    // AddTriggerRegion @ X360 0x8238ECF8): the concrete BOX-trigger element pushed onto the add
    // queue as event type 2. The producer builds this exact 96-byte record on the stack, then calls
    // VariableEventQueue<131072,16>::AddEvent<InAddBoxTriggerEvent>(this, &event, 2) (the mangled
    // AddEvent<...InAddBoxTriggerEvent...> in the asm). Field offsets are the X360 stack stores:
    //   +0x00 mTransform   -- BoxRegion::ComputeTransform() result (4 vectors, stvx128 x4)
    //   +0x40 mPacked...   -- (queryFlags << 24) | (sign-extended TriggerRegion::GetRegionIndex())
    //   +0x44 muTriggerType-- TriggerRegion::meType byte (lbz 0x2A)
    //   +0x45 muSubType    -- 0xFF when meType == E_TYPE_GENERIC_REGION, else GenericRegion::meType
    //                         byte (lbz 0x36) -- reproduces the asm's branch exactly.
    //   +0x50 mDimensions  -- abs(BoxRegion dimensions): X360 vandc clears each lane's sign bit.
    // sizeof == 0x60 (the compile-time size AddEvent<InAddBoxTriggerEvent> forwards as liSize).
    struct InAddBoxTriggerEvent : public CgsModule::Event
    {
        Matrix44Affine mTransform;                // +0x00  box world transform
        u32            mPackedQueryFlagsAndIndex;  // +0x40  (queryFlags<<24) | regionIndex
        u8             muTriggerType;              // +0x44  TriggerRegion::meType
        u8             muSubType;                  // +0x45  generic sub-type / 0xFF
        u8             maPad[10];                  // +0x46..+0x4F  (pad to the 16-byte lane)
        Vector3        mDimensions;                // +0x50  abs box dimensions
    };

    // ADDITIVE GROW (Process* TU): element record of the trigger-query input queue
    // (CgsModule::VariableEventQueue<4096,16>). X360 ProcessTriggerQueryEvents (0x82306BB0) reads,
    // for an E_TRIGGERQUERY_LINETEST (event type 3):
    //   mQueryId          = event @ +0   (lwz  r31,0(r30))           -> InEventLineTestFine.mQueryId
    //   mxVolumeTypeFlags = event @ +4   (lbz  r29,4(r30)) one byte  -> InEventLineTestFine.mxVolumeTypeFlags
    //   mLineStart        = event @ +16  (lvx128 r30,r19  r19=0x10)  -> InEventLineTestFine.mLineStart
    //   mLineEnd          = event @ +32  (lvx128 r30,r25  r25=0x20)  -> InEventLineTestFine.mLineEnd
    // FLAG: interior offsets reconstructed from the asm loads (lwz/lbz/lvx128 displacements).
    struct InTriggerQueryEvent : public CgsModule::Event
    {
        CgsSceneManager::SceneQueryId mQueryId;          // +0x00
        u8                            mxVolumeTypeFlags; // +0x04  (volume-type filter byte)
        u8                            maPad0[11];        // +0x05..+0x0F  (pad to the 16-byte line lanes)
        Vector3                       mLineStart;        // +0x10
        Vector3                       mLineEnd;          // +0x20

        CgsSceneManager::SceneQueryId GetQueryId() const        { return mQueryId; }
        u8                            GetVolumeTypeFlags() const { return mxVolumeTypeFlags; }
        Vector3                       GetLineStart() const       { return mLineStart; }
        Vector3                       GetLineEnd() const         { return mLineEnd; }
    };

    // ADDITIVE GROW (VariableEventQueue<4096,16> finish TU): the producer-side element record
    // that BrnGameState::TriggerQueryManager::SubmitTriggerQueries pushes onto the module's
    // trigger-query input queue via
    //   CgsModule::VariableEventQueue<4096,16>::AddEvent<InLineTestEvent>(&event, type)
    // (X360 0x8238E960 -- the typed AddEvent tail-forwards `li r6, 0x30` == sizeof == 48 to the
    // three-arg AddEvent). This is one of the "sibling LineTest events" the header preamble notes
    // also live in this DWARF home; only the exact size (48) is attested from THIS TU's exports
    // (the typed-AddEvent stub does not touch the payload interior -- it only memcpy's sizeof
    // bytes). The consumer ProcessTriggerQueryEvents reads the wire record with the same field
    // layout documented on InTriggerQueryEvent above (mQueryId @+0, volume-flags @+4, line start
    // @+16, line end @+32); InLineTestEvent is almost certainly that same 48-byte record under its
    // producer-side name. FLAG: interior field offsets are NOT reconstructed here -- they belong to
    // the SubmitTriggerQueries / TriggerQueryManager producer TU (whose asm is not in this TU's
    // exports); modelled as an opaque 48-byte payload so sizeof is pinned exactly without
    // fabricating member names/offsets. Grow into named fields (mirroring InTriggerQueryEvent) when
    // that producer TU lands.
    struct InLineTestEvent : public CgsModule::Event
    {
        u8 maOpaque[48];   // FLAG: 48 == X360-attested sizeof (li r6,0x30); interior deferred to producer TU
    };

    // ADDITIVE GROW (Process* TU): the management input interface aggregate -- the add + remove
    // trigger queues the world bridge fills each frame. The X360 ProcessAddTriggerEvents drains
    // GetAddTriggerEventQueue() (the first embedded member, a VariableEventQueue<131072,16>).
    // ADDITIVE GROW 2 (WorldBridgeInputToEntityModules TU): the remove queue
    // (EventQueue<InRemoveTriggerEvent,256>) is X360-pinned at +131088 -- immediately after the
    // add queue (sizeof(VariableEventQueue<131072,16>) == 16-byte header + 131072 buffer), the
    // offset BridgeInputToEntityModules @0x827ADF88 adds (`addis/ori 0x20010`) before calling
    // BaseEventQueue<InRemoveTriggerEvent>::Append on it. Append() below reproduces the bridge's
    // inlined X360 whole-interface merge (VariableEventQueue<131072,16>::Append<131072,16> on the
    // add queues, then the InRemoveTriggerEvent queue Append at +131088).
    class TriggerManagementInputInterface
    {
    public:
        typedef CgsModule::VariableEventQueue<131072, 16>           AddTriggerQueue;
        typedef CgsModule::EventQueue<InRemoveTriggerEvent, 256>    RemoveTriggerQueue;

        const AddTriggerQueue& GetAddTriggerEventQueue() const { return mAddTriggerEventQueue; }
        AddTriggerQueue&       GetAddTriggerEventQueue()       { return mAddTriggerEventQueue; }

        const RemoveTriggerQueue& GetRemoveTriggerEventQueue() const { return mRemoveTriggerEventQueue; }
        RemoveTriggerQueue&       GetRemoveTriggerEventQueue()       { return mRemoveTriggerEventQueue; }

        // X360 0x8238ECF8: build one InAddBoxTriggerEvent from a trigger region + a query-flags
        // byte and post it onto the embedded add queue (event type 2). Body in
        // BrnTriggerEntityModuleInputInterface.cpp. The X360 passes `this` straight to
        // AddEvent<InAddBoxTriggerEvent> -- mAddTriggerEventQueue is at interface +0, so the call
        // lands on the embedded queue.
        void AddTriggerRegion(s32 liQueryFlags, const BrnTrigger::TriggerRegion* lpRegion);

        // X360 header-inline (BridgeInputToEntityModules @0x827AE52C/@0x827AE540): merge the
        // source interface's add queue then its remove queue into this one.
        void Append(const TriggerManagementInputInterface& lrSource)
        {
            mAddTriggerEventQueue.Append(lrSource.mAddTriggerEventQueue);
            mRemoveTriggerEventQueue.Append(lrSource.mRemoveTriggerEventQueue);
        }

    private:
        AddTriggerQueue    mAddTriggerEventQueue;      // +0      (X360 GetFirstEvent target)
        RemoveTriggerQueue mRemoveTriggerEventQueue;   // +131088 (X360 bridge Append target)
    };
}
}
