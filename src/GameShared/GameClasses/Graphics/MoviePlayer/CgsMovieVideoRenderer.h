#ifndef CGS_MOVIE_VIDEO_RENDERER_H
#define CGS_MOVIE_VIDEO_RENDERER_H

#include "types.hpp"   // f32

// CgsGraphics::MovieVideoRenderer -- the movie player's rw::movie IVideoRenderer implementation.
// Its position/scale/rendering-delay control surface is fixed: the renderer always presents the
// decoded movie frame full-screen at the default transform, so every position/scale query is
// unsupported (asserts-false, returns the identity value) and every position/scale setter accepts
// only the default value (0 for position, 1 for scale), asserting on any other.
//
// LIGHT DECLARATION SURFACE: this header declares only the ten position/scale/delay accessors whose
// bodies are homed in CgsMovieVideoRenderer.cpp. The full movie-renderer member layout (the
// rw::movie IVideoRenderer base, movie-texture data, decode job, texture state, release callback) is
// X360-documented but NOT modelled here -- those accessor stubs touch no members, so the extra state
// is intentionally omitted to keep this a clean, compilable declaration surface. GROW additively
// (base, members, the frame-submission methods) when those TUs land; do NOT fork the type.

namespace CgsGraphics
{
    class MovieVideoRenderer
    {
    public:
        // --- position (default 0.0f; any other value is unsupported) ---
        float GetPositionX();                 // @ 0x827EAC30  (asserts-false, returns 0)
        float GetPositionZ();                 // @ 0x827EACC0  (asserts-false, returns 0)
        void  SetPositionX(float lfPositionX);// @ 0x827EAB40  (asserts unless == 0)
        void  SetPositionY(float lfPositionY);// @ 0x827EAB90  (asserts unless == 0)
        void  SetPositionZ(float lfPositionZ);// @ 0x827EABE0  (asserts unless == 0)

        // --- scale (default 1.0f; any other value is unsupported) ---
        float GetScaleX();                    // @ 0x827EADA8  (asserts-false, returns 1)
        float GetScaleY();                    // @ 0x827EADF0  (asserts-false, returns 1)
        void  SetScaleX(float lfScaleX);      // @ 0x827EAD08  (asserts unless == 1)
        void  SetScaleY(float lfScaleY);      // @ 0x827EAD58  (asserts unless == 1)

        // --- timing ---
        float GetRenderingDelay();            // @ 0x827EAB30  (this renderer adds no delay: 0)

        // --- lifetime ---
        // Destructor @ 0x827EAA98. Its body (release the four movie output textures, destruct
        // the embedded EA::Jobs::Job at +0x180, restore the vtable words) is owned by the sibling
        // decode/teardown TU keyed to CgsMovieVideoRenderer.cpp and is still todo -- so it is
        // DECLARED here (so the deleting-destructor thunk below can name it) but NOT defined in
        // this file. Non-virtual in this light declaration surface: the real X360 object is
        // polymorphic (a vtable word sits at +0), but this surface deliberately does not model the
        // rw::movie IVideoRenderer base / vtable. GROW this to `virtual ... override` with the full
        // member layout when the destructor TU lands.
        ~MovieVideoRenderer();
    };

    // MovieVideoRenderer::'vector deleting destructor' @ 0x827EF3A0. The MSVC deleting-destructor
    // thunk MSVC emits for `delete`: run the destructor, then -- only when the low bit of the flag
    // argument is set -- hand the block to the global ::operator delete; return the object either
    // way. Modelled with the committed XxxDeletingDtor(T*, char) free-function shape (cf.
    // rw::movie::IVideoRendererScalarDeletingDtor @ 0x827E9F00 and DeviceScalarDeletingDtor).
    MovieVideoRenderer* MovieVideoRendererVectorDeletingDtor(MovieVideoRenderer* lpRenderer, char lcFlags);
}

#endif
