#pragma once

#include "types.hpp"
#include "GameShared/GameClasses/Development/DebugSystem/Core/CgsDebugComponent.h"

namespace rw { struct IResourceAllocator; }

// Reconstructed from BURNOUT_X360_ARTIST.XEX
//   BrnResource::DebugComponent::OnActivate @ 0x826626A8
//   BrnResource::DebugComponent::InterpolateColours @ 0x82661410
//
// The debug component is embedded in BrnResource::GameDataModule. Its full
// allocator-statistics Update/RenderHUD bodies are still being recovered; the
// typed member state here is limited to the constructor and activation fields.

namespace BrnResource
{
    class GameDataModule;

    class DebugComponent
        : public CgsDev::DebugComponent
    {
    public:
        // X360 GameDataModule::Construct initializes this component inline;
        // DecFIGS preserves the two-argument typed constructor shape.
        void Construct(GameDataModule* lpModule, rw::IResourceAllocator* lpAllocator);

        // X360 0x827DD200; DecFIGS declares this accessor const.
        const char* GetName() const override { return "GameDataModule"; }

        // X360 0x826626A8. Registers the Show HUD flag and the two read-only
        // event-slot counters under this component's own path.
        void OnActivate() override;

        // Per-channel linear interpolation of two packed 0xAARRGGBB colours, used by
        // RenderHUD to fade the resource-bar colours. lfFactor selects toward luColourT
        // (factor 1 == luColourT, factor 0 == luColour0). Each channel is rounded to
        // nearest (add 0.5, truncate). X360 0x82661410.
        static uint32_t InterpolateColours(uint32_t luColourT, uint32_t luColour0, f32 lfFactor);

    private:
        GameDataModule*          mpModule;
        rw::IResourceAllocator*  mpAllocator;
        bool                     mbShowHud;
        // X360 +0x18 is an allocator-owned statistics buffer (Update requests
        // 10,988 bytes and RenderHUD walks its 164-byte rows). Its row schema is
        // not yet fully reconstructed, so keep the allocation pointer opaque.
        void*                    mpAllocatorStats;
        s32                      miSlotsUsed;
        s32                      miSlotsFree;
    };
}
