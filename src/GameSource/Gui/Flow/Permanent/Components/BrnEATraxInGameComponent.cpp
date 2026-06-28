// ===================================================================================
// BrnGui::EATraxInGameComponent -- the in-game EATrax "now playing" chyron
//   GameSource/Gui/Flow/Permanent/Components/BrnEATraxInGameComponent.cpp
//
//   Construct                   @ 0x824249F8
//   Initialize                  @ 0x82415C00
//   Prepare                     @ 0x82415AA8
//   SetTime                     @ 0x82415C18
//   Update                      @ 0x82439E70
//   DisplayNewTrackNotification @ 0x82439F08
//
// Reconstructed from BURNOUT_X360_ARTIST.XEX. All are non-static members (asm: r3 =
// this). Member access is BY NAME throughout; the X360 Begin/Fire/End dev-assert
// sequence folds into CGS_ASSERT(cond,"msg") per the module house style.
//
// The component shows the artist/song/album text for a fixed interval when a new
// track starts (DisplayNewTrackNotification arms an anim-out timer at
// now + KF_TIME_FOR_DISPLAYING_EA_TRAX_SECS and plays the "AnimIn" frame); Update
// plays "AnimOut" and goes invisible once the game clock passes that timer. Each
// transition publishes a GuiEATraxChyronActive GUI event (true on show, false on
// hide) onto the owning state's large output queue.
//
// The X360 inlined three things this reconstruction restores to named source:
//   * The base bind/reset in Prepare is the inlined BrnFlaptComponent::Prepare with a
//     bare name (no parent), so it is reconstructed as that base call.
//   * The Construct "set invalid" of mAptRef + the three TextFieldRefs is the inlined
//     MovieClipRef::SetInvalid / TextFieldRef::SetInvalid (each just zeroes its
//     pointer words), restored as the explicit pointer clears.
//   * The "AnimIn"/"AnimOut" frame plays go through MovieClipRef::GotoAndPlayLabel
//     (the X360 sub_8246F3E8, the string-keyed GotoAndPlayLabel form).
//
// The two visibility events the X360 builds on the stack and pushes with channel id
// 40 (GuiEventOut), record size 16, are GuiEATraxChyronActive records: a
// GuiEvent<471> header { muHeader0 = 1, muEventType = 471, muHeader2 = 12 } plus a
// trailing bool mbActive. NOTE: the DecFIGS DWARF (PS3) declares
// GuiEATraxChyronActive as GuiEvent<466>; the X360 build stores type id 471 (asm:
// `li r11, 0x1D7`), so the X360 id wins here (rung 1 over rung 2). The record is
// defined locally -- this TU is its only emitter and the global event-id table
// (BrnGuiEventTypeDefs.h) is not yet reconstructed.
// ===================================================================================
#include "GameSource/Gui/Flow/Permanent/Components/BrnEATraxInGameComponent.h"

#include "GameSource/Gui/Flapt/BrnFlaptFileRef.h"                          // BrnFlapt::FileRef (Prepare param)
#include "GameSource/Gui/Flapt/BrnFlaptMovieClipRef.h"                     // MovieClipRef child lookups / GotoAndPlayLabel
#include "GameShared/GameClasses/Gui/Model/State/CgsGuiStateInterface.h"   // CgsGui::StateInterface, GetOutputEventQueue
#include "GameShared/GameClasses/Gui/CgsGuiEvent.h"                        // CgsGui::GuiEvent, CgsModule::Event
#include "GameShared/GameClasses/Core/CgsAssert.h"                         // CGS_ASSERT

namespace BrnGui
{
    // The EATrax box child clip and its three child text fields (X360 .rdata).
    static const char* const KAC_EATRAX_BOX_NAME   = "EATraxBox_mc";
    static const char* const KAC_ARTIST_FIELD_NAME = "ArtistName_txt";
    static const char* const KAC_SONG_FIELD_NAME   = "SongName_txt";
    static const char* const KAC_ALBUM_FIELD_NAME  = "AlbumName_txt";

