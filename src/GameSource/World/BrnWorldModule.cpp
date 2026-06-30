// ============================================================================
// b5-decomp/src/GameSource/World/BrnWorldModule.cpp
//
// BrnWorld::WorldModule -- the World MODULE spine. See BrnWorldModule.h for the
// full scope/FLAG rationale.
//
// The X360 TU "GameSource/Unity/../World/BrnWorldModule.cpp" has 13 functions:
//   Construct, Destruct, EntityModulePostSceneUpdate, EntityModulePrePhysicsUpdate,
//   ExternalSceneQueriesUpdate, GenerateDispatchLists, GenerateFrustumQueries,
//   GenerateShadowMapDispatchLists, HandleGameActions, LoadDistrictMap, Prepare,
//   Release, UpdatePhysicsNetworkCatchup.
//
// BODIED (1):  LoadDistrictMap  -- faithful, through this TU's own named members +
//              committed deps (RequestInterface<4096>::LoadBundle, VariableEventQueue
//              <4096,16>::AddEvent, CgsResource::ID::HashString, the receiver-queue
//              accessors). All branches/stores/early-outs mirror X360 0x827D11D8.
//
// DECLARATION-ONLY + FLAG (12):  every other dossier function reaches a genuinely
//              un-homed dependency -- either it indexes the embedded sub-module fleet by
//              raw offset (Construct @0x827CF540, Destruct @0x827BD0F0, Release @0x827BCE58,
//              ExternalSceneQueriesUpdate @0x827B06C8, UpdatePhysicsNetworkCatchup @0x827B06E0,
//              EntityModulePostSceneUpdate @0x827C3C58, EntityModulePrePhysicsUpdate @0x827BD5B8,
//              HandleGameActions @0x827C44D8 -- the latter two also call the [todo] Bridge*
//              helpers that live in their own TUs), or it is a multi-stage VMX/VPU pipeline
//              (GenerateDispatchLists @0x827D1CE8, GenerateFrustumQueries @0x827DADF8,
//              GenerateShadowMapDispatchLists @0x827C96D8). Per AGENTS.md these are NOT
//              paraphrased to scalar and NOT poked by raw offset into committed aggregates;
//              they are recorded here with their X360 address + the exact reason they are
//              blocked, to be bodied once their sub-module/IO deps are homed.
// ============================================================================
#include "GameSource/World/BrnWorldModule.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"                      // CGS_ASSERT
#include "GameShared/GameClasses/Module/CgsVariableEventQueue.h"        // VariableEventQueue<4096,16>::AddEvent
#include "GameShared/GameClasses/System/Resource/CgsResourceID.h"       // CgsResource::ID::HashString
#include "GameSource/Resource/SharedIO/BrnGameDataRequestQueue.h"       // BrnResource::GameDataIO::RequestInterface<4096>

namespace BrnWorld
{
    // ------------------------------------------------------------------------
    // FLAG (minimal slice): UpdateOutputBuffer accessor surface LoadDistrictMap forwards
    // through. The real BrnWorldIO::UpdateOutputBuffer + its
    // GetResourceRequestResourceInterface() body are grown by the IO TU (a separate
    // [todo] ledger key); modelled here as the single named entry point LoadDistrictMap
    // invokes, returning the request interface the X360 LoadDistrictMap pushes events onto.
    // The buffer is the per-frame OUTPUT IO-buffer; it was already write-locked by the
    // caller, so the accessor returns the live (already-locked) request interface.
    // ------------------------------------------------------------------------
    struct UpdateOutputBuffer
    {
        // X360: BrnWorldIO::UpdateOutputBuffer::GetResourceRequestResourceInterface(this).
        BrnResource::GameDataIO::RequestInterface<4096>* GetResourceRequestResourceInterface();

        // Lock surface (the X360 LockForWrite/UnlockForWrite the state machine wraps every
        // stage in). The real buffer derives CgsModule::IOBuffer; modelled here by name only.
        void LockForWrite();
        void UnlockForWrite();
    };

