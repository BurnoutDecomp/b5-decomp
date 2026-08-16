#ifndef GAMESOURCE_EFFECTS_PARTICLES_PARTICLEMODULEBRINGUP_H
#define GAMESOURCE_EFFECTS_PARTICLES_PARTICLEMODULEBRINGUP_H

#include "types.hpp"

namespace BrnGame     { struct DispatchThreadInputBuffer; }
// NOTE THE CLASS-KEY: BrnDirector::Camera::Camera is a `struct` (Camera.h:59,
// `struct alignas(16) Camera`, and the DWARF agrees -- Camera.h:40). MSVC mangles struct
// (`U`) and class (`V`) DIFFERENTLY, so declaring it `class` here produced a DEF/UNDEF pair
// that did not match across translation units (caught by the dumpbin delta in this pass's
// gate, not by the compiler -- `cl /c` cannot see it). Several existing headers do declare it
// `class Camera;` and get away with it only because they include Camera.h first.
namespace BrnDirector { namespace Camera { struct Camera; } }

// ==================================================================================================
// [FLAG PC bring-up] GameSource/Effects/Particles/ParticleModuleBringUp.h
//
// The PC stand-in for the console's ParticleRenderData PRODUCER -- the pair
//   BrnParticle::ParticleModule::Update                 @0x822817D8 (DWARF ParticleModule.h:434)
//   BrnParticle::ParticleModule::GenerateRenderRequests @0x82281BD8 (DWARF ParticleModule.h:573)
// which together fill the module's own 528-byte record at ParticleModule+0x8E00 and memcpy it into
// BrnGame::DispatchThreadInputBuffer::mParticleRenderData every frame.
//
// NEITHER RUNS ON THIS BUILD. ParticleModule.cpp is not on the build list and
// BrnEffects::EffectsModule::Update / ::GenerateDispatchLists -- the console's two callers -- are
// not reconstructed:
//   $ grep -n "Particle" tools/build/build_game_exe.bat
//   3281:  echo "%SRC%\SDKs\Packages\Lion\Final\eauk_lion\Dev\LionRuntime\include\ParticleWaveForm.cpp"
//   (one hit, the LION vendor waveform TU -- no ParticleModule.cpp, no EffectsModule.cpp)
// So the record is never written, and BrnRendererModule::Render has had to pass a NULL to
// BrnRendererUpdatePostFxMotionBlur since the rung-7 producers wave -- which is why motion blur has
// been alive-but-motionless (MotionBlurState keeps Construct's identity/identity pair).
//
// This TU produces the record with EXACTLY the console's semantics from the DIRECTOR'S published
// camera and the game module's SIM timer, at the DoDispatch seam where the director camera is
// already staged for the other bring-up consumers (BrnGameModule.cpp, beside
// mRenderModule.PCBringUpSetCameraInput).
//
// DELETE-WHEN: BrnParticle::ParticleModule and BrnEffects::EffectsModule are on the build list and
// EffectsModule::GenerateDispatchLists @0x82296668 drives the real pair.
// ==================================================================================================
namespace BrnParticle
{
    // The producer. Stands in for ParticleModule::Update @0x822817D8 followed by
    // ParticleModule::GenerateRenderRequests @0x82281BD8, plus the LockForWrite/UnlockForWrite
    // bracket their console caller EffectsModule::GenerateDispatchLists @0x82296668 puts round the
    // second one.
    //
    // The three floats are the console's own three virtual-Update arguments, read off the SIM timer
    // exactly as BrnEffects::EffectsModule::Update @0x8229EC28 reads them from the effects input
    // buffer's TimerStatusInterface (its call at pseudocode line 399,
    // `(*(this->field_A80 + 68))(&this->field_A80, v68, v73, v67)`):
    //   lfTimeStep            == f1 == mbRunning ? (mfBaseTimeStep * mfTimeStepMultiplier) : 0.0f
    //   lfTime                == f2 == mTime.miSeconds + mTime.mfFraction
    //   lfTimeStepMultiplier  == f3 == mfTimeStepMultiplier
    // (PPC ABI: each float SKIPS its GPR slot, so the Camera* rides r7 -- AGENTS.md rule 4.)
    //
    // A null buffer or a null camera is ignored (the console can never be handed either).
    void PCBringUpProduceParticleRenderData(BrnGame::DispatchThreadInputBuffer* lpBuffer,
                                            const BrnDirector::Camera::Camera*  lpCamera,
                                            f32                                 lfTimeStep,
                                            f32                                 lfTime,
                                            f32                                 lfTimeStepMultiplier);

    // The "a producer has stamped the record" latch. It is HOST-ONLY and lives in the bring-up TU
    // rather than as a member of BrnGame::DispatchThreadInputBuffer or of ParticleRenderData,
    // because both of those are console types whose layout/behaviour is attested: the console's
    // Construct deliberately leaves mParticleRenderData UNCLEARED (BrnDispatchThreadInputBuffer.cpp
    // :216) and adding a member to say "it is clear now" would be a state the binary does not have,
    // in a struct that is memcpy'd 528 bytes at a time on the console. A file-static in the producer
    // is the smallest thing that cannot be mistaken for console state.
    // ⚠ It is asked PER BUFFER INSTANCE: the dispatch-thread input buffer is double-buffered and
    // the producer only runs on DoDispatch frames, so "some buffer was stamped once" would vouch
    // for the other, still-uninitialised instance (step-9 verify). The renderer passes the buffer
    // it read-locked and gets "THIS instance has been stamped at least once".
    // DELETE with the producer.
    bool PCBringUpParticleRenderDataProducedFor(const BrnGame::DispatchThreadInputBuffer* lpBuffer);
}

#endif // GAMESOURCE_EFFECTS_PARTICLES_PARTICLEMODULEBRINGUP_H
