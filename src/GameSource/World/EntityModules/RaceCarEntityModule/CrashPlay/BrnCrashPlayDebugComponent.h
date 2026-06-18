#ifndef BRN_CRASH_PLAY_DEBUG_COMPONENT_H
#define BRN_CRASH_PLAY_DEBUG_COMPONENT_H

#include "BrnCommonTypes.h"
#include "GameShared/GameClasses/Development/DebugSystem/Core/CgsDebugComponent.h"

namespace BrnWorld
{
    static const s32 KI_NUM_FUEL_TRAIL_NODES = 32;

    struct CrashCombo
    {
        f32 mafSpins[3];
    };

    struct CrashBreakerParams
    {
        bool mbIsActive;
        u8 maPad01[15];
        Vector3 mEpiCentre;
        f32 mfTotalRadius;
        s32 miNumFuelTrailNodes;
        s32 miFirstFuelTrailNodeIndex;
        Vector3 maFuelTrail[KI_NUM_FUEL_TRAIL_NODES];

        f32 GetTotalRadius() const { return mfTotalRadius; }
    };

    struct CrashPlayManager
    {
        u8 maPad00[312];
        f32 mfAftertouchPower;
        u8 maPad13C[18];
        bool mbInfiniteAftertouch;
        bool mbInfiniteBoost;
        CrashCombo mCrashCombo;
        CrashBreakerParams mCrashBreakerParams;
        bool mbIsCrashPlayActive;
    };

    class CrashPlayDebugComponent : public CgsDev::DebugComponent
    {
    public:
        void Construct(CrashPlayManager* lpCrashPlayManager);
        void Destruct();
        void RenderWorld(CgsDev::Debug3DImmediateRender* lpDisplay) override;
        void RenderHUD(CgsDev::Debug2DImmediateRender* lpDisplay) override;
        void Update() override;

    protected:
        const char* GetName() const override;
        const char* GetPath() const override;
        void OnActivate() override;

    private:
        CrashPlayManager* mpCrashPlayManager;
        bool mbDisplayCrashBreaker;
    };
}

#endif
