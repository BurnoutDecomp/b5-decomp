#pragma once

// The job assertion formatting buffer: CgsCore::gacDebugPrintStringBuffer, DWARF
// GameShared/GameClasses/Core/CgsCore.cpp:57 `char gacDebugPrintStringBuffer[8192]`
// (8192 == CgsCore::KI_DEBUGMAXVARGSSTRINGLENGTH, CgsCore.h:105). ARTIST 0x83015F20 (IDA
// byte_83015F20). The job code keeps the older assert expansion LegacySrc shows in full
// (e.g. CgsArray.h:211): `CgsDev::StrStream lStrStream(CgsCore::gacDebugPrintStringBuffer,
// CgsCore::KI_DEBUGMAXVARGSSTRINGLENGTH)`, then PrintStringed. ARTIST's job-TU
// CgsArray<PhysicalRequestInfo,25>::Append 0x82918B98 streams "Array container out of
// space" into 0x83015F20 with size 0x2000, the same CgsArray.h assert.
namespace CgsCore
{
    extern char gacDebugPrintStringBuffer[8192];
}

// Worker assertions use the PrintStringed argument order and omit the function name.
#if defined(CGS_MATCH_ASSERT_LOCATIONS) && CGS_MATCH_ASSERT_LOCATIONS
#define CGS_JOB_ASSERT_LOG(artistFile, artistLine, message) CgsDev::Assert::PrintStringed(artistFile, 0, artistLine, message)
#else
#define CGS_JOB_ASSERT_LOG(artistFile, artistLine, message) CgsDev::Assert::PrintStringed(__FILE__, 0, __LINE__, message)
#endif

// The job-side assertion break: set MSR[SE] (0x400, single-step) so the debugger stops right
// after PrintStringed, saving r12 below the stack pointer by hand around it. Every job-side
// assertion in ARTIST ends this way, in optimised and /Od TUs alike: RadixSortEntry
// 0x82AD20F4 / 0x82AD211C, PolygonSoupTesterEntry 0x82915868, PolygonSoupTesterJob::AllocateMemory
// 0x82916C2C / 0x82916C5C (439 sites in the image, all inline, every one with the r12 save).
// mfmsr/mtmsr have no XDK intrinsic and no XDK or EA SDK library holds the sequence, and the XDK
// compiler never adds the std/ld r12 pair by itself, so the five instructions are written here.
// This is the one inline-assembly block the byte-match campaign allows (DECISIONS.md D13).
// PS3 counterpart (LegacySrc, e.g. CgsArray.h:211): `PrintStringed(...); asm("tw 31,1,1");`.
// PROVISIONAL NAME: no DWARF/PS3/SDK name found for this macro; ARTIST 0x82AD20F4.
// Each instruction is its own one-line block: the XDK inline assembler rejects the `__asm`
// keyword inside a braced block, and a macro joins its lines into one.
// Remaining difference (open, scratch/BYTEMATCH_1123/msgs/sdk-r6-d13-emit.txt): the XDK compiles
// every function holding an __asm block without optimisation (its listing says "Function compile
// flags: /Odtu"), so the job entry points of optimised TUs (RadixSortEntry, RelocatorEntry,
// ObjectToMeshEntry, DecompressionJobEntry, ...) come out unoptimised here while ARTIST's are
// optimised. The same five words written with the XDK's own __emit (ppcintrinsics.h defines
// __sync and YieldProcessor that way) keep those functions optimised.
#define CGS_JOB_ASSERT_BREAK()                                                                  \
    __asm { std   r12, -8(r1) }                                                                 \
    __asm { mfmsr r12 }                                                                         \
    __asm { ori   r12, r12, 0x400 }                                                             \
    __asm { mtmsr r12 }                                                                         \
    __asm { ld    r12, -8(r1) }