    // ========================================================================
    // WorldModule::LoadDistrictMap  @ X360 0x827D11D8   [BODIED]
    //
    // The Districts.dat streaming state machine. Drives meDistrictMapLoadStage through
    //   REQUEST -> RESPONSE -> ACQUIRE_REQUEST -> ACQUIRE_RESPONSE -> DONE,
    // returning false until DONE (stage 4). Each stage write-locks the output buffer,
    // does its one step, unlocks, and returns. (X360 wraps the whole switch in a single
    // LockForWrite/UnlockForWrite per stage -- mirrored exactly below.)
    // ========================================================================
    bool WorldModule::LoadDistrictMap(UpdateOutputBuffer* lpOutput)
    {
        CGS_ASSERT(lpOutput, "lpOutput");

        lpOutput->LockForWrite();

        switch (meDistrictMapLoadStage)
        {
            case E_DISTRICT_MAP_LOAD_REQUEST:
            {
                // Clear the response receiver queue, then push a LoadBundle("Districts.dat",
                // pool 5) request onto the output buffer's request interface.
                mReceiverQueue.Clear();
                BrnResource::GameDataIO::RequestInterface<4096>* lpRequest =
                    lpOutput->GetResourceRequestResourceInterface();
                lpRequest->LoadBundle(&mReceiverQueue, /*liEventId*/ 1, /*liPoolId*/ 5,
                                      "Districts.dat", /*lbUseHDCache*/ false);
                meDistrictMapLoadStage = E_DISTRICT_MAP_LOAD_RESPONSE;
                lpOutput->UnlockForWrite();
                return false;
            }

            case E_DISTRICT_MAP_LOAD_RESPONSE:
            {
                // Wait for the load to report at least one response event, then advance.
                if (mReceiverQueue.GetCount() <= 0)
                {
                    lpOutput->UnlockForWrite();
                    return false;
                }
                meDistrictMapLoadStage = E_DISTRICT_MAP_ACQUIRE_REQUEST;
                lpOutput->UnlockForWrite();
                return false;
            }

            case E_DISTRICT_MAP_ACQUIRE_REQUEST:
            {
                // Clear the queue and push a GetGameDataEvent (type 24) acquiring the loaded
                // "Districts" data from pool 5. The X360 builds the event payload on the stack
                // as { &mReceiverQueue, 1, 5 (pool), (HashString("Districts") | (5 << 32)) } and pushes
                // it via VariableEventQueue<4096,16>::AddEvent(payload, type=4, size=24).
                mReceiverQueue.Clear();
                BrnResource::GameDataIO::RequestInterface<4096>* lpRequest =
                    lpOutput->GetResourceRequestResourceInterface();

                // CONSOLE payload order (asm 0x827D11D8): v9[0]=&queue, v9[1]=1, v9[2]=5 (pool),
                // then the u64 v10 = HashString("Districts")|0x500000000 at sp+0x60. So poolId
                // lands at +8 and the u64 resourceId at +16 (NOT the reverse).
                struct AcquireEvent
                {
                    CgsModule::BaseEventReceiverQueue* mpReceiverQueue; // CONSOLE +0
                    s32                                miEventId;       // CONSOLE +4  (=1)
                    s32                                miPoolId;        // CONSOLE +8  (=5)
                    s32                                mi_pad;          // CONSOLE +12
                    u64                                muResourceId;    // CONSOLE +16 (id | pool<<32)
                } lEvent;
                lEvent.mpReceiverQueue = &mReceiverQueue;
                lEvent.miEventId       = 1;
                lEvent.miPoolId        = 5;
                lEvent.mi_pad          = 0;
                lEvent.muResourceId    =
                    static_cast<u64>(static_cast<u32>(CgsResource::ID::HashString(
                        reinterpret_cast<const u8*>("Districts"))))
                    | 0x500000000ULL;   // pool 5 in the high dword

                // The request interface's queue IS a VariableEventQueue<4096,16> (RequestQueue
                // <4096> -> ResourceRequestQueue<4096> -> VariableEventQueue<4096,16>); the X360
                // pushes the acquire event straight onto it via the 3-arg AddEvent.
                lpRequest->mRequestQueue.AddEvent(
                    reinterpret_cast<const CgsModule::Event*>(&lEvent), /*liType*/ 4, /*liSize*/ 24);

                meDistrictMapLoadStage = E_DISTRICT_MAP_ACQUIRE_RESPONSE;
                lpOutput->UnlockForWrite();
                return false;
            }

            case E_DISTRICT_MAP_ACQUIRE_RESPONSE:
            {
                // Wait for the acquire response, then capture the resolved resource handle from
                // the first response event's payload (X360 reads two dwords at payload + 24 into
                // mDistrictMapResourceHandle).
                if (mReceiverQueue.GetCount() <= 0)
                {
                    lpOutput->UnlockForWrite();
                    return false;
                }
                meDistrictMapLoadStage = E_DISTRICT_MAP_DONE;

                const CgsModule::Event* lpEventData = nullptr;
                s32 liSize = 0;
                // X360: v7 = (count>0) ? mpBuffer + miStartOffset + 8 : 0  (== the first event's
                // payload pointer) -- exactly what GetFirstEvent returns. The handle is at +24.
                mReceiverQueue.GetFirstEvent(&lpEventData, &liSize);

                const u32* lpPayload =
                    lpEventData ? reinterpret_cast<const u32*>(lpEventData) : nullptr;
                // FLAG: the +24 handle dwords live in the opaque LoadGameDataResponse payload
                // (external, forward-declared) -- read by attested offset (allowed per AGENTS.md).
                const u32* lpHandleWords =
                    reinterpret_cast<const u32*>(reinterpret_cast<const u8*>(lpPayload) + 24);
                mDistrictMapResourceHandle.mpResourceMemory =
                    reinterpret_cast<void*>(static_cast<uintptr_t>(lpHandleWords[0]));
                mDistrictMapResourceHandle.mpSourceEntry =
                    reinterpret_cast<CgsResource::Entry*>(static_cast<uintptr_t>(lpHandleWords[1]));

                lpOutput->UnlockForWrite();
                return false;
            }

            case E_DISTRICT_MAP_DONE:
            {
                lpOutput->UnlockForWrite();
                return true;
            }

            default:
            {
                CGS_ASSERT(false, "Unknown meDistrictMapLoadStage");
                lpOutput->UnlockForWrite();
                return false;
            }
        }
    }

