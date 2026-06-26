// Bodies for the trigger-entity-module debug component, reconstructed from BURNOUT_X360_ARTIST.XEX:
//   Construct   @ 0x822A8F68
//   GetName     @ 0x822A8FF8
//   OnActivate  @ 0x822A9018
//   RenderHUD   @ 0x822C4368
//   RenderWorld @ 0x822DA1F0
//
// The X360 inlines the trigger-array walk (the CgsContainers::BitArray<512> used-trigger set and the
// 128-byte-stride trigger records) and folds the primitive draws into vector intrinsics. The draw-
// call SHAPES are taken from the Feb-2007 partial source for this file (the designated inlining
// reference); the X360 pseudocode/asm is authority for the control flow, the per-type counters, the
// colours, and the member offsets (all confirmed against 0x822DA1F0 / 0x822C4368).

#include "GameSource/World/EntityModules/TriggerEntityModule/BrnTriggerEntityModuleDebugComponent.h"

#include "GameSource/World/Trigger/BrnTriggerEntityModule.h"          // TriggerEntityModule (GetTrigger / GetUsedTriggerList)
#include "GameSource/World/EntityModules/TriggerEntityModule/BrnTriggerTypes.h" // Trigger / ETriggerTypeID / KF_PLANE_SEGMENT_TRIGGER_DEPTH
#include "GameShared/GameClasses/Development/DebugSystem/Render/CgsDebug3DImmediateRender.h" // DrawBox / DrawSphere
#include "GameShared/GameClasses/Development/DebugSystem/Render/CgsDebug2DImmediateRender.h" // DrawText
#include "GameShared/GameClasses/Development/CgsStrStream.h"           // CgsDev::StrStream
#include "GameShared/GameClasses/Core/CgsAssert.h"                     // CGS_ASSERT

namespace BrnWorld
{
    namespace
    {
        // Per-type debug colours (X360 .rdata file-scope const RGBA; the sphere/box values match the
        // Feb-2007 source). The plane-segment box is drawn with an inline mid-grey instead.
        const rw::RGBA KU_SPHERE_TRIGGER_COLOUR(0, 255, 0, 255);   // green
        const rw::RGBA KU_BOX_TRIGGER_COLOUR(0, 0, 255, 255);      // blue
    }

    // @ 0x822A8F68. Initialise for the module it debugs, defaulting every render toggle on and the
    // per-type counters to zero. (The X360's leading call is CgsDev::DebugComponent::Construct(),
    // emitted here as an empty COMDAT-folded body @ 0x8284CB38.)
    void TriggerEntityModuleDebugComponent::Construct(TriggerEntityModule* lpTriggerEntityModule)
    {
        DebugComponent::Construct();

        CGS_ASSERT(lpTriggerEntityModule != nullptr, "lpTriggerEntityModule != NULL");
        mpTriggerEntityModule = lpTriggerEntityModule;

        mbRenderTriggers      = true;
        mbRenderTriggerCounts = true;

        mabTriggerTypeRenderFlags[E_TRIGGERTYPE_PLANE_SEGMENT] = true;
        mabTriggerTypeRenderFlags[E_TRIGGERTYPE_SPHERE]        = true;
        mabTriggerTypeRenderFlags[E_TRIGGERTYPE_BOX]           = true;

        miBoxTriggerCount    = 0;
        miSphereTriggerCount = 0;
        miPlaneTriggerCount  = 0;
    }

    // @ 0x822A8FF8.
    const char* TriggerEntityModuleDebugComponent::GetName() const
    {
        return "Trigger Entities";
    }

