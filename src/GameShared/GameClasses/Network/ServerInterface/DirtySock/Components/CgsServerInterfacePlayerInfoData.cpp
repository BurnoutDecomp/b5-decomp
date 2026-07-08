#include "GameShared/GameClasses/Network/ServerInterface/DirtySock/Components/CgsServerInterfacePlayerInfoData.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"            // CgsDev::Assert Begin/Fire/End
#include "GameShared/GameClasses/Development/CgsStrStream.h"  // CgsDev::StrStream (over-length assert)
#include "lobbytagfield.h"                                    // DirtySDK TagFieldGetStructure
#include <cstring>                                            // std::strlen / std::strncpy

// Reconstructed from BURNOUT_X360_ARTIST.XEX
//   CgsNetwork::ServerInterfacePlayerInfoDataBase::Prepare           @ 0x828797E0
//   CgsNetwork::ConvertHWFlags                                  @ 0x82879768
//   CgsNetwork::ServerInterfacePlayerInfoDataBase::SerialiseFromUser @ 0x828798A8
//
// ConvertHWFlags folds a lobby hardware-flag word onto the player-info hardware-flag
// space using the {mask,value} mapping table. SerialiseFromUser copies the lobby user
// record (a DirtySDK lobby user struct) into the named members -- each string copy is
// the inlined StrnCpy<N> with its CgsStringUtils.h:55 over-length guard -- folds the
// user flags, converts the HW flags, and finally parses the custom structure blob.
//
// FLAGGED:
//  * The {mask,value} mapping tables (KAAI_HW_FLAGS_TO_PLAYER_INFO_FLAGS and
//    KAAI_LOBBY_USER_FLAGS_TO_PLAYER_INFO_FLAGS) are private statics whose exact byte
//    contents are not recoverable from the available exports; they are declared extern
//    here as honest placeholders (defined in the not-yet-reconstructed data segment).
//  * The lobby user struct has no reconstructed home; its fields are read at the exact
//    byte offsets the X360 asm uses (verified store-for-store).

namespace CgsNetwork
{
    // {value, mask} fold table for hardware flags (CgsServerInterfacePlayerInfoData.cpp:57).
    //   KI_NUM_HW_FLAGS == 14 entries; ConvertHWFlags ORs in value when (mask & in) == mask.
    //   Field order matches the X360 asm layout verbatim: each 8-byte entry is read as
    //   value at +0, mask at +4 (both ConvertHWFlags @ 0x82879768 and the inlined fold
    //   in SerialiseFromUser @ 0x82879CE8 read mask at the +4 slot of every entry).
    struct FlagMapping
    {
        u32 muValue;
        u32 muMask;
    };

    const s32 KI_NUM_HW_FLAGS = 14;
    const s32 KI_NUM_FLAGS    = 17;

    // HONEST PLACEHOLDERS: real contents live in the unrecovered .rdata of the X360 image.
    extern const FlagMapping KAA_HW_FLAGS_TO_PLAYER_INFO_FLAGS[KI_NUM_HW_FLAGS];
    extern const FlagMapping KAA_LOBBY_USER_FLAGS_TO_PLAYER_INFO_FLAGS[KI_NUM_FLAGS];
}

namespace
{
    using CgsNetwork::FlagMapping;

    // The inlined StrnCpy<N> over-length guard (CgsStringUtils.h:55).
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

    // strncpy with the leading StrnCpy<liMax> over-length bounds check.
    void StrnCpyChecked(char* lpcDest, const char* lpcSrc, s32 liMax)
    {
        if (std::strlen(lpcSrc) >= static_cast<u32>(liMax))
        {
            FireStringTooLong(lpcSrc);
        }
        std::strncpy(lpcDest, lpcSrc, static_cast<u32>(liMax));
    }

    // Fold an input flag word through a {mask,value} table.
    u32 FoldFlags(u32 luInput, const FlagMapping* lpTable, s32 liCount)
    {
        u32 luResult = 0;
        for (s32 li = 0; li < liCount; ++li)
        {
            if ((lpTable[li].muMask & luInput) == lpTable[li].muMask)
            {
                luResult |= lpTable[li].muValue;
            }
        }
        return luResult;
    }
}

