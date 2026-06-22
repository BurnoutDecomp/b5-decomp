// Embed check for the CgsResource-types GetSerialisedResourceDescriptor group:
//   CgsResource::BinaryFileResourceType::GetSerialisedResourceDescriptor   @ 0x828EC990
//   CgsResource::HudMessageResourceType::GetSerialisedResourceDescriptor   @ 0x8267B110
//   CgsResource::MaterialStateResourceType::GetSerialisedResourceDescriptor@ 0x828A98C0 (pre-committed)
//
// Exercises the bodied descriptor virtuals and proves the BaseResourceDescriptors<5>
// layout the bodies depend on. BinaryFileResourceType has sibling virtuals owned by
// other TUs (GetTypeID/FixDown/FixUp), so it is NOT instantiated -- the body is invoked
// through an explicit qualified call on a placement reference to avoid emitting the
// (still-incomplete) vtable.

#include "GameShared/GameClasses/System/Resource/CgsBinaryFileResource.h"
#include "GameShared/GameClasses/Gui/Model/Resources/CgsGuiHudMessageType.h"
#include "GameShared/GameClasses/RenderWare/CgsMaterialStateResourceType.h"
#include "rw/rwcore_structs.h"

#include <cstddef>

using namespace CgsResource;

// Prove the descriptor layout the bodies write into (5 x {u32 size, u32 align}).
static_assert(sizeof(rw::BaseResourceDescriptor) == 8, "BaseResourceDescriptor is {u32,u32}");
static_assert(sizeof(ResourceDescriptor) == 40, "ResourceDescriptor<5> is 40 bytes");
static_assert(offsetof(rw::BaseResourceDescriptor, m_size) == 0, "size @ +0");
static_assert(offsetof(rw::BaseResourceDescriptor, m_alignment) == 4, "alignment @ +4");

// HudMessageResourceType is fully bodied (all four virtuals live in its .cpp), so it can
// be constructed and called virtually.
ResourceDescriptor ExerciseHudMessage(const void* lpResource)
{
    HudMessageResourceType lType;
    return lType.GetSerialisedResourceDescriptor(lpResource);
}

// MaterialStateResourceType is fully bodied (CgsMaterialStateResourceType.cpp), construct
// and call virtually.
ResourceDescriptor ExerciseMaterialState(const void* lpResource)
{
    MaterialStateResourceType lType;
    return lType.GetSerialisedResourceDescriptor(lpResource);
}

// BinaryFileResourceType has sibling virtuals owned by other TUs; invoke just the bodied
// descriptor via an explicit qualified (non-virtual) call so no vtable is required.
ResourceDescriptor ExerciseBinaryFile(const BinaryFileResourceType& lrType, const void* lpResource)
{
    return lrType.BinaryFileResourceType::GetSerialisedResourceDescriptor(lpResource);
}
