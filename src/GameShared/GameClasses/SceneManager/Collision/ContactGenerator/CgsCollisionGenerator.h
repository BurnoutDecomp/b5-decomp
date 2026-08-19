#pragma once

// GameShared/GameClasses/SceneManager/Collision/ContactGenerator/CgsCollisionGenerator.h
//
// CgsSceneManager::CgsCollision::BaseCollisionGenerator -- an IOBuffer-derived contact
// generator. It owns a ring of collision "batches" (each an EA::Jobs::Job + descriptor,
// CgsCollisionBatch.h), a set of collision-result lists carved from a LinearMalloc bump
// allocator, and the used-slot bookkeeping that drives the async collide/test family. This
// header is the owning home DecFIGS attributes the class to (CgsCollisionGenerator.h:63); it
// is grown ADDITIVELY -- only the members + the lifecycle/batch methods reconstructed so far
// are declared. The remaining Collide*/Test*/Run* API (declared in the DWARF) lands as its
// TUs are worked; declaring them now would pull in the (as-yet-unrecovered) geometry list /
// spatial-map / result-list parameter types and break the compile gate.
//
// Member NAMES/TYPES: DecFIGS DWARF (CgsCollisionGenerator.h). Member COUNTS/OFFSETS: X360
// ARTIST asm (ground truth, arbitrates over the DWARF where they differ):
//   * KU16_MAX_NUM_BATCHES is 64 on X360 (Finish loops i<0x40; CreateNewBatch masks %64 via
//     an srawi/slwi<<6; Prepare zeroes 64 used-flag bytes and constructs 64 jobs). The DWARF,
//     the older PS3/DecFIGS shape, says 32 -- the X360 build widened the ring to 64.
//   * X360 byte layout (this=r3): maCollisionBatches @ +0x80 (stride 1152), then
//     mapCollisionResultLists[200] (@+0x12080, 4-byte console ptrs), mCollisionResultsAllocator
//     @ +0x123A0, mu16NumUsedResultLists @ +0x123BC, mu16NumUsedBatches @ +0x123BE,
//     mabUsedBatches[64] @ +0x123C0. Per the project rule the PC compile does NOT reproduce
//     byte offsets (pointers widen to 8 bytes) -- this is semantic-parity-by-named-member.
//     The ~0x80 of members between the 1-byte IOBuffer base and maCollisionBatches are not
//     named in the DWARF and are not touched by any reconstructed method here, so they are
//     honestly omitted rather than invented.

#include "types.hpp"
#include <cstddef>

#include "GameShared/GameClasses/Module/CgsIOBuffer.h"                                  // CgsModule::IOBuffer (base)
#include "GameShared/GameClasses/Memory/CgsLinearMalloc.h"                              // CgsMemory::LinearMalloc (by value)
#include "GameShared/GameClasses/SceneManager/Collision/ContactGenerator/CgsCollisionBatch.h" // CollisionBatch (by value)

// Producer factory return type (pointer-only use in this header); full layout in
// Memory/DataStream/CgsSimpleDataStreamProducer.h, included by the .cpp.
namespace CgsMemory { struct SimpleDataStreamProducer; }

namespace EA { namespace Jobs { struct Job; } }

// RunFillTriangleCacheStream's query structure (pointer use only). Full home:
// GameShared/GameClasses/Geometric/Primitives/PolygonSoup/CgsPolygonSoupListSpatialMap.h.
namespace CgsGeometric { struct PolygonSoupListSpatialMap; }

// ⭐ ADDED 2026-08-06 (big-five #2): the collide-stream family's collaborators, pointer use
// only. Class keys match their homes (CgsDebugRenderStreamReader.h:23 -- class, in CgsDev).
namespace CgsDev { class DebugRenderStreamReader; }

namespace CgsSceneManager
{
namespace CgsCollision
{
    // Result-list pointer array element; only referenced by pointer here (its full layout
    // lives in Collision/Primitives/CgsCollisionResult.h). Forward-declared to avoid a header
    // cascade for a pointer-only member.
    struct CollisionResultList;

    // CollidePrimitivePairList's list argument (home: Primitives/CgsPrimitivePairList.h:32,
    // class key struct). Pointer use only here.
    struct PrimitivePairList;

    // ⭐ ADDED 2026-08-14 (walls leg 1): the two Add* posters' list arguments (homes:
    // Primitives/CgsSphereList.h, Primitives/CgsSweptSphereList.h, Primitives/CgsTriangleList.h
    // -- all class key struct). Pointer use only here.
    struct SphereList;
    struct SweptSphereList;
    struct TriangleList;

    // DWARF CgsCollisionGenerator.h:63.
    struct BaseCollisionGenerator : public CgsModule::IOBuffer
    {
        // --- capacities (DWARF h:314/318/320; batch count corrected to the X360 ring size) ---
        static const u16 KU16_MAX_NUM_RESULTS      = 2000; // h:314
        static const u16 KU16_MAX_NUM_BATCHES      = 64;   // h:318 says 32 (PS3); X360 asm = 64
        static const u16 KU16_MAX_NUM_RESULT_LISTS = 200;  // h:320

        // --- lifecycle -----------------------------------------------------------------
        void Construct();                        // h:67  / X360 0x828105F8
        void Destruct();                         // h:70  / X360 0x8284CB38 (empty; ICF-folded)
        bool Prepare(void* lpResultBuffer, s32 liResultBufferSize); // h:73 / X360 0x82810660
        void Finish();                           // h:80  / X360 0x828128D8

