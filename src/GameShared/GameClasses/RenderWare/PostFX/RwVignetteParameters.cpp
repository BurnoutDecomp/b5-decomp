#include "RwVignetteParameters.h"

namespace rw::graphics::postfx
{
Vignette::Parameters::Parameters()
    : muFlags(0),
      mZeroColour{0.0f, 0.0f, 0.0f, 0.0f},
      mUnitColourA{1.0f, 1.0f, 1.0f, 1.0f},
      mCentreScaleA{0.5f, 0.5f, 1.0f, 1.0f},
      mZeroColourB{0.0f, 0.0f, 0.0f, 0.0f},
      mUnitColourB{1.0f, 1.0f, 1.0f, 1.0f},
      mUnitColourC{1.0f, 1.0f, 1.0f, 1.0f},
      mCentreScaleB{0.5f, 0.5f, 1.0f, 1.0f},
      mUnitColourD{1.0f, 1.0f, 1.0f, 1.0f}
{
}
}
