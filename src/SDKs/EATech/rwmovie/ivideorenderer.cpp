// =====================================================================================
// rw::movie::IVideoRenderer -- scalar deleting destructor.
//
// Reconstructed from BURNOUT_X360_ARTIST.XEX; the PowerPC asm is authoritative. No
// reference source and no DecFIGS DWARF hints exist for this TU.
//
//   rw::movie::IVideoRenderer::`scalar deleting destructor'  @0x827E9F00
//
// IVideoRenderer is an abstract video-renderer interface (a polymorphic base: the first
// word of the object is its vtable pointer). The scalar deleting destructor is the compiler
// thunk MSVC emits for `delete`:
//   *this = &IVideoRenderer::vtable;   // stw r11,0(r31) -- reinstall this level's vtable
//   if (flags & 1)                     // clrlwi r10,r4,31; cmplwi; beq
//       operator delete(this);         // bl operator_delete  (global ::operator delete)
//   return this;
// The base destructor itself is trivial (no member teardown -- the asm does nothing but the
// vtable store, which the C++ dtor prologue emits implicitly); only the vtable reinstall and
// the (flags & 1) global-delete branch are observable. Modelled with the committed
// XxxScalarDeletingDtor(T*, char) free-function shape (cf. rw::core::filesys::Handle
// @0x82BBDFA8). The vtable word at +0 (X360 off_820D5798) is the address of this class's
// vtable; in the PC reconstruction it is installed implicitly by the virtual destructor.
// =====================================================================================

#include "types.hpp"

namespace rw
{
    namespace movie
    {
        // Abstract video-renderer interface. Polymorphic: a vtable pointer occupies the
        // first object word (X360 off_820D5798 == this class's vtable). The virtual
        // destructor reinstalls that vtable word and runs no member teardown (the X360
        // ~IVideoRenderer body is just the implicit vtable store).
        class IVideoRenderer
        {
        public:
            virtual ~IVideoRenderer() {}
        };

        // IVideoRenderer::`scalar deleting destructor' @0x827E9F00.
        //   ~IVideoRenderer(this);          // implicit vtable reinstall at +0
        //   if (flags & 1)
        //       operator delete(this);      // global ::operator delete
        //   return this;
        IVideoRenderer* IVideoRendererScalarDeletingDtor(IVideoRenderer* lpRenderer, char lcFlags)
        {
            lpRenderer->~IVideoRenderer();
            if (lcFlags & 1)
            {
                ::operator delete(lpRenderer);
            }
            return lpRenderer;
        }
    }
}
