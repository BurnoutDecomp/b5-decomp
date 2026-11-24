#pragma once

// ARTIST byte_83015F20: shared job assertion formatting buffer.
extern char byte_83015F20[8192];

// Worker assertions use the PrintStringed argument order and omit the function name.
#if defined(CGS_MATCH_ASSERT_LOCATIONS) && CGS_MATCH_ASSERT_LOCATIONS
#define CGS_JOB_ASSERT_LOG(artistFile, artistLine, message) CgsDev::Assert::PrintStringed(artistFile, 0, artistLine, message)
#else
#define CGS_JOB_ASSERT_LOG(artistFile, artistLine, message) CgsDev::Assert::PrintStringed(__FILE__, 0, __LINE__, message)
#endif
