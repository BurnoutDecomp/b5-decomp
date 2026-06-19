#pragma once

#include "types.hpp"
#include "GameShared/GameClasses/Development/DebugSystem/Core/CgsDebugComponent.h"  // CgsDev::DebugComponent (real base)

// CgsLanguage::LanguageManagerDebugComponent - a debug HUD that dumps every LanguageManager value
// formatter (integer / percentage / currency / date / a family of time strings / small+large+smallest
// distance strings) against a set of fixed sample inputs, so localisation/formatting can be eyeballed
// on-screen. Derives from the real CgsDev::DebugComponent.
//
// Layout (DecFIGS DWARF CgsLanguageManagerDebugComponent.h + X360 binary offsets, all verified
// against the Construct/RenderHUD disassembly):
//   DebugComponent base sub-object    +0x00..+0x0B
//   mpLanguageManager                 +0x0C
//   mbShowDebugComponent              +0x10
//   mbShowKeys                        +0x11
//   mbShowLocalisedTextAsStars        +0x12
//   mapStars[5]                       +0x14   (star strings indexed by digit count)
//   mrPosX / mrPosY                   +0x28 / +0x2C
//   mrTextSpacing / mrTextSize        +0x30 / +0x34
//   mnNumber1 / mnNumber2 / mnCurrency+0x38 / +0x3C / +0x40
//   mnDays / mnMonths / mnYears       +0x44 / +0x48 / +0x4C
//   mnHours / mnMinutes               +0x50 / +0x54
//   mrSeconds                         +0x58
//   mrDistance                        +0x5C
//
// The X360 ledger attests Construct / DrawText / GetName / RenderHUD for this TU; the DWARF's
// Destruct / GetPath / OnActivate / ShowKeysOnly / ShowLocalisedTextAsStars / GetStarString are not
// in the X360 set for this slice and are left out (the base GetPath default applies).

namespace CgsDev { struct Debug2DImmediateRender; }
namespace CgsLanguage { class LanguageManager; }

namespace CgsLanguage
{
    class LanguageManagerDebugComponent : public CgsDev::DebugComponent
    {
    public:
        void Construct( const LanguageManager* lpLanguageManager );   // @ 0x82860A68

        void RenderHUD( CgsDev::Debug2DImmediateRender* lpRender ) override;   // @ 0x82863930

    protected:
        const char* GetName() const override;   // @ 0x82860A58 -> "LanguageManager"

    private:
        // Draw a "<label><value>" pair at (lfX, lrY) - label, then value just past the label's width -
        // and advance lrY by one line (mrTextSpacing * mrTextSize). @ 0x82860E70.
        void DrawText( CgsDev::Debug2DImmediateRender* lpRender, const char* lpcLabel,
                       const char* lpcValue, f32 lfX, f32& lrY ) const;

        const LanguageManager* mpLanguageManager;     // +0x0C
        bool                   mbShowDebugComponent;   // +0x10
        bool                   mbShowKeys;             // +0x11
        bool                   mbShowLocalisedTextAsStars; // +0x12
        const char*            mapStars[5];            // +0x14
        f32                    mrPosX;                 // +0x28
        f32                    mrPosY;                 // +0x2C
        f32                    mrTextSpacing;          // +0x30
        f32                    mrTextSize;             // +0x34
        s32                    mnNumber1;              // +0x38
        s32                    mnNumber2;              // +0x3C
        s32                    mnCurrency;             // +0x40
        s32                    mnDays;                 // +0x44
        s32                    mnMonths;               // +0x48
        s32                    mnYears;                // +0x4C
        s32                    mnHours;                // +0x50
        s32                    mnMinutes;              // +0x54
        f32                    mrSeconds;              // +0x58
        f32                    mrDistance;             // +0x5C
    };
}
