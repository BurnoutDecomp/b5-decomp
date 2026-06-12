#include "types.hpp"

// Reconstructed from BURNOUT_X360_ARTIST.XEX @ 0x827DEB98
//   (BrnDirector::DirectorResourceManager::DirectorResourceManager  — constructor)
//
// Behaviour-faithful to the X360 pseudocode. Installs the vtable at offset 552,
// then default-constructs a run of 65 contiguous 16-byte sub-objects spanning
// offsets 568..1592 (each via the shared constructor sub_827DC838), with the one
// at offset 1496 built by a different constructor (sub_827DC8C8). Finally clears
// three trailing fields (1608, 1616, 1624).
//
// The two element constructors are unnamed in the export; forward-declared here as
// data-stub externals (real bodies land with their own TUs). The vtable symbol
// off_820CEA3C is likewise an external data reference.

namespace BrnDirector
{
    // Unnamed sub-object constructors from the export.
    extern "C" int sub_827DC838(void* lpObject, int liArg0, int liArg1);   // @ 0x827DC838
    extern "C" int sub_827DC8C8(void* lpObject, int liArg0, int liArg1);   // @ 0x827DC8C8

    // Vtable for DirectorResourceManager (off_820CEA3C), defined elsewhere.
    extern void* const gpDirectorResourceManagerVTable;

    // This is the real C++ constructor (DirectorResourceManager::DirectorResourceManager
    // @ 0x827DEB98) — not the class's separate empty inline Construct(). The pseudocode's
    // `return a1` is the constructor returning `this` per the PPC ABI; in C++ that is implicit.
    struct DirectorResourceManager
    {
        DirectorResourceManager();
    };

    static const int KI_SUBOBJECT_FIRST  = 568;
    static const int KI_SUBOBJECT_LAST   = 1592;   // inclusive
    static const int KI_SUBOBJECT_STRIDE = 16;
    static const int KI_SUBOBJECT_SPECIAL_OFFSET = 1496;   // built by sub_827DC8C8

    DirectorResourceManager::DirectorResourceManager()
    {
        char* lpBase = reinterpret_cast<char*>(this);

        *reinterpret_cast<void**>(lpBase + 552) = gpDirectorResourceManagerVTable;

        for (int liOffset = KI_SUBOBJECT_FIRST; liOffset <= KI_SUBOBJECT_LAST;
             liOffset += KI_SUBOBJECT_STRIDE)
        {
            if (liOffset == KI_SUBOBJECT_SPECIAL_OFFSET)
                sub_827DC8C8(lpBase + liOffset, 0, 0);
            else
                sub_827DC838(lpBase + liOffset, 0, 0);
        }

        *reinterpret_cast<u32*>(lpBase + 1608) = 0;
        *reinterpret_cast<u32*>(lpBase + 1616) = 0;
        *reinterpret_cast<u32*>(lpBase + 1624) = 0;
    }
}
