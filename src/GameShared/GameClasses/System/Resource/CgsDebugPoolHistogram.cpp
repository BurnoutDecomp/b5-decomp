#include "GameShared/GameClasses/System/Resource/CgsDebugPoolHistogram.h"
#include "GameShared/GameClasses/System/Resource/CgsResourcePool.h"
#include "GameShared/GameClasses/Core/CgsStringUtils.h"
#include "GameShared/GameClasses/Development/DebugSystem/Render/CgsDebug2DImmediateRender.h"

#include <cstring>

namespace CgsResource
{
    // ARTIST820F7158: descending reference-count thresholds, not byte sizes.
    const s32 KAI_HISTOGRAMCOUNTS[KI_NUMHISTOGRAMCOUNTS] =
    {
        35, 31, 27, 23, 20, 17, 14, 12, 10, 8, 5, 4, 3, 2, 1,
    };

    // ARTIST828D9ED0. The original Prepare parameters occupy f1/f2 and
    // argument slots r4/r5; the pool-name pointer and flags are in r6/r7.
    void DebugPoolHistogram::Reset(const Pool* lpPool)
    {
        CgsDev::DebugUI::Window::Prepare(200.0f, 250.0f, lpPool->GetName(), KX_FLAGNORMAL);
        mpPool = lpPool;
    }

    // ARTIST828E3BF0, including the DrawBar argument setup in r3..r6/f1.
    void DebugPoolHistogram::Render(CgsDev::Debug2DImmediateRender* lpRender)
    {
        CgsDev::DebugUI::Window::Render(lpRender);
        if (mpPool)
        {
            char lacText[256];
            CgsCore::SPrintf(lacText, sizeof(lacText), "x<%d", 1);
            DrawBar(lpRender, lacText, KI_NUMHISTOGRAMCOUNTS, 5.0f);
            f32 lfY = 18.0f;
            for (s32 liIndex = KI_NUMHISTOGRAMCOUNTS - 1; liIndex >= 1; --liIndex)
            {
                CgsCore::SPrintf(lacText, sizeof(lacText), "%d<=x<%d",
                                KAI_HISTOGRAMCOUNTS[liIndex], KAI_HISTOGRAMCOUNTS[liIndex - 1]);
                DrawBar(lpRender, lacText, liIndex, lfY);
                lfY += 13.0f;
            }
            CgsCore::SPrintf(lacText, sizeof(lacText), "x>=%d", 35);
            DrawBar(lpRender, lacText, 0, lfY);
        }
    }

    // ARTIST828E3D00. Its base Update is the empty folded body8284CB38.
    void DebugPoolHistogram::Update(f32 lfTimeStep, CgsDev::DebugUI::InputEvent leEvent)
    {
        CgsDev::DebugUI::Window::Update(lfTimeStep, leEvent);
        if (mpPool)
        {
            std::memset(mauValues, 0, sizeof(mauValues));
            // The original clears all six descriptor words, including alignment.
            std::memset(&mSaved, 0, sizeof(mSaved));
            miLoadedResources = 0;
            miMaxResources = static_cast<s32>(mpPool->GetMaxResources());
            for (s32 liIndex = 0; liIndex < miMaxResources; ++liIndex)
            {
                if ((mpPool->GetEntryStatusDirect(liIndex) & 2u) != 0)
                {
                    ++miLoadedResources;
                    const s32 liReferences = mpPool->GetEntryRefCount(liIndex);
                    AddToCount(liReferences);
                    if (liReferences > 1)
                    {
                        const Entry* lpEntry = mpPool->GetEntryDirect(liIndex);
                        for (u32 luType = 0; luType < BasePool::KI_NUM_TYPES; ++luType)
                        {
                            mSaved.m_baseResourceDescriptors[luType].m_size +=
                                lpEntry->mResourceDescriptor.m_baseResourceDescriptors[luType].m_size *
                                static_cast<u32>(liReferences - 1);
                        }
                    }
                }
            }
        }
    }

    // Inlined at ARTIST828E3DBC..828E3DF0; DecFIGS names this helper.
    void DebugPoolHistogram::AddToCount(s32 liSize)
    {
        s32 liIndex = 0;
        while (liIndex < KI_NUMHISTOGRAMCOUNTS && liSize < KAI_HISTOGRAMCOUNTS[liIndex])
        {
            ++liIndex;
        }
        ++mauValues[liIndex];
    }

    // ARTIST828D9F28. liCount is the bucket index; the denominator is the
    // number of loaded resources, not the number of slots or peak bucket count.
    void DebugPoolHistogram::DrawBar(CgsDev::Debug2DImmediateRender* lpRender, char* lpcText,
                                     s32 liCount, f32 lfY)
    {
        lpRender->DrawText(lpcText, GetX() + 5.0f, lfY + GetY(), 10.0f, 0xFF000000u);
        if (miLoadedResources > 0)
        {
            const s32 liValue = mauValues[liCount];
            const f32 lfWidth = (static_cast<f32>(liValue) / static_cast<f32>(miLoadedResources)) *
                                ((GetWidth() - 60.0f) - 5.0f);
            char lacText[32];
            CgsCore::SPrintf(lacText, sizeof(lacText), "%d", liValue);
            lpRender->DrawBox(GetX() + 60.0f, GetY() + lfY,
                              (GetX() + 60.0f) + lfWidth, (GetY() + lfY) + 11.0f, 0xFF0000FFu);
            lpRender->DrawText(lacText, (GetX() + 60.0f) + 2.0f,
                               (lfY + GetY()) + 0.5f, 10.0f, 0x88000000u);
        }
    }
}
