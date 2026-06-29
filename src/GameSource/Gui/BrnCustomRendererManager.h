#ifndef BRN_CUSTOM_RENDERER_MANAGER_H
#define BRN_CUSTOM_RENDERER_MANAGER_H

#include "types.hpp"

#include "GameShared/GameClasses/Gui/View/CustomRenderer/CgsCustomRenderer.h" // CgsGui::CustomRenderComponentInterface, eCustomRenderLayer
#include "GameShared/GameClasses/Module/CgsVariableEventQueue.h"              // CgsModule::VariableEventQueue<4096,16>, CgsModule::Event

// Reconstructed from BURNOUT_X360_ARTIST.XEX.
//   BrnGui::CustomRendererManager::SetReplaySerialiser  @ 0x82445648
//
// The manager owns the GUI custom-renderer set; the one in-scope function stores a
// replay-serialiser pointer into a member deep in the object:
//
//   0x82445648  lis  r11, 1
//   0x8244564C  ori  r11, r11, 0xBED0        ; r11 = 0x1BED0 (114384)
//   0x82445650  stwx r4, r3, r11             ; *(this + 0x1BED0) = a2
//   0x82445654  blr                          ; returns this (result == r3)
//
// MINIMAL OWNING SLICE: the real manager is ~114 KB of renderer state (the full custom
// renderer array, per-renderer texture/event state, etc. -- all uncommitted). This models
// ONLY the member the ledger function writes: the replay-serialiser pointer (guest
// +0x1BED0). FLAG: minimal-slice class -- every other manager member is intentionally
// OMITTED (uncommitted dependencies, none in scope). The exact guest offset is NOT
// reproduced (host is 64-bit; the offset is not load-bearing here).
//
// The serialiser parameter is an opaque pointer in scope (its type, a replay serialiser,
// is uncommitted): modelled as void* held BY NAME, faithful to the raw stwx store.

// ---- GROW (FLAG): the canonical home of BrnGui::CustomRendererManager, grown from the
// SetReplaySerialiser-only minimal slice to the FULL custom-renderer-set manager
// reconstructed in GameSource/Gui/CustomRenderer/BrnCustomRenderer.cpp (16 functions @
// 0x82444040 .. 0x82450908). The X360 object is ~114 KB of by-value renderer subobjects;
// the lifecycle is driven entirely through a 10-entry array of polymorphic
// CgsGui::CustomRenderComponentInterface* pointers (guest +0x101C) plus the manager's own
// base-class chain. On the 64-bit host gate the per-renderer subobject byte offsets are
// NOT load-bearing, so the components are modelled as the polymorphic pointer array the
// asm dispatches through (in-game these point into the embedded subobjects). The
// per-renderer by-value subobjects, the GuiCustRendererDebugComponent, the two rw::IResource
// tables and the trailing replay/text/language back-pointers are modelled by NAME.

namespace CgsGui
{
    // ---- (FLAG) uncommitted base CgsGui::CustomRendererManager: no committed home exists
    // (grep GameShared finds none). The BrnGui derived manager chains to the base
    // Construct/Prepare/Release/Destruct (the X360 calls CgsGui::CustomRendererManager::
    // Construct() etc. as the first/last statement of the matching derived method). Modelled
    // minimally: the base lifecycle hooks the derived class extends, declared virtual so the
    // derived overrides bind, with neutral defaults. The base render-target / im-renderer /
    // CgsID-keyed component registry members are uncommitted and intentionally OMITTED.
    class CustomRendererManager
    {
    public:
        virtual ~CustomRendererManager() {}

        virtual void Construct()                                  {}
        virtual bool Prepare(void* lpResourceAllocatorA, void* lpResourceAllocatorB)
        {
            (void)lpResourceAllocatorA; (void)lpResourceAllocatorB; return true;
        }
        virtual bool Release()                                    { return true; }
        virtual void Destruct()                                   {}
        virtual void RecvEvent(const CgsModule::Event* lpEvent, s32 liEventType)
        {
            (void)lpEvent; (void)liEventType;
        }
        virtual void Update()                                     {}

        // The component accessors the derived manager overrides; the base provides the
        // virtual slots the X360 dispatches `(*(*this + N))(this, ...)` against.
        virtual u32  GetComponentID(s32 liComponent) const        { (void)liComponent; return 0; }
        virtual void SetComponentRenderable(s32 liComponent, bool lbRenderable)
        {
            (void)liComponent; (void)lbRenderable;
        }
        virtual bool GetComponentRenderable(s32 liComponent)      { (void)liComponent; return false; }
        virtual s32  GetNumComponents() const                     { return 0; }
        virtual s32  GetNumTexturesForComponent(s32 liComponent) const
        {
            (void)liComponent; return 0;
        }
        virtual void SetAllRenderingState(bool lbRenderable)      { (void)lbRenderable; }
    };
}

namespace BrnGui
{

// BrnCustomRenderer.h:88 (DWARF) -- index of the manager's renderer-component array. The
// order matches the construction order in Construct() and the component-pointer slots
// guest +0x101C..+0x1040.
enum ECustomRenderTypes
{
    E_NETWORK_PLAYER_IMAGE = 0,
    E_SATNAV               = 1,
    E_MAINMAP              = 2,
    E_CRASHNAVICONS        = 3,
    E_BOOSTBAR             = 4,
    E_ABOVECAR             = 5,
    E_PROGRESSBAR          = 6,
    E_BLACKBAR             = 7,
    E_INGAME_MESSAGE       = 8,
    E_CREDITS_TEXT         = 9,

