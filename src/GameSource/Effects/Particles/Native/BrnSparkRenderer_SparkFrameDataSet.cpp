// ============================================================================
// GameSource/Effects/Particles/Native/BrnSparkRenderer_SparkFrameDataSet.cpp
//
// BrnParticle::Native::SparkFrameDataSet::GetFrame @ 0x8291FA40
//
// SparkFrameDataSet is the ring of knSparksMaxBlurFrames (== 8) motion-blur frame
// snapshots (SparkFrameData maFrames[8]). GetFrame indexes maFrames[] by frame id.
//   asm: cmplwi r29,8 ; blt -> skip assert ; mulli r11,r29,0xD0 ; add r3,r11,r28
//   -> returns this + 208*luFrameId == &maFrames[luFrameId]. The 0xD0 (208)
//   multiplier IS the SparkFrameData stride (confirmed by DecFIGS DWARF
//   BrnSparkRenderer.h and the committed ParticleModule.h frame-set placeholder).
// ============================================================================

#include "GameSource/Effects/Particles/Native/BrnSparkRenderer.h"
#include "GameShared/GameClasses/Core/CgsAssert.h" // CGS_ASSERT

namespace BrnParticle { namespace Native {

// X360 @ 0x8291FA40. Called from SparkArray::RenderBank.
const SparkFrameData& SparkFrameDataSet::GetFrame(u32 luFrameId) const
{
    CGS_ASSERT( luFrameId < knSparksMaxBlurFrames, "luFrameId < knSparksMaxBlurFrames" );
    return maFrames[luFrameId];
}

} } // namespace BrnParticle::Native
