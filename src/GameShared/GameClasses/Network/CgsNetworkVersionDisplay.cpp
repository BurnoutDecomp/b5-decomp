#include "GameShared/GameClasses/Network/CgsNetworkVersionDisplay.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"
#include "GameShared/GameClasses/Core/CgsStringUtils.h"
#include "GameShared/GameClasses/Development/DebugSystem/Core/UI/CgsDebugUI.h"
#include "GameShared/GameClasses/Development/DebugSystem/Core/UI/CgsTypes.h"
#include "GameShared/GameClasses/Development/DebugSystem/Render/CgsDebug2DImmediateRender.h"

// Reconstructed from BURNOUT_X360_ARTIST.XEX
//   CgsNetwork::VersionDisplay::GetName   @ 0x8286FA60
//   CgsNetwork::VersionDisplay::Prepare   @ 0x8286F9F0
//   CgsNetwork::VersionDisplay::RenderHUD @ 0x8286FA70
//
// The debug-HUD overlay that prints the network build banner. See the header for the
// layout/DWARF notes.

namespace CgsNetwork
{
    // The debug-render / debug-UI value types the RenderHUD body reaches unqualified (the
    // sibling debug components live in namespace CgsDev so reach these directly; here the
    // component lives in CgsNetwork, so alias them by name).
    using CgsDev::Debug2DImmediateRender;
    using CgsDev::RGBA;
    namespace DebugUI = CgsDev::DebugUI;

    // ------------------------------------------------------------------------------------
    // File-scope constants RenderHUD reads (consolidator-owned; see header + TU issues):
    //   KAPC_SERVER_TYPES - the seven server-type display names indexed by EServerType. Only
    //   element 0 ("Local Server") is byte-attested in the X360 asm (off_82F33478@l comment);
    //   elements 1..6 are inferred from the EServerType enum names and are NOT byte-attested.
    const char* const KAPC_SERVER_TYPES[E_SERVER_TYPE_COUNT] =
    {
        "Local Server",   // E_SERVER_TYPE_LOCAL  (byte-attested)
        "Dev Server",     // E_SERVER_TYPE_DEV    (inferred)
        "Test Server",    // E_SERVER_TYPE_TEST   (inferred)
        "Juice Server",   // E_SERVER_TYPE_JUICE  (inferred)
        "Artist Server",  // E_SERVER_TYPE_ARTIST (inferred)
        "Demo 1 Server",  // E_SERVER_TYPE_DEMO_1 (inferred)
        "Demo 2 Server",  // E_SERVER_TYPE_DEMO_2 (inferred)
    };

    // The banner text colour (X360: a .rdata data-load, value not byte-attested) and the
    // text size (X360 flt_820E8C0C == 12.0f).
    static const RGBA KU_TEXT_COLOUR = 0xFFFFFFFFu;
    static const f32  KF_TEXT_SIZE   = 12.0f;

    // X360 0x8286FA60. The component's debug-menu display name (rodata "Network Version Display").
    const char* VersionDisplay::GetName() const
    {
        return "Network Version Display";
    }

    // X360 0x8286F9F0. Seed the version-display component from the network prepare params: the server
    // version string, an associated count (muField_04, shown as "/%d" in the HUD), and the server type
    // (range-checked against [E_SERVER_TYPE_LOCAL, E_SERVER_TYPE_COUNT)). The game-server-game flag
    // starts cleared (SetGameServerGame drives it later). Returns true (asm li r3,1).
    bool VersionDisplay::Prepare(const char* lpcVersion, u32 luField_04, EServerType leServerType)
    {
        CGS_ASSERT(
            (leServerType >= E_SERVER_TYPE_LOCAL) && (leServerType < E_SERVER_TYPE_COUNT),
            "(leServerType>=E_SERVER_TYPE_LOCAL) && (leServerType<E_SERVER_TYPE_COUNT)");

        mpcVersion       = lpcVersion;
        muField_04       = luField_04;
        meServerType     = leServerType;
        mbGameServerGame = false;

        return true;
    }

