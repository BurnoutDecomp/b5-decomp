#pragma once

// =================================================================================================
// CgsSceneManager::CgsCollision::PrimitiveListWithTriangleListStreamJobDesc -- the descriptor for
// the PRIMITIVE-PAIR-LIST vs TRIANGLE-LIST *stream* collision job (job-type id 12), plus the
// fixed-stride command record that flows through it.
//
// ⭐ THIS IS THE FAMILY BREAKABLE PROPS RUN ON. BrnPhysics::Props::PropManager::
// BeginPropWorldContactGeneration @0x82628CB0 creates the stream, DoPart/DoPropInstanceWorld-
// ContactGeneration post one command per prop/part, and RunCollidePrimitiveListWithTriangleList-
// Stream @0x82811F58 dispatches it. Without it a smashed prop's parts have no world collision at
// all and free-fall until KVF_PROP_OUT_OF_WORLD_HEIGHT deletes them.
//
// ---- WHERE THIS TYPE'S DWARF HOME IS, STATED PLAINLY --------------------------------------------
// The DecFIGS DWARF homes this class in the NON-Stream sibling header:
//   references/DecFIGS/dwarfdump/.../JobDescription/CgsPrimitiveListWithTriangleListJobDesc.h
//     :107  struct PrimitiveListWithTriangleListStreamJobDesc : public CollisionJobDescription
//     :111  struct StreamCommand { :113 PrimitivePairList mPairList; :114 TriangleList
//           mTriangleList; :115 CollisionResultList* mpResultsList;
//           :116 bool mbUseOptimisedBoxTests; }
//     :121 Construct  :124 Destruct  :128 bool Prepare(SimpleDataStreamProducer*)
//     :131 Release    :134 SimpleDataStreamProducer* GetDataStreamProducer()
//     :139  struct Data { :141 SimpleDataStreamProducer* mpStreamProducer; }
// (verified this wave by reading that dumpfile directly, not inherited from a park banner).
//
// It is landed in its OWN file here, which is the tree's established placement for every *Stream*
// descriptor whose DWARF home is a non-Stream header: CgsLineWithTriangleListStreamJobDesc.h and
// CgsFillTriangleCacheStreamJobDesc.h are both separate tree files while the DWARF homes
// LineWithTriangleListStreamJobDesc in CgsLineWithTriangleListJobDesc.h:56 and
// FillTriangleCacheStreamJobDesc in CgsFillTriangleCacheJobDesc.h:80. (The sphere/swept families
// went the other way and share one header.) NOT A FORK: `grep -rn
// PrimitiveListWithTriangleListStreamJobDesc b5-decomp/src vendor` finds exactly one definition,
// this one. FOLLOW-UP FOR WHOEVER OWNS CgsPrimitiveListWithTriangleListJobDesc.h: folding this
// block into that header (and deleting this file) is a zero-risk tidy that would put the family
// back on its DWARF home; it is not done here because that header is outside this owner's scope.
//
// ---- X360 GROUNDING, RE-MEASURED THIS WAVE (not inherited) --------------------------------------
// AddPrimitiveListWithTriangleListToStream @0x82811D40 is an EXPORT-SET HOLE (no
// .ida-exports/BURNOUT_X360_ARTIST.XEX/0x82811D40.json). Its 35 instructions were read out of a
// PRIVATE copy of IDA Files/BURNOUT_X360_ARTIST.XEX.i64 with headless IDA 9.3 this wave
// (scratchpad/waveQ6/ida_pstream/, raw instruction words included), and they give the command
// record store-for-store:
//     0x82811D84  stw   [pairList +0]  -> cmd +0x00 |
//     0x82811D88  lwz   [pairList +4]                | PrimitivePairList, 12 CONSOLE bytes
//     0x82811D8C  stw                  -> cmd +0x04 |
//     0x82811D90  lwz   [pairList +8]                |
//     0x82811D94  stw                  -> cmd +0x08 |
//     0x82811D98  ld    [triList  +0]                  TriangleList, 8 CONSOLE bytes
//     0x82811D9C  std                  -> cmd +0x0C
//     0x82811DB0  stw   mapCollisionResultLists[idx] -> cmd +0x14  (4 CONSOLE bytes)
//     0x82811D7C  stb   the bool arg (r26)           -> cmd +0x18
// == 25 CONSOLE bytes rounded to the family's 16-alignment == the console command size 32
// (`li r4, 0x20` at 0x82811EA4 and `li r5, 0x20` at 0x82811F30, both inside
// CreateCollidePrimitiveListWithTriangleListStream @0x82811DD0).
//
// The descriptor's own five stores are INLINED into RunCollidePrimitiveListWithTriangleListStream
// @0x82811F58 (batch base +0x350 is the CollisionJobDescriptionStorage slot; the asm reaches it
// as generator+0x3D0 == 0x80 maCollisionBatches + 0x350):
//     0x82811FF8  stw  r22, 0x3D0  -> +0x00  mpStreamProducer = the producer
//     0x82811FF0  stw  r27, 0x4C0  -> +0xF0  mpResultsList    = NULL
//     0x82811FE8  stfs f31, 0x4C4  -> +0xF4  mfRadius         = 0.0f   (flt_82001CC0; the four
//                                            bytes at that address read 00 00 00 00 -- measured
//                                            in the same idat run, not assumed)
//     0x82811FF4  stw  r27, 0x4C8  -> +0xF8  mpDebugStream    = NULL   (this family carries NO
//                                            DebugRenderStreamReader; Run takes no reader arg)
//     0x82811FEC  stb  r23, 0x4CF  -> +0xFF  muJobType        = 12     (`li r23, 0xC`)
// which is exactly the shape the committed LineWithTriangleListStreamJobDesc::Prepare already
// models for its own family.
//
// ---- ⚠️ THE HOST COMMAND SIZE IS A CONTRACT, NOT A LAYOUT (AGENTS.md gotcha 1) -------------------
// The console's 32 counts CONSOLE bytes, and three of the record's fields are 4-byte console
// pointers. On this host the same record is 48 -- MEASURED, not predicted:
// scratchpad/waveQ6/probe_pstream/probe_sizeof.cpp compiles green with
// sizeof(PrimitivePairList)==16, sizeof(TriangleList)==16, sizeof(StreamCommand)==48 and member
// offsets 0 / 16 / 32 / 40 -- still %16 == 0 as the poster requires
// (DataStreamCommandPoster::Construct's own tripwire). These commands are RUNTIME-CARVED, never
// serialised, so the standing rule says widen. The Create factory is therefore constructed with
// `sizeof(StreamCommand)` and NEVER the literal 32: the poster
// (AddPrimitiveListWithTriangleListToStream) and the consumer
// (ContactGeneratorJob::ExecutePrimitiveListWithTriangleListStream) stride the same buffer, and a
// mismatch silently corrupts every command after the first. The static_asserts below are the
// gate, not a comment.
// =================================================================================================

