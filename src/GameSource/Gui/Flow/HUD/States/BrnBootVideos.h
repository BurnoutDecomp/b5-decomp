#ifndef BRN_BOOT_VIDEOS_H
#define BRN_BOOT_VIDEOS_H

#include "types.hpp"
#include "BrnCommonTypes.h"                                       // CgsID
#include "GameShared/GameClasses/Gui/Model/State/CgsGuiState.h"   // CgsGui::State (+ sResourceTuple, ScriptedFsm)

// BrnGui::BootVideos -- the boot-logo GUI state (DecFIGS GameSource/Gui/Flow/HUD/States/BrnBootVideos;
// ARTIST Construct 0x824744B8 / OnEnter 0x824786B8 / Update 0x82478778 / OnLeave 0x82478D38). It is a
// CgsGui::State in the HUD flow that plays the boot branding videos in sequence by emitting GuiEventPlayVideo
// requests at the MovieManager (via the StateInterface) and advancing on the video-finished feedback.
//
// Sequence (EUpdateStage): wait for the resource/cache-ready event, play the EA logo ("EAFranchise"), then
// on video-finished play the Criterion logo, then signal the boot videos are done. A video's resource id is
// CgsResource::ID::HashString(name) (so VIDEOLIST.BUNDLE's VideoDataResource ids are the name hashes).
namespace BrnGui
{
    class GuiCache;

    class BootVideos : public CgsGui::State
    {
    public:
        // X360 BrnBootVideos.h:78 -- the per-frame stage the state is in.
        enum EUpdateStage
        {
            E_UPDATE_STAGE_LOADING = 0,
            E_UPDATE_STAGE_MAIN_HD_MOVIE = 1,
            E_UPDATE_STAGE_MAIN_EA_LOGO_MOVIE = 2,
            E_UPDATE_STAGE_MAIN_CRITERION_LOGO_MOVIE = 3,
            E_UPDATE_STAGE_DONE = 4,
        };

        BootVideos()
            : meUpdateStage(E_UPDATE_STAGE_LOADING)
            , mfHDVideoStartTime(0.0f)
            , mfLogoVideoStartTime(0.0f)
            , mpGuiCache(0)
        {
        }

        void Construct(CgsID liId, CgsFsm::ScriptedFsm* lpFsm) override;
        void OnEnter() override;
        void OnLeave() override;
        void Update() override;
        void GetResourcesToLoad(const CgsGui::sResourceTuple** lppResourceTuples,
                                u32* lpuNumberOfResources) const override;

    private:
        EUpdateStage meUpdateStage;
        f32          mfHDVideoStartTime;
        f32          mfLogoVideoStartTime;
        GuiCache*    mpGuiCache;
    };
}

#endif
