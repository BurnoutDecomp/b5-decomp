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

#include "GameSource/World/EntityModules/TriggerEntityModule/BrnTriggerQueryId.h"
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

    // DWARF h:55..79; ARTIST822D9428..943C reads the base fields;
    // all three payloads begin at80 (radius822D95B8, box822D94C8,
    // plane822D9700). Matrix alignment pads the base to80 bytes.
    struct InAddTriggerEvent : public CgsModule::Event
    {
        Matrix44Affine mTransform;
        TriggerId mTriggerID;
        s8 miTriggerRegionType;
        s8 miGenericRegionType;
    };
    struct InAddPlaneSegmentTriggerEvent : public InAddTriggerEvent
    {
        Vector2 mDimensions;
        bool mbIsBidirectional;
    };
    struct InAddSphereTriggerEvent : public InAddTriggerEvent
    {
        f32 mfRadius;
    };
    struct InAddBoxTriggerEvent : public InAddTriggerEvent
    {
        Vector3 mDimensions;
    };
    static_assert(sizeof(InAddTriggerEvent) == 80, "trigger event base");
    static_assert(sizeof(InAddBoxTriggerEvent) == 96, "box trigger event");

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

    // DWARF h:83..88; ARTIST SubmitTriggerQueries82392844..82392858.
    struct InLineTestEvent : public CgsModule::Event
    {
        typedef u8 TriggerTypeFlags;
        BrnWorld::TriggerQueryId mQueryID;
        TriggerTypeFlags mTriggerTypeFlags;
        Vector3 mLineStart;
        Vector3 mLineEnd;
    };
    static_assert(sizeof(InLineTestEvent) == 48, "line query payload");

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

        // ADDITIVE GROW 3 (BridgeGameStateToWorld wave, 2026-08-01): drop one armed region by
        // posting an InRemoveTriggerEvent onto the embedded remove queue. X360 header-inline --
        // the TriggerQueryManager::UpdateTriggers loop @0x823923C4 AddEvents straight onto
        // interface+131088, which is mRemoveTriggerEventQueue. Re-homed here from the local
        // 16-byte GameStateModuleIO::TriggerManagementInputInterface placeholder that used to
        // declare it (that placeholder is now a typedef of THIS type); its only caller,
        // BrnTriggerQueryManager.cpp:214, is not mounted yet, so this keeps that TU compiling
        // against the real aggregate instead of silently losing the method in the retype.
        void RemoveTrigger(const InRemoveTriggerEvent& lrRemoveEvent)
        {
            mRemoveTriggerEventQueue.AddEvent(lrRemoveEvent);
        }

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
