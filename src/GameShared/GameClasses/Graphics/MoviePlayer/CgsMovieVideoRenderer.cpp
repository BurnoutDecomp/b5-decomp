#include "GameShared/GameClasses/Graphics/MoviePlayer/CgsMovieVideoRenderer.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"

// CgsGraphics::MovieVideoRenderer position/scale/delay control surface, reconstructed from
// BURNOUT_X360_ARTIST.XEX. The renderer always presents the movie frame full-screen at the default
// transform: position queries assert-false and return 0, scale queries assert-false and return the
// identity 1, the position setters accept only 0 (flt_82001CC0), the scale setters accept only 1
// (flt_82001C98), and the renderer adds no rendering delay.

namespace CgsGraphics
{
    // @ 0x827EAC30. Position-X query is unsupported: asserts-false and returns 0.
    float MovieVideoRenderer::GetPositionX()
    {
        CGS_ASSERT(false, "false");
        return 0.0f;
    }

    // @ 0x827EACC0. Position-Z query is unsupported: asserts-false and returns 0.
    float MovieVideoRenderer::GetPositionZ()
    {
        CGS_ASSERT(false, "false");
        return 0.0f;
    }

    // @ 0x827EAB30. This renderer adds no rendering delay -- returns 0.
    float MovieVideoRenderer::GetRenderingDelay()
    {
        return 0.0f;
    }

    // @ 0x827EADA8. Scale-X query is unsupported: asserts-false and returns identity 1.
    float MovieVideoRenderer::GetScaleX()
    {
        CGS_ASSERT(false, "false");
        return 1.0f;
    }

    // @ 0x827EADF0. Scale-Y query is unsupported: asserts-false and returns identity 1.
    float MovieVideoRenderer::GetScaleY()
    {
        CGS_ASSERT(false, "false");
        return 1.0f;
    }

    // @ 0x827EAB40. Repositioning in X is unsupported: only the default value (0) is accepted;
    // the X360 compares the argument against 0.0 (flt_82001CC0) and asserts only when they differ.
    void MovieVideoRenderer::SetPositionX(float lfPositionX)
    {
        if (lfPositionX != 0.0f)
        {
            CGS_ASSERT(false, "false");
        }
    }

    // @ 0x827EAB90. Default position Y is 0.0f (flt_82001CC0); any other value asserts.
    void MovieVideoRenderer::SetPositionY(float lfPositionY)
    {
        CGS_ASSERT(lfPositionY == 0.0f, "false");
    }

    // @ 0x827EABE0. Default position Z is 0.0f (flt_82001CC0); any other value asserts.
    void MovieVideoRenderer::SetPositionZ(float lfPositionZ)
    {
        CGS_ASSERT(lfPositionZ == 0.0f, "false");
    }

    // @ 0x827EAD08. Default X scale is 1.0f (flt_82001C98); any other value asserts.
    void MovieVideoRenderer::SetScaleX(float lfScaleX)
    {
        CGS_ASSERT(lfScaleX == 1.0f, "false");
    }

    // @ 0x827EAD58. Default Y scale is 1.0f (flt_82001C98); any other value asserts.
    void MovieVideoRenderer::SetScaleY(float lfScaleY)
    {
        CGS_ASSERT(lfScaleY == 1.0f, "false");
    }
}
