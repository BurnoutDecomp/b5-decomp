#include "GameShared/GameClasses/Gui/Model/Resources/CgsAptDataHeaderType.h"
#include "GameShared/GameClasses/Gui/View/AptInterface/CgsAptDataHeader.h"
#include "rw/rwcore_structs.h"

// ARTIST 82502C30; DecFIGS B76FC4.
CgsResource::ResourceDescriptor CgsResource::AptDataHeaderType::GetSerialisedResourceDescriptor(const void* lpResource) const
{
    const CgsGui::AptDataHeader* lpHeader = static_cast<const CgsGui::AptDataHeader*>(lpResource);
    CgsResource::ResourceDescriptor lResult;
    lResult.m_baseResourceDescriptors[0].m_size = lpHeader->muSizeOfHeader;
    lResult.m_baseResourceDescriptors[0].m_alignment = 16;
    return lResult;
}
