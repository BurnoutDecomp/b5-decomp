#include "GameSource/World/AI/BrnAIBoundaryLine.h"
#include <stddef.h>
CGS_STATIC_ASSERT((sizeof(BrnAI::BoundaryLine) == 16), "native boundary extent");
CGS_STATIC_ASSERT((__alignof(BrnAI::BoundaryLine) == 16), "native boundary alignment");
CGS_STATIC_ASSERT((sizeof(Vector2) == 16), "native vector extent");
CGS_STATIC_ASSERT((offsetof(BrnAI::BoundaryLine, mfEndX) == 8), "endpoint placement");
typedef Vector2 (BrnAI::BoundaryLine::*InterpSignature)(f32) const;
InterpSignature gInterpSignature = &BrnAI::BoundaryLine::GetInterp;
