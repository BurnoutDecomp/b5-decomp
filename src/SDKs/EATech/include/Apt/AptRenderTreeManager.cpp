// ===========================================================================
// EATech Apt -- AptRenderTreeManager factory + the scene-node render hooks.
// DECOMPILED from the PS3 EXTERNAL ELF.
//   AptRenderItem::Manager_CreateItem @0x814094 (the per-character-type factory).
//   AptRTM_CreateItem / AptRTM_GetTickItemWritable / AptCurrentRenderTreeManager
//   -- the helpers AptCharacterInst calls (homed here against the manager).
// ===========================================================================

#include "SDKs/EATech/include/Apt/AptRenderTreeManager.h"
#include "SDKs/EATech/include/Apt/AptRenderItemShape.h"
#include "SDKs/EATech/include/Apt/AptRenderItemSprite.h"
#include "SDKs/EATech/include/Apt/AptRenderItemAnimation.h"
#include "SDKs/EATech/include/Apt/AptCharacter.h"
#include "SDKs/EATech/include/Apt/AptCharacterInst.h"   // AptCurrentRenderTreeManager / AptRTM_* decls
#include "SDKs/EATech/include/Apt/AptDefine.h"           // gpNonGCPoolManager
#include "SDKs/EATech/Apt/DogmaAllocator.h"

#include <new>   // placement new

// ---------------------------------------------------------------------------
// Manager_CreateItem @0x814094 -- allocate the render-item subtype for a
// character's type (the Manager_CreateItem switch). The built subtypes
// (Shape/Sprite/Animation) dispatch precisely; the not-yet-built ones
// (DynamicText[2]/Morph[8]/StaticText[10]/Button/CustomControl + the null-char
// Level) fall back to a base render item (which draws nothing) -- FLAG: replace
// with the real subtype as each lands.
// ---------------------------------------------------------------------------
AptRenderItem* AptRenderItem::Manager_CreateItem(AptCharacter* pCharacter, int nTick)
{
    if (!pCharacter)
    {
        void* p = gpNonGCPoolManager->Allocate(sizeof(AptRenderItem));   // FLAG: -> AptRenderItemLevel
        return new (p) AptRenderItem(nullptr, nTick);
    }

    switch (pCharacter->mnType)
    {
        case 1:   // shape
        {
            void* p = gpNonGCPoolManager->Allocate(sizeof(AptRenderItemShape));
            return new (p) AptRenderItemShape(pCharacter, nTick);
        }
        case 5:   // sprite / movie clip
        {
            void* p = gpNonGCPoolManager->Allocate(sizeof(AptRenderItemSprite));
            return new (p) AptRenderItemSprite(pCharacter, nTick);
        }
        case 9:   // imported animation
        {
            void* p = gpNonGCPoolManager->Allocate(sizeof(AptRenderItemAnimation));
            return new (p) AptRenderItemAnimation(pCharacter, nTick);
        }
        default:  // FLAG: 2/8/10/Button/CustomControl not yet built -> base placeholder
        {
            void* p = gpNonGCPoolManager->Allocate(sizeof(AptRenderItem));
            return new (p) AptRenderItem(pCharacter, nTick);
        }
    }
}

// ---------------------------------------------------------------------------
// The render-tree-manager helpers AptCharacterInst calls (were FLAG'd externs).
// ---------------------------------------------------------------------------

// FLAG: the current target sim's render-tree manager (console gpCurrentTargetSim
// + 0x2C); wired by the AptTarget/AptInit startup. Null during bring-up, in which
// case AptCharacterInst creates no render item yet.
AptRenderTreeManager* AptCurrentRenderTreeManager()
{
    return 0;
}

AptRenderItem* AptRTM_CreateItem(AptRenderTreeManager* pMgr, AptCharacter* pCharacter, int nTick)
{
    return pMgr->Update_CreateItem(pCharacter, nTick);
}

AptRenderItem* AptRTM_GetTickItemWritable(AptRenderTreeManager* pMgr, const AptRenderItem* pItem, int nTick)
{
    return pMgr->Update_GetTickItemWritable(pItem, nTick);
}
