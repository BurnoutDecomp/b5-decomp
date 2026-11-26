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