    // @ 0x822A9018. Register the render toggles with the debug menu.
    void TriggerEntityModuleDebugComponent::OnActivate()
    {
        RegisterVariable(&mbRenderTriggers,      "Render Triggers");
        RegisterVariable(&mbRenderTriggerCounts, "Render Trigger Counts");
        RegisterVariable(&mabTriggerTypeRenderFlags[E_TRIGGERTYPE_PLANE_SEGMENT], "Draw Plane Segment Triggers");
        RegisterVariable(&mabTriggerTypeRenderFlags[E_TRIGGERTYPE_SPHERE],        "Draw Sphere Triggers");
        RegisterVariable(&mabTriggerTypeRenderFlags[E_TRIGGERTYPE_BOX],           "Draw Box Triggers");
    }

    // @ 0x822DA1F0. Draw every active trigger as its primitive, recounting the per-type totals as it
    // goes. Skipped entirely if the master toggle is off or all three type toggles are off.
    void TriggerEntityModuleDebugComponent::RenderWorld(CgsDev::Debug3DImmediateRender* lpDisplay)
    {
        if (!mbRenderTriggers)
        {
            return;
        }

        if (!mabTriggerTypeRenderFlags[E_TRIGGERTYPE_PLANE_SEGMENT] &&
            !mabTriggerTypeRenderFlags[E_TRIGGERTYPE_SPHERE] &&
            !mabTriggerTypeRenderFlags[E_TRIGGERTYPE_BOX])
        {
            return;
        }

        miBoxTriggerCount    = 0;
        miSphereTriggerCount = 0;
        miPlaneTriggerCount  = 0;

        const CgsContainers::BitArray<512>& lrUsedTriggers = mpTriggerEntityModule->GetUsedTriggerList();
        for (s32 liIndex = lrUsedTriggers.GetFirstNonZeroBit();
             liIndex != -1;
             liIndex = lrUsedTriggers.GetNextNonZeroBit(liIndex))
        {
            Trigger& lrTrigger = mpTriggerEntityModule->GetTrigger(liIndex);

            CGS_ASSERT(lrTrigger.meType > E_TRIGGERTYPE_INVALID && lrTrigger.meType < E_TRIGGERTYPE_COUNT,
                       "lTrigger.meType > BrnWorld::E_TRIGGERTYPE_INVALID && lTrigger.meType < BrnWorld::E_TRIGGERTYPE_COUNT");

            if (!mabTriggerTypeRenderFlags[lrTrigger.meType])
            {
                continue;
            }

            switch (lrTrigger.meType)
            {
                case E_TRIGGERTYPE_PLANE_SEGMENT:
                {
                    // Half-extents from the trigger dimensions, with the Z (depth) collapsed to the
                    // thin plane-segment depth.
                    rw::math::vpu::Vector3 lHalfDimensions;
                    lHalfDimensions.x = lrTrigger.mDimensions.x * 0.5f;
                    lHalfDimensions.y = lrTrigger.mDimensions.y * 0.5f;
                    lHalfDimensions.z = KF_PLANE_SEGMENT_TRIGGER_DEPTH * 0.5f;
                    lHalfDimensions.w = 0.0f;

                    lpDisplay->DrawBox(lHalfDimensions, lHalfDimensions, lrTrigger.mTransform, rw::RGBA(100, 100, 100, 100));
                    ++miPlaneTriggerCount;
                }
                break;

                case E_TRIGGERTYPE_SPHERE:
                {
                    lpDisplay->DrawSphere(lrTrigger.mTransform.Pos(), lrTrigger.mfRadius, KU_SPHERE_TRIGGER_COLOUR);
                    ++miSphereTriggerCount;
                }
                break;

                case E_TRIGGERTYPE_BOX:
                {
                    rw::math::vpu::Vector3 lHalfDimensions;
                    lHalfDimensions.x = lrTrigger.mDimensions.x * 0.5f;
                    lHalfDimensions.y = lrTrigger.mDimensions.y * 0.5f;
                    lHalfDimensions.z = lrTrigger.mDimensions.z * 0.5f;
                    lHalfDimensions.w = 0.0f;

                    rw::math::vpu::Vector3 lNegHalfDimensions;
                    lNegHalfDimensions.x = -lHalfDimensions.x;
                    lNegHalfDimensions.y = -lHalfDimensions.y;
                    lNegHalfDimensions.z = -lHalfDimensions.z;
                    lNegHalfDimensions.w = 0.0f;

                    lpDisplay->DrawBox(lNegHalfDimensions, lHalfDimensions, lrTrigger.mTransform, KU_BOX_TRIGGER_COLOUR);
                    ++miBoxTriggerCount;
                }
                break;

                case E_TRIGGERTYPE_INVALID:
                case E_TRIGGERTYPE_COUNT:
                default:
                {
                    CGS_ASSERT(false, "Invalid trigger type!");
                }
                break;
            }
        }
    }