    // The timeline frames the chyron plays (X360 off_82F24F74 / off_82F24F78).
    static const char* const KAPC_ANIM_IN_FRAME  = "AnimIn";
    static const char* const KAPC_ANIM_OUT_FRAME = "AnimOut";

    // How long the chyron stays up after a new track starts (seconds). The X360 adds
    // this to the current game time to arm mfAnimOutTime_Seconds (asm: fadds with
    // flt_82052E64); Hex-Rays resolves the .rdata float as 4.0.
    static const f32 KF_TIME_FOR_DISPLAYING_EA_TRAX_SECS = 4.0f;

    // The string-id type DisplayNewTrackNotification passes to SetLocalisedText when
    // the supplied track text is a localisation id (asm: `li r5, 9`).
    static const s32 KI_EATRAX_STRING_ID_TYPE = 9;

    // The visibility GUI event the chyron publishes. The X360 fills a GuiEvent<471>
    // header { muHeader0 = 1, muEventType = 471, muHeader2 = 12 } and a trailing
    // bool mbActive, then pushes a 16-byte record onto the state's large output queue
    // with channel id 40 (GuiEventOut). See the file note on the 471-vs-466 id.
    struct GuiEATraxChyronActive : public CgsGui::GuiEvent<471>
    {
        bool mbActive;   // +0x0C (record size 16: 12-byte header + bool + 3 pad)

        GuiEATraxChyronActive() : CgsGui::GuiEvent<471>(1, 12), mbActive(false) {}
    };

    // The GuiEventOut channel id and the on-queue record size the X360 passes to
    // AddEvent (asm: `li r5, 0x28` == 40, `li r6, 0x10` == 16).
    static const s32 KI_GUI_EVENT_OUT_CHANNEL = 40;
    static const s32 KI_CHYRON_EVENT_RECORD_SIZE = 16;

    // Publish a GuiEATraxChyronActive(lbActive) event onto the owning state's output
    // queue. Factored out of Update / DisplayNewTrackNotification (the X360 inlined
    // the same record build + AddEvent at both sites).
    static void PublishChyronActiveEvent(CgsGui::StateInterface* lpStateInterface, bool lbActive)
    {
        GuiEATraxChyronActive lActiveEvent;
        lActiveEvent.mbActive = lbActive;
        lpStateInterface->GetOutputEventQueue()->AddEvent(
            reinterpret_cast<const CgsModule::Event*>(&lActiveEvent),
            KI_GUI_EVENT_OUT_CHANNEL, KI_CHYRON_EVENT_RECORD_SIZE);
    }

    // @ 0x824249F8 -- bind the state interface, invalidate the apt clip + the three
    // text-field handles, and zero the runtime state.
    void EATraxInGameComponent::Construct(const char* /*lpcMovieClipName*/,
                                          CgsGui::StateInterface* lpStateInterface,
                                          s32 /*liFlags*/)
    {
        CGS_ASSERT(lpStateInterface != 0, "lpStateInterface");

        // Inlined base init: bind the state channel and invalidate the apt clip handle.
        mpStateInterface        = lpStateInterface;
        mAptRef.mpMovieClipInst = 0;
        mAptRef.mpTransform     = 0;

        // Idle runtime state (mfAnimOutTime_Seconds is left armed by Display..., not
        // cleared here -- the X360 Construct does not store +0x14).
        meComponentState          = E_CS_INVISIBLE;
        mfCurrentGameTime_Seconds = 0.0f;

        // Invalidate the three text-field handles (inlined TextFieldRef::SetInvalid).
        mArtistNameRef.mpTextFieldInstance = 0;
        mArtistNameRef.mpParentMovie       = 0;
        mArtistNameRef.mpTransform         = 0;

        mSongNameRef.mpTextFieldInstance = 0;
        mSongNameRef.mpParentMovie       = 0;
        mSongNameRef.mpTransform         = 0;

        mAlbumNameRef.mpTextFieldInstance = 0;
        mAlbumNameRef.mpParentMovie       = 0;
        mAlbumNameRef.mpTransform         = 0;
    }

