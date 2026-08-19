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
//   LIVE   BridgePhysicsModuleToPropModule_PostPhysics -- the CONTACT-SPY leg. Its
//          updated-prop-queue leg is parked (PhysicsModuleIO::OutputBuffer models
//          mPropManagerOutputInterface as 1-byte opaque storage).
//   LIVE   BridgeSceneContactsToPropModule_PrePhysics -- ⭐ UNPARKED 2026-08-19 (wave Q5/F2).
//          SceneManagerIO::OutputBuffer grew the real mPotentialContactQueue (+32800), the
//          read-lock accessor @0x8279C098 and the console's own Construct in wave Q5 round 2,
//          so the source seat this leg reads is a named member of the same type the
//          destination Append takes. See that function's banner.
//   PARKED BridgeCrashModuleToPropModule_PostScene -- BrnWorld::CrashModuleIO::
//          OutputBuffer_PostScene is `u8 maDeferredPayload[16]` (BrnCrashModule.h:75).
//   PARKED BridgePropModuleToTrafficModule_PrePhysics -- the TRAFFIC side's
//          PropToTrafficInterface is a 1-byte `{ u8 muDUMMY; }` placeholder
//          (BrnTrafficEntityModuleIO.h:210) while the PROP side already holds the real type.
//   PARKED BridgeAIToEntityModules_PrePhysics -- sizeof(BrnAI::AIModuleIO::OutputBuffer) == 1
//          on the host while its accessors return this+98128. See that function's banner:
//          this park caught a live corruption that had already been written and compiled.
//
// ⚠️ THE MEASUREMENT THAT DECIDED THE PARKS -- run it before unparking anything, and before
// adding any accessor to a buffer that has no named members. The host sizeof of each source
// buffer, against the console offset the bridge reads:
//
//     PhysicsModuleIO::OutputBuffer          998224   contact spy @998192   IN BOUNDS  -> LIVE
//     PhysicsModuleIO::OutputBuffer          998224   updated props @71792  in bounds, but the
//                                                     seat is a 1-byte placeholder -> PARKED
//     SceneManagerIO::OutputBuffer            32800   contacts   @32800     OUT OF BOUNDS
//                                                     -> ⭐ FIXED: the buffer is 201,824 B with
//                                                     a NAMED mPotentialContactQueue since wave
//                                                     Q5 round 2, so this row is LIVE now
//     CrashModuleIO::OutputBuffer_PostScene      16   racecar iface @143824 OUT OF BOUNDS
//     BrnAI::AIModuleIO::OutputBuffer             1   AI result  @98128     OUT OF BOUNDS
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
// ⛔ LEG 1 (updated-prop queue) IS PARKED -- honestly, not silently. Two independent blockers,
//    neither of which this TU may fix:
//      (a) PhysicsModuleIO::OutputBuffer models mPropManagerOutputInterface as
//          `struct PropOutputInterfaceStorage { unsigned char maBytes[1]; }`
//          (BrnPhysicsModuleIO.h's own "FLAG (foreign types)" block), so the const getter
//          returns a pointer to a ONE-BYTE struct. Reaching mUpdatedProps through it would
//          mean reinterpret_cast'ing that placeholder onto a 76,864-byte interface and reading
//          a queue out of it -- the exact live-corruption class the sibling
//          WorldBridgePhysicsToEntityModules.cpp refused for its own legs 3-5.
//      (b) Props::PropOutputInterface::GetUpdatedProps() const is DECLARED
//          (BrnPropOutputInterface.h:65) and DEFINED NOWHERE in the tree, so even with (a)
//          fixed this leg would be an unresolved external that `cl /c` cannot see.
//    COST OF THE PARK: the prop module's post-physics input never sees the physics side's
//    per-prop UpdatePropEvent stream, so a prop that has been broken and is being simulated
//    does not get its new pose back into the world module. It does NOT block the break
//    itself -- that runs off the contact-spy leg below.
//    DELETE-WHEN: mPropManagerOutputInterface is promoted to Props::PropOutputInterface (the
//    same storage-typedef promotion the vehicle trio already had, BrnPhysicsModuleIO.h
//    2026-08-09) and GetUpdatedProps() is bodied. Exact text in bridges.owner.md.
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

    // ---- LEG 2 -- the contact-spy handle. The console copies the interface's single
    // ContactSpyData* word; on the host that word is the whole struct, so this is the
    // struct assignment (_AssertLayout above pins that equivalence).
    *lpPropInputBuffer_PostPhysics->GetContactSpyInterface() =
        *lpPhysicsModuleOutputBuffer->GetContactSpyInterface();

    // ---- LEG 1 -- parked. See the banner for the two blockers.
    {
        static bool sbLoggedUpdatedPropsPark = false;
        if (!sbLoggedUpdatedPropsPark && CgsDev::Log::gpDebugPrint != 0)
        {
            sbLoggedUpdatedPropsPark = true;
            *CgsDev::Log::gpDebugPrint
                << "[FLAG PC bring-up] BridgePhysicsModuleToPropModule_PostPhysics: the "
                   "contact-spy leg is LIVE; the updated-prop-queue leg (X360 @0x8279F640 -> "
                   "AppendUpdatedPropQueue) is PARKED because PhysicsModuleIO::OutputBuffer "
                   "still models mPropManagerOutputInterface as 1-byte opaque storage and "
                   "Props::PropOutputInterface::GetUpdatedProps() has no body in the tree.\n";
        }
    }
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
    const BrnWorld::CrashModuleIO::OutputBuffer_PostScene* lpCrashOutputBuffer_PostScene)
{
    (void)lpWorldModule;                  // X360 r3 -- overwritten at 0x827AAD88, never read
    (void)lpPropInputBuffer_PostScene;    // consumed by the parked Append
    (void)lpCrashOutputBuffer_PostScene;  // source seat unrecovered -- see the banner

    {
        static bool sbLoggedCrashPark = false;
        if (!sbLoggedCrashPark && CgsDev::Log::gpDebugPrint != 0)
        {
            sbLoggedCrashPark = true;
            *CgsDev::Log::gpDebugPrint
                << "[FLAG PC bring-up] BridgeCrashModuleToPropModule_PostScene: PARKED. "
                   "BrnWorld::CrashModuleIO::OutputBuffer_PostScene is still a 16-byte "
                   "placeholder, so the crash module's race-car output interface (console "
                   "+0x231D0) and its RaceCarCrashCompleteEvent ring cannot be reached by "
                   "name. The prop-side destination queue is ready.\n";
        }
    }
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
// ⛔ PARKED. The DESTINATION seat is a placeholder: BrnTrafficIO::InputBuffer_PrePhysics
//    declares its own nested `struct PropToTrafficInterface { u8 muDUMMY; };`
//    (BrnTrafficEntityModuleIO.h:210, with the note "Its concrete home is not yet in-tree ...
//    adopt the named type when its home lands"). That home HAS landed -- it is
//    BrnWorld::PropEntityIO::PropToTrafficInterface, which the PROP side of this very bridge
//    already holds by value -- but adopting it on the traffic side is a member RETYPE that
//    grows that buffer, not an additive declaration, so it belongs to that buffer's owner and
//    not to this TU.
//    ⭐ This one is a ONE-LINE unpark and it is the only parked leg here whose data actually
//    exists today (the prop module fills its side via PropZoneManager's
//    SendTrafficLightRestoreEvents). Exact text in bridges.owner.md.
//    COST OF THE PARK: smashed traffic lights do not tell the traffic system to change. It
//    does not block the break.
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

    {
        static bool sbLoggedTrafficPark = false;
        if (!sbLoggedTrafficPark && CgsDev::Log::gpDebugPrint != 0)
        {
            sbLoggedTrafficPark = true;
            *CgsDev::Log::gpDebugPrint
                << "[FLAG PC bring-up] BridgePropModuleToTrafficModule_PrePhysics: PARKED. "
                   "BrnTrafficIO::InputBuffer_PrePhysics still declares its own 1-byte "
                   "PropToTrafficInterface placeholder; the real type "
                   "(BrnWorld::PropEntityIO::PropToTrafficInterface) is committed and the "
                   "prop side already holds it. One-line retype unparks the traffic lights.\n";
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
// ⛔⛔ PARKED -- AND THIS ONE IS A PARK THAT PREVENTED A LIVE CORRUPTION. Both halves were
//    WRITTEN AND COMPILING in this wave before the source buffer was measured. THE MEASUREMENT:
//
//        static_assert(sizeof(BrnAI::AIModuleIO::OutputBuffer) == 1)   -- PASSES.
//
//    BrnAI::AIModuleIO::OutputBuffer is a documented MINIMAL SLICE with NO NAMED MEMBERS: its
//    eleven accessors are all `reinterpret_cast<u8*>(this) + <console offset>`, and its host
//    sizeof is therefore ONE BYTE (the IOBuffer status byte). But CgsIOBufferStack::
//    CreateIOBuffer<T> allocates `Alloc(sizeof(T))` -- BrnWorldModule.cpp:1132 creates this
//    buffer -- so the live object IS one byte, while GetAIModuleResultInterface() hands back
//    `this + 98128`.
//
//    Landing this bridge would therefore have made SetAIModuleResultInterface Clear() the
//    race-car module's two REAL AI rings and then Append() up to 128 ResetOnTrackResult and
//    128 PlaceOnTrackRequest events read out of whatever sits 98 KB past a one-byte
//    allocation -- with the ring's own miLength taken from that same foreign memory. That is a
//    guaranteed corruption of the race-car reset path (and a probable AV), and it would have
//    been invisible to the compile gate, to the faithfulness lint, and to a boot smoke test
//    that does not reset a car.
//
//    ⚠️ THE DEFECT IS NOT THIS BRIDGE'S -- it is the AI output buffer's, and it predates this
//    wave. Every one of that buffer's eleven accessors is an out-of-bounds walk today. Nothing
//    reads them yet only because every consumer is still gated; this bridge would have been the
//    first. REPORTED, not worked around: see bridges.owner.md.
//    DELETE-WHEN: BrnAI::AIModuleIO::OutputBuffer gets a real member layout (or at minimum a
//    correctly-sized payload) so that sizeof(OutputBuffer) covers +98128. The DESTINATIONS are
//    both ready and real today (PropEntityIO::InputBuffer_PrePhysics::
//    AppendResetOnTrackResultQueue and RaceCarEntityModuleIO::InputBuffer_PrePhysics::
//    SetAIModuleResultInterface are bodied), so this becomes a four-line unpark.
//    COST OF THE PARK: the AI's reset-on-track / place-on-track results reach neither the prop
//    module nor the race-car module. It does not block the break.
// =================================================================================================
void BridgeAIToEntityModules_PrePhysics(
    void* lpWorldModule,
    BrnWorld::RaceCarEntityModuleIO::InputBuffer_PrePhysics* lpRaceCarInputBuffer_PrePhysics,
    BrnWorld::PropEntityIO::InputBuffer_PrePhysics* lpPropInputBuffer_PrePhysics,
    const BrnAI::AIModuleIO::OutputBuffer* lpAIOutputBuffer)
{
    (void)lpWorldModule;                   // X360 r3 -- the perf-monitor id only; see the [FLAG]
    (void)lpRaceCarInputBuffer_PrePhysics; // destination ready; source seat is out of bounds
    (void)lpPropInputBuffer_PrePhysics;    // destination ready; source seat is out of bounds
    (void)lpAIOutputBuffer;                // ⛔ a ONE-BYTE object -- see the banner

    {
        static bool sbLoggedAIPark = false;
        if (!sbLoggedAIPark && CgsDev::Log::gpDebugPrint != 0)
        {
            sbLoggedAIPark = true;
            *CgsDev::Log::gpDebugPrint
                << "[FLAG PC bring-up] BridgeAIToEntityModules_PrePhysics: PARKED. "
                   "sizeof(BrnAI::AIModuleIO::OutputBuffer) == 1 on the host while its "
                   "result-interface accessor returns this+98128, so reading the AI result "
                   "rings would Append foreign memory into the prop AND race-car pre-physics "
                   "inputs. Both destinations are real and ready; the AI output buffer needs a "
                   "layout.\n";
        }
    }
}

}   // namespace WorldModule
