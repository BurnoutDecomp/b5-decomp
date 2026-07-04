#include "CgsServerInterfaceUsersetParams.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"

#include <string.h>

// Reconstructed from BURNOUT_X360_ARTIST.XEX
//   CgsNetwork::ServerInterfaceUsersetParamsBase::SetName        @ 0x82541598
//   CgsNetwork::ServerInterfaceUsersetParamsBase::SetPassword    @ 0x8286F178
//   CgsNetwork::ServerInterfaceUsersetParamsBase::SetDescription @ 0x8286F278
//
// ---------------------------------------------------------------------------
// The three string setters share one shape (only the destination buffer, its
// capacity and the assert limit differ). Each:
//   1. measures strlen(src)            (asm: `while ( *v4++ ) ;`, end-start-1)
//   2. if strlen >= capacity, fires the CgsStringUtils.h:55 "String too long"
//      assert. The X360 build streams "String too long: " + src into the assert
//      message buffer (CgsDev::Assert::gpcMessageBuffer) before FireAssert;
//      that StrStream message construction is a logging side effect, modelled
//      here through CGS_ASSERT (matching the sibling GameParams TU convention).
//   3. strncpy(dest, src, capacity)    (asm: `strncpy(this+off, src, N)`)
//      and returns the strncpy result (the destination pointer); the return is
//      unused by all callers, so the setters are void here (as in GameParams).
// ---------------------------------------------------------------------------

namespace CgsNetwork
{
    ServerInterfaceUsersetParamsBase::ServerInterfaceUsersetParamsBase()
    {
    }

    ServerInterfaceUsersetParamsBase::~ServerInterfaceUsersetParamsBase()
    {
    }

    // SetName @ 0x82541598 -- strncpy(this+4, src, 36); assert if strlen >= 36.
    void ServerInterfaceUsersetParamsBase::SetName(const char* lpcName)
    {
        CGS_ASSERT(strlen(lpcName) < static_cast<size_t>(KI_USERSETPARAMS_NAME_LENGTH),
                   "String too long");
        strncpy(macName, lpcName, KI_USERSETPARAMS_NAME_LENGTH);
    }

    // SetPassword @ 0x8286F178 -- strncpy(this+40, src, 20); assert if strlen >= 20.
    void ServerInterfaceUsersetParamsBase::SetPassword(const char* lpcPassword)
    {
        CGS_ASSERT(strlen(lpcPassword) < static_cast<size_t>(KI_USERSETPARAMS_PASSWORD_LENGTH),
                   "String too long");
        strncpy(macPassword, lpcPassword, KI_USERSETPARAMS_PASSWORD_LENGTH);
    }

    // SetDescription @ 0x8286F278 -- strncpy(this+60, src, 68); assert if strlen >= 68.
    void ServerInterfaceUsersetParamsBase::SetDescription(const char* lpcDescription)
    {
        CGS_ASSERT(strlen(lpcDescription) < static_cast<size_t>(KI_USERSETPARAMS_DESCRIPTION_LENGTH),
                   "String too long");
        strncpy(macDescription, lpcDescription, KI_USERSETPARAMS_DESCRIPTION_LENGTH);
    }
}