#include "types.hpp"

#include <cstddef>   // offsetof (the layout gates at the foot of this header)

#include "GameShared/GameClasses/SceneManager/Collision/ContactGenerator/JobDescription/CgsCollisionJobDescription.h"
#include "GameShared/GameClasses/SceneManager/Collision/Primitives/CgsPrimitivePairList.h"
#include "GameShared/GameClasses/SceneManager/Collision/Primitives/CgsTriangleList.h"

// Stream producer -- pointer-only use here; full layout in
// Memory/DataStream/CgsSimpleDataStreamProducer.h, included by the TUs that touch it.
namespace CgsMemory { struct SimpleDataStreamProducer; }

namespace CgsSceneManager
{
namespace CgsCollision
{
    struct PrimitiveListWithTriangleListStreamJobDesc : public CollisionJobDescription
    {
        // One posted primitive-pair-vs-triangle-list test. DWARF :111-:116; console offsets in
        // the banner. Written by AddPrimitiveListWithTriangleListToStream @0x82811D40, read by
        // ContactGeneratorJob::ExecutePrimitiveListWithTriangleListStream @0x82926650 (whose
        // four field reads -- cmd+0x00 / +0x0C / +0x14 / +0x18 -- independently confirm this
        // member order from the consumer side).
        struct StreamCommand
        {
            PrimitivePairList    mPairList;                // console +0x00 (12B), host +0x00 (16B)
            TriangleList         mTriangleList;            // console +0x0C  (8B), host +0x10 (16B)
            CollisionResultList* mpResultsList;            // console +0x14  (4B), host +0x20  (8B)
            bool                 mbUseOptimisedBoxTests;   // console +0x18,       host +0x28
        };

        // X360 +0x00, the only member of the derived part (DWARF Data, :139-:141).
        CgsMemory::SimpleDataStreamProducer* mpStreamProducer;

        // DWARF :134.
        CgsMemory::SimpleDataStreamProducer* GetDataStreamProducer() const { return mpStreamProducer; }

        // DWARF :128 spells this `bool Prepare(...)`. Kept `void` to match the committed
        // structural twins LineWithTriangleListStreamJobDesc::Prepare and
        // SphereListWithTriangleListStreamJobDesc::Prepare -- like those, the X360 INLINES this
        // into its Run* dispatcher, so no return value is observable anywhere and the tree's
        // established spelling for an inlined stream-descriptor Prepare wins. The DWARF's
        // spelling is recorded here so a later name/shape join finds either.
        void Prepare(CgsMemory::SimpleDataStreamProducer* lpStreamProducer)
        {
            mpStreamProducer = lpStreamProducer;   // 0x82811FF8

            mpResultsList = NULL;                  // 0x82811FF0
            mfRadius      = 0.0f;                  // 0x82811FE8 (flt_82001CC0 == 0.0f, measured)
            mpDebugStream = 0;                     // 0x82811FF4 -- this family carries NO reader
            muJobType     = static_cast<u8>(E_COLLISIONJOB_PRIMITIVE_LIST_WITH_TRIANGLE_LIST_STREAM);
        }
    };

    // GATES, not comments -- see the banner. The producer is constructed with
    // sizeof(StreamCommand) and both the poster and the consumer stride the shared buffer by it.
    static_assert(sizeof(PrimitiveListWithTriangleListStreamJobDesc::StreamCommand) % 16 == 0,
                  "the collide-stream families require a 16-multiple command stride "
                  "(DataStreamCommandPoster::Construct tripwire)");
    static_assert(offsetof(PrimitiveListWithTriangleListStreamJobDesc::StreamCommand, mTriangleList)
                      == sizeof(PrimitivePairList),
                  "mTriangleList follows mPairList with no gap (console +0x0C after a 12B list)");
    static_assert(offsetof(PrimitiveListWithTriangleListStreamJobDesc::StreamCommand, mpResultsList)
                      == sizeof(PrimitivePairList) + sizeof(TriangleList),
                  "mpResultsList follows mTriangleList with no gap (console +0x14 after an 8B list)");
}
}
