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
    };
}

#endif