    // ========================================================================
    // DECLARATION-ONLY + FLAG (12 functions). See the file banner for why each is blocked.
    // These are the EXACT functions in this TU's postmortem dossier; none is fabricated and
    // none is paraphrased. They will be bodied once their listed deps are homed.
    // ========================================================================

    // WorldModule::Construct @ 0x827CF540 -- FLAG: stores ~50 perf-monitor handles and
    // forwards Construct() to every embedded sub-module by raw object offset (+640 .. +6168096),
    // and memcpy's a 160-byte BrnCpuMonitors blob. All targets are un-homed sub-aggregates;
    // bodying it requires raw-offset pokes into committed aggregates (forbidden).

    // WorldModule::Destruct @ 0x827BD0F0 -- FLAG: virtual-dispatch Destruct() to each
    // sub-module by raw offset; un-homed sub-aggregates.

    // WorldModule::Release @ 0x827BCE58 -- FLAG: per-stage virtual Release() of each
    // sub-module by raw offset; un-homed sub-aggregates.

    // WorldModule::ExternalSceneQueriesUpdate @ 0x827B06C8 -- FLAG: tail-calls the scene
    // module's vtable+0x44 by raw offset (+2002304, SceneManagerModule); un-homed.

    // WorldModule::UpdatePhysicsNetworkCatchup @ 0x827B06E0 -- FLAG: forwards to
    // BrnPhysics::PhysicsModule::UpdateNetworkCatchup at raw offset +1561376; PhysicsModule
    // is un-homed at full layout.

    // WorldModule::EntityModulePostSceneUpdate @ 0x827C3C58 -- FLAG: orchestrates the
    // post-scene IO bridges across every entity module (calls the [todo] Bridge*_PostScene
    // helpers + per-module PostSceneUpdate by raw offset); un-homed sub-aggregates + helpers.

    // WorldModule::EntityModulePrePhysicsUpdate @ 0x827BD5B8 -- FLAG: same shape, the
    // pre-physics IO-bridge orchestration; un-homed sub-aggregates + [todo] Bridge* helpers.

    // WorldModule::HandleGameActions @ 0x827C44D8 -- FLAG: drains the GameActionQueue and
    // applies actions, then calls the [todo] BridgeActionsToPhysicsModule /
    // BridgeActionsToTrafficModule helpers and pokes per-active-race-car control state by raw
    // offset (+6167272/+6167280/+6167312); un-homed helpers + opaque control arrays.

    // WorldModule::GenerateDispatchLists @ 0x827D1CE8 -- FLAG: multi-stage VMX/VPU dispatch-list
    // builder (frustum filter + per-entity-module dispatch generation). NOT paraphrased.

    // WorldModule::GenerateFrustumQueries @ 0x827DADF8 -- FLAG: multi-stage VMX frustum-query
    // builder (per-camera plane setup feeding the scene manager). NOT paraphrased.

    // WorldModule::GenerateShadowMapDispatchLists @ 0x827C96D8 -- FLAG: multi-stage VMX
    // shadow-map dispatch builder reaching the un-homed BehaviourManager/ShadowMap aggregates.
    // NOT paraphrased; NOT raw-offset-poked into the behaviour-manager aggregate.

    // WorldModule::Prepare @ 0x827D53B0 -- FLAG: the real staged module-prepare entry point.
    // X360 signature: int Prepare(WorldModule* this, int, int, u8*, int) -- a multi-stage prepare
    // state machine switching on the prepare-stage word (EWorldPrepareStage) that initialises the
    // embedded sub-module fleet (RaceCar/Traffic/Prop/Trigger/World entity modules, Physics/Scene/
    // AI/Crash modules, environment manager) by raw offset into the un-homed sub-aggregates.
    // DECLARATION-ONLY (distinct from the no-arg CgsModule base Prepare() override in the header):
    // bodying requires forbidden raw-offset pokes into those un-homed sub-aggregates.
}