        // Copy of the luIndex'th result list (by value, bounds-checked against
        // mu16NumUsedResultLists). X360 0x825B2AE0. (Incomplete return type is fine
        // for a declaration — CollisionResultList is forward-declared above and the
        // definition in the .cpp includes its owning header.)
        CollisionResultList GetResultList(u16 luIndex) const;   // X360 0x825B2AE0

        // ==========================================================================================
        // ADDED 2026-08-18 (wave Q round 2, shared-header owner): the used-result-list count
        // reader. DWARF CgsCollisionGenerator.h:298 -- `uint16_t GetNumUsedResultLists() const;`
        // (verbatim, in the class's public section, immediately above GetResultList at :302).
        //
        // MUST be a header INLINE, not a declaration: there is NO out-of-line body anywhere in
        // the X360 export set (I scanned the `name` field of every 0x*.json under
        // .ida-exports/BURNOUT_X360_ARTIST.XEX for "NumUsedResult"/"GetNumUsed" -- 0 hits), and
        // its one recovered consumer has it fully inlined --
        // BrnPhysics::Props::PropManager::EndPropWorldContactGeneration @0x82628E18 reads the
        // member directly with `lis r11,1 ; ori r11,r11,0x23BC ; lhzx r11, r30, r11`
        // (0x82628E50/0x82628E5C), r30 being the generator. That +0x123BC is exactly the console
        // offset this header's own member table already records for mu16NumUsedResultLists, and
        // the same consumer's assert string names the accessor verbatim:
        //   "miNumJobsAdded == lpContactGenerator->GetNumUsedResultLists()"
        // So the member was already correct and PRIVACY was the only blocker. (The console read
        // is a HALFWORD -- lhzx -- which is why the return type stays u16; the consumer widens it
        // itself for the signed `cmpw` against its s32 job counter.)
        //
        // WARNING for anyone tempted to shortcut this: +0x123BC is a CONSOLE offset. The host
        // BaseCollisionGenerator is wider (8-byte CollisionResultList* in mapCollisionResultLists
        // [200], plus the host IOBuffer base), so reaching the member by that byte offset lands
        // somewhere else entirely. Reach it by NAME, which is what this accessor is for.
        // ==========================================================================================
        u16 GetNumUsedResultLists() const { return mu16NumUsedResultLists; }   // DWARF h:298

        // Allocate + construct a SimpleDataStreamProducer for a streamed collision pass out of
        // the result allocator (128-byte aligned, alignment saved/restored around the burst).
        // X360 0x828109F8 (ledger identity "Crea" -- the IDA symbol is truncated; this is the
        // producer-factory it names). Sizes both the command and result buffers via
        // SimpleDataStreamProducer::GetRequiredBufferSizes before constructing.
        CgsMemory::SimpleDataStreamProducer* CreateStreamProducer(s32 liMaxCommands);

        // ==========================================================================================
        // ⭐ ADDED 2026-08-06 (big-five #2, contact-generation wave): the collide-stream family
        // VehicleManager::StartVehicleContactGeneration @0x8262AEE8 consumes. Names are the DWARF's
        // (CgsCollisionGenerator.h :96/:99/:108/:111/:120/:123/:144); the X360 addresses bind
        // Create/Run pairs adjacently in the image, and the driver's store seats match the DWARF
        // member names 1:1 (mpSphereSphereStreamProducer <- @0x82811A78, mpSphereTriangleStream-
        // Producer <- @0x828113C8, mpSweptSphereTriangleStreamProducer <- @0x82811720; the three
        // Run* results land in the matching *StreamJob members).
        // ⭐⭐ UPDATED 2026-08-14 (walls leg 1): SIX OF THE SEVEN ARE REAL NOW, in
        // CgsCollisionGenerator_CollideStreams.cpp -- the three Create* factories (byte-identical
        // to each other on the console bar assert line numbers; the landed CreateLineWithTriangle-
        // ListStream shape with command size 32-console/sizeof-host and NO result buffer) and the
        // three Run* dispatchers (the landed RunLineWithTriangleListStream shape wiring
        // ContactGeneratorEntry over desc types 6/14/8 -- whose workers are the three loud named
        // gates in ContactGeneratorJob.cpp until the intersection kernels land).
        // ⭐⭐⭐ UPDATED 2026-08-19 (wave Q7, cluster `pairlist`): SEVEN OF SEVEN NOW. The sentence
        // that stood here -- "Only CollidePrimitivePairList remains a gate
        // (CgsCollisionGenerator_StreamStubs.cpp)" -- is retired; that body is in
        // CgsCollisionGenerator.cpp beside its sibling synchronous leg, and StreamStubs.cpp is a
        // banner-only tombstone reported for UNMOUNT (bat:1001). See the declaration's own block.
        // The DebugRenderStreamReader parameter is spelled bare in the DWARF; bound to
        // CgsDev::DebugRenderStreamReader (the one committed type of that name).
        // ==========================================================================================
        CgsMemory::SimpleDataStreamProducer* CreateCollideSphereListWithTriangleListStream(s32 liMaxCommands);   // @0x828113C8 (:96)
        EA::Jobs::Job* RunCollideSphereListWithTriangleListStream(CgsMemory::SimpleDataStreamProducer* lpProducer,
                                                                  CgsDev::DebugRenderStreamReader* lpDebugReader); // @0x82811550 (:99)
        CgsMemory::SimpleDataStreamProducer* CreateCollideSweptSphereListWithTriangleListStream(s32 liMaxCommands); // @0x82811720 (:108)
        EA::Jobs::Job* RunCollideSweptSphereListWithTriangleListStream(CgsMemory::SimpleDataStreamProducer* lpProducer,
                                                                       CgsDev::DebugRenderStreamReader* lpDebugReader); // @0x828118A8 (:111)
        CgsMemory::SimpleDataStreamProducer* CreateCollideSphereListWithSphereListStream(s32 liMaxCommands);     // @0x82811A78 (:120)
        EA::Jobs::Job* RunCollideSphereListWithSphereListStream(CgsMemory::SimpleDataStreamProducer* lpProducer); // @0x82811C00 (:123)

