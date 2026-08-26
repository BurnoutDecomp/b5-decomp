// ============================================================================
// b5-decomp/src/GameSource/World/Bridges/WorldBridgePropModule.cpp
//
// THE FIVE PROP BRIDGES. Wave Q landed the world-side break pipeline
// (PropEntityModule 22/22) and the physics-side PropManager, but every seam
// between them was an inert gate in WorldLinkStubs.cpp. This TU bodies the five
// WorldModule bridges that carry data INTO (and one OUT OF) the prop entity
// module:
//
//   BridgePhysicsModuleToPropModule_PostPhysics @0x827AB998  (41 insns)  ⭐ THE AV
//   BridgeSceneContactsToPropModule_PrePhysics  @0x827ABCB0  (32 insns)
//   BridgeCrashModuleToPropModule_PostScene     @0x827AAD78  (15 insns)
//   BridgePropModuleToTrafficModule_PrePhysics  @0x827AEA70  (41 insns)
//   BridgeAIToEntityModules_PrePhysics          @0x827AD540  (24 insns)
//
// (insn counts = (last_addr - first_addr)/4 + 1 over the exported listings.)
//
// ---- WHY THIS FILE EXISTS (the FILE-SPLIT precedent) -----------------------
// Each of the five has a DIFFERENT DWARF home TU (WorldBridgePhysicsToEntityModules.cpp,
// WorldBridgeSceneToEntityModules.cpp, WorldBridgeCrashToEntityModules.cpp,
// WorldBridgeEntityModulesToEntityModules.cpp, WorldBridgeAIToEntityModules.cpp) and each
// of those TUs carries OTHER, still-unreconstructed bridges. Landing these five in their
// homes would either mean editing four concurrently-owned TUs or waiting on bridges that
// have nothing to do with props. This is exactly the split the two RETIRED prop bridges
// already took (WorldBridgeWorldModuleToPropModule.cpp @0x827AACF8 and
// WorldBridgeRaceCarToPropModule.cpp @0x827A5510, both 2026-08-12), and this file follows
// their shape statement for statement.
// DELETE-WHEN: the four home TUs become mountable whole; move each body back then.
//
// Their declarations already live in the four home HEADERS (included below) -- nothing
// here re-declares anything.
//
// ---- LIVE vs PARKED (read this before believing the pipeline is closed) ----
// TWO legs carry their full console payload today. The remaining three are PARKED on the SAME
// root cause: the SOURCE buffer models the seat this bridge reads as opaque placeholder
// storage, or has no member there at all. A parked leg logs once and does nothing -- it never
// fabricates a transfer. Per-leg blockers are in each function's banner; the exact
// declarations the conductor must land are collected in scratchpad/waveQ4/bridges.owner.md
// and scratchpad/waveQ5/f2.owner.md.
//
//   LIVE   BridgePhysicsModuleToPropModule_PostPhysics -- ⭐⭐ BOTH LEGS, since 2026-08-19
//          (wave Q6/A3). The updated-prop-queue leg was parked while
//          PhysicsModuleIO::OutputBuffer modelled mPropManagerOutputInterface as 1-byte
//          opaque storage; wave Q6 cluster A1 promoted that seat to the real
//          BrnPhysics::Props::PropOutputInterface and made GetUpdatedProps() a header
//          inline, so the leg is the console's two calls with no cast. See that
//          function's banner.
//   LIVE   BridgeSceneContactsToPropModule_PrePhysics -- ⭐ UNPARKED 2026-08-19 (wave Q5/F2).
//          SceneManagerIO::OutputBuffer grew the real mPotentialContactQueue (+32800), the
//          read-lock accessor @0x8279C098 and the console's own Construct in wave Q5 round 2,
//          so the source seat this leg reads is a named member of the same type the
//          destination Append takes. See that function's banner.
//   PARKED BridgeCrashModuleToPropModule_PostScene -- BrnWorld::CrashModuleIO::
//          OutputBuffer_PostScene is `u8 maDeferredPayload[16]` (BrnCrashModule.h:75).
//   LIVE   BridgePropModuleToTrafficModule_PrePhysics -- ⭐ UNPARKED 2026-08-19 (wave Q6/C3).
//          The TRAFFIC side's PropToTrafficInterface was a 1-byte `{ u8 muDUMMY; }`
//          placeholder; it is the committed BrnWorld::PropEntityIO::PropToTrafficInterface
//          now (BrnTrafficEntityModuleIO.h), and BrnTrafficIO::InputBuffer_PrePhysics::
//          Construct grew the two queue legs the console's Construct @0x827615F8 runs at
//          +0x30C60 / +0x30CEC. Smashed traffic lights reach the traffic system again.
//   PARKED BridgeAIToEntityModules_PrePhysics -- sizeof(BrnAI::AIModuleIO::OutputBuffer) == 1
//          on the host while its accessors return this+98128. See that function's banner:
//          this park caught a live corruption that had already been written and compiled.
//
// ⚠️ THE MEASUREMENT THAT DECIDED THE PARKS -- run it before unparking anything, and before
// adding any accessor to a buffer that has no named members. The host sizeof of each source
// buffer, against the console offset the bridge reads:
//
//     PhysicsModuleIO::OutputBuffer          998224   contact spy @998192   IN BOUNDS  -> LIVE
//     PhysicsModuleIO::OutputBuffer          998224   updated props @71792  in bounds, and the
//                                                     seat is the REAL PropOutputInterface since
//                                                     wave Q6/A1 -> ⭐ LIVE (row kept as the
//                                                     record of what unparked it)
//     SceneManagerIO::OutputBuffer            32800   contacts   @32800     OUT OF BOUNDS
//                                                     -> ⭐ FIXED: the buffer is 201,824 B with
//                                                     a NAMED mPotentialContactQueue since wave
//                                                     Q5 round 2, so this row is LIVE now
//     CrashModuleIO::OutputBuffer_PostScene      16   racecar iface @143824 OUT OF BOUNDS
//     BrnAI::AIModuleIO::OutputBuffer             1   AI result  @98128     OUT OF BOUNDS
//     BrnTrafficIO::InputBuffer_PrePhysics   199777+  prop->traffic @199776 in bounds, and the
//                                                     seat is the REAL PropToTrafficInterface
//                                                     since wave Q6/C3 -> ⭐ LIVE
//
// (probe: scratchpad/waveQ4/probe_bridges/probe_sizes.cpp + probe_ai_size.cpp. The remaining
// out-of-bounds rows are not "missing accessors" -- they are buffers whose LIVE ALLOCATION is
// smaller than the offset, because CgsIOBufferStack::CreateIOBuffer<T> allocates sizeof(T).
// An accessor added over any of them returns a pointer into the next IO-stack tenant.)
//
// ---- LOCKING --------------------------------------------------------------
// Every caller brackets its buffers (BrnWorldModule.cpp:1679/1875/2141/2735 use
// CgsModule::LockBuffersForIO(dest, src) or the explicit LockForWrite/LockForRead pair),
// destination write-locked and source read-locked. That is why every source-side getter
// below is the READ-lock const form and every destination-side one the WRITE-lock form --
// the console picks the same overloads, and the lock-bit tripwires inside them are the
// check that the pairing is right.
//
// ---- THE PERF MONITOR -----------------------------------------------------
// BridgeAIToEntityModules_PrePhysics is the ONE bridge here whose console body dereferences
// the WorldModule `this`: it brackets itself with
// PerfMonCpu::Start/StopMonitor(*(u32*)(worldModule + 6167720)). The parameter is `void*` in
// this tree's bridge model, so the monitor is NOT modelled -- the identical disposition the
// committed WorldBridgeEntityModulesToOutput.cpp takes for the same +6167720 id on four of
// its own legs (see its :278 FLAG). Marked [FLAG] in that function.
// ============================================================================

