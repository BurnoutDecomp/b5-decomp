#ifndef BRN_TRIGGER_ENTITY_MODULE_DEBUG_COMPONENT_H
#define BRN_TRIGGER_ENTITY_MODULE_DEBUG_COMPONENT_H

#include "types.hpp"
#include "GameShared/GameClasses/Development/DebugSystem/Core/CgsDebugComponent.h" // CgsDev::DebugComponent (real base)

// BrnWorld::TriggerEntityModuleDebugComponent - the in-game debug overlay for the trigger entity
// module: a world-space pass that draws every active trigger as its primitive (plane/sphere/box)
// and a HUD pass that prints the per-type counts. Derives from the real CgsDev::DebugComponent.
// Reconstructed from BURNOUT_X360_ARTIST.XEX:
//   Construct   @ 0x822A8F68   GetName @ 0x822A8FF8   OnActivate @ 0x822A9018
//   RenderHUD   @ 0x822C4368   RenderWorld @ 0x822DA1F0
//
// Member layout pinned from the X360 Construct stores (the base DebugComponent occupies +0x00..0x0B):
//   mpTriggerEntityModule     +0x0C   miBoxTriggerCount    +0x10
//   miSphereTriggerCount      +0x14   miPlaneTriggerCount  +0x18
//   mbRenderTriggerCounts     +0x1C   mbRenderTriggers     +0x1D
//   mabTriggerTypeRenderFlags +0x1E..0x20  (one bool per E_TRIGGERTYPE_PLANE_SEGMENT/SPHERE/BOX)

namespace CgsDev { struct Debug2DImmediateRender; struct Debug3DImmediateRender; }

namespace BrnWorld
{
    class TriggerEntityModule;   // back-pointer member only; the .cpp includes the full header

    class TriggerEntityModuleDebugComponent : public CgsDev::DebugComponent
    {
    public:
        void Construct(TriggerEntityModule* lpTriggerEntityModule);   // @ 0x822A8F68

        void RenderWorld(CgsDev::Debug3DImmediateRender* lpDisplay) override; // @ 0x822DA1F0
        void RenderHUD(CgsDev::Debug2DImmediateRender* lpRenderer) override;  // @ 0x822C4368

    protected:
        const char* GetName() const override;   // @ 0x822A8FF8
        void        OnActivate() override;        // @ 0x822A9018

    private:
        TriggerEntityModule* mpTriggerEntityModule;   // +0x0C
        s32                  miBoxTriggerCount;        // +0x10
        s32                  miSphereTriggerCount;     // +0x14
        s32                  miPlaneTriggerCount;      // +0x18
        bool                 mbRenderTriggerCounts;    // +0x1C
        bool                 mbRenderTriggers;         // +0x1D
        bool                 mabTriggerTypeRenderFlags[3]; // +0x1E (indexed by E_TRIGGERTYPE_*)
    };
}

#endif // BRN_TRIGGER_ENTITY_MODULE_DEBUG_COMPONENT_H
