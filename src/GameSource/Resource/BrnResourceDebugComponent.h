#pragma once

#include "types.hpp"
#include "GameShared/GameClasses/Development/DebugSystem/Core/CgsDebugComponent.h"
#include "GameShared/GameClasses/Memory/MemoryMap/CgsMemoryMap.h"
#include "ppmalloc/EAGeneralAllocator.h"

namespace rw { struct IResourceAllocator; }

// Reconstructed from BURNOUT_X360_ARTIST.XEX
//   BrnResource::DebugComponent::OnActivate @ 0x826626A8
//   BrnResource::DebugComponent::InterpolateColours @ 0x82661410
//
// The debug component is embedded in BrnResource::GameDataModule. The resource
// allocator statistics row and report callback follow the DecFIGS member shape;
// their readers are being connected to the original GameDataModule memory map.

namespace BrnResource
{
    class GameDataModule;

    class DebugComponent
        : public CgsDev::DebugComponent
    {
    public:
        struct MemTypeInfo
        {
            s32 miSize;
            s32 miCurrUsed;
            s32 miMaxUsed;
            f32 mfColInterp;

            void UpdateMax();
        };

        struct AllocatorStatsRow
        {
            CgsMemory::MemoryMap::EAllocatorType meAllocatorType;
            char macName[128];
            MemTypeInfo mMainMemInfo;
            MemTypeInfo mVideoMemInfo;
        };

        struct GenAllocUsage
        {
            u32 muAllocated;
            u32 muFree;
        };

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
        // ARTIST 0x826627A8 passes this address as GeneralAllocator::ReportHeap's
        // callback; the first argument is the block record (the PPC callback ABI
        // has no component `this` parameter).
        static bool GeneralAllocatorReportCB(const EA::Allocator::GeneralAllocator::BlockInfo* lpBlockInfo,
                                             void* lpContext);

        GameDataModule*          mpModule;
        rw::IResourceAllocator*  mpAllocator;
        bool                     mbShowHud;
        // X360 +0x18 is an allocator-owned buffer containing 67 rows (164 bytes
        // each); only 24 allocator records are visited by Update.
        AllocatorStatsRow*       mpAllocatorStats;
        s32                      miSlotsUsed;
        s32                      miSlotsFree;
    };

    static_assert(sizeof(DebugComponent::AllocatorStatsRow) == 164,
                  "ARTIST AllocatorInfo rows use a 0xA4 byte stride");
}
