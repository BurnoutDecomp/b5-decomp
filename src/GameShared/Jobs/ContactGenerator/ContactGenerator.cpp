// =================================================================================================
// GameShared/Jobs/ContactGenerator/ContactGenerator.cpp
//
// ContactGeneratorEntry @0x82920F10 (80) -- the EA::Jobs entry point every CollisionBatch is
// wired to. The console's path for this TU is baked into its one assert:
//   D:\P4\B5_MAIN\Burnout\MAIN\Code\GameShared\Jobs\ContactGenerator\ContactGenerator.cpp:55
//
// ⚠️ THIS FUNCTION IS AN X360 EXPORT-SET HOLE -- there is no 0x82920F10.json among the 30,084,
// because the export directory jumps 0x82920D78 -> 0x82921050 with nothing between. Neither its
// name nor its body is guessed:
//   * The NAME comes out of a neighbour's xref table -- `ContactGeneratorJob::Execute`
//     @0x829267E0 has exactly one `xrefs_to` entry and IDA spells it `ContactGeneratorEntry`.
//     ("Missing from the JSON export" is not the same as "unnamed in the IDA database.")
//   * The BODY was lifted from the image with ppcdis.py, 80 instructions, and every `bl` target
//     resolved against the 30,084-entry name index. The lift is quoted below.
//
// ```
// 0x82920F10  mflr   r12
// 0x82920F14  bl     __savegprlr_29
// 0x82920F1C  mr     r29, r4                 ; the SECOND EA::Jobs::Param -- the job data
// 0x82920F20  bl     0x82B42560              ; EA::Jobs::GetJobThreadId
// 0x82920F24  lis    r12, 0xE0FF             ; \
// 0x82920F28  rldicl r11, r3, 0, 32          ;  | spuId = ((threadId & 0xFFFFFFFF)
// 0x82920F2C  ori    r12, r12, 0xFFEF        ;  |          - 0xF8000088) >> 2
// 0x82920F30  rldicr r12, r12, 3, 60         ;  | (0xFFFFFFFFE0FFFFEF << 3 == -0xF8000088
// 0x82920F34  add    r11, r11, r12           ;  |  in 64-bit two's complement -- checked by
// 0x82920F38  rldicl r30, r11, 62, 2         ; /   hand against the exported twin below)
// 0x82920F3C  cmpli  cr6, r30, 6
// 0x82920F40  blt    cr6, 0x82921020         ; in range -> straight to the dispatch
// 0x82920F44..0x8292101C                     ; StrStream "SPU Id out of range: " + PrintStringed(:55)
// 0x82921020  lis    r11, 1
// 0x82921028  ori    r9,  r11, 0x300         ; 0x10300 == sizeof(ContactGeneratorJob)
// 0x82921030  mullw  r10, r10, r9
// 0x8292102C  lis    r11, 0x831C
// 0x82921034  addi   r11, r11, -16512        ; 0x831BBF80 == &gaContactGeneratorJobs[0]
// 0x8292103C  add    r3,  r10, r11
// 0x82921038  mr     r4,  r29
// 0x82921040  bl     0x829267E0              ; ContactGeneratorJob::Execute(this, jobData)
// ```
// The spuId arithmetic is the same value as the already-landed PolygonSoupTesterEntry
// @0x829157B8, which spells it openly (`oris r11, r11, 0xF800` -> 0xF8000088; `subf`; `divdu 4`).
// This one folds the subtraction into a 64-bit constant. Two entries, two encodings, one formula
// -- which is the cross-check that the fold was decoded right.
//
// ⚠️⚠️ FLAG PC-platform leaf: THE SPU ID, and only that.
// `(EA::Jobs::GetJobThreadId() - 0xF8000088) / 4` is X360 hardware-thread-address arithmetic.
// There is no such id on this host and EA::Jobs::GetJobThreadId has no body in this tree at all.
// The console's INTENT is "index this thread's private job context", and on this single-threaded
// PC build there is exactly one. Only that one computation is replaced; the bounds assert, the
// six-entry context array, its 0x10300 stride and the Execute call are all the console's. Same
// precedent, same marker and the same one-line justification as PolygonSoupTester.cpp and
// CgsLooseOctree.cpp:997.
// =================================================================================================

#include "GameShared/Jobs/ContactGenerator/ContactGeneratorJob.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"   // CGS_ASSERT

#include "SDKs/EATech/eajobs/job_types.h"            // EA::Jobs::Param

// The entry's signature is EA::Jobs' local-job entry shape (four Params); the second Param is the
// payload EA::Jobs::Job::SetData attached, i.e. the CollisionBatch's 256-byte descriptor slot.
// The X360 spills all four and reads only the second (`mr r29, r4` and nothing else).
void ContactGeneratorEntry(EA::Jobs::Param laParam0,
                           EA::Jobs::Param laParam1,
                           EA::Jobs::Param laParam2,
                           EA::Jobs::Param laParam3)
{
    (void)laParam0;
    (void)laParam2;
    (void)laParam3;

    // ---- FLAG PC-platform leaf: the job-thread index ----
    // X360: spuId = ((EA::Jobs::GetJobThreadId() & 0xFFFFFFFF) - 0xF8000088) >> 2.
    const s32 liJobThreadIndex = 0;

    CGS_ASSERT(liJobThreadIndex < KI_NUM_CONTACT_GENERATOR_JOBS,
               "SPU Id out of range: ");   // ContactGenerator.cpp:55

    gaContactGeneratorJobs[liJobThreadIndex].Execute(laParam1.mpValue);
}
