#pragma once

#include "types.hpp"

namespace CgsGui { struct StateInterface; }

// BrnGui::Selectable - the highlight/select item base every menu-ish component
// mixes in (an id + a state-flag byte + the optional selection text, with the
// Select/Update virtuals the concrete items override). Class shape / member
// names / StateFlags values verbatim from the DecFIGS DWARF (BrnSelectable.h);
// X360 layout: vptr @+0x00, mpcSelectionText @+0x08, mxFlags @+0x0C, mId @+0x10.
// Only the DWARF-attested surface is declared; the out-of-line bodies
// (Construct, the Set* virtuals, Select/Update defaults) are the Selectable TU's
// own ledger functions (declaration-only here). The flag accessors are
// header-inline on the X360 (e.g. the rlwinm tests inside
// ImageGalleryCarouselSelectable::Update @0x82419A90), so they are defined here.
namespace BrnGui
{
    struct Selectable
    {
        // DWARF BrnSelectable.h -- the state-flag bits in mxFlags.
        enum StateFlags
        {
            E_FLAG_ACTIVE        = 1,
            E_FLAG_HIGHLIGHTABLE = 2,
            E_FLAG_SELECTABLE    = 4,
            E_FLAG_HIGHLIGHTED   = 8,
            E_FLAG_DIRTY         = 16,
        };

        static const u64 K_INVALID_ID = 0xFFFFFFFFull;   // DWARF K_INVALID_ID

        // DWARF -- declaration-only (their own ledger functions).
        void Construct(const char* lpacName, CgsGui::StateInterface* lpStateInterface,
                       const char* lpacParentName, u64 luId);
        virtual bool SetActive(bool lbActive);
        virtual bool SetHighlightable(bool lbHighlightable);
        virtual bool SetSelectable(bool lbSelectable);
        virtual bool SetHighlighted(bool lbHighlighted);
        virtual void Select();
        virtual void Update();

        // The header-inline flag surface (DWARF-declared; inlined by the X360).
        bool GetFlag(StateFlags leFlag) const { return (mxFlags & static_cast<u8>(leFlag)) != 0; }
        void SetFlag(StateFlags leFlag)       { mxFlags = static_cast<u8>(mxFlags | static_cast<u8>(leFlag)); }
        void ClearFlag(StateFlags leFlag)     { mxFlags = static_cast<u8>(mxFlags & ~static_cast<u8>(leFlag)); }
        void ClearFlags()                     { mxFlags = 0; }

        bool IsActive() const        { return GetFlag(E_FLAG_ACTIVE); }
        bool IsHighlightable() const { return GetFlag(E_FLAG_HIGHLIGHTABLE); }
        bool IsSelectable() const    { return GetFlag(E_FLAG_SELECTABLE); }
        bool IsHighlighted() const   { return GetFlag(E_FLAG_HIGHLIGHTED); }
        bool IsDirty() const         { return GetFlag(E_FLAG_DIRTY); }
        void SetDirty()              { SetFlag(E_FLAG_DIRTY); }

        u64  GetId() const  { return mId; }
        void SetId(u64 luId) { mId = luId; }

    protected:
        const char* mpcSelectionText;   // +0x08 (DWARF)

    private:
        u8  mxFlags;   // +0x0C (DWARF)
        u64 mId;       // +0x10 (DWARF)
    };
}
