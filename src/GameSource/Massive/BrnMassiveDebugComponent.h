#ifndef GAMESOURCE_MASSIVE_BRNMASSIVEDEBUGCOMPONENT_H
#define GAMESOURCE_MASSIVE_BRNMASSIVEDEBUGCOMPONENT_H

#include "types.hpp"
#include "GameShared/GameClasses/Development/DebugSystem/Core/CgsDebugComponent.h"

// BrnMassive::BrnMassiveDebugComponent -- the "Massive" debug HUD: one row per impression event
// (zone name, delivered inventory-element id, download state bar), the selected subscriber's
// impression data, the pause flag and the impression counter. Embedded by value in
// BrnMassive::BrnMassive, which feeds its counters/selection from Update.
// Members are named by their debug-menu labels and assert strings; offsets are the console's.

namespace CgsDev { struct Debug2DImmediateRender; }

namespace BrnMassive
{
    class BrnMassive;
    class BrnMassiveSubscriber;

    class BrnMassiveDebugComponent : public CgsDev::DebugComponent
    {
    public:
        void Construct(const char* const* lpapcMassiveNames, s32 liNumMassiveNames,
                       s32* lpaiSubscriberStates, BrnMassiveSubscriber** lpapSubscribers);

        void RenderHUD(CgsDev::Debug2DImmediateRender* lpRender) override;

    protected:
        const char* GetName() const override;
        void        OnActivate() override;

    private:
        friend class BrnMassive;   // Update feeds the impression count and the selected subscriber

        const char* GetState(s32 liState) const;
        void DrawText(CgsDev::Debug2DImmediateRender* lpRender, const char* lpcLabel,
                      const char* lpcValue, f32 lfX, f32* lpfY);
        void DrawSubscriber(const BrnMassiveSubscriber* lpSubscriber,
                            CgsDev::Debug2DImmediateRender* lpRender, f32 lfX, f32 lfY);

        bool                   mbDisplay;               // +0x0C "Display Massive Debug"
        BrnMassiveSubscriber** mapSubscribers;          // +0x10 the owner's active-subscriber table
        s32*                   mpSubscriberStates;      // +0x14 one state code per impression event
        const char* const*     mpMassiveNames;          // +0x18 impression-event zone names
        s32                    miNumMassiveNames;       // +0x1C
        s32                    miNumImpressions;        // +0x20 "Number of Impressions"
        bool                   mbResetImpressionCount;  // +0x24 "Reset Impression Count"
        s32                    miDebugSubscriber;       // +0x28 "Debug Subscriber" (-1 = none)
        BrnMassiveSubscriber*  mpDebugSubscriber;       // +0x2C resolved by BrnMassive::Update
        f32                    mfTextScale;             // +0x30
        f32                    mfLineSpacing;           // +0x34 (line advance, in text scales)
        bool                   mbPaused;                // +0x38 mirror of the owner's pause flag
    };
}

#endif // GAMESOURCE_MASSIVE_BRNMASSIVEDEBUGCOMPONENT_H