    // X360 0x8286FA70. Draw the one-line network build banner along the bottom-right of the debug HUD:
    //   "Server:  <server-type>    Version:<version-or-Not-Available>/<N>"
    // right-aligned to (mfScreenWidth - total-run-width), baseline at mfScreenHeight - mfScreenBorderBottom.
    // Below it, if this is a game-server game, a second right-aligned line
    // "    Rebroadcast server available". Layout is measured with CalcTextWidth first (to right-align),
    // then each segment is drawn left-to-right, advancing x by its measured width.
    void VersionDisplay::RenderHUD(Debug2DImmediateRender* lpDisplay)
    {
        // The trailing "/N" count printed after the version (X360: SnPrintf(buf, 5, "/%d", muField_04)).
        char acNumber[16];
        CgsCore::SnPrintf(acNumber, 5, "/%d", muField_04);

        const DebugUI::Metrics& lrMetrics = GetUI().GetMetrics();
        const f32 lfBottom = lrMetrics.mfScreenBorderBottom;

        const char* lpcServerType = KAPC_SERVER_TYPES[meServerType];
        const char* lpcVersion    = mpcVersion ? mpcVersion : "Not Available";

        // Total width of the run (seeded with the right-border inset), used to right-align x0.
        f32 lfWidth = lrMetrics.mfScreenBorderRight;
        lfWidth += lpDisplay->CalcTextWidth("Server:", KF_TEXT_SIZE);
        lfWidth += lpDisplay->CalcTextWidth(lpcServerType, KF_TEXT_SIZE);
        lfWidth += lpDisplay->CalcTextWidth("    Version:", KF_TEXT_SIZE);
        lfWidth += lpDisplay->CalcTextWidth(lpcVersion, KF_TEXT_SIZE);
        lfWidth += lpDisplay->CalcTextWidth(acNumber, KF_TEXT_SIZE);

        f32       lfX = lrMetrics.mfScreenWidth - lfWidth;
        const f32 lfY = lrMetrics.mfScreenHeight - lfBottom;

        lpDisplay->DrawText("Server:", lfX, lfY, KF_TEXT_SIZE, KU_TEXT_COLOUR);
        lfX += lpDisplay->CalcTextWidth("Server:", KF_TEXT_SIZE);

        lpDisplay->DrawText(KAPC_SERVER_TYPES[meServerType], lfX, lfY, KF_TEXT_SIZE, KU_TEXT_COLOUR);
        lfX += lpDisplay->CalcTextWidth(KAPC_SERVER_TYPES[meServerType], KF_TEXT_SIZE);

        lpDisplay->DrawText("    Version:", lfX, lfY, KF_TEXT_SIZE, KU_TEXT_COLOUR);
        lfX += lpDisplay->CalcTextWidth("    Version:", KF_TEXT_SIZE);

        const char* lpcDrawnVersion;
        if (mpcVersion)
        {
            lpDisplay->DrawText(mpcVersion, lfX, lfY, KF_TEXT_SIZE, KU_TEXT_COLOUR);
            lpcDrawnVersion = mpcVersion;
        }
        else
        {
            lpDisplay->DrawText("Not Available", lfX, lfY, KF_TEXT_SIZE, KU_TEXT_COLOUR);
            lpcDrawnVersion = "Not Available";
        }
        lfX += lpDisplay->CalcTextWidth(lpcDrawnVersion, KF_TEXT_SIZE);

        lpDisplay->DrawText(acNumber, lfX, lfY, KF_TEXT_SIZE, KU_TEXT_COLOUR);

        // Second line: the rebroadcast note, only when this is a game-server game.
        const f32 lfRightEdge    = GetUI().GetMetrics().mfScreenWidth - GetUI().GetMetrics().mfScreenBorderRight;
        const f32 lfRebroadcastY = GetUI().GetMetrics().mfScreenHeight
                                 - (GetUI().GetMetrics().mfScreenBorderBottom + KF_TEXT_SIZE);
        if (mbGameServerGame)
        {
            const f32 lfNoteWidth = lpDisplay->CalcTextWidth("    Rebroadcast server available", KF_TEXT_SIZE);
            lpDisplay->DrawText(
                "    Rebroadcast server available",
                lfRightEdge - lfNoteWidth,
                lfRebroadcastY,
                KF_TEXT_SIZE,
                KU_TEXT_COLOUR);
        }
    }
}