    E_CUSTOM_RENDER_TYPES_COUNT = 10
};

class CustomRendererManager : public CgsGui::CustomRendererManager
{
public:
    // BrnCustomRenderer.h:69 / :75 -- prepare/release state machine markers.
    enum EPrepareStage { E_PREPARESTAGE_START = 0, E_PREPARESTAGE_DONE = 1 };
    enum EReleaseStage { E_RELEASESTAGE_START = 0, E_RELEASESTAGE_DONE = 1 };

    // ---- the 16 reconstructed functions (BrnCustomRenderer.cpp) ----
    virtual void Construct();                                                   // 0x82444040
    virtual bool Prepare(void* lpResourceAllocatorA, void* lpResourceAllocatorB);// 0x82444140
    virtual bool Release();                                                     // 0x824442B0
    virtual void Destruct();                                                    // 0x82444378
    virtual void RecvEvent(const CgsModule::Event* lpEvent, s32 liEventType);   // 0x824443D0
    virtual void Update();                                                      // 0x82450908

    virtual u32  GetComponentID(s32 liComponent) const;                         // 0x82445378
    virtual void SetComponentRenderable(s32 liComponent, bool lbRenderable);    // 0x824453F8
    virtual bool GetComponentRenderable(s32 liComponent);                       // 0x82445468
    virtual s32  GetNumTexturesForComponent(s32 liComponent) const;            // 0x82445658
    virtual void SetAllRenderingState(bool lbRenderable);                       // 0x824454E0

    // +0x10-style texture fetch (matched-by-CgsID lookup over the component array). Returns
    // an opaque texture pointer (renderengine::Texture is uncommitted).
    void* GetComponentTexture(u32 luComponentID, s32 liTextureIndex,            // 0x824452B0
                              void* lpiShaderProgram, void* lpRendererSet);

    void  EndOfFrame();                                                         // 0x82449930

    // Setters that wire shared sub-systems into the renderers (the back-pointers below).
    CustomRendererManager* SetFlaptRenderer(void* lpFlaptRenderer);            // 0x82449920
    void* SetTextRenderer(void* lpTextRenderer);                               // 0x82445538
    void* SetLanguageManager(void* lpLanguageManager);                        // 0x824455A8

    // SetReplaySerialiser @ 0x82445648 (original minimal-slice ledger fn; preserved).
    CustomRendererManager* SetReplaySerialiser( void* lpReplaySerialiser );

private:
    // RecvEvent's case-213 sub-routine (the SatNav/MainMap/CrashNav map-toggle path); split
    // out of the giant switch for readability. Reads the event payload directly.
    void RecvEvent_Event213(const CgsModule::Event* lpEvent);

    // Guest +0x0C: the manager's variable event queue (the Update() Clear() target). The
    // X360 embeds it right after the base subobject (Update calls
    // CgsModule::VariableEventQueue<4096,16>::Clear(this + 12)).
    CgsModule::VariableEventQueue<4096, 16> mEventQueue;

    // Guest +0x101C: the 10 polymorphic render components, in ECustomRenderTypes order.
    // FLAG: in-game these point into the embedded by-value renderer subobjects
    // (mSatNavRendererInstance @ +0x1050, mMainMapRendererInstance @ +0x28D0, ...); here
    // they are the polymorphic interface the lifecycle loops dispatch through.
    CgsGui::CustomRenderComponentInterface* mapCustomRenderComponents[E_CUSTOM_RENDER_TYPES_COUNT];

    // Guest +0x1F498: the master rendering-enable flag SetAllRenderingState() stores.
    bool mbRenderingEnable;

    // Per-component renderable-state cache the Update() pass reads (guest +0x1F4AD/0x1F4AE/
    // 0x1F4B0) and the gate flag (guest +0x1F4B1). FLAG: modelled by name.
    bool mbSatNavRenderable;       // guest +0x1F4AD (SatNav slot, RecvEvent 213 sub-mode 1)
    bool mbMainMapRenderable;      // guest +0x1F4AE (MainMap slot, RecvEvent 213 sub-mode 0)
    bool mbThirdSlotRenderable;    // guest +0x1F4B0 (the +0x1030 / AboveCar slot in Update)
    bool mbHaveValidMapPosition;   // guest +0x1F4B1 (gate for the three above)

    // Prepare / Release state-machine markers (guest +0x1F490 / +0x1F494).
    EPrepareStage mePrepareStage;
    EReleaseStage meReleaseStage;

    // Guest +0x1F4CC: per-frame "rendered without an update" HACK counter, zeroed by
    // Construct() and Update().
    s32 miHACK_NumberOfRendersWithoutUpdate;

    // Shared sub-system back-pointers the setters store (held by name; the exact guest
    // offsets are not load-bearing on the host). FLAG: opaque pointers (the concrete
    // FlaptRenderer / TextRenderer / LanguageManager / replay-serialiser types are
    // out of scope for the manager TU).
    void* mpFlaptRenderer;       // guest +0x1B91C
    void* mpTextRenderer;        // guest +0x1BEC4
    void* mpLanguageManager;     // guest +0x1BEC8
    void* mpReplaySerialiser;    // guest +0x1BED0 (original minimal-slice member)
};

} // namespace BrnGui

#endif // BRN_CUSTOM_RENDERER_MANAGER_H