    // @ 0x82415AA8 -- bind this component's own apt clip, then locate the EATrax box
    // child clip and bind its Artist / Song / Album child text fields.
    void EATraxInGameComponent::Prepare(const char* lacName, const BrnFlapt::FileRef& lFile)
    {
        // Bind + reset this component's movie clip (bare-name lookup, no parent); the
        // base asserts lacName and the bound clip instance.
        BrnFlaptComponent::Prepare(lacName, lFile, 0);

        // The three text fields live under the "EATraxBox_mc" child clip.
        BrnFlapt::MovieClipRef lEATraxBox;
        mAptRef.FindChildMovieClip(&lEATraxBox, KAC_EATRAX_BOX_NAME);

        lEATraxBox.FindChildTextField(&mArtistNameRef, KAC_ARTIST_FIELD_NAME);
        mArtistNameRef.SetAutoSize(true);

        lEATraxBox.FindChildTextField(&mSongNameRef,  KAC_SONG_FIELD_NAME);
        lEATraxBox.FindChildTextField(&mAlbumNameRef, KAC_ALBUM_FIELD_NAME);
    }

    // @ 0x82415C00 -- reset the runtime state to idle.
    void EATraxInGameComponent::Initialize()
    {
        mfCurrentGameTime_Seconds = 0.0f;
        meComponentState          = E_CS_INVISIBLE;
    }

    // @ 0x82415C18 -- record the current game time (seconds).
    void EATraxInGameComponent::SetTime(f32 lfGameTime_Seconds)
    {
        mfCurrentGameTime_Seconds = lfGameTime_Seconds;
    }

    // @ 0x82439E70 -- once a visible chyron's display interval elapses, play "AnimOut",
    // go invisible and tell the front end the chyron is no longer active.
    void EATraxInGameComponent::Update()
    {
        if (meComponentState == E_CS_VISIBLE &&
            mfCurrentGameTime_Seconds > mfAnimOutTime_Seconds)
        {
            meComponentState = E_CS_INVISIBLE;
            mAptRef.GotoAndPlayLabel(KAPC_ANIM_OUT_FRAME);
            PublishChyronActiveEvent(mpStateInterface, false);
        }
    }

    // @ 0x82439F08 -- show a new track: arm the anim-out timer, fill the three text
    // fields (localised by id or raw per lbLocalised), play "AnimIn" and tell the
    // front end the chyron is now active.
    void EATraxInGameComponent::DisplayNewTrackNotification(const char* lpcArtistName,
                                                            const char* lpcSongName,
                                                            const char* lpcAlbumName,
                                                            bool lbLocalised)
    {
        meComponentState     = E_CS_VISIBLE;
        mfAnimOutTime_Seconds = mfCurrentGameTime_Seconds + KF_TIME_FOR_DISPLAYING_EA_TRAX_SECS;

        if (lbLocalised)
        {
            mArtistNameRef.SetLocalisedText(lpcArtistName, KI_EATRAX_STRING_ID_TYPE);
            mSongNameRef.SetLocalisedText(lpcSongName,    KI_EATRAX_STRING_ID_TYPE);
            mAlbumNameRef.SetLocalisedText(lpcAlbumName,  KI_EATRAX_STRING_ID_TYPE);
        }
        else
        {
            mArtistNameRef.SetText(lpcArtistName, false);
            mSongNameRef.SetText(lpcSongName,    false);
            mAlbumNameRef.SetText(lpcAlbumName,  false);
        }

        mAptRef.GotoAndPlayLabel(KAPC_ANIM_IN_FRAME);
        PublishChyronActiveEvent(mpStateInterface, true);
    }
}