        // ==========================================================================================
        // ⭐⭐ BODIED 2026-08-19 (wave Q7, cluster `pairlist`) -- the SYNCHRONOUS primitive-pair leg,
        // and the LAST gate of the collide family. Body + full 40-instruction decode:
        // CgsCollisionGenerator.cpp, beside its sibling CollidePrimitiveListAgainstTriangleList.
        //
        // ⚠️ PARAMETER NAMES CORRECTED WITH THE BODY. This declaration used to read
        //     (const PrimitivePairList*, u16 lu16MaxResults, u32 luFlags, u16 lu16Tag)
        // and `luFlags` was a MISNAME: the asm passes r6/r7 straight through to
        // PrepareNewPrimitiveTestResultsList(u16 maxResults, u32 tagA, u16 tagB) (the r4<-r5,
        // r5<-r6, r6<-r7 shuffle at 0x82814148..0x82814150), which stores them into the
        // CollisionResultList header as the caller's USER TAGS -- nothing reads them as a flag
        // word. DecFIGS agrees and supplies the spelling: CgsCollisionGenerator.h:292 declares
        //     uint16_t CollidePrimitivePairList(const PrimitivePairList *, uint16_t, uint32_t,
        //                                       uint16_t);
        // and CgsCollisionGenerator.cpp:1662 names them lpPrimitiveList / lu16MaxNumCollisions /
        // luUserTagA / lu16UserTagB. (Four arguments, no float -> no GPR slot skipped.)
        // The caller's literals carried the same misname and were corrected in the same wave:
        // BrnVehicleManagerContactGeneration.cpp now spells them KU_COLLIDE_USER_TAG_A (tagA == 11)
        // and KU16_COLLIDE_USER_TAG_B (tagB == 0).
        //
        // ⚠️ RETURN TYPE: the DWARF's u16 is kept (this declaration is DWARF-shaped), but the
        // console returns the result-list index UNTRUNCATED -- 0x8281415C `mr r30, r3`, a
        // `clrlwi` at 0x82814168 for the mapCollisionResultLists SUBSCRIPT only, and 0x828141CC
        // `mr r3, r30` with no narrowing on the return path. Same register truth for which the
        // three Add*...ToStream siblings are deliberately typed s32. All FIVE measured call sites
        // (two functions, five `bl`s -- see the body's banner) drop the value, so the spelling
        // changes no behaviour.
        //
        // ⭐ THE WHOLE CLOSURE IS REAL AS OF WAVE Q7 (2026-08-19). The type-10 worker --
        // ContactGeneratorJob::ExecutePrimitivePairList @0x82925798, dispatched from Execute
        // case 10 -- landed as a full body in ContactGeneratorJob.cpp in the SAME wave as this
        // dispatcher; it is no longer a gate. Everything
        // else in the closure was already a real body (PrepareNewPrimitiveTestResultsList @0x82810798,
        // CreateNewBatch @0x82810960, PrimitivePairListJobDesc::Prepare @0x82810478,
        // CollisionBatch::SetupJob @0x82810508, PerfMonCpu::Start/StopMonitor).
        // ==========================================================================================
        u16 CollidePrimitivePairList(const PrimitivePairList* lpPrimitiveList,
                                     u16                     lu16MaxNumCollisions,
                                     u32                     luUserTagA,
                                     u16                     lu16UserTagB);                                      // @0x82814138 (DWARF :292)

