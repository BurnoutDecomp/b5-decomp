#include "types.hpp"

#include "GameShared/GameClasses/Sound/Playback/RWAC/CgsGenericRwacContent.h"

// Reconstructed from BURNOUT_X360_ARTIST.XEX @ 0x826DA0E0
//   (CgsSound::Playback::GenericRwacWaveContent::DoUnload)
//
// X360: `return ContentLoader<BinaryFileResource>::Unload(this + 32, this, *(this + 12))`
//   this + 32     == the embedded `mLoader` (ContentLoader<BinaryFileResource>,
//                    object +0x20 -- CgsGenericRwacContent.h:70 / DWARF)
//   *(this + 12)  == Content::mContentSpec (the const ContentSpec& member @ +0x0C)
// i.e. BY NAME: delegate the unload to the embedded loader with this content and
// its spec.
//
// (2026-08-25, audio-faithfulness wave 3: the former TU-local fabricated
// `GenericRwacWaveContent { virtual bool DoUnload(); }` + `BinaryFileResource_`
// rivals -- which ODR-collided with the real CgsGenericRwacContent.h home this TU's
// own sibling dtor uses, and returned int over raw byte offsets -- are retired.)

namespace CgsSound
{
namespace Playback
{

GenericRwacWaveContent::GenericRwacWaveContent(Factory& arFactory,
                                               const ContentSpec& akrSpec,
                                               u32 au32Ident)
    : Content(arFactory, akrSpec, au32Ident),
      mLoader()
{
}

bool GenericRwacWaveContent::DoLoad()
{
    return mLoader.Load(*this, GetContentSpec());
}

bool GenericRwacWaveContent::DoUnload()
{
    return mLoader.Unload(*this, GetContentSpec());
}

void GenericRwacWaveContent::DoUpdate(f32 /*af32Dt*/)
{
    mLoader.Update(*this, GetContentSpec());
}

void* GenericRwacWaveContent::DoGetData()
{
    return mLoader.GetData();
}

bool GenericRwacWaveContent::GetStreamPath(char* apBuffer, size_t aBufferSize) const
{
    return GetContentSpec().GetPathZone(1, apBuffer, aBufferSize);
}

} // namespace Playback
} // namespace CgsSound