#include "GameSource/World/Bridges/WorldBridgePhysicsToEntityModules.h"        // BridgePhysicsModuleToPropModule_PostPhysics
#include "GameSource/World/Bridges/WorldBridgeSceneToEntityModules.h"          // BridgeSceneContactsToPropModule_PrePhysics
#include "GameSource/World/Bridges/WorldBridgeCrashToEntityModules.h"          // BridgeCrashModuleToPropModule_PostScene
#include "GameSource/World/Bridges/WorldBridgeEntityModulesToEntityModules.h"  // BridgePropModuleToTrafficModule_PrePhysics
#include "GameSource/World/Bridges/WorldBridgeAIToEntityModules.h"             // BridgeAIToEntityModules_PrePhysics

#include "GameSource/Physics/PropManager/SharedIO/BrnPropOutputInterface.h"       // Props::PropOutputInterface (leg 1's source seat)
#include "GameSource/World/EntityModules/PropEntityModule/SharedIO/BrnPropToTrafficInterface.h" // PropEntityIO::PropToTrafficInterface (the traffic bridge's payload)

#include "GameShared/GameClasses/Core/CgsAssert.h"           // CGS_ASSERT
#include "GameShared/GameClasses/Development/Log/CgsLog.h"   // CgsDev::Log::gpDebugPrint
#include <stdlib.h>                                          // getenv ([DIAG] BRN_PROP_DIAG, host only)

