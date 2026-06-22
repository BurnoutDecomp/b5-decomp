// Embed-check for class:CgsResource::Events::BundleLoaderEvent::SetFileName.
// Instantiates a BundleLoaderEvent by value and exercises SetFileName on both the
// non-null and null paths so the gate links the whole TU. Not shipped; lives next
// to the home.
#include "GameShared/GameClasses/System/Resource/CgsResourceIOEvents.h"

int CgsResourceIOEvents_EmbedCheck()
{
    CgsResource::Events::LoadBundleRequest kRequest;

    kRequest.SetFileName("BUNDLES/COMMON.BUNDLE");
    int liSum = static_cast<int>(kRequest.macFileName[0]);

    kRequest.SetFileName(nullptr);
    liSum += static_cast<int>(kRequest.macFileName[0]);

    return liSum;
}
