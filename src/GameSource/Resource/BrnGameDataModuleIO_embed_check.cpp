// Compile-only embedder check for this Resource group's two TUs:
//   - BrnResource::GameDataIO::InputBuffer  (the two GetRequestInterface overloads)
//   - CgsResource::BundleV2::ResourceEntry  (GetDiskSize / GetUncompresssedResourceDescriptor)
// Not part of the shipping build: it embeds InputBuffer by value (forcing the declared member prefix
// -- the IOBuffer base + RequestInterface<32768> mRequestInterface -- to instantiate) and exercises
// each owned accessor by name, the way the producing/consuming code calls them.
#include "GameSource/Resource/BrnGameDataModuleIO.h"
#include "GameShared/GameClasses/System/Resource/CgsResourceBundle2.h"

#include "GameShared/GameClasses/System/Resource/CgsResourceType.h"   // CgsResource::ResourceDescriptor (descriptor fill target)

namespace BrnResource
{
namespace GameDataIO
{

// Stand-in for the owning game-data module: embeds the buffer by value (forces the declared layout
// prefix to instantiate; mRequestInterface lands at this+4 after the 4-byte IOBuffer base).
struct InputBufferEmbedderCheck
{
    InputBuffer mInputBuffer;
};

void EmbedderCheck_Exercise(InputBufferEmbedderCheck& lrEmbedder)
{
    InputBuffer& lrBuffer = lrEmbedder.mInputBuffer;

    // write-locked (non-const) accessor -- the writer building requests into the interface.
    (void)lrBuffer.GetRequestInterface();

    // read-locked (const) accessor -- the reader consuming the interface.
    (void)static_cast<const InputBuffer&>(lrBuffer).GetRequestInterface();
}

}   // namespace GameDataIO
}   // namespace BrnResource

namespace CgsResource
{

void EmbedderCheck_ExerciseResourceEntry(const BundleV2::ResourceEntry& lrEntry,
                                         CgsResource::ResourceDescriptor* lpDescriptor)
{
    // packed size accessor (low 28 bits of the per-pool on-disk size/alignment word).
    (void)lrEntry.GetDiskSize(0);

    // per-pool uncompressed descriptor fill (size + alignment for each of the 3 mem types).
    lrEntry.GetUncompresssedResourceDescriptor(lpDescriptor);
}

}   // namespace CgsResource
