#pragma once

#include "types.hpp"
#include <xtl.h>
#include "SDKs/RenderEngineClub/MAIN/components/src/states/programvariablehandle.h"
#include "rw/math/vpu/types.h"

// ARTIST off_83271608. The native SDK owns the D3DDevice declaration and ABI.
extern D3DDevice* gpD3DDevice;

namespace renderengine
{
    class PixelBuffer;
    // ARTIST Device-owned native surfaces 83271100/83271140 and flag8327161C.
    extern PixelBuffer gOriginalColorBuffer;
    extern PixelBuffer gOriginalDepthStencilBuffer;
    extern u32 guTilingEnabled;


    class RenderTargetState;
    enum PrimitiveType;

    // ARTIST 822768D0 writes the destination followed by the complete handle.
    struct FloatShaderStateIterator
    {
        rw::math::vpu::Vector4* m_dest;
        ProgramVariableHandle m_handle;
    };

    class Device
    {
    public:
        struct DirectDraw
        {
            struct Parameters
            {
                Parameters() : m_primitiveType(static_cast<PrimitiveType>(0)) {}
                void SetPrimitiveType(PrimitiveType leType) { m_primitiveType = leType; }
                PrimitiveType GetPrimitiveType() const { return m_primitiveType; }

            protected:
                PrimitiveType m_primitiveType;
            };

            // BeginDirectDraw captures these two values for BeginBatch/EndBatch.
            // ARTIST 823C7098..7290 keeps them in r29/r31; 827DFFF0..FFFC
            // initializes only the device word in the persistent iterator.
            struct BatchIterator
            {
                BatchIterator() : m_device(0) {}

            protected:
                D3DDevice* m_device;
                PrimitiveType m_primitiveType;
            };
        };

        // ARTIST 822768D0. The native SDK owns the constant arrays and masks.
        static void BeginShaderStates(const ProgramVariableHandle& lrHandle,
                                      FloatShaderStateIterator& lrIterator);

        // ARTIST 82B61B38 and the inlined default-state load at 823F926C.
        static void SetState(const RenderTargetState* lpState);
        static const RenderTargetState* GetDefaultRenderTargetState() { return m_renderTargetState; }

        // ARTIST 82B61F08: remove every texture binding of this resource.
        static void UnSetResource(D3DResource* lpResource);

    protected:
        // DecFIGS device.h:1662; ARTIST dword_83271614.
        static RenderTargetState* m_renderTargetState;
    };
}
