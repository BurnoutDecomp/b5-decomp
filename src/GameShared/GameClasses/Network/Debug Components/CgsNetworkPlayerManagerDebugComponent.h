#pragma once

#include "types.hpp"
#include "GameShared/GameClasses/Development/DebugSystem/Core/CgsDebugComponent.h"  // CgsDev::DebugComponent base (complete type; needed for inheritance)

// Reconstructed from BURNOUT_X360_ARTIST.XEX
//   CgsNetwork::PlayerManagerDebugComponent::_QSortMessageAndMax  @ 0x8287F370  (bodied)
//   CgsNetwork::PlayerManagerDebugComponent::OnActivate           @ 0x8287F278  (not yet homed)
//   CgsNetwork::PlayerManagerDebugComponent::DrawRow              @ 0x82893B50  (not yet homed)
//   CgsNetwork::PlayerManagerDebugComponent::DrawBar              @ 0x8288BA28  (not yet verified)
//   CgsNetwork::PlayerManagerDebugComponent::RenderHUD            @ 0x82893C88  (not yet verified)
//
// DebugComponent for the network PlayerManager (bandwidth-usage HUD). Class SHAPE + member
// NAMES from DWARF (CgsNetworkPlayerManagerDebugComponent.h/.cpp); member byte OFFSETS pinned
// to X360 asm.
//
// LAYOUT (X360 offsets, accessed BY NAME): the CgsDev::DebugComponent base occupies
// +0x00..+0x0B, then this class adds its members starting at +0x0C. Offsets cross-checked in
// RenderHUD asm: lwz 0xC (mpPlayerManager), lfs 0x10 (mfMaxBandwidthForGraph),
// lfs 0x14 (mfMaxBandwidth), lbz 0x18 (mbDrawBandwidthUsage), lbz 0x19
// (mbShowUsedRegisteredMessages), lwz 0x1C (miAverageType).
//
// SCOPE: _QSortMessageAndMax, OnActivate, DrawRow and RenderHUD are bodied (in the .cpp).
// DrawBar stays declared-but-unbodied: its per-bar geometry is an intricate VMX clamp chain
// whose exact draw-line endpoints did not verify, so it is left blocked rather than
// reconstructed with uncertain coordinates. RenderHUD / DrawRow only call it.

namespace CgsNetwork
{
    struct PlayerManager;
    struct NetworkPlayer;
    typedef s32 EMessageType;
}

namespace CgsDev
{
    class Debug2DImmediateRender;
}

namespace CgsNetwork
{
    struct PlayerManagerDebugComponent : public CgsDev::DebugComponent
    {
        // (No standalone X360 symbol in this TU; declared for the call surface, bodied elsewhere.)
        void Construct(PlayerManager* lpPlayerManager);
        void Prepare();
        void Release();
        void Destruct();

        // qsort element: one message type and its max byte count. 8-byte stride (matches the asm
        // qsort SizeOfElements == 8 and the (type@0, bytes@4) field reads).
        struct MessageTypeAndMax
        {
            EMessageType meMessageType;   // +0x00
            s32          miMaxBytes;      // +0x04
        };

    protected:
        // @0x8287F278 -- register the debug-menu surface (not yet homed).
        virtual void OnActivate();

        // @0x82893C88 -- the player-manager overlay (not yet verified).
        virtual void RenderHUD(CgsDev::Debug2DImmediateRender* lpRender);

        // @0x82893B50 -- build one message-type row + forward to DrawBar (not yet homed).
        void DrawRow(s32 liIndex, s32 liMessageType, s32 liMaxBytes,
                     NetworkPlayer* lpPlayer, CgsDev::Debug2DImmediateRender* lpRender);

        // @0x8288BA28 -- draw one labelled bandwidth bar (not yet verified).
        void DrawBar(s32 liIndex, const char* lpcName, f32 lfValue, f32 lfMax,
                     f32 lfX, f32 lfMaxOnGraph, CgsDev::Debug2DImmediateRender* lpRender);

    private:
        // @0x8287F370 -- qsort comparator: order MessageTypeAndMax by miMaxBytes descending.
        static int _QSortMessageAndMax(const void* lpA, const void* lpB);

        // ---- members (byte offsets pinned to the X360 asm; see header note) ----
        PlayerManager* mpPlayerManager;                 // +0x0C
        f32            mfMaxBandwidthForGraph;           // +0x10
        f32            mfMaxBandwidth;                   // +0x14
        bool           mbDrawBandwidthUsage;             // +0x18
        bool           mbShowUsedRegisteredMessages;     // +0x19
        s32            miAverageType;                    // +0x1C
    };
}