        // ==========================================================================================
        // ⭐ ADDED 2026-08-19 (wave Q6, cluster B -- prop-vs-world contact generation). The
        // SYNCHRONOUS twin of AddPrimitiveListWithTriangleListToStream: carve a result list,
        // create a batch, prepare a PrimitiveListWithTriangleListJobDesc over it and dispatch.
        // Returns the result-list index.
        //
        // SIGNATURE IS DWARF-SETTLED AND REGISTER-CONFIRMED. DWARF CgsCollisionGenerator.h, SOURCE
        // LINE 257:
        //     uint16_t CollidePrimitiveListAgainstTriangleList(const PrimitivePairList *,
        //         const TriangleList *, uint16_t, uint32_t, uint16_t, bool);
        // Six parameters, none of them a float, so NO GPR slot is skipped (gotcha 3 does not bite
        // here) and r4..r9 confirm the order one-for-one in the raw `assembly` of
        // .ida-exports/BURNOUT_X360_ARTIST.XEX/0x828141D8.json (86 insns, 0x828141D8..0x8281432C):
        //     r4 -> `cmplwi 0` + assert "lpPrimitiveList != NULL"     (CgsCollisionGenerator.cpp:1940)
        //     r5 -> `cmplwi 0` + assert "lpTriangleList != NULL"                             (:1941)
        //     `lhz r11, 6(r4)` + assert "lpPrimitiveList->GetNumTests() > 0"                  (:1942)
        //     r6/r7/r8 -> PrepareNewPrimitiveTestResultsList(maxResults, tagA, tagB)
        //     r9  -> `clrlwi r11, r24, 24`, ANDed with the global byte_82F310B0 to pick the
        //            descriptor's optimised-box flag -- so r9 IS the trailing bool
        //     return: `mr r3, r29` == the index PrepareNewPrimitiveTestResultsList returned.
        // ⚠️ RETURN TYPE: the DWARF's u16 is kept (this declaration is DWARF-shaped), but the
        // console returns the index UNTRUNCATED -- 0x8281428C `mr r29,r3`, 0x82814298
        // `clrlwi r10,r29,16` for the LOCAL use only, and 0x82814324 `mr r3,r29` with no clrlwi on
        // the return path. That is the same register truth for which the two Add*...ToStream
        // siblings above are deliberately typed s32. Every call site drops the result, so neither
        // spelling changes behaviour; the delta is recorded here rather than left undocumented.
        // ⚠️ IT IS THE DEAD ARM AT RUNTIME, AND IT STILL CANNOT BE DROPPED. Its two prop call
        // sites sit under a RUNTIME byte read (`lbz byte_82F310B0`'s sibling `lbz byte_82F2A39C`
        // in PropManager_wQ2_03.cpp), not a compile-time constant, so the console emits both arms.
        // IDA truncates the export symbol to "...CollidePrimitiveListAgainstTriang"; the full name
        // is in the .i64 and in .pdata at 0x821D8770.
        // SEVEN measured call sites (headless IDA 9.3 on a private .i64 copy, this wave):
        // DeformableObject::DoBodyPartWorldContactGeneration x2 (0x82609708 / 0x82609828),
        // ::DoDetachedWheelWorldContactGeneration (0x82609A8C),
        // VehicleManager::DoTrafficCarWorldContactGeneration x2 (0x8261C0CC / 0x8261C154),
        // PropManager::DoPartWorldContactGeneration (0x826120A8) and
        // ::DoPropInstanceWorldContactGeneration (0x82612604).
        // BODY: CgsCollisionGenerator.cpp (landed with this declaration, wave Q6).
        // ==========================================================================================
        u16 CollidePrimitiveListAgainstTriangleList(const PrimitivePairList* lpPrimitiveList,
                                                    const TriangleList*     lpTriangleList,
                                                    u16                     lu16MaxResults,
                                                    u32                     luUserTagA,
                                                    u16                     lu16UserTagB,
                                                    bool                    lbUseOptimisedBoxTests); // @0x828141D8 (DWARF :257)

        // ==========================================================================================
        // ⭐ ADDED 2026-08-14 (walls leg 1): the two collide-stream COMMAND POSTERS
        // DoRaceCarWorldContactGeneration @0x825EB140 calls per live race car per frame. Console
        // bodies are byte-identical to each other (33 insns, diffed): allocate a fresh
        // CollisionResultList via PrepareNewPrimitiveTestResultsList, build the family's
        // StreamCommand {list pair, triangle pair, padding float, result list}, post it into the
        // producer, and return the result-list index. Argument order is the console's own
        // (f1 takes the float's GPR slot, so lu32UserTagA rides in r8): (lists, maxResults,
        // padding, userTagA == the bridge's custom-queue index, userTagB == 1, producer).
        // No DWARF twins (X360-only exports); signatures are register-truth from the two bodies
        // plus their one caller.
        // ==========================================================================================
        s32 AddSphereListWithTriangleListToStream(const SphereList* lpSphereList,
                                                  const TriangleList* lpTriangleList,
                                                  u16 lu16MaxResults, f32 lfPadding,
                                                  u32 lu32UserTagA, u16 lu16UserTagB,
                                                  CgsMemory::SimpleDataStreamProducer* lpProducer);      // @0x82811340
        s32 AddSweptSphereListWithTriangleListToStream(const SweptSphereList* lpSweptSphereList,
                                                       const TriangleList* lpTriangleList,
                                                       u16 lu16MaxResults, f32 lfPadding,
                                                       u32 lu32UserTagA, u16 lu16UserTagB,
                                                       CgsMemory::SimpleDataStreamProducer* lpProducer); // @0x82811698