    // @ 0x822C4368. HUD pass: a "Total trigger count" line, then one row per trigger type giving a
    // freshly recounted per-type total. Each per-type total is a LIVE rescan of the used-trigger set
    // (not the RenderWorld counters, which only feed the total here), so the HUD is accurate even on
    // frames where RenderWorld did not run. NOTE: the X360 reconstructs this directly (the Feb-2007
    // source has a different, drifted HUD); this follows the X360 build.
    void TriggerEntityModuleDebugComponent::RenderHUD(CgsDev::Debug2DImmediateRender* lpRenderer)
    {
        if (!mbRenderTriggerCounts)
        {
            return;
        }

        CgsDev::SimpleStrStream lStream;
        lStream << "Total trigger count: " << (miBoxTriggerCount + miSphereTriggerCount + miPlaneTriggerCount);
        lpRenderer->DrawText(lStream.GetBuffer(), 50.0f, 80.0f, 16.0f, 0xFFFFFFFFu);

        // The X360 names each type via an .rdata name table (off_82F2C8EC); its string bytes are not
        // in the available exports, so the labels are inferred from the ETriggerTypeID constants. The
        // X360 walks the type counter from E_TRIGGERTYPE_INVALID (-1, "Unknown") up to and INCLUDING
        // E_TRIGGERTYPE_COUNT (3) -- the table read for COUNT runs past its three entries and yields
        // the "<NULLSTRING>" fallback, modelled here by the >= COUNT branch.
        static const char* const KAPC_TRIGGER_TYPE_NAMES[E_TRIGGERTYPE_COUNT] =
        {
            "Plane Segment", // E_TRIGGERTYPE_PLANE_SEGMENT
            "Sphere",        // E_TRIGGERTYPE_SPHERE
            "Box",           // E_TRIGGERTYPE_BOX
        };

        const CgsContainers::BitArray<512>& lrUsedTriggers = mpTriggerEntityModule->GetUsedTriggerList();
        for (s32 liType = E_TRIGGERTYPE_INVALID; liType <= E_TRIGGERTYPE_COUNT; ++liType)
        {
            s32 liCount = 0;
            for (s32 liIndex = lrUsedTriggers.GetFirstNonZeroBit();
                 liIndex != -1;
                 liIndex = lrUsedTriggers.GetNextNonZeroBit(liIndex))
            {
                if (mpTriggerEntityModule->GetTrigger(liIndex).meType == liType)
                {
                    ++liCount;
                }
            }

            const char* lpcTypeName;
            if (liType == E_TRIGGERTYPE_INVALID)
            {
                lpcTypeName = "Unknown";
            }
            else if (liType < E_TRIGGERTYPE_COUNT && KAPC_TRIGGER_TYPE_NAMES[liType] != nullptr)
            {
                lpcTypeName = KAPC_TRIGGER_TYPE_NAMES[liType];
            }
            else
            {
                lpcTypeName = "<NULLSTRING>";
            }

            lStream.Reset();
            lStream << lpcTypeName << ": " << liCount;
            lpRenderer->DrawText(lStream.GetBuffer(), 50.0f, (static_cast<f32>(liType) * 16.0f) + 120.0f, 16.0f, 0xFFFFFFFFu);
        }
    }
}
