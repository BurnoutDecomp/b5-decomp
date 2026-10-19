# GameState source dependency handoff at0f2a6f374

Private checkout: scratch/DEBUG_MENU_1011/recovery_lanes/vehicle-deformation-131. Branch codex/debug-menu-gamestate-caller is pushed at0f2a6f374c72aa2e5b423767dd9e4ac75ed687e0. Tracked tree is clean; inherited unknown scratch remains untouched. This worker ends the turn here for Root's combined review/integration; no new source slice or activation is started.

## Coherent increments after frozen df336bbff

| Commit | Recovered source | Narrow checks and frozen packet |
| --- | --- | --- |
| 6a156cc8c0ccc652ae994d255260170380460f6a | GetFlybyManager selector, original count virtual dispatch, original online StartModeIntro roster copy | 5 canonical TUs; captured symbols resolved; flyby_dispatch_checkpoint |
| 8fb6acda5a39bafbd6cd64bd57ec1bf8fad1c600 | Original DoUpdate_World read-locked ImmediateStreamed inversion into existing streaming-stall request | 2 canonical TUs; const status accessor resolved; world_stall_latch_checkpoint |
| 0f2a6f374c72aa2e5b423767dd9e4ac75ed687e0 | Original QueryRequestDoStepFrame and shared single online latch across existing partfiles | 3 canonical TUs; query/latch/pad helper resolved; frame_step_query_checkpoint |

All commits carry the verified Codex GPT-6.1 Sol trailer. Each packet has exact pre/post images, raw primary asm, compile logs, scoped faithfulness delta (zero new findings), preservation receipts, source diff and a SHA256 manifest. No whole-program link or native run occurred. Current increment's only extra owner was approved BrnGameModule_wN1_02.cpp; no further owner expansion is proposed.

## Actual path and remaining connections

Original DoUpdate_GameStatePreWorld823EE0E8 constructs transient GameStatePreWorld input, handles frame-step and streaming pause events, bridges controller/network input under locks, skips video or disk-error updates as established, invokes full GameStateModule::PreWorldUpdate823A5328, and drains CheckGameActions823C54D8 (original actions86/87 with timer state). The original caller/body and full PreWorld are recovered. The selected GameMain still calls DoUpdate_GameStatePreWorldNativePC (BrnGameModule.cpp:4853); ResourceUpdateThread still calls PrepareNativePC. These choices are unchanged.

| Supporting control/provider | Original witness and current source | Remaining prerequisite before claiming the behavior |
| --- | --- | --- |
| Debug/Sim Step/Play and pad step/play chord | StepFrameCB823A8F68, PlayFrameCB823A8F88, UpdateRequestDoStepFrame823A8FB0 already exist. New Query823C5580 skips online before locking, uses the real pad helper, tests32-bit action21 pressed then57 held then56 held, and latches the appropriate existing request. NetworkPreSim823C5998 writes the same original byte82FAE28D. | InputPreWorld823C5808 originally invokes Query; that call remains unactivated. Original GameStatePreWorld's frame-step pause-event handling remains bypassed by NativePC. Menu callbacks alone are not full frame-step parity proof. |
| Show stall message / streaming pause | DoUpdate_World823E8D70..823E8DA0 now read-locks world output, reads GetImmediateStreamed and stores its inverse to mbWorldStreamingRequestedStall. Original GameStatePreWorld reads this request and sbShowStreamStallMessage. | Selected NativePC flow bypasses both original top-level callers, so the source latch and consumer are ready but their selected-route connection remains absent. |
| Landmark debug / Draw arrow to next landmark | Full PreWorld invokes GameStateDebugComponent::RenderLandmarkDebug at823A5F08 (current GSM.cpp:619). Registrations/read bodies already exist. | NativePC partial pump does not invoke this full debug-render leg. |
| Flyby roster/count and original Prepare | Original Prepare8239E578..8239ED10 has28cases, original DONE27 and meReleaseStage tail. GetNumberOfCarsInFlyby82311E38 dispatches slot1; StartModeIntro82343220..264 dispatches slot0 and copies592-byte typed payload. | Full PreWorld823A5734..5744 updates OnlineFlybyManager::mPlayerInputInterface (current GSM.cpp:484) before ModeManager/Intro; NativePC partial pump does not. NativePC Prepare is still selected. Current Native Construct lacks the original offline Construct seat8238061C..634 even though its canonical body now exists. Full original lifecycle readiness must be reviewed before activation. |

The normal world status producer exists: DriveWorldUpdateFrame uses WorldModule::Update outside video (BrnGameMainFlowStates.cpp:703); normal Update reaches EntityModulePostPhysicsUpdate, then WorldEntityModule::PostPhysicsUpdate823080F0, whose SetImmediateStreamed stores reflect world/zone residency and the streaming setting; BridgeEntityModulesToOutput_PostPhysics copies the result to world output. UpdateForBootUpVideo is a separate branch. An earlier NativePC comment claiming the normal producer is absent is stale. The missing selected-route connection here is the original game-module latch/consumer, not a new world-status producer.

End Current Event is not classified as bypassed: FinshMode writes mbFinishCurrentModeNextUpdate; the existing ModeManager::PreWorldUpdate reader at BrnModeManager_WorldTick.cpp:618 is reached by the NativePC stunt pump when unpaused. No causal claim about the historical F2 transition is made. ChallengeManager and HUDMessageLogic PreWorld bodies are also already called despite stale parked-function banners. Four other GSM debug flags have registrations/seeds but no current C++ readers; the frozen direct-immediate ARTIST probe is candidate evidence, not a complete alias analysis or an original no-op verdict.

Root owns combined fresh review, integration, constructor/lifecycle readiness decisions, full link/build, activation and behavior-specific native witnesses. NativePC pump/startup/Prepare/banks/budgets remain preserved. No ledger, mounts, tests, parent tree, shared IDBs or game process was changed.

## Additional original Traffic evidence delivered

- scratch/DEBUG_MENU_1011/traffic_reader_holes_20261014: complete sub_82881250 enclosing both requested82881324/82881338 sites, single288-byte chunk/72instructions, raw chunks/calls/xrefs and completeness/source-attribution receipts. It is unsigned-short Message::PackOrUnpack evidence; no direct82F30000 ref is established and caller-pointer provenance remains unresolved.
- scratch/DEBUG_MENU_1011/traffic_initializer_82004A24_20261014: loaded `.rdata` datum bytes43 7A 00 00, original=current, big-endian float32=250.0,36-byte context identical and no patches. Raw binaries and byte/segment/xref receipts are frozen. IDA retained original bytes are the witness; the private database has no input filename/file-region mapping for a fresh executable-file comparison.