        // ==========================================================================================
        // ⭐ ADDED 2026-08-19 (wave Q7, cluster `carcar`) — the THIRD collide-stream poster, and
        // the last of the sphere family: the SPHERE-LIST vs SPHERE-LIST command
        // VehicleManager::DoCarCarContactGeneration @0x8261BB38 posts once per overlapping
        // (non-simple-traffic) car pair per frame. Landing it is what makes the type-7/8
        // sphere-sphere arm reachable at run time at all.
        //
        // ⚠️⚠️ OUT-OF-OWNERSHIP EDIT, DECLARED HERE ON PURPOSE AND REPORTED. This header is the
        // `pairlist` cluster's this wave; the `carcar` cluster added ONLY this declaration (and
        // the matching body beside its two siblings in CgsCollisionGenerator_CollideStreams.cpp)
        // because DoCarCarContactGeneration cannot be landed without it and no other Q7 cluster
        // is scoped to the poster. If this block and the body do not arrive together, the link
        // reports it (LNK2019 from BrnVehicleManagerContactGeneration.obj).
        //
        // The body @0x828119F0 (33 insns) is the SAME BODY as its two siblings above, diffed:
        // PrepareNewPrimitiveTestResultsList -> build the family StreamCommand {list A, list B,
        // padding, result list} -> AddCommand -> return the result-list index. Register truth
        // from that asm: r4/r5 the two lists, r6 maxResults, r7 SKIPPED (f1's GPR slot, gotcha 3),
        // r8 userTagA, r9 userTagB, r10 the producer, f1 the padding — i.e. the identical
        // argument order the two siblings already carry. Its ONE caller passes userTagB == 0
        // (`li r9, 0` @0x8261BCF8) where the world path passes 1.
        // ==========================================================================================
        s32 AddSphereListWithSphereListToStream(const SphereList* lpSphereListA,
                                                const SphereList* lpSphereListB,
                                                u16 lu16MaxResults, f32 lfPadding,
                                                u32 lu32UserTagA, u16 lu16UserTagB,
                                                CgsMemory::SimpleDataStreamProducer* lpProducer);      // @0x828119F0

        // ==========================================================================================
        // ADDED 2026-08-18 (wave Q round 2, shared-header owner): the PRIMITIVE-PAIR-LIST vs
        // TRIANGLE-LIST stream family -- the fourth collide-stream family, and the one BREAKABLE
        // PROPS run on. It is the stream BrnPhysics::Props::PropManager::BeginPropWorldContact-
        // Generation @0x82628CB0 creates into mpPrimitiveWithTriangleStream and dispatches from,
        // and the same pair BrnPhysics::Vehicle::VehicleManager::StartPartContactGeneration
        // @0x8262C220 uses for the deformable-part pass (that one is still a documented PARTIAL in
        // BrnVehicleManagerContactGeneration.cpp:868).
        //
        // ---- IDENTITY OF THE Create HALF (it carries no IDA symbol) -----------------------------
        // sub_82811DD0 is CreateCollidePrimitiveListWithTriangleListStream. MEASURED with headless
        // IDA 9.3 on IDA Files/BURNOUT_X360_ARTIST.XEX.i64 this wave, not inferred from a name:
        //   * boundaries 0x82811D40..0x82811DCC AddPrimitiveListWithTriangleListT<runcated> (35),
        //     0x82811DD0..0x82811F58 sub_82811DD0 (98), 0x82811F58..0x82812098 RunCollidePrimitive
        //     ListWit<runcated> (80) -- the Add / Create / Run triple sits contiguous, exactly the
        //     layout the three committed families use one block earlier;
        //   * its 98 instructions are the SAME BODY as the three committed Create* (the shared
        //     CreateCollideStreamProducer helper in CgsCollisionGenerator_CollideStreams.cpp):
        //     GetAlignment / SetAlignment(0x80) / Malloc(0x180) / "Failed to allocate stream
        //     producer\n" (:2037) / GetRequiredBufferSizes(n, 0x20, 0, 0, &cmd, &res) / Malloc /
        //     "Failed to allocate stream buffers\n" (:2049) / Construct(n, 0x20, buf, 0,0,0) /
        //     SetAlignment(saved) / return producer;
        //   * both halves are called by exactly the two consumers above (xrefs measured), and the
        //     Create call site passes `li r4, 0x64` == 100 max commands (0x82628CF0).
        // The DWARF settles the source shape: CgsCollisionGenerator.h:271 / :275 declare exactly
        // these two, and CgsCollisionGenerator.cpp:2028 / :2076 name the parameters (liMaxTests /
        // lpStream) and the locals -- Run's are liNumJobs / lpNULLJob / liJobIndex / lu16BatchIndex,
        // which is the AllocateJob + per-batch-wiring dispatcher shape its 80 instructions have.
        //
        // ---- ⭐⭐ BOTH BODIES ARE REAL AS OF 2026-08-19 (wave Q6, cluster pstream) ----------------
        // The paragraph that stood here said they could not be landed "because the family's job
        // descriptor has no home in this tree". That blocker is CLOSED, and it was exactly one
        // type plus one enum member:
        //   * CgsSceneManager::CgsCollision::PrimitiveListWithTriangleListStreamJobDesc (with its
        //     nested StreamCommand) now lives in
        //     JobDescription/CgsPrimitiveListWithTriangleListStreamJobDesc.h. Its DWARF home is
        //     the non-Stream sibling header (CgsPrimitiveListWithTriangleListJobDesc.h:107, with
        //     StreamCommand at :111 and Data at :139); it is landed in its own file because that
        //     is the tree's placement for every other *Stream* descriptor whose DWARF home is a
        //     non-Stream header (CgsLineWithTriangleListStreamJobDesc.h,
        //     CgsFillTriangleCacheStreamJobDesc.h). One definition, no fork.
        //   * E_COLLISIONJOB_PRIMITIVE_LIST_WITH_TRIANGLE_LIST_STREAM = 12 is in
        //     JobDescription/CgsCollisionJobDescription.h (MEASURED on both sides: the Run half's
        //     `li r23, 0xC` -> `stb r23, 0x4CF(batch)`, and Execute's jump-table case 7).
        // BODIES: CgsCollisionGenerator.cpp (this class's DWARF home TU, mounted at
        // build_game_exe.bat:1018).
        //   * the console command size is 32 (`li r4/r5, 0x20`), but 32 is a CONSOLE size --
        //     AddPrimitiveListWithTriangleListToStream @0x82811D40 builds the record as
        //     {PrimitivePairList 12B @0x00, TriangleList 8B @0x0C, CollisionResultList* 4B @0x14,
        //      bool @0x18} == 25 bytes rounded to 32, and three of those are 4-byte console
        //     pointers that widen here. The factory passes sizeof(StreamCommand) == 48, gated by
        //     the descriptor header's static_asserts.
        //   * Run's descriptor writes are (batch+0x3D0 == the job-description slot):
        //       +0x00 = the producer          (0x82811FF8 stw r22, 0x3D0(batch base))
        //       +0xF0 = NULL   mpResultsList  (0x82811FF0 stw r27, 0x4C0)
        //       +0xF4 = 0.0f   mfRadius       (0x82811FE8 stfs f31, 0x4C4; flt_82001CC0, read out
        //                                      of the image again this wave: 00000000 == 0.0f)
        //       +0xF8 = NULL   mpDebugStream  (0x82811FF4 stw r27, 0x4C8 -- this family passes NO
        //                                      DebugRenderStreamReader; the Run signature has no
        //                                      reader parameter, matching DWARF :275)
        //       +0xFF = 12     muJobType      (0x82811FEC stb r23, 0x4CF, r23 == 0xC)
        // The type-12 WORKER is a real body too as of the same wave (ContactGeneratorJob::Execute
        // case 12 -> ExecutePrimitiveListWithTriangleListStream @0x82926650, 100 insns), so a
        // posted command is drained rather than dropped. ⭐ What that worker delegates to --
        // ExecutePrimitiveListWithTriangleList @0x82925908 (849) -- is a REAL BODY too, since wave
        // Q6 round 3 (2026-08-19), in ContactGeneratorJob.cpp. This family has no gated leg left.
        // ==========================================================================================
        CgsMemory::SimpleDataStreamProducer* CreateCollidePrimitiveListWithTriangleListStream(s32 liMaxTests); // h:271 / X360 0x82811DD0 (sub_ -- see banner)
        EA::Jobs::Job* RunCollidePrimitiveListWithTriangleListStream(
            CgsMemory::SimpleDataStreamProducer* lpStream);                                                   // h:275 / X360 0x82811F58

