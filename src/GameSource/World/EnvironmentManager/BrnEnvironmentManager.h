#pragma once

#include "types.hpp"

namespace BrnWorld
{
namespace EnvironmentSettings
{
// EnvironmentManager -- only the two functions UpdateFromTool (@0x827B0DA8) and
// DiscardCurrSeason (@0x827B0E50) are reconstructed in this TU. The class layout
// below names ONLY the members those two functions provably touch (offsets from
// their asm); everything else is opaque padding. The manager's constructor and the
// full member set (an intrusive receiver/dependency/file-request queue set, a
// resource ptr at +0x424, an embedded AsyncOp, ...) are NOT reconstructed here --
// their asm is not in this TU's dossier, so they are left as padding rather than
// fabricated. A future wave that homes the ctor should replace the pad regions with
// the real members (the +0x424 region is a ResourcePtr, not a list head).
class EnvironmentManager
{
public:
    // Blend / pause state-machine transition driven by the environment tool. Returns
    // whether the manager is (or has just been put) in a blocking operation.
    // @ 0x827B0DA8
    bool UpdateFromTool(bool lbPause);

    // Commit the pending season swap and clear the current-season ref once the season
    // stream-in has completed. @ 0x827B0E50
    void DiscardCurrSeason();

    // Stream-in stage of the season resource. Only the terminal E_STREAMIN_DONE value is
    // attested here (the DiscardCurrSeason assert "meStreamInStage == E_STREAMIN_DONE");
    // the earlier streaming stages exist but are not exercised by this TU.
    enum EStreamInStage
    {
        E_STREAMIN_DONE = 7,
    };

private:
    u8             mPad0[0x444];                // 0x000  (incl. the +0x424 ResourcePtr region, un-homed)
    // --- named members proven by DiscardCurrSeason (0x827B0E50) / UpdateFromTool (0x827B0DA8) ---
    u32            muCurrSeasonRef;             // 0x444  cleared to 0 by DiscardCurrSeason
    u8             mbCurrSeason;                // 0x448  (byte) set from muDiscardSeason low byte
    u8             mPad449[3];                  // 0x449
    EStreamInStage meStreamInStage;             // 0x44C  asserted == E_STREAMIN_DONE
    u8             mPad450[0x8C];               // 0x450  (incl. the +0x490 dependency-queue region, un-homed)
    u32            muDiscardSeason;             // 0x4DC  season index copied into mbCurrSeason
    u8             mPad4E0[0x18];               // 0x4E0
    s32            miBlendState;                // 0x4F8  blend/pause state machine (0..3)
    u8             mPad4FC[4];                  // 0x4FC
    s32            miSavedBlendState;           // 0x500  miBlendState saved across a tool blocking op
    u8             mPad504[0x35C];              // 0x504
    s32            miToolUpdateFrameCounter;    // 0x860  reset by UpdateFromTool when entering a blocking op
    u8             mPad864[0x984];              // 0x864  (incl. the +0x1174 file-request-queue + +0x11C8 AsyncOp regions, un-homed)
};
}
}