namespace WorldModule
{

// ----------------------------------------------------------------------------
// Never called. Pins the structural facts the LIVE legs below rely on that the
// compiler does not already enforce.
//
// No console byte offsets are pinned: every field this TU touches is reached through
// its named accessor, and all four IO buffers involved deliberately depart from the
// console layout on x64 (8-byte pointers inside their event queues).
// ----------------------------------------------------------------------------
static void _AssertLayout()
{
    // BridgePhysicsModuleToPropModule_PostPhysics's contact-spy leg is ONE WORD on the
    // console (`lwz r11,0(src) ; stw r11,0(dest)`), which is the WHOLE interface: it holds
    // exactly one ContactSpyData*. Modelled below as a whole-struct assignment -- correct
    // only while that stays true. If a second member is ever recovered into
    // ContactSpyInterface, the console's single-word copy must be re-derived from the asm
    // before this assignment is widened.
    static_assert(sizeof(BrnPhysics::ContactSpy::ContactSpyInterface) == sizeof(void*),
                  "ContactSpyInterface is the single ContactSpyData* the X360 copies as one word");

    // ⭐ THE TRIPWIRE THAT KEEPS THE LIVE LEG HONEST, and the one that would have caught the
    // AI bridge's out-of-bounds source before it was written: the seat this bridge reads must
    // actually be INSIDE the object CgsIOBufferStack::CreateIOBuffer<T> allocates (it allocates
    // sizeof(T)). PhysicsModuleIO::OutputBuffer is a REAL layout with explicit padding, so its
    // contact-spy member is a named member and its host sizeof covers the console seat. The
    // three parked bridges' sources fail exactly this test today -- see the file banner's table.
    static_assert(sizeof(BrnPhysics::PhysicsModuleIO::OutputBuffer) > 998192,
                  "the physics output buffer must be large enough to hold mContactSpyInterface "
                  "(console +998192) -- CreateIOBuffer allocates sizeof(T)");

    // ⭐ THE UPDATED-PROP LEG'S "no cast anywhere" PROOF (wave Q6 cluster A3). The console
    // hands GetUpdatedProps()'s result straight to AppendUpdatedPropQueue with no conversion;
    // the two spellings must therefore be the SAME C++ type. A plain pointer assignment is a
    // compile-time type-identity test that no reinterpret_cast could ever silence -- if the
    // physics side's queue and the prop-entity side's queue ever diverge (different element
    // type, different capacity), this line stops compiling instead of the bridge quietly
    // shipping a cast over two different layouts.
    const BrnPhysics::Props::PropOutputInterface::UpdatePropEventQueue*      lpPhysicsSideQueue = 0;
    const BrnWorld::PropEntityIO::InputBuffer_PostPhysics::UpdatePropEventQueue* lpPropSideQueue =
        lpPhysicsSideQueue;
    (void)lpPropSideQueue;

    // The seat this leg reads must be a REAL member of the buffer CreateIOBuffer<T> allocates,
    // not a placeholder: the console's mPropManagerOutputInterface sits at +71792 and the whole
    // interface is 76,864 console bytes wide, so the host object has to cover it by name.
    static_assert(sizeof(BrnPhysics::Props::PropOutputInterface) > 41632,
                  "PropOutputInterface must be the real interface (its mUpdatedProps is at "
                  "console +41632), not a 1-byte placeholder");
}

// =================================================================================================
// ⭐⭐ WorldModule::BridgePhysicsModuleToPropModule_PostPhysics  @ 0x827AB998  (41 insns)
//
// THIS IS THE BRIDGE THE BOOT AV IS ABOUT. PropEntityModule::PostPhysicsUpdate ->
// ProcessContacts @0x822FA890 opens with
//     lpInput->GetContactSpyInterface()->GetPropContacts()
// and GetPropContacts asserts "mpData != NULL" (BrnContactSpyInterface.h:82/211) before
// dereferencing it. mpData arrives ONLY here: the physics module publishes its
// ContactSpyData aggregate into PhysicsModuleIO::OutputBuffer::mContactSpyInterface
// (PhysicsModule::BridgeSimulationToOutput @0x825B0448 calls SetData), and nothing else in
// the XEX copies that handle into the prop module's post-physics input. While this bridge
// was the inert gate at WorldLinkStubs.cpp:2725 the prop input kept Construct's NULL, so the
// assert fired and the dereference AV'd on the very first frame the prop module ticked.
//
// ---- The console body, instruction for instruction (0x827AB998..0x827ABA38) ----
//   r4 = lpPropInputBuffer_PostPhysics (dest), r5 = lpPhysicsModuleOutputBuffer (src);
//   r3 (the WorldModule `this`) is overwritten at 0x827AB9FC and never read.
//
//   if (!dest) FireAssert("lpPropInputBuffer_PostPhysics != NULL", <file>, 0x66 == 102)
//   if (!src)  FireAssert("lpPhysicsModuleOutputBuffer != NULL",   <file>, 0x67 == 103)
//   bl 0x8279F640  PhysicsModuleIO::OutputBuffer::GetPropManagerOutputInterface() const
//                                                      (read-lock, +71792, BrnPhysicsModuleIO.h:357)
//   addis r4,r11,1 ; addi r4,r4,-0x5D60                -- +0xA2A0 == 41632 into that interface,
//                                                         which IS Props::PropOutputInterface::
//                                                         mUpdatedProps (BrnPropOutputInterface.h),
//                                                         i.e. the INLINED GetUpdatedProps()
//   bl 0x827AA2D8  PropEntityIO::InputBuffer_PostPhysics::AppendUpdatedPropQueue
//   bl 0x8279F8E0  PhysicsModuleIO::OutputBuffer::GetContactSpyInterface() const
//                                                      (read-lock, +998192, BrnPhysicsModuleIO.h:369)
//   bl 0x827A1628  PropEntityIO::InputBuffer_PostPhysics::GetContactSpyInterface()
//                                                      (write-lock, +4, BrnPropEntityModuleIO.h:579)
//   lwz r11,0(src) ; stw r11,0(dest)                   -- the whole ContactSpyInterface: one pointer
//
// There are no other stores, no early-outs beyond the two asserts, and no loops. The tail
// `b __restgprlr_29` forwards the last call's register as an artifact; the logical return
// type is void.
//
// ⭐⭐ LEG 1 (updated-prop queue) IS LIVE -- UNPARKED 2026-08-19 (wave Q6 cluster A3), and it is
//    THE line the whole "smashed props do not visibly move" complaint hangs on. The park that
//    stood here named two blockers; BOTH were closed by wave Q6 cluster A1 in the seat's own
//    owning header, not worked around here:
//      (a) PhysicsModuleIO::OutputBuffer modelled mPropManagerOutputInterface as
//          `struct PropOutputInterfaceStorage { unsigned char maBytes[1]; }`. It is now
//          `typedef Props::PropOutputInterface PropOutputInterfaceStorage`
//          (BrnPhysicsModuleIO.h:112 / member :208), X360-attested by
//          PhysicsModuleIO::OutputBuffer::Construct @0x825ABB10, which runs
//          PropOutputInterface::Construct(this+71792) -- so the const getter returns a REAL,
//          CONSTRUCTED interface and there is no reinterpret_cast anywhere in this leg.
//      (b) Props::PropOutputInterface::GetUpdatedProps() const is a header inline now
//          (BrnPropOutputInterface.h:83, `{ return mUpdatedProps; }`) -- which is exactly what
//          the console emits: it has NO out-of-line symbol for it, it folds the accessor into
//          this bridge as `addis r4,r11,1 ; addi r4,r4,-0x5D60` (== +0xA2A0 == 41632 ==
//          offsetof(PropOutputInterface, mUpdatedProps)).
//    NO CAST: BrnPhysics::Props::PropOutputInterface::UpdatePropEventQueue and
//    BrnWorld::PropEntityIO::InputBuffer_PostPhysics::UpdatePropEventQueue are the SAME
//    instantiation, CgsModule::EventQueue<BrnPhysics::Props::UpdatePropEvent, 200>.
//    _AssertLayout below proves that at compile time by a plain pointer assignment.
//    ORDER: the console runs this leg FIRST and the contact-spy leg SECOND (0x827ABA00 before
//    0x827ABA1C); the two are independent, but the order is reproduced.
//    WHAT IT BUYS: PropManager::ReadUpdatedBodies fills mUpdatedProps every physics frame with
//    the post-solve transform/velocity/frozen state of every prop and part;
//    PropManager::OutputUpdatedProps publishes it into this buffer; this leg is the ONLY hop
//    that carries it into PropEntityModule::PostPhysicsUpdate -> UpdateProps ->
//    PropZoneManager::UpdateInstance, whose part arm is the single writer of
//    PropPartInstance::mWorldTransform -- the matrix BrnPropEntityModule_Render.cpp:774 draws.
//
// ⭐ LEG 2 (contact spy) IS LIVE. It needed one ADDITIVE declaration that this tree was
//    already carrying a written TODO for: WorldBridgePhysicsToEntityModules.cpp's own banner
//    says of the same accessor "the console's read-locked const twin is @0x8279F8E0; adding
//    and bodying it is a two-line follow-up". That follow-up is landed with this wave
//    (BrnPhysicsModuleIO.h + BrnPhysicsModuleIO_OutputBuffer.cpp; declaration + body only, no
//    layout and no existing signature touched).
// =================================================================================================
void BridgePhysicsModuleToPropModule_PostPhysics(
    void* lpWorldModule,
    BrnWorld::PropEntityIO::InputBuffer_PostPhysics* lpPropInputBuffer_PostPhysics,
    const BrnPhysics::PhysicsModuleIO::OutputBuffer* lpPhysicsModuleOutputBuffer)
{
    (void)lpWorldModule;   // X360 r3 -- overwritten at 0x827AB9FC, never read

    CGS_ASSERT(lpPropInputBuffer_PostPhysics != 0, "lpPropInputBuffer_PostPhysics != NULL");  // :102
    CGS_ASSERT(lpPhysicsModuleOutputBuffer != 0, "lpPhysicsModuleOutputBuffer != NULL");      // :103

    // ---- LEG 1 -- the updated-prop queue (0x827ABA00 GetPropManagerOutputInterface const,
    // then the folded +0xA2A0 GetUpdatedProps, then 0x827ABA14 AppendUpdatedPropQueue).
    const BrnPhysics::Props::PropOutputInterface::UpdatePropEventQueue& lrUpdatedProps =
        lpPhysicsModuleOutputBuffer->GetPropManagerOutputInterface()->GetUpdatedProps();

    // [DIAG] NOT IN THE X360 BINARY. Opt-in one-shot behind BRN_PROP_DIAG: the single line that
    // says the physics side's per-prop pose stream reached the world module for the first time.
    // Fires on the FIRST frame the source queue is non-empty and never again, so a silent log
    // means the producer (PropManager::OutputUpdatedProps @0x82627EC8, wave Q6 cluster A2) is
    // still an inert conductor gate -- a different failure from "the bridge dropped the data".
    // The getenv latch is a function-local static const evaluated ONCE (a per-frame getenv
    // would be a per-frame syscall).
    {
        static const bool sbPropDiag = ( getenv( "BRN_PROP_DIAG" ) != 0 );
        static bool       sbLoggedFirstUpdatedProps = false;
        if ( sbPropDiag && !sbLoggedFirstUpdatedProps
             && lrUpdatedProps.GetLength() > 0
             && CgsDev::Log::gpDebugPrint != 0 )
        {
            sbLoggedFirstUpdatedProps = true;
            *CgsDev::Log::gpDebugPrint
                << "[Q6-bridge] first " << lrUpdatedProps.GetLength()
                << " UpdatePropEvents -> prop post-physics input\n";
        }
    }

    lpPropInputBuffer_PostPhysics->AppendUpdatedPropQueue(&lrUpdatedProps);   // @0x827AA2D8

    // ---- LEG 2 -- the contact-spy handle. The console copies the interface's single
    // ContactSpyData* word; on the host that word is the whole struct, so this is the
    // struct assignment (_AssertLayout above pins that equivalence).
    *lpPropInputBuffer_PostPhysics->GetContactSpyInterface() =
        *lpPhysicsModuleOutputBuffer->GetContactSpyInterface();
}

// =================================================================================================
// WorldModule::BridgeSceneContactsToPropModule_PrePhysics  @ 0x827ABCB0  (32 insns)
//
// The scene manager's broad-phase result for props. PropEntityModule::PrePhysicsUpdate ->
// ProcessPotentialContacts drains PropEntityIO::InputBuffer_PrePhysics::mPotentialContactQueue;
// this bridge is its ONLY producer.
//
// ---- The console body (0x827ABCB0..0x827ABD2C) -----------------------------
//   r4 = lpPropInputBuffer_PrePhysics (dest), r5 = lpSceneModuleOutputBuffer (src);
//   r3 (the WorldModule `this`) is overwritten at 0x827ABD14 and never read.
//
//   if (!dest) FireAssert("lpPropInputBuffer_PrePhysics != NULL", <file>, 0xD1 == 209)
//   if (!src)  FireAssert("lpSceneModuleOutputBuffer != NULL",    <file>, 0xD2 == 210)
//   bl 0x8279C098  SceneManagerIO::OutputBuffer::<potential-contact getter> const
//                  -- read-lock (bit 4, "Not locked for reading"), baked line 0x271 == 625,
//                     tail `addis r3,this,1 ; addi r3,r3,-0x7FE0` == this + 0x8020 == +32800
//   bl 0x827AA170  PropEntityIO::InputBuffer_PrePhysics::AppendPotentialContactQueue
//
// That is the whole function: two asserts and one queue Append.
//
// ⭐⭐ UNPARKED 2026-08-19 (wave Q5 cluster F2). The park that used to stand here said
//    "CgsSceneManager::SceneManagerIO::OutputBuffer has no potential-contact queue member at
//    all" -- true when it was written, STALE since wave Q5 round 2. That buffer now carries
//    all five DWARF members, its Construct is the console's own body (X360 0x828C7CA0, which
//    runs EventQueue<PotentialContact,2048>::Construct at +32800), and the read-lock accessor
//    the console calls here IS declared:
//        SceneManagerIO::OutputBuffer::GetPotentialContactQueue() const   (CgsSceneManagerIO.h)
//        == X360 0x8279C098, bit-4 read tripwire, baked CgsSceneManagerModuleIO.h:625,
//           `return this + 32800`
//    and it returns EXACTLY the type the destination takes:
//        PropEntityIO::InputBuffer_PrePhysics::AppendPotentialContactQueue(
//            const OutPotentialContactQueue*)                (BrnPropEntityModuleIO.h:960)
//    -- both spell CgsModule::EventQueue<SceneManagerIO::PotentialContact, 2048>, so there is
//    no cast anywhere in this leg. The destination Append is REAL and MOUNTED
//    (BrnPropEntityModuleIO_InputBuffer_PrePhysics.cpp, X360 0x827AA170).
//
//    LOCKING: the console's read-lock tripwire fires on the SOURCE and the write-lock one on
//    the DESTINATION -- which is exactly how WorldModule::EntityModulePrePhysicsUpdate
//    @0x827BD5B8 brackets this call, so no lock handling belongs in the bridge itself.
//
//    NOTE the parameter-name divergence, recorded rather than resolved: the console's own
//    assert string names the source `lpSceneModuleOutputBuffer`, while the committed
//    declaration in WorldBridgeSceneToEntityModules.h names it `lpSceneContactsFromWorld`.
//    The assert string is ground truth for the original identifier; the declaration is not
//    edited by this TU, so the definition below uses the console's name.
// =================================================================================================
void BridgeSceneContactsToPropModule_PrePhysics(
    void* lpWorldModule,
    BrnWorld::PropEntityIO::InputBuffer_PrePhysics* lpPropInputBuffer_PrePhysics,
    const CgsSceneManager::SceneManagerIO::OutputBuffer* lpSceneModuleOutputBuffer)
{
    (void)lpWorldModule;   // X360 r3 -- overwritten at 0x827ABD14, never read

    CGS_ASSERT(lpPropInputBuffer_PrePhysics != 0, "lpPropInputBuffer_PrePhysics != NULL");  // :209
    CGS_ASSERT(lpSceneModuleOutputBuffer != 0, "lpSceneModuleOutputBuffer != NULL");        // :210

    const CgsSceneManager::SceneManagerIO::OutputBuffer::OutPotentialContactQueue* const
        lpPotentialContacts = lpSceneModuleOutputBuffer->GetPotentialContactQueue();  // @0x8279C098

    // [DIAG] NOT IN THE X360 BINARY. Opt-in one-shot: set BRN_PROP_DIAG to see whether the
    // scene middle ever produced a car-vs-prop broad-phase pair at all. It fires on the FIRST
    // frame the source queue is non-empty and never again, so a silent log means the producer
    // (SceneManagerModule::BridgeOverlapCullerToOutputBuffer @0x828BA8C8, round 4 cluster F1)
    // is still inert -- which is a different failure from "the bridge dropped the data".
    // The latch is evaluated ONCE: getenv per frame would be a per-frame syscall.
    {
        static const bool sbPropDiag = ( getenv( "BRN_PROP_DIAG" ) != 0 );
        static bool       sbLoggedFirstContacts = false;
        if ( sbPropDiag && !sbLoggedFirstContacts
             && lpPotentialContacts->GetLength() > 0
             && CgsDev::Log::gpDebugPrint != 0 )
        {
            sbLoggedFirstContacts = true;
            *CgsDev::Log::gpDebugPrint
                << "[Q5-world] first " << lpPotentialContacts->GetLength()
                << " potential contacts -> prop module\n";
        }
    }

    lpPropInputBuffer_PrePhysics->AppendPotentialContactQueue(lpPotentialContacts); // @0x827AA170
}

// =================================================================================================
// WorldModule::BridgeCrashModuleToPropModule_PostScene  @ 0x827AAD78  (15 insns)
//
// The crash module's "this race car has finished crashing" notifications, handed to the prop
// module so it can retire the props that car destroyed.
//
// ---- The console body (0x827AAD78..0x827AADB0) -----------------------------
//   r4 = lpPropInputBuffer_PostScene (dest), r5 = lpCrashOutputBuffer_PostScene (src);
//   r3 (the WorldModule `this`) is overwritten at 0x827AAD88 and never read.
//   ⚠️ NO ASSERTS. This bridge really has none -- do not add the null guards its four
//   siblings carry; the console body is a straight two-call sequence.
//
//   mr r3,r5 ; bl 0x827A2530   <race-car output-interface getter> const
//                              -- read-lock (bit 4), baked line 0x88 == 136,
//                                 tail `addis r3,this,2 ; addi r3,r3,0x31D0` == this + 0x231D0
//   addi r3,r31,8              -- dest + 8 == InputBuffer_PostScene::mRaceCarCrashCompleteEventQueue
//                                 (that buffer's Construct @0x822EFC40 pins the member at +8),
//                                 i.e. the INLINED AppendRaceCarCrashQueue()
//   bl 0x827A7D70              EventQueue<CrashIO::RaceCarCrashCompleteEvent,10>::Append
//
// ⚠️ IDA NAMES 0x827A2530 `BrnWorld::CrashIO::OutputBuffer_PreScene::GetRa...`, but the call
// site's argument is EntityModulePostSceneUpdate's crash POST-SCENE output buffer. Both
// buffers place a race-car output interface at +0x231D0 behind an identical read-lock getter,
// so the two bodies are byte-identical and ICF folded them; IDA kept one of the two names.
// The declaration in WorldBridgeCrashToEntityModules.h (CrashModuleIO::OutputBuffer_PostScene)
// is the correct one and is NOT changed here. Recorded because the raw symbol is misleading.
//
// ⛔ PARKED. BrnWorld::CrashModuleIO::OutputBuffer_PostScene is
//    `struct { u8 maDeferredPayload[16]; }` (BrnCrashModule.h:75-78) -- a 16-byte placeholder
//    with no members, no accessor, and no race-car output interface. The +0x231D0 seat and
//    the RaceCarCrashCompleteEventQueue at its head are both unrecovered on the crash side.
//    (The DESTINATION side is ready: PropEntityIO::InputBuffer_PostScene holds the real
//    EventQueue<RaceCarCrashCompleteEvent,10> and declares AppendRaceCarCrashQueue -- whose
//    own header comment already names THIS bridge as the reason it has no body yet.)
//    COST OF THE PARK: a prop broken by a crashing car is not retired at crash-complete. It
//    does not block the break.
//    DELETE-WHEN: CrashModuleIO::OutputBuffer_PostScene gets a real layout. Exact text in
//    bridges.owner.md.
// =================================================================================================
void BridgeCrashModuleToPropModule_PostScene(
    void* lpWorldModule,
    BrnWorld::PropEntityIO::InputBuffer_PostScene* lpPropInputBuffer_PostScene,
    const BrnWorld::CrashIO::OutputBuffer_PreScene* lpCrashOutputBuffer)
{
    (void)lpWorldModule;   // X360 r3 -- overwritten at 0x827AAD88, never read

    // ⭐ UNPARKED 2026-08-25 (crash exit). The park's premise is retired: the source type is not
    // a 16-byte placeholder, it is CrashIO::OutputBuffer_PreScene -- the crash module's ONE
    // output buffer -- and its +0x231D0 RaceCarOutputInterface now names the real
    // EventQueue<RaceCarCrashCompleteEvent,10>. Full derivation in
    // Bridges/WorldBridgeCrashPostScene.cpp; the short version is that WorldModule::Update
    // creates no post-scene crash buffer and hands PreSceneUpdate's own output buffer straight
    // to EntityModulePostSceneUpdate.
    //
    // The console's Append source argument is the INTERFACE pointer, unadjusted (no `addi`
    // between the two calls at 0x827AAD90/0x827AAD9C), which is what pins the queue to offset 0
    // of the interface; the DESTINATION does carry `addi r3,r31,8` -- the inlined
    // AppendRaceCarCrashQueue onto the prop buffer's own +8 queue.
    lpPropInputBuffer_PostScene->AppendRaceCarCrashQueue(
        lpCrashOutputBuffer->GetRaceCarOutputInterface()->GetRaceCarCrashCompleteEventQueue());
}

// =================================================================================================
// WorldModule::BridgePropModuleToTrafficModule_PrePhysics  @ 0x827AEA70  (41 insns)
//
// The one bridge in this file that carries data OUT of the prop module: the traffic-light
// knock-down / restore requests a smashed traffic-light prop raises, handed to the traffic
// system so the junction's lights actually change.
//
// ---- The console body (0x827AEA70..0x827AEB10) -----------------------------
//   r4 = lpTrafficInputBuffer_PrePhysics (dest), r5 = lpPropOutputBuffer_PrePhysics (src);
//   r3 (the WorldModule `this`) is overwritten at 0x827AEAD4 and never read.
//
//   if (!dest) FireAssert("lpTrafficInputBuffer_PrePhysics != NULL", <file>, 0x7B == 123)
//   if (!src)  FireAssert("lpPropOutbutBuffer_PrePhysics != NULL",   <file>, 0x7C == 124)
//                         ^^^^^^^^ the console's own typo, reproduced verbatim in the assert
//                         string below. The identifier keeps the project spelling.
//   bl 0x827A1C10  PropEntityIO::OutputBuffer_PrePhysics::GetPropToTrafficInterface() const
//                                            (read-lock, +11296, BrnPropEntityModuleIO.h:705)
//   bl 0x827A02D0  BrnTrafficIO::InputBuffer_PrePhysics::GetPropToTrafficInterface()
//                                            (write-lock, +199776, BrnTrafficEntityModuleIO.h:294)
//   stw 0, 8(dstIface)      ; Clear() on the knock-down ring (miLength = 0)
//   bl 0x827A8650           ; EventQueue<TrafficLightKnockDownEvent,32>::Append(dst+0,    src+0)
//   stw 0, 8(dstIface+0x8C) ; Clear() on the restore ring
//   bl 0x827A8730           ; EventQueue<TrafficLightRestoreEvent,80>::Append(dst+0x8C, src+0x8C)
//
// The two +0x8C offsets are the second queue in PropToTrafficInterface: the knock-down ring is
// 12 (console base) + 32*4 == 140 == 0x8C bytes, exactly the committed layout in
// BrnPropToTrafficInterface.h. Both ends are therefore the SAME struct -- the console is
// clearing and re-appending a pair of rings, i.e. the Clear()+Append() idiom the committed
// RaceCar/Traffic setters use (BrnRaceCarEntityModuleIO.cpp:772, BrnTrafficEntityModuleIO.cpp:269).
//
// ⭐⭐ UNPARKED 2026-08-19 (wave Q6 cluster C3). The park said the DESTINATION seat was a
//    placeholder -- `struct PropToTrafficInterface { u8 muDUMMY; };` on the traffic side -- and
//    that retyping it belonged to that buffer's owner. It does, and it is done: the traffic
//    InputBuffer_PrePhysics member is `typedef BrnWorld::PropEntityIO::PropToTrafficInterface`
//    now, and BrnTrafficIO::InputBuffer_PrePhysics::Construct grew the two queue legs the
//    console's Construct @0x827615F8 runs at +0x30C60 / +0x30CEC. Both ends of this bridge are
//    the SAME type, so there is no cast in the leg below.
//
//    THE PRODUCER IS ALREADY LIVE: PropZoneManager::SendTrafficLightRestoreEvents and the
//    knock-down post inside the break path fill the prop side's copy
//    (BrnPropZoneManager.cpp), which is why this was the cheapest closeable leg in the file.
//
//    ⛔ THE ONE THING THIS TU CANNOT SUPPLY, AND IT IS A LINK HOLE, NOT A PARK:
//    PropToTrafficInterface::GetTrafficLightKnockDownQueue() const and
//    ::GetTrafficLightRestoreQueue() const are DECLARED (BrnPropToTrafficInterface.h:77/:78,
//    DWARF :125/:126) and DEFINED NOWHERE. The console emits no out-of-line symbol for either
//    -- it folds both into this bridge -- so each is a one-line body
//    (`return &mTrafficLightKnockDownQueue;`) in the already-mounted
//    SharedIO/BrnPropToTrafficInterface.cpp, which this cluster does not own. Reported to the
//    conductor with the exact text rather than forked here (AGENTS.md gotcha 7): a second
//    definition in this TU would be an LNK2005 the per-TU `cl /c` gate cannot see.
//
//    THE const_cast IS THE COMMITTED IN-TREE IDIOM, not a workaround: PropToTrafficInterface
//    keeps its two rings private behind const-only getters, and the console reaches them from
//    outside the class exactly the way BrnTrafficEntityModuleIO.cpp:194
//    (InputBuffer_PreScene::SetTrafficNetworkInputInterface) already does for
//    TrafficNetworkInputInterface's private ActivateHull ring -- const accessor + const_cast +
//    Clear() + Append(). The DESTINATION is write-locked by the caller
//    (WorldModule::EntityModulePrePhysicsUpdate), so writing through it is correct.
// =================================================================================================
void BridgePropModuleToTrafficModule_PrePhysics(
    void* lpWorldModule,
    BrnTraffic::BrnTrafficIO::InputBuffer_PrePhysics* lpTrafficInputBuffer_PrePhysics,
    const BrnWorld::PropEntityIO::OutputBuffer_PrePhysics* lpPropOutputBuffer_PrePhysics)
{
    (void)lpWorldModule;   // X360 r3 -- overwritten at 0x827AEAD4, never read

    CGS_ASSERT(lpTrafficInputBuffer_PrePhysics != 0, "lpTrafficInputBuffer_PrePhysics != NULL"); // :123
    // Console string verbatim, typo included ("Outbut"): CGS_ASSERT messages are transcribed,
    // not corrected.
    CGS_ASSERT(lpPropOutputBuffer_PrePhysics != 0, "lpPropOutbutBuffer_PrePhysics != NULL");     // :124

    typedef BrnWorld::PropEntityIO::PropToTrafficInterface PropToTrafficInterface;

    // asm order: the SOURCE getter first (0x827A1C10, read-lock, +11296), then the DESTINATION
    // getter (0x827A02D0, write-lock, +199776).
    const PropToTrafficInterface* const lpSource =
        lpPropOutputBuffer_PrePhysics->GetPropToTrafficInterface();
    PropToTrafficInterface* const lpDestination =
        lpTrafficInputBuffer_PrePhysics->GetPropToTrafficInterface();

    // ---- ring 0: the knock-down requests (`stw 0,8(dst)` == Clear, then Append at 0x827A8650)
    CgsModule::EventQueue<BrnWorld::PropEntityIO::TrafficLightKnockDownEvent, 32>& lrKnockDown =
        const_cast<CgsModule::EventQueue<BrnWorld::PropEntityIO::TrafficLightKnockDownEvent, 32>&>(
            *lpDestination->GetTrafficLightKnockDownQueue());
    lrKnockDown.Clear();
    lrKnockDown.Append(*lpSource->GetTrafficLightKnockDownQueue());

    // ---- ring 1: the restore requests (`stw 0,8(dst+0x8C)` == Clear, then Append at 0x827A8730)
    CgsModule::EventQueue<BrnWorld::PropEntityIO::TrafficLightRestoreEvent, 80>& lrRestore =
        const_cast<CgsModule::EventQueue<BrnWorld::PropEntityIO::TrafficLightRestoreEvent, 80>&>(
            *lpDestination->GetTrafficLightRestoreQueue());
    lrRestore.Clear();
    lrRestore.Append(*lpSource->GetTrafficLightRestoreQueue());

    // [DIAG] NOT IN THE X360 BINARY. Opt-in one-shot behind BRN_PROP_DIAG: fires the first
    // frame a smashed traffic light actually raises a request, so a silent log separates "the
    // bridge dropped it" from "PropZoneManager never posted one". Rate: once, ever.
    {
        static const bool sbPropDiag = ( getenv( "BRN_PROP_DIAG" ) != 0 );
        static bool       sbLoggedFirstLights = false;
        if ( sbPropDiag && !sbLoggedFirstLights
             && ( lrKnockDown.GetLength() > 0 || lrRestore.GetLength() > 0 )
             && CgsDev::Log::gpDebugPrint != 0 )
        {
            sbLoggedFirstLights = true;
            *CgsDev::Log::gpDebugPrint
                << "[Q6-lights] first traffic-light requests -> traffic module: knockdown "
                << lrKnockDown.GetLength() << " restore " << lrRestore.GetLength() << "\n";
        }
    }
}

// =================================================================================================
// ⭐ WorldModule::BridgeAIToEntityModules_PrePhysics  @ 0x827AD540  (24 insns)  -- LIVE
//
// The AI module's reset-on-track / place-on-track results, fanned out to BOTH the prop module
// and the race-car module in one call. WorldModule::Update @0x827D63E8 runs it just before the
// pre-physics spine (BrnWorldModule.cpp:2737), with both destinations write-locked and the AI
// output read-locked.
//
// ---- The console body (0x827AD540..0x827AD59C) -----------------------------
//   r3 = the WorldModule `this` (⚠️ THE ONE BRIDGE HERE THAT READS IT),
//   r4 = lpRaceCarInputBuffer_PrePhysics, r5 = lpPropInputBuffer_PrePhysics,
//   r6 = lpAIOutputBuffer. NO asserts.
//
//   addis r30,r3,0x5E ; addi r30,r30,0x1CA8   -- worldModule + 0x5E1CA8 == +6167720
//   lwz r3,0(r30) ; bl PerfMonCpu::StartMonitor
//   bl 0x8279CD48  AIModuleIO::OutputBuffer::GetAIModuleResultInterface() const
//                                          (read-lock, +0x17F50 == 98128, BrnAIModuleIO.h:479)
//   bl 0x827AA220  PropEntityIO::InputBuffer_PrePhysics::AppendResetOnTrackResultQueue
//   bl 0x8279CD48  the SAME getter, called a SECOND time (not cached in a register)
//   bl 0x827ACAF8  RaceCarEntityModuleIO::InputBuffer_PrePhysics::SetAIModuleResultInterface
//   lwz r3,0(r30) ; bl PerfMonCpu::StopMonitor
//
// ⚠️ THE APPEND'S ARGUMENT. AppendResetOnTrackResultQueue @0x827AA220 takes the pointer in r4
// and forwards it UNCHANGED to BaseEventQueue<ResetOnTrackResult>::Append -- so the console is
// passing the AIModuleResultInterface pointer straight in as a queue pointer, which works only
// because mResetPosResultEventQueue is that interface's FIRST member (DWARF :148). The named
// form below (`->GetResetOnTrackResultQueue()`) is that same address reached by name, which is
// what this project's "parity by named members" rule requires. It is NOT a widening: the
// second consumer on the very next line takes the whole interface, and the two arguments are
// the same pointer value on the console.
//
// ⚠️ THE RACE-CAR HALF IS THE CONSOLE'S, and it has NO other producer: grepping the tree for
// RaceCarEntityModuleIO::InputBuffer_PrePhysics::SetAIModuleResultInterface finds exactly one
// caller-shaped site, and it is this bridge (WorldBridgeAIToEntityModules.h declares only this
// bridge, the post-physics AI -> race-car latch @0x827A4F58 and the AI -> physics staging).
// So the race-car half is kept here, in the same call and the same order as the console has it,
// rather than being split out or assumed to arrive elsewhere.
//
// [FLAG] the console's CPU monitor (worldModule + 6167720) is NOT modelled -- the parameter is
// `void*` in this tree's bridge model and dereferencing it through a console byte offset is
// precisely the bug WorldBridgeRaceCarToWorldModule.cpp's banner exists to warn about
// (67,504 bytes wrong on x64). Same disposition as WorldBridgeEntityModulesToOutput.cpp:278
// takes for the same monitor id. The caller already brackets this call with its own
// miUT_World monitor (BrnWorldModule.cpp:2735/2745), so no timing region is lost outright.
//
// ⭐⭐⭐ TOMBSTONE + UNPARK, 2026-08-26 (resetpump wave). THE PARK THAT STOOD HERE FOR
//    EIGHT DAYS IS RETIRED, AND ITS REASON IS RECORDED RATHER THAN DELETED BECAUSE IT WAS RIGHT:
//
//        "static_assert(sizeof(BrnAI::AIModuleIO::OutputBuffer) == 1)   -- PASSES."
//
//    That buffer USED to declare no data members at all while its eleven accessors returned
//    `reinterpret_cast<u8*>(this) + <console offset>`, up to this+110448 -- and
//    CgsIOBufferStack::CreateIOBuffer<T> allocates exactly sizeof(T). Landing this bridge then
//    would have made SetAIModuleResultInterface Clear() the race-car module's two REAL AI rings
//    and then Append() up to 128 ResetOnTrackResult and 128 PlaceOnTrackRequest events read out
//    of whatever sat 98 KB past a ONE-BYTE allocation, with the ring's own miLength taken from
//    that same foreign memory -- invisible to the compile gate, to the faithfulness lint, and to
//    any boot smoke test that does not reset a car.
//
//    ⭐ THE PARK'S OWN DELETE-WHEN CONDITION -- "BrnAI::AIModuleIO::OutputBuffer gets a real
//    member layout" -- WAS MET by the aimodule wave on 2026-08-25. VERIFIED BEFORE UNPARKING,
//    not assumed: BrnAIModuleIO_OutputBuffer.h's private block now declares seven REAL typed
//    members in the X360's attested order (mAIModuleResultInterface among them, a real
//    AIModuleResultInterface), every accessor returns `&member`, and that buffer's own
//    Construct() constructs BOTH of the result interface's rings.
//
//    ⚠️ THE SOURCE-SIDE GUARANTEE IS ONLY HALF THE JOB. The DESTINATION -- the race-car
//    module's InputBuffer_PrePhysics -- had the mirror-image defect: its Construct never
//    constructed mAIModuleResultInterface's two rings either, and SetAIModuleResultInterface
//    Clear()s + Append()s them. Fixed in the same commit (BrnRaceCarEntityModuleIO.h). Un-gating
//    a producer CREATES that fault; it does not reveal it.
//
// [FLAG] the console's CPU monitor (worldModule + 6167720) is still NOT modelled -- see the
// note above.
// =================================================================================================
void BridgeAIToEntityModules_PrePhysics(
    void* lpWorldModule,
    BrnWorld::RaceCarEntityModuleIO::InputBuffer_PrePhysics* lpRaceCarInputBuffer_PrePhysics,
    BrnWorld::PropEntityIO::InputBuffer_PrePhysics* lpPropInputBuffer_PrePhysics,
    const BrnAI::AIModuleIO::OutputBuffer* lpAIOutputBuffer)
{
    (void)lpWorldModule;   // X360 r3 -- the perf-monitor id only; see the [FLAG]

    if (lpAIOutputBuffer == 0)
    {
        return;
    }

    // The console calls the SAME getter twice rather than caching it in a register
    // (`bl 0x8279CD48` at both 0x827AD56C and 0x827AD580). Reproduced: each call is also the
    // read-lock tripwire, so collapsing them would drop one of the two checks.
    if (lpPropInputBuffer_PrePhysics != 0)
    {
        lpPropInputBuffer_PrePhysics->AppendResetOnTrackResultQueue(
            lpAIOutputBuffer->GetAIModuleResultInterfaceConst()->GetResetOnTrackResultQueue());
    }

    if (lpRaceCarInputBuffer_PrePhysics != 0)
    {
        lpRaceCarInputBuffer_PrePhysics->SetAIModuleResultInterface(
            lpAIOutputBuffer->GetAIModuleResultInterfaceConst());
    }
}

}   // namespace WorldModule
