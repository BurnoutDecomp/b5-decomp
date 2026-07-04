#include "GameSource/World/EntityModules/WorldEntityModule/PVSModule/SharedIO/BrnPVSModuleEvents.h" // BrnWorld::PVSIO::InputBuffer (Construct)

// BrnWorld::<owner>::ConstructPVSInputBuffer  @ 0x827E4110
// Reconstructed from BURNOUT_X360_ARTIST.XEX. A thin construct-and-return wrapper:
//   addi r31,this,0x230; PVSIO::InputBuffer::Construct(this+0x230); return this+0x230;
// It constructs the PVSIO::InputBuffer sub-object embedded at +0x230 of its owner and returns
// its address. PVSIO::InputBuffer::Construct is the committed inline hook in BrnPVSModuleEvents.h
// (it drives EventQueue<GetZoneRequest,8>::Construct).
//
// LOW CONFIDENCE (flag for consolidator): the owning class is not attested by this slice
// (Hex-Rays truncated the name to 'PVS'; the dossier's caller list is empty). Modelled here as a
// free function taking the owner's `this`. BrnPVSModule.h shows PVSModule :
// ModuleSingleBufferedTemplate<PVSIO::InputBuffer, PVSIO::OutputBuffer>, so the real owner is most
// likely BrnWorld::PVSModule with its InputBuffer at +0x230 -- re-home this as a member of the
// real owner when that +0x230 embed is confirmed, keeping the +0x230 offset and the Construct
// call exact.
namespace BrnWorld
{
    // Owner-relative: pOwner is the object that embeds a PVSIO::InputBuffer at +0x230.
    PVSIO::InputBuffer* ConstructPVSInputBuffer(void* pOwner)
    {
        PVSIO::InputBuffer* lpInputBuffer =
            reinterpret_cast<PVSIO::InputBuffer*>(static_cast<unsigned char*>(pOwner) + 0x230);
        lpInputBuffer->Construct();
        return lpInputBuffer;
    }
}
