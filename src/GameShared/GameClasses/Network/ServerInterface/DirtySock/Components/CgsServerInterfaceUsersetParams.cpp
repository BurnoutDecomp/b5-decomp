#include "CgsServerInterfaceUsersetParams.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"
#include "GameShared/GameClasses/Development/CgsStrStream.h"  // CgsDev::StrStream (over-length assert)
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

namespace
{
    // The over-length guard inlined from CgsStringUtils.h:55 (StrnCpy bounds check).
    // Mirrors the sibling ServerInterfacePlayerParams.cpp helper verbatim.
    void FireStringTooLong(const char* lpcValue)
    {
        CgsDev::Assert::BeginAssert();
        char lacMessageBuffer[CgsDev::Assert::KI_MESSAGEBUFFERSIZE];
        CgsDev::StrStream lStrStream(lacMessageBuffer, CgsDev::Assert::KI_MESSAGEBUFFERSIZE);
        lStrStream << "String too long: " << (lpcValue ? lpcValue : "<NULLSTRING>");
        CgsDev::Assert::FireAssert(
            lacMessageBuffer,
            "..\\..\\..\\GameShared\\GameClasses\\Core/CgsStringUtils.h",
            55);
        CgsDev::Assert::EndAssert();
    }
}

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
        CGS_ASSERT(lpcRecord != 0, "lpcString");

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

        // Emit the custom userset structure blob only when the structure exposes
        // data (GetData, non-const slot +0x10) and a non-zero size (GetDataSize, +0xC).
        if (const_cast<ServerInterfaceUsersetParamsBase*>(this)->GetData() && GetDataSize())
        {
            CGS_ASSERT(std::strlen(GetPattern()) < static_cast<u32>(GetPatternLength()),
                       "strlen( GetPattern()) < (size_t) GetPatternLength()");

            const char* lpcPattern = GetPattern();
            const u32   luDataSize = GetDataSize();
            const void* lpData     = const_cast<ServerInterfaceUsersetParamsBase*>(this)->GetData();
            TagFieldSetStructure(lpcRecord, liRecLen, "PARAMS",
                                 lpData, static_cast<s32>(luDataSize), lpcPattern);
        }

        TagFieldSetNumber(lpcRecord, liRecLen, "SIZE", miMaxNumPlayers);
        TagFieldSetNumber(lpcRecord, liRecLen, "TYPE", miType);
        TagFieldSetFlags(lpcRecord, liRecLen, "CUSTFLAGS", static_cast<s32>(muCustomFlags));

        const s32 liSysFlags = ConvertUsersetFlags(static_cast<int>(muUsersetFlags), 1u);
        TagFieldSetFlags(lpcRecord, liRecLen, "SYSFLAGS", liSysFlags);
    }

    // SerialiseFromUserset @ 0x82889BF8 -- populate this block from a raw DirtySDK
    //   lobby userset record (LobbyApiUserSetT*). Fields are read by the documented
    //   byte offsets the X360 asm uses (verified store-for-store). lpUserset must be
    //   non-null (asserted).
    void ServerInterfaceUsersetParamsBase::SerialiseFromUserset(const void* lpUserset)
    {
        CGS_ASSERT(lpUserset != 0, "lpUserset");

        const u8* lpcRec = static_cast<const u8*>(lpUserset);

        // The record carries no password; clear it, then copy name (+0x28) and
        // description (+0x4C) via the bounds-checked setters.
        SetPassword("");
        SetName(reinterpret_cast<const char*>(lpcRec + 0x28));
        SetDescription(reinterpret_cast<const char*>(lpcRec + 0x4C));

        miMaxNumPlayers = *reinterpret_cast<const s32*>(lpcRec + 0x20); // stw -> 0x9C
        miType          = *reinterpret_cast<const s32*>(lpcRec + 0x04); // stw -> 0x94

        // Host name lives at record + 0x10; copy 16 bytes into macHostName, guarding
        // the over-length case (inlined StrnCpy<16> bounds check).
        const char* lpcHostName = reinterpret_cast<const char*>(lpcRec + 0x10);
        if (std::strlen(lpcHostName) >= 0x10u)
        {
            FireStringTooLong(lpcHostName);
        }
        std::strncpy(macHostName, lpcHostName, KI_USERSETPARAMS_HOSTNAME_LENGTH);

        miNumPlayers   = *reinterpret_cast<const s32*>(lpcRec + 0x24); // stw -> 0x98
        miUsersetId    = *reinterpret_cast<const s32*>(lpcRec + 0x00); // stw -> 0x90
        muCustomFlags  = *reinterpret_cast<const u32*>(lpcRec + 0x0C); // stw -> 0xA4
        muUsersetFlags = static_cast<u32>(ConvertUsersetFlags(
                            *reinterpret_cast<const s32*>(lpcRec + 0x08), 0)); // -> 0xA0

        // Parse the custom userset structure blob (record + 0x90) into the
        // structure-interface data buffer, but only when the structure exposes data
        // and a non-zero size. The binary reads GetData through the const overload
        // (vtable slot +0x14).
        const void* lpcData = static_cast<const ServerInterfaceUsersetParamsBase*>(this)->GetData();
        if (lpcData && GetDataSize())
        {
            CGS_ASSERT(std::strlen(GetPattern()) < static_cast<u32>(GetPatternLength()),
                       "strlen( GetPattern()) < (size_t) GetPatternLength()");

            const char* lpcPattern   = GetPattern();
            const u32   luDataSize   = GetDataSize();
            const void* lpcParseInto =
                static_cast<const ServerInterfaceUsersetParamsBase*>(this)->GetData();
            TagFieldGetStructure(reinterpret_cast<const char*>(lpcRec + 0x90),
                                 const_cast<void*>(lpcParseInto),
                                 static_cast<s32>(luDataSize), lpcPattern);
        }
    }
}
