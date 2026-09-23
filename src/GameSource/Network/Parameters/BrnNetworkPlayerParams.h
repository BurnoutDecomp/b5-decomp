#ifndef BRN_NETWORK_PLAYER_PARAMS_H
#define BRN_NETWORK_PLAYER_PARAMS_H

#include "types.hpp"
#include "GameShared/GameClasses/Network/ServerInterface/DirtySock/X360/CgsServerInterfacePlayerParamsX360.h" // CgsNetwork::ServerInterfacePlayerParamsX360 (base)
#include "GameShared/GameClasses/Core/CgsID.h"                 // CgsID (car-id accessors)
#include "GameShared/GameClasses/System/Timer/CgsFrameRate.h"  // CgsSystem::EFrameRate
#include "GameSource/GameState/BrnGameStateSharedIO.h"         // BrnGameState::GameStateModuleIO::EPlayerTeam
#include "GameSource/Network/SharedIO/BrnNetworkSharedIO.h"    // BrnNetwork::NetworkPlayerID

// ===========================================================================
// BrnNetwork::PlayerParamsBase
//   Home: GameSource/Network/Parameters/BrnNetworkPlayerParams.{h,cpp}
//   DWARF: references/DecFIGS/dwarfdump/GameSource/Network/Parameters/BrnNetworkPlayerParams.h
//
// The game-side player-parameter intermediate: it adds the 28-byte replicated
// lobby payload (mData) on top of the platform server-interface player params
// and implements the ServerInterfaceStructureInterface pattern/data virtuals
// over it. The DWARF base is the platform alias `CgsNetwork::
// ServerInterfacePlayerParams`; on X360 the concrete base is the X360 leaf --
// attested by mData landing at X360 object offset +0x84 == end of
// ServerInterfacePlayerParamsX360 (base 0x60 + macNetworkAddress[36]).
//
// X360 OBJECT LAYOUT (32-bit ABI, documentation only -- access is BY NAME):
//   +0x00  vptr
//   +0x04..+0x5F  ServerInterfacePlayerParamsBase (machine address, name, ident, ...)
//   +0x60..+0x83  ServerInterfacePlayerParamsX360::macNetworkAddress[36]
//   +0x84  mData  (CLobbyPlayerParamsData, 28 bytes -> object ends +0xA0)
//
// STATICS (X360): macPattern == byte_82FB7150, mbPatternPrepared == byte_82FB7164
// (0x14 apart -> the DWARF macPattern[20] length is confirmed by the guard byte
// sitting immediately after the buffer).
//
// The leaf built on the stack by the BrnNetwork managers is
// BrnNetwork::PlayerParams (BrnNetworkPlayerParamsClass.h) -- vtable
// off_82083550; see that header for the recovered slot map.
// ===========================================================================

namespace BrnNetwork
{
    class PlayerParamsBase : public CgsNetwork::ServerInterfacePlayerParamsX360
    {
    public:
        // The replicated lobby payload (DWARF BrnNetworkPlayerParams.h:159, nested
        // in PlayerParamsBase). 28-byte wire record -- the serialisation pattern
        // "13sbbblll" PreparePattern builds is exactly this struct: a 13-char
        // string, three bytes, three longs.
        struct CLobbyPlayerParamsData
        {
            // mxFlags bits (h:160..164)
            static const s32 KX_IS_READY   = 1;
            static const s32 KX_IS_PLAYING = 2;
            static const s32 KX_IS_50HZ    = 4;
            static const s32 KX_HAS_FEVER  = 8;
            static const s32 KX_DEVELOPER  = 16;
            // Console-only bit (see IsCarDeformed); FLAG: name is ours.
            static const s32 KX_CAR_DEFORMED = 32;

            // muCarColourIndex packing (h:165..167): car colour in the low 16 bits,
            // paint finish in the high 16.
            static const u32 KU_LOW_BITS_MASK    = 0x0000FFFFu;
            static const u32 KU_HIGH_BITS_MASK   = 0xFFFF0000u;
            static const u32 KU_COLOUR_BIT_SHIFT = 16;