        // The family's command POSTER. ⭐ REAL as of 2026-08-19 (wave Q6, cluster pstream), body in
        // CgsCollisionGenerator.cpp; the loud named gate that used to be its only definition
        // (CgsCollisionGenerator_StreamStubs.cpp) was deleted in the same change, because both
        // TUs are mounted and `cl /c` cannot see an LNK2005. Two of its five call sites --
        // PropManager::DoPart/DoPropInstanceWorldContactGeneration -- are landed AND mounted
        // (PropManager_wQ2_03.cpp), which is what forced the body this wave; the other three are
        // DeformableObject::DoBodyPart<x2>/DoDetachedWheelWorldContactGeneration.
        // X360 0x82811D40 (35 insns) -- an EXPORT-SET HOLE: the .i64 names it (truncated to
        // "AddPrimitiveListWithTriangleListT") but .ida-exports has no 0x82811D40.json. Boundaries,
        // callers and the full 35-instruction decode were read out of the image this wave.
        // Parameter ORDER + names are the DWARF's (h:267 / cpp:1996) and are corroborated
        // register-for-register by that decode: r4 pair list, r5 triangle list, r6 max results,
        // r7 the bool (`stb r26`), r8 tag A, r9 tag B, r10 producer -- seven GPR arguments, no
        // float (this family carries a bool where the sphere families carry lfPadding).
        // RETURN TYPE: the DWARF says uint16_t, but the console returns the result-list index
        // untruncated (`mr r3, r28`, no clrlwi) exactly like the two committed Add* siblings above,
        // which this tree deliberately types s32 for the same register-truth reason. Kept s32 for
        // family consistency; the DWARF's spelling is recorded here so a later join finds either.
        s32 AddPrimitiveListWithTriangleListToStream(const PrimitivePairList* lpSPrimList,
                                                     const TriangleList* lpTriangleList,
                                                     u16 lu16MaxNumCollisions,
                                                     bool lbUseOptimisedBoxTests,
                                                     u32 luUserTagA, u16 lu16UserTagB,
                                                     CgsMemory::SimpleDataStreamProducer* lpPrimitiveTriangleStream); // h:267 / X360 0x82811D40 (export hole)