namespace CgsNetwork
{

s32 ConvertHWFlags(s32 liFlags, EConversionFlags leDirection)
{
    // The X360 build picks one of two interleaved columns of the mapping table by the
    // direction argument (cntlzw-based select). Only the to-player-info direction
    // (E_CONVERSION_FROM_WIRE == 0, the sole reachable caller) is reconstructed: fold the
    // input flag word through the 14-entry hardware-flag mapping table.
    (void)leDirection;
    return static_cast<s32>(FoldFlags(static_cast<u32>(liFlags),
                                      KAA_HW_FLAGS_TO_PLAYER_INFO_FLAGS,
                                      KI_NUM_HW_FLAGS));
}

bool ServerInterfacePlayerInfoDataBase::Prepare()
{
    // Zero the string / attr buffers (the X360 XMemSet moves @ 0x828797FC..0x82879840)
    // then reset each scalar (the run of stw @ 0x82879850..0x82879890). miID starts at -1.
    std::memset(macName,     0, sizeof(macName));      // +0x04, 16
    std::memset(macMotto,    0, sizeof(macMotto));     // +0x14, 132
    std::memset(macLocation, 0, sizeof(macLocation));  // +0x98, 20
    std::memset(macClanTag,  0, sizeof(macClanTag));   // +0xAC, 8
    std::memset(macTitleId,  0, sizeof(macTitleId));   // +0xB4, 8

    maAttr[0]   = 0;   // +0xBC
    maAttr[1]   = 0;   // +0xBD
    maAttr[2]   = 0;   // +0xBE
    maAttr[3]   = 0;   // +0xBF

    miID        = -1;  // +0xC0
    muInfoFlags = 0;   // +0xC4
    miRank      = 0;   // +0xC8
    miLocality  = 0;   // +0xCC
    miGameID    = 0;   // +0xD0
    miField_D4  = 0;   // +0xD4
    miField_D8  = 0;   // +0xD8
    miField_DC  = 0;   // +0xDC
    miField_E0  = 0;   // +0xE0
    miField_E4  = 0;   // +0xE4
    muHWFlags   = 0;   // +0xE8
    miField_EC  = 0;   // +0xEC
    miField_F0  = 0;   // +0xF0

    return true;
}

bool ServerInterfacePlayerInfoDataBase::SerialiseFromUser(const void* lpUser)
{
    CGS_ASSERT(lpUser != 0, "lpUser");

    const u8* lpcRec = static_cast<const u8*>(lpUser);

    StrnCpyChecked(macName,     reinterpret_cast<const char*>(lpcRec + 0x008), 16);
    StrnCpyChecked(macMotto,    reinterpret_cast<const char*>(lpcRec + 0x12C), 132);
    StrnCpyChecked(macLocation, reinterpret_cast<const char*>(lpcRec + 0x218), 20);
    StrnCpyChecked(macClanTag,  reinterpret_cast<const char*>(lpcRec + 0x22C), 8);
    StrnCpyChecked(macTitleId,  reinterpret_cast<const char*>(lpcRec + 0x018), 8);

    maAttr[0] = lpcRec[0x1B8];
    maAttr[1] = lpcRec[0x1B9];
    maAttr[2] = lpcRec[0x1BA];
    maAttr[3] = lpcRec[0x1BB];

    const s32 liID = *reinterpret_cast<const s32*>(lpcRec + 0x000);
    miID = (liID != 0) ? liID : -1;

    muInfoFlags = FoldFlags(*reinterpret_cast<const u32*>(lpcRec + 0x004),
                            KAA_LOBBY_USER_FLAGS_TO_PLAYER_INFO_FLAGS,
                            KI_NUM_FLAGS);

    miRank      = *reinterpret_cast<const s32*>(lpcRec + 0x1B4);
    miLocality  = *reinterpret_cast<const s32*>(lpcRec + 0x024);
    miGameID    = *reinterpret_cast<const s32*>(lpcRec + 0x1B0);
    miField_D4  = *reinterpret_cast<const s32*>(lpcRec + 0x1CC);
    miField_D8  = *reinterpret_cast<const s32*>(lpcRec + 0x1C4);
    miField_DC  = *reinterpret_cast<const s32*>(lpcRec + 0x020);
    miField_E0  = *reinterpret_cast<const s32*>(lpcRec + 0x1BC);
    miField_E4  = *reinterpret_cast<const s32*>(lpcRec + 0x1C0);
    muHWFlags   = static_cast<u32>(ConvertHWFlags(*reinterpret_cast<const s32*>(lpcRec + 0x1C8),
                                                  E_CONVERSION_FROM_WIRE));
    miField_EC  = *reinterpret_cast<const s32*>(lpcRec + 0x210);
    miField_F0  = *reinterpret_cast<const s32*>(lpcRec + 0x214);

    // Parse the custom structure blob (held at lobby-user + 0x28) into the data buffer,
    // gated on a non-null buffer + the GetPattern()/GetPatternLength() bounds assert.
    // The X360 uses the const GetData() slot (vtable +0x14) here.
    const ServerInterfacePlayerInfoDataBase* lpcThis = this;
    const void* lpData = lpcThis->GetData();
    if (lpData == 0)
    {
        return false;
    }

    CGS_ASSERT(std::strlen(GetPattern()) < static_cast<u32>(GetPatternLength()),
               "strlen( GetPattern() ) < (size_t) GetPatternLength()");

    const char* lpcPattern = GetPattern();
    const u32   luDataSize = GetDataSize();
    void*       lpDataBuf  = const_cast<void*>(lpcThis->GetData());
    return TagFieldGetStructure(reinterpret_cast<const char*>(lpcRec + 0x28),
                                lpDataBuf, static_cast<s32>(luDataSize), lpcPattern) != 0;
}

}
