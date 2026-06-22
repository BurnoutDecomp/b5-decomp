// Compile-gate embed check for the IO-OutputBuffers group's four IOBuffer-derived
// OutputBuffer types. Instantiates each by value (forcing the full member layout + every
// _AssertLayout() static_assert offset pin to compile) and references every reconstructed
// accessor so declarations and definitions are type-checked together.

#include "GameShared/GameClasses/Gui/CgsGuiModuleIO.h"
#include "GameShared/GameClasses/System/Resource/CgsPoolModuleIO.h"
#include "GameSource/World/EntityModules/PropEntityModule/BrnPropEntityModuleIO.h"
#include "GameSource/Physics/BrnPhysicsModuleIO.h"

namespace
{
    void ExerciseGuiOutputBuffer()
    {
        CgsGui::CgsGuiModuleIO::OutputBuffer lBuffer;
        CgsGui::CgsGuiModuleIO::OutputBuffer::_AssertLayout();

        (void)lBuffer.GetGuiResourceRequestQueue();
        (void)static_cast<const CgsGui::CgsGuiModuleIO::OutputBuffer&>(lBuffer).GetOutEventQueue();

        CgsGui::CgsGuiModuleIO::OutputBuffer::GuiEventQueueSmall lSource;
        (void)lBuffer.AddGuiOutEvents(&lSource);
    }

    void ExercisePoolOutputBuffer()
    {
        CgsResource::PoolIO::OutputBuffer lBuffer;
        CgsResource::PoolIO::OutputBuffer::_AssertLayout();

        const CgsResource::PoolIO::OutputBuffer& lrConst = lBuffer;
        (void)lrConst.GetPoolOutputQueue();
        (void)lrConst.GetPoolResourceRequestQueue();
        (void)lBuffer.GetPoolOutputQueue();
        (void)lBuffer.GetPoolResourceRequestQueue();
    }

    void ExercisePropPreSceneOutputBuffer()
    {
        // ~800KB / ~1MB buffers: use static storage so the compile-gate exerciser does not
        // allocate an enormous stack frame.
        static BrnWorld::PropEntityIO::OutputBuffer_PreScene lBuffer;
        BrnWorld::PropEntityIO::OutputBuffer_PreScene::_AssertLayout();

        (void)lBuffer.GetResourceRequestInterface();
        (void)static_cast<const BrnWorld::PropEntityIO::OutputBuffer_PreScene&>(lBuffer).GetPropInputInterface();
        (void)lBuffer.GetPropInputInterface();
    }

    void ExercisePhysicsOutputBuffer()
    {
        static BrnPhysics::PhysicsModuleIO::OutputBuffer lBuffer;
        BrnPhysics::PhysicsModuleIO::OutputBuffer::_AssertLayout();

        const BrnPhysics::PhysicsModuleIO::OutputBuffer& lrConst = lBuffer;
        (void)lBuffer.GetVehicleOutputRequestInterface();
        (void)lrConst.GetVehicleOutputInterface();
        (void)lBuffer.GetVehicleOutputInterface();
        (void)lrConst.GetPropManagerOutputInterface();
        (void)lBuffer.GetPropManagerOutputInterface();
        (void)lBuffer.GetDeformationOutputInterface();
        (void)lBuffer.GetContactSpyInterface();
    }
}

void* gpIoOutputBuffersEmbedCheck[] = {
    reinterpret_cast<void*>(&ExerciseGuiOutputBuffer),
    reinterpret_cast<void*>(&ExercisePoolOutputBuffer),
    reinterpret_cast<void*>(&ExercisePropPreSceneOutputBuffer),
    reinterpret_cast<void*>(&ExercisePhysicsOutputBuffer),
};