            char macCarId[13];          // +0   (X360 obj +0x84) h:169
            s8   mi8PlayerTeam;         // +13  (X360 obj +0x91) h:170
            s8   mxFlags;               // +14  (X360 obj +0x92) h:171
            s8   mi8PlayerColourIndex;  // +15  (X360 obj +0x93) h:172
            s32  mMarkedPlayer;         // +16  (X360 obj +0x94) h:173
            s32  miRank;                // +20  (X360 obj +0x98) h:174
            u32  muCarColourIndex;      // +24  (X360 obj +0x9C) h:175
        };

        // DWARF declares both; the X360 inlines them at every construction site
        // (the ctor's surviving store is the leaf vptr, the dtor's the base
        // chain's). Empty out-of-line bodies in the .cpp anchor the vtable
        // (StructureInterface convention).
        PlayerParamsBase();
        virtual ~PlayerParamsBase();

        // --- ServerInterfaceStructureInterface implementation over mData ------------
        // @ 0x8258A818 -- platform prepare, then reset the payload's bookkeeping.
        virtual bool Prepare();
        // @ 0x82584148 (DWARF .cpp:78, void) -- build the shared pattern string once.
        void PreparePattern();
        // @ 0x825841B8 -- the shared static pattern string.
        virtual const char* GetPattern() const;
        // @ 0x825841C8 -- sizeof(CLobbyPlayerParamsData) == 28.
        virtual u32 GetDataSize() const;
        // @ 0x825841D0 -- &mData (the const overload is COMDAT-folded onto the same
        // X360 body; both vtable slots of off_82083550 hold 0x825841D0).
        virtual void* GetData();
        virtual const void* GetData() const;

        // --- replicated-payload accessors with recovered bodies (.cpp) --------------
        void                                         GetFreeBurnCarID(CgsID* lpCarId);   // @ 0x82541D50 (h:348) VOID
        BrnGameState::GameStateModuleIO::EPlayerTeam GetPlayerTeam() const;              // @ 0x82541BA0 (h:194)
        void                                         SetConsoleFrameRate(CgsSystem::EFrameRate leFrameRate); // @ 0x82541C38 (h:286)
        void                                         SetFreeBurnCarID(CgsID liId);       // @ 0x82541CF0 (h:339)

        // Header-inline accessor (DWARF h:395). Attested inlined in
        // TeamSelectionManager::ActionAssignFFAStuntRunTeams @ 0x8256C278 /
        // ActionAutobalanceStuntRunTeams @ 0x8256C4C8 (lbz obj+0x93 ; extsb).
        s32 GetPlayerColourIndex() const { return mData.mi8PlayerColourIndex; }

        // Header-inline payload accessors: every console call site inlines them as a
        // plain load/store or bit twiddle on mData (the stack PlayerParams of the
        // network manager's status / menu-data writers and the state manager's
        // parameter updates).
        bool IsReady() const     { return (mData.mxFlags & CLobbyPlayerParamsData::KX_IS_READY) != 0; }
        bool IsPlaying() const   { return (mData.mxFlags & CLobbyPlayerParamsData::KX_IS_PLAYING) != 0; }
        bool HasFever() const    { return (mData.mxFlags & CLobbyPlayerParamsData::KX_HAS_FEVER) != 0; }
        bool IsDeveloper() const { return (mData.mxFlags & CLobbyPlayerParamsData::KX_DEVELOPER) != 0; }
        void SetReady(bool lbReady)
        {
            mData.mxFlags = static_cast<s8>(mData.mxFlags & ~CLobbyPlayerParamsData::KX_IS_READY);
            if (lbReady)
                mData.mxFlags = static_cast<s8>(mData.mxFlags | CLobbyPlayerParamsData::KX_IS_READY);
        }
        void SetPlaying(bool lbPlaying)
        {
            mData.mxFlags = static_cast<s8>(mData.mxFlags & ~CLobbyPlayerParamsData::KX_IS_PLAYING);
            if (lbPlaying)
                mData.mxFlags = static_cast<s8>(mData.mxFlags | CLobbyPlayerParamsData::KX_IS_PLAYING);
        }
        void SetHasFever(bool lbHasFever)
        {
            mData.mxFlags = static_cast<s8>(mData.mxFlags & ~CLobbyPlayerParamsData::KX_HAS_FEVER);
            if (lbHasFever)
                mData.mxFlags = static_cast<s8>(mData.mxFlags | CLobbyPlayerParamsData::KX_HAS_FEVER);
        }

