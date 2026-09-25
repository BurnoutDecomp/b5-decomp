#include "SharedClasses/DataLists/BrnPopupController.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"
#include "GameShared/GameClasses/Core/CgsID.h"                            // CgsIDCompress / CgsIDUnCompress / KI_CGSID_STRING_LEN
#include "GameShared/GameClasses/Development/Log/CgsLog.h"                // gpDebugPrint / gxMessageFilterFlags
#include "GameShared/GameClasses/Development/CgsStrStream.h"              // StrStreamBase operator<<
#include "GameShared/GameClasses/System/Resource/CgsResourceIOEvents.h"  // CgsResource::Events::AcquireResourceResponse
#include "GameShared/GameClasses/System/Resource/CgsResourceHandle.h"    // CgsResource::ResourceHandle (the bind)
#include "GameSource/Gui/BrnGuiEventTypeDefs.h"                           // BrnGui::GuiOverlayFullInfoResponse

#include <cstring>   // memcpy

// BrnResource::PopupController: GetIndexFromPopupHash, AddPopupResource and GetPopup.
// Construct has no body in the image (see the header).

namespace BrnResource
{
// Linear-scan the loaded popup table for the record whose 64-bit
// name-id hash matches lPopupId; -1 when the table is empty or the hash is absent.
// The not-loaded guard (cpp:142) is non-gating.
s32 PopupController::GetIndexFromPopupHash(CgsID lPopupId) const
{
    CGS_ASSERT(mbIsPopupLoaded, "Trying to use a popup resource before it is loaded");

    s32 liIndex = 0;                              // cpp:144
    const s32 liTotal = mPopupsPtr->miPopupCount; // cpp:145 (s16 sign-extended)
    if (liTotal <= 0)
        return -1;

    s32 liEntry = 0;
    while (mPopupsPtr->mppPopupData[liEntry]->mNameId != lPopupId)
    {
        ++liIndex;
        ++liEntry;
        if (liIndex >= liTotal)
            return -1;
    }
    return liIndex;
}

// Bind the controller to a freshly-acquired popup resource bundle.
// Asserts the controller is not already holding one, then binds mPopupsPtr to the
// ResourceHandle the pool reply carries and latches the
// loaded flag. Called by BrnResource::GameDataModule::PreparePopups.
void PopupController::AddPopupResource(const CgsResource::Events::AcquireResourceResponse* lpResource)
{
    CGS_ASSERT(!mbIsPopupLoaded, "Trying to use a popup resource when one is already being used");   // cpp:122

    // The console binds from the response's handle pair ({mpResourceMemory, mpSourceEntry},
    // CreateFromHandle on response +0x18); spelled by name through the ResourcePtr
    // assign-from-handle, which is the same CreateFromHandle.
    CgsResource::ResourceHandle lHandle;
    lHandle.mpResourceMemory = lpResource->mpResourceMemory;
    lHandle.mpSourceEntry    = lpResource->mpSourceEntry;
    mPopupsPtr = lHandle;
    mbIsPopupLoaded = true;
}

// Resolve the popup named by lpOverlayInfo->mNameId and stamp its record
// into the overlay-info response. Falls back to the "TestPopup" default when the hash is
// absent; returns false only when even the default is missing. Called by
// BrnGui::GuiOverlaysDirector::SetUpOverlayInfo.
bool PopupController::GetPopup(BrnGui::GuiOverlayFullInfoResponse* lpOverlayInfo) const
{
    CGS_ASSERT(lpOverlayInfo, "lpOverlayInfo");                                          // cpp:57
    CGS_ASSERT(mbIsPopupLoaded, "Trying to use a popup resource before it is loaded");   // cpp:58

    s32 liIndex = GetIndexFromPopupHash(lpOverlayInfo->mNameId);   // cpp:60
    if (liIndex == -1)
    {
        char lacCurrentHash[KI_CGSID_STRING_LEN];                 // cpp:63
        CgsIDUnCompress(lpOverlayInfo->mNameId, lacCurrentHash);
        if ((CgsDev::Message::gxMessageFilterFlags & 1) != 0)
            *CgsDev::Log::gpDebugPrint << "Popup not found with hash : " << lacCurrentHash << "\n";

        liIndex = GetIndexFromPopupHash(CgsIDCompress("TestPopup"));
        if (liIndex == -1)
        {
            CGS_ASSERT(false, "Can't find default popup to display");   // cpp:72
            return false;
        }
    }

    // Stamp the located popup record into the overlay response. The two records share the
    // leading name/style/icon/title/message layout but place the button-id spans at
    // different offsets, so the copy is field-for-field (per-field copies).
    const CgsGui::GuiPopup* lpCurrentPopup = mPopupsPtr->mppPopupData[liIndex];   // cpp:80

    memcpy(lpOverlayInfo->macTitleId,   lpCurrentPopup->macTitleId,   CgsGui::GuiPopup::MKI_MAX_LENGTH_OF_STRING_ID);
    memcpy(lpOverlayInfo->macMessageId, lpCurrentPopup->macMessageId, CgsGui::GuiPopup::MKI_MAX_LENGTH_OF_STRING_ID);
    memcpy(lpOverlayInfo->macButton1Id, lpCurrentPopup->macButton1Id, CgsGui::GuiPopup::MKI_MAX_LENGTH_OF_STRING_ID);
    memcpy(lpOverlayInfo->macButton2Id, lpCurrentPopup->macButton2Id, CgsGui::GuiPopup::MKI_MAX_LENGTH_OF_STRING_ID);

    memcpy(lpOverlayInfo->macName, lpCurrentPopup->macName, sizeof(lpCurrentPopup->macName));  // 13 bytes
    lpOverlayInfo->meStyle = lpCurrentPopup->meStyle;
    lpOverlayInfo->meIcon  = lpCurrentPopup->meIcon;
    return true;
}
}