        // ==========================================================================================
        // ⭐ ADDED 2026-08-10 (ground wave): the LINE-vs-triangle-list stream pair -- the floor of
        // the vehicle traction-line chain. VehicleManager::DoVehicleTractionLineAllocations
        // @0x825B5098 calls the Create and seats the result in mpTractionLineStreamProducer;
        // RunTractionLineTestJobs @0x825B5168 calls the Run and seats the returned job in
        // mpTractionLineTestsJob.
        //
        // ⚠️ Neither carries an IDA symbol -- this is the "missing body == a NAME search failing"
        // shape, not an absence:
        //   * Create @0x82810B98 is exported as the unnamed `sub_82810B98`; identity is pinned by
        //     its two asserts (CgsCollisionGenerator.cpp :533 "Failed to allocate stream producer\n"
        //     / :550 "Failed to allocate stream buffers\n" -- the same pair CreateStreamProducer
        //     @0x828109F8 carries at other lines) and by its single caller/consumer seat.
        //   * Run @0x82810E80 is a GENUINE export-set hole: the export dir jumps from
        //     RunFillTriangleCacheStream @0x82810D38 (ends 0x82810E7C) straight to 0x82810FE8.
        //     The address is proved by DECODING the `bl` word at the call site
        //     (0x825B5248 = 0x4825BC39 -> 0x82810E80), with the decoder proved on four
        //     independently-named neighbours in the same run. Its 90 instructions read out of the
        //     image are the exact call sequence of its exported twin RunFillTriangleCacheStream:
        //     AllocateJob / CreateNewBatch / Job::Clear / EntryPoint::SetName / EntryPoint::SetCode
        //     / Job::SetData / Job::DependsOn / JobScheduler::AddTree.
        // ⭐⭐⭐ UPDATED 2026-08-11 (traction-line wave): THE Run BODY IS REAL NOW and its boot
        // gate is DELETED. The paragraph that used to sit here said the body was not reconstructed
        // "because its job entry point and the ContactGeneratorJob::ExecuteLineWithTriangleList-
        // Stream @0x82921968 worker are not in the tree" -- both are, as of this wave, in
        // GameShared/Jobs/ContactGenerator/ (ContactGenerator.cpp + ContactGeneratorJob.cpp).
        // The dispatcher's ONE PC divergence is the JobScheduler::AddTree call, replaced by
        // running the batch entry inline; see the body's banner.
        // ⚠️ Run takes NO DebugRenderStreamReader (unlike the three collide-stream Run*): the call
        // site loads r3=generator, r4=producer and nothing else.
        // ==========================================================================================
        CgsMemory::SimpleDataStreamProducer* CreateLineWithTriangleListStream(s32 liMaxCommands);                 // @0x82810B98
        EA::Jobs::Job* RunLineWithTriangleListStream(CgsMemory::SimpleDataStreamProducer* lpProducer);            // @0x82810E80 (export hole)

        // ==========================================================================================
        // ⭐ ADDED 2026-08-10 (cache-fill wave): the TRIANGLE-CACHE FILL stream dispatcher, the
        // Run* half TriangleCacheManager::StartUpdateTriangleCaches @0x828BF130 calls (its Create*
        // half is the general CreateStreamProducer above, which that same function calls with 298).
        //
        // Signature is DWARF-SETTLED, not read off the call site alone -- the PS3 mangle
        //   _ZN15CgsSceneManager12CgsCollision22BaseCollisionGenerator26RunFillTriangleCacheStreamE
        //     PKN12CgsGeometric25PolygonSoupListSpatialMapEPN9CgsMemory24SimpleDataStreamProducerE
        // gives (const PolygonSoupListSpatialMap*, SimpleDataStreamProducer*), matching the X360
        // call site's r4/r5 exactly.
        //
        // ⛔ BODY NOT RECONSTRUCTED -- named boot gate, same precedent and same reason as
        // RunLineWithTriangleListStream above. Its 82 instructions are the familiar dispatcher
        // shape (AllocateJob / per batch { CreateNewBatch, Job::Clear, EntryPoint::SetName,
        // SetCode(<entry>), SetData, DependsOn } / JobScheduler::AddTree), but what it installs is
        // a JOB ENTRY POINT whose worker is absent from this tree. Writing the dispatcher without
        // the worker would hand EndUpdateTriangleCaches a stream of zero-batch results, i.e.
        // "every cached object has no triangles" -- the silent-drop shape, indistinguishable from
        // a working empty cache.
        //
        // ⭐ THE WORKER FAMILY, RE-COSTED 2026-08-10 (fill-worker wave) from the X360 export JSONs
        // -- machine-counted, and the previously-published list was missing the half that does the
        // actual work. The 82 instructions of the dispatcher itself were read this wave: it writes
        // JobDesc+0x00 = the spatial map, +0x04 = the producer, +0xFF = 3
        // (E_COLLISION_TYPE_FILL_TRIANGLE_CACHE_STREAM), +0xF4 = 0.0f, entry = PolygonSoupTesterEntry,
        // Job::SetData(job, batch+0x3D0, 256), clamped to 3 batches, and -- unlike its Line sibling
        // -- it does NOT write the per-batch flag array at generator+0x123C0.
        //   PolygonSoupTesterEntry                      @0x829157B8   80   this = unk_83123940 +
        //                                                                  spuId*0x19040, spuId<6
        //   PolygonSoupTesterJob::Execute               @0x82915930  107   switch on GetType():
        //                                                                  2/3/4 -> FillTriangleCache /
        //                                                                  ...Stream / LineTest
        //   ...::ExecuteFillTriangleCacheStream         @0x82915D88  145   the ReadCommand loop
        //   ...::FillTriangleCache                      @0x82915FD0  219   sphere -> AABB -> box query
        //                                                                  -> per-leaf extract
        //   ...::AllocateMemory                         @0x82916B98   99   bump allocator in the job
        //   ...::RunBoxQuery                            @0x82916D28   46   wraps RunQuery
        //   ...::LoadPrimitive                          @0x82916AB8    8
        //   PolygonSoupListSpatialMap::RunQuery         @0x82843A80  261   the spatial traversal
        //   ⭐ ExtractTriangle4ListIntersectingSphere    @0x82844C80  602   THE FUNCTION THAT ACTUALLY
        //                                                                  PRODUCES THE TRIANGLES --
        //                                                                  omitted from every earlier
        //                                                                  costing of this leg
        //   + the three JobDesc/type accessors (28), the leaf-cache Get (46), the three
        //     SimpleDataStreamConsumer entry points (~19, AddResult @0x82868550 is an export hole)
        // TOTAL ~1,650 across ~17. Every one of them carries an IDA symbol; there are no holes in
        // the family itself.
        EA::Jobs::Job* RunFillTriangleCacheStream(
            const CgsGeometric::PolygonSoupListSpatialMap* lpPolySoupListSpacialMap,
            CgsMemory::SimpleDataStreamProducer* lpProducer);                                                    // @0x82810D38

