#include "types.hpp"
#include "GameShared/GameClasses/Core/CgsStringUtils.h"
#include "GameShared/GameClasses/Core/CgsID.h"                 // CgsID, CgsIDCompress/UnCompress, KI_CGSID_STRING_LEN
#include "GameShared/GameClasses/Core/CgsAssert.h"             // CGS_ASSERT
#include "GameShared/GameClasses/System/Timer/CgsFrameRate.h"  // CgsSystem::EFrameRate
#include "GameSource/GameState/BrnGameStateSharedIO.h"         // BrnGameState::GameStateModuleIO::EPlayerTeam

#include <string.h>   // strncpy, strlen

// Reconstructed from BURNOUT_X360_ARTIST.XEX
//   BrnNetwork::PlayerParamsBase::GetData        @ 0x825841D0
//   BrnNetwork::PlayerParamsBase::GetDataSize    @ 0x825841C8
//   BrnNetwork::PlayerParamsBase::GetPattern     @ 0x825841B8
//   BrnNetwork::PlayerParamsBase::Prepare        @ 0x8258A818
//   BrnNetwork::PlayerParamsBase::PreparePattern @ 0x82584148
//
// PlayerParamsBase wraps a 28-byte network-replicated payload living at byte offset
// 132 of the object. GetData/GetDataSize/GetPattern expose the payload, its size, and
// the (lazily built) serialisation pattern string. PreparePattern builds that pattern
// once into a shared static buffer: "%dsbbblll" formatted with a per-subclass field
// count obtained through the virtual at vtable slot 2. Prepare resets the payload's
// bookkeeping fields after the platform server-params prepare succeeds.

// Platform server-interface params (other TU); declared for the compile-only gate.
namespace CgsNetwork
{
    struct ServerInterfacePlayerParamsX360
    {
        static int Prepare();
    };
}

namespace BrnNetwork
{
    namespace
    {
        // Static serialisation-pattern buffer (X360 byte_82FB7150) and its
        // build-once guard (byte_82FB7164).
        char gaPatternBuffer[13];
        bool gbPatternBuilt = false;
    }

    // Replicated lobby payload (BrnNetworkPlayerParams.h:159). 28-byte on-disk record
    // (GetDataSize()==28) living at object offset 132 (+0x84).
    struct CLobbyPlayerParamsData
    {
        static const s32 KX_IS_READY   = 1;   // h:160
        static const s32 KX_IS_PLAYING = 2;   // h:161
        static const s32 KX_IS_50HZ    = 4;   // h:162
        static const s32 KX_HAS_FEVER  = 8;   // h:163
        static const s32 KX_DEVELOPER  = 16;  // h:164

        char macCarId[13];          // +0   (obj +132) h:169
        s8   mi8PlayerTeam;         // +13  (obj +145) h:170
        s8   mxFlags;               // +14  (obj +146) h:171
        s8   mi8PlayerColourIndex;  // +15  (obj +147) h:172
        s32  mMarkedPlayer;         // +16  (obj +148) h:173
        s32  miRank;                // +20  (obj +152) h:174
        u32  muCarColourIndex;      // +24  (obj +156) h:175
    };

    class PlayerParamsBase
    {
    public:
        void* GetData()      { return &mData; }        // object offset 132 (+0x84)
        int   GetDataSize()  { return 28; }
        void* GetPattern()   { return gaPatternBuffer; }

        int  Prepare();
        void* PreparePattern();

        // Replicated-payload accessors (this batch). Signatures are DWARF-authoritative
        // (BrnNetworkPlayerParams.h): GetFreeBurnCarID(CgsID*) returns VOID; GetPlayerTeam()
        // returns EPlayerTeam; SetConsoleFrameRate takes CgsSystem::EFrameRate; SetFreeBurnCarID
        // takes CgsID (replaces the earlier declared-only int overload).
        void                                         GetFreeBurnCarID(CgsID* lpCarId);   // @ 0x82541D50 (h:348) VOID
        BrnGameState::GameStateModuleIO::EPlayerTeam GetPlayerTeam() const;              // @ 0x82541BA0 (h:194)
        void                                         SetConsoleFrameRate(CgsSystem::EFrameRate leFrameRate); // @ 0x82541C38
        void                                         SetFreeBurnCarID(CgsID liId);       // @ 0x82541CF0 (h:339)

    private:
        // Base + prior members are not modelled here; the replicated payload lands at object
        // offset 132 (+0x84). Prepare()'s byte-granular pokes target this region by raw offset
        // off `this` (the established pattern for this un-DWARFed serialised block); the
        // accessors reach named sub-fields.
        u8  mPad0[132];                 // +0x00 .. +0x83
        CLobbyPlayerParamsData mData;   // object offset 132 (+0x84)
    };

