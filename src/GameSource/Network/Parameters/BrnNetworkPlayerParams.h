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

        // Declared-only (DWARF h:243/h:221/h:265): the X360 inlines these
        // payload-bit twiddles at every call site; no standalone body to recover.
        void SetPlaying(bool lbPlaying);
        void SetReady(bool lbReady);
        void SetMarkedPlayerID(NetworkPlayerID lMarkedPlayerID);

        // (The remaining DWARF accessor set -- SetPlayerTeam/IsReady/IsPlaying/
        // GetRank/SetRank/GetConsoleFrameRate/Get+SetCarColourIndex/
        // Get+SetPaintFinishIndex/HasFever/SetHasFever/IsDeveloper/SetIsDeveloper --
        // is left undeclared until an X360 call site attests each one.)

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
