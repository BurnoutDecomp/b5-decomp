#ifndef GAMESOURCE_DIRECTOR_DIRECTORMODULE_BRN_DIRECTOR_INPUT_OUTPUT_H
#define GAMESOURCE_DIRECTOR_DIRECTORMODULE_BRN_DIRECTOR_INPUT_OUTPUT_H

#include "types.hpp"

// ============================================================================
// GameSource/Director/DirectorModule/BrnDirectorInputOutput.h
//
// BrnDirector::DirectorInputOutput -- the per-frame IO bundle the DirectorModule builds
// on its own stack and hands to every director sub-update (MainDirector::Update /
// ::PreSceneQueryUpdate / ::PostGuiUpdate, ReplayDirector::Update / ::PreSceneQueryUpdate,
// and, through them, the arbitrator states and camera behaviours).
//
// RECOVERED THIS WAVE. `BrnMainDirector.h` already declared every one of its methods as
// taking `const class DirectorInputOutput* lpIO`, but the type had NO definition anywhere
// in the tree. Its shape is proven three times over by the identical stack-frame
// construction in the three X360 DirectorModule entry points:
//
//   DirectorModule::PreSceneQueryUpdate @0x8225C768   (locals v24[5])
//   DirectorModule::Update              @0x82275300   (locals v28[5])
//   DirectorModule::PostGuiUpdate       @0x82250DD0   (locals v12[5])
//
//     v[0] = a4/a1              -> the director INPUT buffer     (DirectorIO::InputBuffer*)
//     v[1] = a5                 -> the director OUTPUT buffer    (DirectorIO::OutputBuffer*)
//     v[2] = this + 584         -> the module's DirectorResourceManager (module +0x248)
//     v[3] = this + 2216        -> the module's WorldMap              (module +0x8A8)
//     v[4] = &<stack SceneQueryInterface>  -> the per-frame scene-query post office
//
// Corroboration from the consumer side (MainDirector::Update @0x82274070):
//     *a2      is passed to DirectorIO::InputBuffer::GetPlayerCarIndex / GetUsedRaceCars /
//              GetTimerStatusInter / GetControll        -> slot 0 IS the input buffer
//     a2[1]    is passed to DirectorIO::OutputBuffer::SetCgsCamera / SetCameraOutput /
//              GetTimerRequestInterfac / GetDirectorOutputIn -> slot 1 IS the output buffer
//     a2[2]    is passed to CameraFinaliser::Update and
//              Camera::BehaviourManager::PrepareBehaviours   -> slot 2 IS the resource manager
//
// X360 CONSOLE size is 5 words (0x14). This is a plain by-pointer bundle of live objects
// (no serialised content), so on the x64 host it simply widens to 5 pointers -- parity is
// BY NAMED MEMBER, and nothing byte-pins it (the console offsets above are provenance).
// ----------------------------------------------------------------------------

namespace BrnDirector
{
    // Slot types. The buffers and the resource manager have committed homes; including them
    // here would drag the whole IO/camera cone into every consumer, so they are forward-
    // declared -- this bundle only ever holds and forwards their addresses.
    namespace DirectorIO
    {
        struct InputBuffer;
        struct OutputBuffer;
    }
    class  DirectorResourceManager;
    class  WorldMap;
    struct SceneQueryInterface;

    struct DirectorInputOutput
    {
        // +0x00 (console) -- the frame's published director input (read-locked by the caller).
        const DirectorIO::InputBuffer* mpInputBuffer;

        // +0x04 -- the frame's director output (write-locked by the caller). This is where the
        // finished camera lands: OutputBuffer::SetCameraOutput / ::SetCgsCamera.
        DirectorIO::OutputBuffer*      mpOutputBuffer;

        // +0x08 -- the module's own DirectorResourceManager (module +0x248).
        DirectorResourceManager*       mpResourceManager;

        // +0x0C -- the module's own WorldMap (module +0x8A8).
        WorldMap*                      mpWorldMap;

        // +0x10 -- the per-frame scene-query post office the module rebuilds on its stack each
        // call (Clear()ed before hand-off; see DirectorModule::PreSceneQueryUpdate).
        SceneQueryInterface*           mpSceneQueryInterface;
    };
}

#endif // GAMESOURCE_DIRECTOR_DIRECTORMODULE_BRN_DIRECTOR_INPUT_OUTPUT_H