        // mxFlags bit 0x20: set when the chosen freeburn car's deformation amount is not
        // below 0.85; readers turn it back into a menu deformation of 0.85 or 0.0.
        // FLAG: the bit and both accessor names are ours (no reference name).
        bool IsCarDeformed() const { return (mData.mxFlags & CLobbyPlayerParamsData::KX_CAR_DEFORMED) != 0; }
        void SetIsCarDeformed(bool lbIsCarDeformed)
        {
            mData.mxFlags = static_cast<s8>(mData.mxFlags & ~CLobbyPlayerParamsData::KX_CAR_DEFORMED);
            if (lbIsCarDeformed)
                mData.mxFlags = static_cast<s8>(mData.mxFlags | CLobbyPlayerParamsData::KX_CAR_DEFORMED);
        }

        NetworkPlayerID GetMarkedPlayerID() const                   { return mData.mMarkedPlayer; }
        void            SetMarkedPlayerID(NetworkPlayerID lMarkedPlayerID) { mData.mMarkedPlayer = lMarkedPlayerID; }
        s32             GetRank() const                             { return mData.miRank; }
        // Inlined by the matchmaking create / join / quick-join actions.
        void            SetRank(s32 liRank)                         { mData.miRank = liRank; }
        void SetIsDeveloper(bool lbIsDeveloper)
        {
            mData.mxFlags = static_cast<s8>(mData.mxFlags & ~CLobbyPlayerParamsData::KX_DEVELOPER);
            if (lbIsDeveloper)
                mData.mxFlags = static_cast<s8>(mData.mxFlags | CLobbyPlayerParamsData::KX_DEVELOPER);
        }

        // The team byte is stored without a range check (the state manager's team swap).
        void SetPlayerTeam(BrnGameState::GameStateModuleIO::EPlayerTeam lePlayerTeam)
        {
            mData.mi8PlayerTeam = static_cast<s8>(lePlayerTeam);
        }

        // Car colour in the low 16 bits of muCarColourIndex, paint finish in the high 16.
        u16 GetCarColourIndex() const
        {
            return static_cast<u16>(mData.muCarColourIndex & CLobbyPlayerParamsData::KU_LOW_BITS_MASK);
        }
        u16 GetPaintFinishIndex() const
        {
            return static_cast<u16>(mData.muCarColourIndex >> CLobbyPlayerParamsData::KU_COLOUR_BIT_SHIFT);
        }
        void SetCarColourIndex(u16 lu16CarColourIndex)
        {
            mData.muCarColourIndex = (mData.muCarColourIndex & CLobbyPlayerParamsData::KU_HIGH_BITS_MASK)
                                   + lu16CarColourIndex;
        }
        void SetPaintFinishIndex(u16 lu16PaintFinishIndex)
        {
            mData.muCarColourIndex = (mData.muCarColourIndex & CLobbyPlayerParamsData::KU_LOW_BITS_MASK)
                                   + (static_cast<u32>(lu16PaintFinishIndex) << CLobbyPlayerParamsData::KU_COLOUR_BIT_SHIFT);
        }

        // The remote console's simulation rate travels as the 50 Hz flag bit
        // (BrnNetworkPlayer::Prepare inlines it: byte +0x92, bit 2).
        CgsSystem::EFrameRate GetConsoleFrameRate() const
        {
            return (mData.mxFlags & CLobbyPlayerParamsData::KX_IS_50HZ) != 0
                       ? CgsSystem::E_FRAMERATE_50HZ : CgsSystem::E_FRAMERATE_60HZ;
        }

    protected:
        // DWARF h:178. X360 object offset +0x84; on the PC host the compiler places
        // it naturally after the (pointer-widened) base -- access stays BY NAME.
        CLobbyPlayerParamsData mData;

        // Class statics (DWARF .cpp:22-23; X360 byte_82FB7150 / byte_82FB7164).
        static char macPattern[20];
        static bool mbPatternPrepared;
    };
}

#endif // BRN_NETWORK_PLAYER_PARAMS_H
