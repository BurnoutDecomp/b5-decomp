#ifndef BRN_EATRAX_IN_GAME_COMPONENT_H
#define BRN_EATRAX_IN_GAME_COMPONENT_H

#include "types.hpp"
#include "GameSource/Gui/Flow/Shared/FlaptComponents/BrnGuiFlaptComponent.h"  // BrnFlaptComponent (base)
#include "GameSource/Gui/Flapt/BrnFlaptTextFieldRef.h"                        // TextFieldRef (embedded)

// ============================================================================
// GameSource/Gui/Flow/Permanent/Components/BrnEATraxInGameComponent.h
//
// BrnGui::EATraxInGameComponent -- the in-game EATrax "now playing" chyron. When a
// new track starts it shows the artist / song / album text, plays the "AnimIn"
// frame, and arms a timer; once the game clock passes that timer it plays "AnimOut"
// and goes invisible. Each visibility transition publishes a GuiEATraxChyronActive
// GUI event (true on show, false on hide) so the rest of the front end can react.
//
// It derives from BrnFlaptComponent (it drives one apt movie clip via the inherited
// mAptRef) and embeds the three child text-field handles by value.
//
// Reconstructed from BURNOUT_X360_ARTIST.XEX. Member names / logical types are from
// the DecFIGS DWARF (BrnEATraxInGameComponent.h), gated on the X360 ledger; offsets
// are proven by the ARTIST asm of Construct @ 0x824249F8, Initialize @ 0x82415C00,
// Prepare @ 0x82415AA8, SetTime @ 0x82415C18, Update @ 0x82439E70 and
// DisplayNewTrackNotification @ 0x82439F08.
//
// LAYOUT (X360-attested; the owner AlwaysAvailableComponentsManager embeds it BY
// VALUE, the next embedded component lands +0x40 later, so the guest footprint is
// 0x40 -- the named members fill +0x00..+0x3C, with 4 bytes of trailing tail pad):
//   +0x00  BrnFlaptComponent base (mpStateInterface @ +0x00, mAptRef @ +0x04)  -- 0x0C
//   +0x0C  meComponentState           ComponentState
//   +0x10  mfCurrentGameTime_Seconds  f32
//   +0x14  mfAnimOutTime_Seconds      f32   (NOT zeroed by Construct/Initialize;
//                                            armed by DisplayNewTrackNotification)
//   +0x18  mArtistNameRef             TextFieldRef  (sizeof 0x0C)
//   +0x24  mSongNameRef               TextFieldRef  (sizeof 0x0C)
//   +0x30  mAlbumNameRef              TextFieldRef  (sizeof 0x0C)
// Every member is accessed BY NAME; there are no raw-offset hacks. (Host pointer
// width differs from the guest, so the byte offsets above are documentary only --
// the embedding manager accesses each component BY NAME too.)
// ============================================================================

namespace CgsGui { class StateInterface; }
namespace BrnFlapt { struct FileRef; }

namespace BrnGui
{
    class EATraxInGameComponent : public BrnFlaptComponent
    {
    public:
        // The chyron's visibility state. The Update state machine compares
        // meComponentState against E_CS_VISIBLE (== 1); E_CS_INVISIBLE (== 0) is the
        // idle value Construct / Initialize / the anim-out path reset to.
        enum ComponentState
        {
            E_CS_INVISIBLE = 0,
            E_CS_VISIBLE   = 1,
        };

        // Construct @ 0x824249F8 -- bind the state interface, invalidate the apt clip
        // + the three text-field handles, and zero the runtime state. lpcMovieClipName
        // is the manager's "EATrax_mc" debug name (unused by the body); liFlags is the
        // manager's 0 (unused).
        void Construct(const char* lpcMovieClipName,
                       CgsGui::StateInterface* lpStateInterface,
                       s32 liFlags);

        // Prepare @ 0x82415AA8 -- bind this component's own apt clip out of lFile, then
        // locate the "EATraxBox_mc" child clip and bind its Artist/Song/Album text
        // fields (Artist set to auto-size).
        void Prepare(const char* lacName, const BrnFlapt::FileRef& lFile);

        // Initialize @ 0x82415C00 -- reset the runtime state (time, visibility) to idle.
        void Initialize();

        // Update @ 0x82439E70 -- while visible, once the game clock passes the anim-out
        // time, play "AnimOut", go invisible and publish GuiEATraxChyronActive(false).
        void Update();

        // SetTime @ 0x82415C18 -- record the current game time (seconds).
        void SetTime(f32 lfGameTime_Seconds);

        // DisplayNewTrackNotification @ 0x82439F08 -- show a new track: arm the
        // anim-out timer, set the three text fields (localised or raw per lbLocalised),
        // play "AnimIn", and publish GuiEATraxChyronActive(true).
        void DisplayNewTrackNotification(const char* lpcArtistName,
                                         const char* lpcSongName,
                                         const char* lpcAlbumName,
                                         bool lbLocalised);

    private:
        ComponentState         meComponentState;            // +0x0C
        f32                    mfCurrentGameTime_Seconds;   // +0x10
        f32                    mfAnimOutTime_Seconds;       // +0x14
        BrnFlapt::TextFieldRef mArtistNameRef;              // +0x18
        BrnFlapt::TextFieldRef mSongNameRef;                // +0x24
        BrnFlapt::TextFieldRef mAlbumNameRef;               // +0x30
    };
}

#endif // BRN_EATRAX_IN_GAME_COMPONENT_H
