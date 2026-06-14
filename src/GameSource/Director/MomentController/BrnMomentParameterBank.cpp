#include "types.hpp"

// Reconstructed from BURNOUT_X360_ARTIST.XEX
//   BrnDirector::MomentParameterBank::Construct      @ 0x82209E38
//   BrnDirector::MomentParameterBank::GetParameters  @ 0x821F7428
//
// The bank holds a min/max moment duration followed by a table of per-moment-type parameter
// records (a leading pair of 3-byte records, then seven 8-byte {type, flagA, flagB} records).
// Construct installs the default table; GetParameters maps a parameter-type id to the address
// of its record (id 0 -> none, id 1 -> the whole bank, ids 2..10 -> the individual records).

namespace CgsDev
{
namespace Assert
{
    void  BeginAssert();
    void  FireAssert(const char* pacMessage, const char* pacFile, int liLine);
    void* EndAssert();
}
}

namespace BrnDirector
{
struct MomentRecord
{
    u32 muType;     // +0
    u8  mbFlagA;    // +4
    u8  mbFlagB;    // +5
    u8  mPad[2];
};

class MomentParameterBank
{
public:
    void* Construct();
    void* GetParameters(int liType);

private:
    f32          mfMinDuration;     // +0
    f32          mfMaxDuration;     // +4
    u8           maLeadA[3];        // +8
    u8           maLeadB[3];        // +11
    u8           mPad[2];           // +14
    MomentRecord maRecords[7];      // +16
};

void* MomentParameterBank::Construct()
{
    mfMinDuration = 3.0f;
    mfMaxDuration = 5.0f;

    maLeadA[0] = 1; maLeadA[1] = 0; maLeadA[2] = 1;
    maLeadB[0] = 1; maLeadB[1] = 1; maLeadB[2] = 0;

    static const MomentRecord KA_DEFAULTS[7] =
    {
        { 0, 1, 0, {0, 0} },
        { 0, 1, 1, {0, 0} },
        { 1, 1, 0, {0, 0} },
        { 2, 1, 0, {0, 0} },
        { 3, 1, 0, {0, 0} },
        { 3, 1, 1, {0, 0} },
        { 4, 1, 0, {0, 0} },
    };
    for (int liIndex = 0; liIndex < 7; ++liIndex)
        maRecords[liIndex] = KA_DEFAULTS[liIndex];

    return this;
}

void* MomentParameterBank::GetParameters(int liType)
{
    switch (liType)
    {
        case 0:  return 0;
        case 1:  return this;
        case 2:  return maLeadA;
        case 3:  return maLeadB;
        case 4:  return &maRecords[0];
        case 5:  return &maRecords[1];
        case 6:  return &maRecords[2];
        case 7:  return &maRecords[3];
        case 8:  return &maRecords[4];
        case 9:  return &maRecords[5];
        case 10: return &maRecords[6];
        default:
            CgsDev::Assert::BeginAssert();
            CgsDev::Assert::FireAssert(
                "unhandled parameter type",
                "..\\..\\..\\GameSource\\Director/MomentController/BrnMomentParameterBank.cpp",
                195);
            CgsDev::Assert::EndAssert();
            return 0;
    }
}
}