    private:
        u16  CreateNewBatch();                   // h:350 / X360 0x82810960
        void FinishBatch(u16 lu16BatchIndex);    // h:353 / X360 0x82810718

        // ⭐ ADDED 2026-08-14 (walls leg 1). X360 0x82810798 (the truncated export name
        // "PrepareNewPrimitiveTestResultsLi"): claim the next mapCollisionResultLists slot
        // (assert "Ran out of result lists" past 200, CgsCollisionGenerator.cpp:251), carve the
        // 16-byte-console list header + an 80-byte-per-record results buffer out of the result
        // allocator, seat {tags, capacity, count 0, type 0} through the DWARF-named fields, and
        // return the slot index. Called by the two Add* posters above.
        s32 PrepareNewPrimitiveTestResultsList(u16 lu16MaxResults, u32 lu32UserTagA, u16 lu16UserTagB);

        // Allocate + placement-construct one standalone (empty) EA::Jobs::Job out of the result
        // allocator (128-byte aligned, alignment saved/restored). Returns null if the bump
        // allocator overflows. X360 0x82810588. The Run* dispatch family uses the returned job
        // as the parent/root the per-batch jobs depend on.
        EA::Jobs::Job* AllocateJob();

    private:
        // Layout in DWARF declaration order (see file header for X360 offsets).
        CollisionBatch          maCollisionBatches[KU16_MAX_NUM_BATCHES];        // h:322  @+0x80
        CollisionResultList*    mapCollisionResultLists[KU16_MAX_NUM_RESULT_LISTS]; // h:323 @+0x12080
        CgsMemory::LinearMalloc mCollisionResultsAllocator;                      // h:324  @+0x123A0
        u16                     mu16NumUsedResultLists;                          // h:325  @+0x123BC
        u16                     mu16NumUsedBatches;                              // h:326  @+0x123BE
        bool                    mabUsedBatches[KU16_MAX_NUM_BATCHES];            // h:327  @+0x123C0

        // Static "register StartJobs perfmon once" latch (DWARF h:329/330; X360 statics
        // byte_83011D70 / dword_82F310B4).
        static bool _mbInitializedPerfMons;      // h:329
        static s32  _miStartJobsPerfMon;         // h:330
    };

    // ⭐ ADDED 2026-08-06 (PhysicsModule::Update leaves wave). The FULL-SIZE derived generator --
    // the type VehicleManager owns through mpContactGenerator and destroys in FreeAllocations
    // @0x8261BAE0 (whose bl target's mangled name pins the exact qualified type:
    // ??$DestroyIOBuffer@VCollisionGenerator@CgsCollision@CgsSceneManager@@@...). Verbatim from the
    // DecFIGS DWARF (CgsCollisionGenerator.h:382..:391): the base plus one 2 MB results arena.
    // CROSS-CHECK: the console DestroyIOBuffer<CollisionGenerator> instantiation @0x8259DE50 frees
    // exactly 2171904 bytes == 0x12400 (base span) + 0x200000 (this arena) -- the two sources agree.
    // Prepare (DWARF :386) is the no-arg override that feeds the arena to the base's two-arg
    // Prepare; declare-only until its own TU lands (X360 body not yet identified).
    struct CollisionGenerator : public BaseCollisionGenerator
    {
        // ⭐ BODIED 2026-08-06 (big-five #2 wave): the "X360 body not yet identified" note is
        // retired -- VehicleManager::StartVehicleContactGeneration @0x8262AFE4 INLINES it:
        // `addis/addi -> generator + 0x12400 (the arena seat); lis 0x20 (0x200000); bl
        // BaseCollisionGenerator::Prepare`. The derived no-arg override just feeds the embedded
        // 2 MB arena to the base's two-arg Prepare, exactly as this banner always predicted.
        bool Prepare() { return BaseCollisionGenerator::Prepare(mau8CollisionResultsMemory, KI_RESULTS_MEMORY_SIZE); }

    private:
        static const s32 KI_RESULTS_MEMORY_SIZE = 2097152;         // DWARF :390
        u8 mau8CollisionResultsMemory[KI_RESULTS_MEMORY_SIZE];     // DWARF :391
    };
}
}
