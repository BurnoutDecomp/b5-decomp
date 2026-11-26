#include "CgsServerInterfaceUsersetParams.h"
#include "GameShared/GameClasses/Core/CgsAssertMessageAt.h"
#include "GameShared/GameClasses/Core/CgsStringUtils.h"      // CgsCore::StrnCpy (over-length assert)
#include "lobbyapi.h"                                         // LobbyApiUserSetT
#include "lobbytagfield.h"                                    // DirtySDK TagFieldGet/Set*

#include <string.h>
#include <cstring>                                            // std::strlen / std::strncpy

// Reconstructed from BURNOUT_X360_ARTIST.XEX
//   CgsNetwork::ServerInterfaceUsersetParamsBase::SetName               @ 0x82541598
//   CgsNetwork::ServerInterfaceUsersetParamsBase::SetPassword           @ 0x8286F178
//   CgsNetwork::ServerInterfaceUsersetParamsBase::SetDescription        @ 0x8286F278
//   CgsNetwork::ServerInterfaceUsersetParamsBase::Prepare               @ 0x8287C708
//   CgsNetwork::ServerInterfaceUsersetParamsBase::SerialiseToString     @ 0x828899D0
//   CgsNetwork::ServerInterfaceUsersetParamsBase::SerialiseFromUserset  @ 0x82889BF8
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
//
// Prepare / SerialiseToString / SerialiseFromUserset are the tagfield (de)serialise
// virtuals, mirroring the sibling PlayerParams / GameParams components.
// ---------------------------------------------------------------------------

// The StrnCpy over-length guard is CgsCore::StrnCpy's own (CgsStringUtils.h:53, assert
// at :55); a file-local copy of it collided with its siblings in CgsNetworkUnity.

#define CGS_ARTIST_FILE \
    "..\\..\\..\\GameShared\\GameClasses\\Network\\ServerInterface\\DirtySock\\Components\\CgsServerInterfaceUsersetParams.cpp"

namespace CgsNetwork
{
    // Flag translation table (CgsServerInterfaceUsersetFlags.cpp @ 0x8287C7C0). No public
    // header, forward-declared here as the two serialise bodies below both call it.
    int ConvertUsersetFlags(int lxFlags, unsigned int luIndex);

    ServerInterfaceUsersetParamsBase::ServerInterfaceUsersetParamsBase()
    {
    }

    ServerInterfaceUsersetParamsBase::~ServerInterfaceUsersetParamsBase()
    {
    }

    // Prepare @ 0x8287C708 -- reset the userset-params block to defaults; returns true.
    //   Clears the three string fields to "" via the setters, zeroes the host-name
    //   buffer's first byte, and seeds the scalar defaults.
    bool ServerInterfaceUsersetParamsBase::Prepare()
    {
        SetName("");
        SetPassword("");
        SetDescription("");

        macHostName[0]  = 0;      // stb 0,  0x80(this)
        miType          = 0;      // stw 0,  0x94(this)
        miMaxNumPlayers = 8;      // stw 8,  0x9C(this)
        miUsersetId     = -1;     // stw -1, 0x90(this)
        miNumPlayers    = 0;      // stw 0,  0x98(this)
        muUsersetFlags  = 0;      // stw 0,  0xA0(this)
        muCustomFlags   = 0;      // stw 0,  0xA4(this)

        return true;
    }

    // SerialiseToString @ 0x828899D0 -- emit this block into a tagfield record.
    //   lpcRecord must be non-null (asserted).
    void ServerInterfaceUsersetParamsBase::SerialiseToString(char* lpcRecord, s32 liRecLen) const
    {
        const char* const lpcSource =
            "..\\..\\..\\GameShared\\GameClasses\\Network\\ServerInterface\\DirtySock\\Components\\"
            "CgsServerInterfaceUsersetParams.cpp";
        CGS_ASSERT_AT(lpcRecord != 0, "lpcString", lpcSource, 84);

        if (macName[0])
        {
            TagFieldSetString(lpcRecord, liRecLen, "NAME", macName);
        }
        if (macDescription[0])
        {
            // NOTE: the X360 build gates on macDescription[0] but passes macName
            // (this+4) as the DESC value -- reproduced verbatim.
            TagFieldSetString(lpcRecord, liRecLen, "DESC", macName);
        }
        TagFieldSetString(lpcRecord, liRecLen, "PASS", macPassword);

        // Serialize the custom structure through its const accessor when it has data and a size.
        if (GetData() && GetDataSize())
        {
            CGS_ASSERT_AT(std::strlen(GetPattern()) < static_cast<u32>(GetPatternLength()),
                "strlen( GetPattern()) < (size_t) GetPatternLength()", lpcSource, 102);

            TagFieldSetStructure(lpcRecord, liRecLen, "PARAMS",
                                 GetData(), static_cast<s32>(GetDataSize()), GetPattern());
        }

        TagFieldSetNumber(lpcRecord, liRecLen, "SIZE", miMaxNumPlayers);
        TagFieldSetNumber(lpcRecord, liRecLen, "TYPE", miType);
        TagFieldSetFlags(lpcRecord, liRecLen, "CUSTFLAGS", static_cast<s32>(muCustomFlags));

        const s32 liSysFlags = ConvertUsersetFlags(static_cast<int>(muUsersetFlags), 1u);
        TagFieldSetFlags(lpcRecord, liRecLen, "SYSFLAGS", liSysFlags);
    }

    // SerialiseFromUserset @ 0x82889BF8 -- populate this block from the lobby's userset record.
    void ServerInterfaceUsersetParamsBase::SerialiseFromUserset(const LobbyApiUserSetT* lpUserset)
    {
        CGS_ASSERT_AT(lpUserset != 0, "lpUserset", CGS_ARTIST_FILE, 128 /* ARTIST 0x82889C24 */);

        // The record carries no password.
        SetPassword("");
        SetName(lpUserset->strName);
        SetDescription(lpUserset->strDesc);
        SetMaxPlayers(lpUserset->iSize);
        SetType(lpUserset->iType);
        CgsCore::StrnCpy(macHostName, lpUserset->strOwner, KI_USERSETPARAMS_HOSTNAME_LENGTH);
        miNumPlayers   = lpUserset->iCount;
        miUsersetId    = lpUserset->iIdent;
        muCustomFlags  = lpUserset->uCustFlags;
        muUsersetFlags = static_cast<u32>(ConvertUsersetFlags(static_cast<int>(lpUserset->uSysFlags), 0));

        // The custom structure blob, parsed only when the structure exposes data. The
        // mutable GetData() is vtable slot +0x14 (MSVC lays the two overloads out in
        // reverse declaration order), ARTIST 0x82889D84.
        if (GetData() && GetDataSize())
        {
            CGS_ASSERT_AT(std::strlen(GetPattern()) < static_cast<u32>(GetPatternLength()),
                          "strlen( GetPattern()) < (size_t) GetPatternLength()",
                          CGS_ARTIST_FILE, 146 /* ARTIST 0x82889E10 */);
            TagFieldGetStructure(lpUserset->strParams, GetData(), GetDataSize(), GetPattern());
        }
    }
}

#undef CGS_ARTIST_FILE
