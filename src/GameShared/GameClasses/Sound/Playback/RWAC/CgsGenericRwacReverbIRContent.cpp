#include "GameShared/GameClasses/Sound/Playback/RWAC/CgsGenericRwacContent.h"

// Reconstructed from BURNOUT_X360_ARTIST.XEX @ 0x826D7EE0
//   (CgsSound::Playback::GenericRwacReverbIRContent::`scalar deleting destructor')
//
// The X360 dtor is the compiler synthesis over this class's layout:
//   v4 = *(this+44); if (v4) *(v4+16) = *(this+48);   // unlink alias-list node
//   v5 = *(this+48); if (v5) *(v5+12) = *(this+44);
//   *(this+44) = this+32; *(this+48) = this+32;        // re-self-link the ring
//   CgsSound::Playback::Content::~Content(this);        // base dtor
//   if (a2 & 1) operator delete(this);                  // scalar-deleting tail
//
// this+32 (0x20) is mLoader.mpResource (a ResourcePtr<AlignedBinaryFileResource>),
// whose BaseResourcePtr alias links live at +0x0C/+0x10 == object +44/+48. The
// unlink + re-self-link IS CgsResource::BaseResourcePtr::~BaseResourcePtr()
// (@0x821F1E18). Defining the class destructor out-of-line makes the compiler emit
// that exact sequence: destroy mLoader (-> ~ResourcePtr -> ~BaseResourcePtr unlink)
// then run the Content base dtor. The body is empty -- the members and base do all
// the work the asm performs.

namespace CgsSound
{
namespace Playback
{
    GenericRwacReverbIRContent::GenericRwacReverbIRContent(Factory& arFactory,
                                                           const ContentSpec& akrSpec,
                                                           u32 au32Ident)
        : Content(arFactory, akrSpec, au32Ident),
          mLoader()
    {
    }

    bool GenericRwacReverbIRContent::DoLoad()
    {
        return mLoader.Load(*this, GetContentSpec());
    }

    bool GenericRwacReverbIRContent::DoUnload()
    {
        return mLoader.Unload(*this, GetContentSpec());
    }

    void GenericRwacReverbIRContent::DoUpdate(f32 /*af32Dt*/)
    {
        mLoader.Update(*this, GetContentSpec());
    }

    void* GenericRwacReverbIRContent::DoGetData()
    {
        return mLoader.GetData();
    }

    GenericRwacReverbIRContent::~GenericRwacReverbIRContent()
    {
        // mLoader (-> ResourcePtr<AlignedBinaryFileResource> -> ~BaseResourcePtr
        // alias-list unlink) and the Content base dtor are run implicitly here.
    }
}
}
