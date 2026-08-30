#pragma once

// Attrib::Gen::presentationactionlist -- generated AttribSys class (a presentation
// effect's action list). The generated accessor / `using Instance::...` API is inlined
// away at the call sites, so the constructor is the only presentationactionlist entry
// point the X360 ledger attests (same minimal-recon convention as the sibling generated
// classes iceanim / surfacelist / debrisparams / physicsvehiclebaseattribs). Derives from
// Attrib::Instance.
//
// Reconstructed from BURNOUT_X360_ARTIST.XEX:
//   Attrib::Gen::presentationactionlist::presentationactionlist @ 0x8269E208
//
// called by BrnSound::Logic::PresentationEffect::PresentationEffect.
#include "SDKs/Packages/AttribSys/1.2.1.2/AttribSys/runtime/common/attribinstance.h"
#include "GameSource/AttribSys/Generated/attrib_private.h"

namespace Attrib
{
namespace Gen
{
    class presentationactionlist : private Instance
    {
    public:
        explicit presentationactionlist(Collection* lpCollection = nullptr, void* lpOwner = nullptr);
        explicit presentationactionlist(const RefSpec& lrRefSpec, void* lpOwner = nullptr)
            : Instance(lrRefSpec, lpOwner) {}

        void ChangeWithDefault(const RefSpec& lrRefSpec)
        {
            RefSpec& lrMutable = const_cast<RefSpec&>(lrRefSpec);
            Change(const_cast<Collection*>(lrMutable.GetCollectionWithDefault()));
        }

        struct ResolvedAction
        {
            u64 muStringId;
            u64 muScreenId;
            u32 muContentSpec;
            u16 mu16Splice;
            u8  mu8ChokeGroup;
            u8  mu8Valid;
            u8  mu8Behaviour;
            u8  mu8MixerOutput;
        };

        u32 NumOfActions() const;
        bool Resolve(u32 auAction, u64 auStringId, u64 auScreenId,
                     ResolvedAction& arResult) const;
    };

    // X360 @0x8269E208: chain the Instance ctor, assert the collection's class is
    // ClassName::presentationactionlist (skipping the assert when the class is unset/0),
    // then give the instance a default data area (0x1D48 bytes) if it has none.
    inline presentationactionlist::presentationactionlist(Collection* lpCollection, void* lpOwner)
        : Instance(lpCollection, lpOwner)
    {
        static const int KI_PRESENTATIONACTIONLIST_CLASS = -1959977425; // Attrib::ClassName::presentationactionlist
        if (GetClass() != KI_PRESENTATIONACTIONLIST_CLASS && GetClass() != 0)
            AssertOnClassCheck(GetClass(), KI_PRESENTATIONACTIONLIST_CLASS, GetCollection());
        if (!mpAttributeData)
            mpAttributeData = DefaultDataArea(0x1D48u);
    }

    inline u32 presentationactionlist::NumOfActions() const
    {
        const u8* lpData = static_cast<const u8*>(mpAttributeData);
        return reinterpret_cast<const Private*>(lpData + 0x1418u)->GetLength();
    }

    inline bool presentationactionlist::Resolve(
        u32 auAction, u64 auStringId, u64 auScreenId,
        ResolvedAction& arResult) const
    {
        const u8* lpData = static_cast<const u8*>(mpAttributeData);
        const u32 luCount = NumOfActions();
        const u64* lpaScreen = reinterpret_cast<const u64*>(lpData + 0x008u);
        const u64* lpaString = reinterpret_cast<const u64*>(lpData + 0x810u);
        const u32* lpaSpec   = reinterpret_cast<const u32*>(lpData + 0x1018u);
        const u32* lpaAction = reinterpret_cast<const u32*>(lpData + 0x1420u);
        const u16* lpaSplice = reinterpret_cast<const u16*>(lpData + 0x1828u);
        const u8*  lpaMixer  = lpData + 0x1A30u;
        const u8*  lpaChoke  = lpData + 0x1B38u;
        const u8*  lpaBehave = lpData + 0x1C40u;

        // Resolve @0x82687990 tries the fully-qualified row first, then a
        // string-only row, then a screen-only row.
        s32 liMatch = -1;
        for (s32 liPass = 0; liPass < 3 && liMatch < 0; ++liPass)
        {
            const u64 luString = liPass == 2 ? 0 : auStringId;
            const u64 luScreen = liPass == 1 ? 0 : auScreenId;
            if ((liPass == 1 && auStringId == 0) ||
                (liPass == 2 && auScreenId == 0))
                continue;
            for (u32 luIndex = 0; luIndex < luCount; ++luIndex)
            {
                if (lpaAction[luIndex] == auAction &&
                    lpaString[luIndex] == luString &&
                    lpaScreen[luIndex] == luScreen)
                {
                    liMatch = static_cast<s32>(luIndex);
                    break;
                }
            }
        }
        if (liMatch < 0)
        {
            arResult = ResolvedAction();
            return false;
        }

        const u32 luIndex = static_cast<u32>(liMatch);
        arResult.muStringId      = lpaString[luIndex];
        arResult.muScreenId      = lpaScreen[luIndex];
        arResult.muContentSpec   = lpaSpec[luIndex];
        arResult.mu16Splice      = lpaSplice[luIndex];
        arResult.mu8ChokeGroup   = lpaChoke[luIndex];
        arResult.mu8Valid        = 1;
        arResult.mu8Behaviour    = lpaBehave[luIndex];
        arResult.mu8MixerOutput  = lpaMixer[luIndex];
        return true;
    }
}
}