    void* PlayerParamsBase::PreparePattern()
    {
        if (!gbPatternBuilt)
        {
            gbPatternBuilt = true;

            // Virtual call at vtable slot 2 -> per-subclass leading field count.
            typedef int (*PatternCountFn)(void*);
            void** lpVtable = *reinterpret_cast<void***>(this);
            int liCount = reinterpret_cast<PatternCountFn>(lpVtable[2])(this);

            CgsCore::SPrintf(gaPatternBuffer, sizeof(gaPatternBuffer), "%dsbbblll", liCount);
        }
        return this;
    }

    int PlayerParamsBase::Prepare()
    {
        // Platform server-params prepare must succeed first.
        if (!CgsNetwork::ServerInterfacePlayerParamsX360::Prepare())
            return 0;

        PreparePattern();

        u8* lpThis = reinterpret_cast<u8*>(this);
        u8 luFlags = lpThis[146] & 0xF8;   // preserve high 5 bits
        lpThis[145] = 0;
        lpThis[152] = 0;
        lpThis[148] = 0xFF;                // -1
        lpThis[146] = luFlags;
        lpThis[158] = 0;
        lpThis[147] = 0xFF;                // -1
        SetFreeBurnCarID(0);
        return 1;
    }

    // GetFreeBurnCarID @ 0x82541D50 -- read the stored car-id string, translate the
    // '?' placeholders back to spaces, compress it to a CgsID, and hand it out
    // through *lpCarId. DWARF-declared return type is void.
    void PlayerParamsBase::GetFreeBurnCarID(CgsID* lpCarId)
    {
        CGS_ASSERT(lpCarId != nullptr, "lpCarId");

        // The stored string uses '?' where a space would otherwise sit; measure it in
        // its current form and fire the CgsStringUtils.h:55 length assert if it has
        // overrun its 13-byte field.
        CGS_ASSERT(strlen(mData.macCarId) < static_cast<size_t>(KI_CGSID_STRING_LEN),
                   "String too long: ");

        char laCarId[KI_CGSID_STRING_LEN];
        strncpy(laCarId, mData.macCarId, KI_CGSID_STRING_LEN);
        for (int i = 0; i < KI_CGSID_STRING_LEN; ++i)
        {
            if (laCarId[i] == '?')
                laCarId[i] = ' ';
        }

        *lpCarId = CgsIDCompress(laCarId);
    }

    // GetPlayerTeam @ 0x82541BA0 -- return the signed player-team enum, bracketed by
    // the [E_PLAYER_TEAM_NONE, 9) range asserts (the upper bound is the compiled
    // literal 9, even though the rodata names E_PLAYER_TEAM_COUNT).
    BrnGameState::GameStateModuleIO::EPlayerTeam PlayerParamsBase::GetPlayerTeam() const
    {
        CGS_ASSERT(mData.mi8PlayerTeam >= 0,
                   "mData.mi8PlayerTeam >= GsmIO::E_PLAYER_TEAM_NONE");
        CGS_ASSERT(mData.mi8PlayerTeam < 9,
                   "mData.mi8PlayerTeam < GsmIO::E_PLAYER_TEAM_COUNT");
        return static_cast<BrnGameState::GameStateModuleIO::EPlayerTeam>(mData.mi8PlayerTeam);
    }

    // SetConsoleFrameRate @ 0x82541C38 -- record the 50Hz flag from the requested
    // console frame rate. UNKNOWN(-1) leaves the flag cleared; 50 sets it; 60 clears
    // it; any other value is invalid.
    void PlayerParamsBase::SetConsoleFrameRate(CgsSystem::EFrameRate leFrameRate)
    {
        mData.mxFlags &= static_cast<s8>(~CLobbyPlayerParamsData::KX_IS_50HZ);

        if (leFrameRate != CgsSystem::E_FRAMERATE_UNKNOWN)
        {
            if (leFrameRate == CgsSystem::E_FRAMERATE_50HZ)
            {
                mData.mxFlags |= CLobbyPlayerParamsData::KX_IS_50HZ;
            }
            else if (leFrameRate != CgsSystem::E_FRAMERATE_60HZ)
            {
                CGS_ASSERT(false, "Invalid frame rate ");
            }
        }
    }

    // SetFreeBurnCarID @ 0x82541CF0 -- un-compress a CgsID into the stored car-id
    // string, then translate spaces to '?' placeholders so the field never carries a
    // literal space over the wire.
    void PlayerParamsBase::SetFreeBurnCarID(CgsID liId)
    {
        CgsIDUnCompress(liId, mData.macCarId);
        for (int i = 0; i < KI_CGSID_STRING_LEN; ++i)
        {
            if (mData.macCarId[i] == ' ')
                mData.macCarId[i] = '?';
        }
    }
}
