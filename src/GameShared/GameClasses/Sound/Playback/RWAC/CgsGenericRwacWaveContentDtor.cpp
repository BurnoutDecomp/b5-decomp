#include "GameShared/GameClasses/Sound/Playback/RWAC/CgsGenericRwacContent.h"

// Reconstructed from BURNOUT_X360_ARTIST.XEX @ 0x826C7A48
//   (CgsSound::Playback::GenericRwacWaveContent::`scalar deleting destructor')
//
// Identical-shape to the ReverbIR dtor (same Content + ContentLoader<> layout):
//   v4 = *(this+44); if (v4) *(v4+16) = *(this+48);   // unlink alias-list node
//   v5 = *(this+48); if (v5) *(v5+12) = *(this+44);
//   *(this+44) = this+32; *(this+48) = this+32;        // re-self-link the ring
//   CgsSound::Playback::Content::~Content(this);        // base dtor
//   if (a2 & 1) operator delete(this);                  // scalar-deleting tail
//
// this+32 (0x20) is mLoader.mpResource (a ResourcePtr<BinaryFileResource>); the
// alias links at +0x0C/+0x10 of its BaseResourcePtr == object +44/+48. The unlink +
// re-self-link IS CgsResource::BaseResourcePtr::~BaseResourcePtr() (@0x821F1E18),
// which the compiler runs when it destroys mLoader.mpResource. Defining the class
// destructor out-of-line emits that exact sequence; the body is empty.
//
// NOTE: This is the destructor TU. The class's DoUnload TU (@0x826DA0E0) lives in
// CgsGenericRwacWaveContent.cpp; both are members of the same class and resolve at
// consolidation against this shared home.

namespace CgsSound
{
namespace Playback
{
    GenericRwacWaveContent::~GenericRwacWaveContent()
    {
        // mLoader (-> ResourcePtr<BinaryFileResource> -> ~BaseResourcePtr alias-list
        // unlink) and the Content base dtor are run implicitly here.
    }
}
}
