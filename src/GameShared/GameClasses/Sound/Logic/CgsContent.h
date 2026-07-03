#pragma once

#include "types.hpp"
#include "GameShared/GameClasses/Sound/Playback/CgsObject.h"   // CgsSound::Playback::Object (the held ref)

// CgsSound::Logic::Content - the logic-side handle onto one refcounted playback
// content object (an AEMS bank / CSIS interface / splicer bank the state managers
// load). Canonical home (previously a FLAGGED un-homed 12-byte descriptor inside
// BrnAIVehicleStateManager.h). X360 shape {vptr @+0 (off_820B3250), mpContent
// @+0x04, one opaque trailing word @+0x08} == the 12-byte element stride the
// AI-engine spec tables use, and the same vtable/member pair every inlined
// ~Content instance restores/releases (e.g. the four members of
// ~TrafficStateManager @0x826FC320..0x826FC384, and the GlobalStateManager ctor's
// {off_820B3250, 0, 0} seeds). DISTINCT from the refcounted
// CgsSound::Playback::Content (Playback/CgsContent.h).
namespace CgsSound
{
namespace Logic
{
    class Content
    {
    public:
        Content() : mpContent(0), muTrailing(0) {}

        // Recovered from the inlined per-member teardown in
        // ~TrafficStateManager @0x826FC320..: drop the held reference (no
        // refcount-zero disposal here -- that is RegisteredContent's richer
        // teardown, not this handle's). Declaration-only so the out-of-line body
        // (CgsLogicContentDtor.cpp) emits the single ~Content symbol the X360
        // `vector deleting destructor' @ 0x826DC378 is synthesised from.
        virtual ~Content();

        // Queried by the AI-engine spec tripwires (GetDecelGinsuContent /
        // GetLoopModelContent). Deferred body -- returns true (the specs are
        // asserted loaded on the boot path that reaches those accessors).
        bool IsLoaded() const { return true; }

    private:
        CgsSound::Playback::Object* mpContent;   // +0x04 (held refcounted content, or null)
        u32                         muTrailing;  // +0x08 (opaque; untouched by recon'd code)
    };
}
}
