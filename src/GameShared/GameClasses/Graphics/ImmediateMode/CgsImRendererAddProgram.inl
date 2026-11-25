// DecFIGS CgsImRenderer.h:479 and ARTIST 827EFC48. Instantiate per native vertex owner.
template <typename V>
s8 ImRenderer<V>::AddProgram(rw::IResourceAllocator* lpAllocator,
                             const void* lpVertexProgramBinary, u32 luVertexProgramSize,
                             const void* lpPixelProgramBinary, u32 luPixelProgramSize)
{
    s8 li8ProgramIndex = 0;
    do
    {
        if (!mapVertexProgramBuffer[li8ProgramIndex])
        {
            CGS_ASSERT(mapPixelProgramBuffer[li8ProgramIndex] == 0,
                CGS_ASSERT_MESSAGE_AT("mapPixelProgramBuffer[ li8ProgramIndex ] == NULL", KAC_IM_RENDERER_ASSERT_FILE, 487));
            break;
        }
    } while (++li8ProgramIndex < KI8_MAX_PROGRAMS);

    CGS_ASSERT(li8ProgramIndex < KI8_MAX_PROGRAMS,
               CGS_ASSERT_MESSAGE_AT("Adding too many shader programs to the immediate mode renderer", KAC_IM_RENDERER_ASSERT_FILE, 491));

    renderengine::ProgramBufferParameters lVertexParams;
    lVertexParams.mMicrocodeParts.cbCachedPartSize    = 0;
    lVertexParams.mMicrocodeParts.cbPhysicalPartSize    = 0;
    lVertexParams.muConstantTableSize = 0;
    lVertexParams.muNumVariables      = 0;
    lVertexParams.miShaderType        = 0;
    lVertexParams.mpFunction          = lpVertexProgramBinary;
    lVertexParams.muProgramSize         = luVertexProgramSize;

    const rw::ResourceDescriptor lVertexDescriptor = renderengine::ProgramBuffer::GetResourceDescriptor(lVertexParams);

    rw::Resource lVertexLayout = lpAllocator->Allocate(lVertexDescriptor, 0);
    mapVertexProgramBuffer[li8ProgramIndex] =
        reinterpret_cast<renderengine::ProgramBuffer*>(
            renderengine::ProgramBuffer::Initialize(&lVertexLayout, &lVertexParams));

    renderengine::ProgramBufferParameters lPixelParams;
    lPixelParams.mMicrocodeParts.cbCachedPartSize    = 0;
    lPixelParams.mMicrocodeParts.cbPhysicalPartSize    = 0;
    lPixelParams.muConstantTableSize = 0;
    lPixelParams.muNumVariables      = 0;
    lPixelParams.mpFunction          = lpPixelProgramBinary;
    lPixelParams.muProgramSize         = luPixelProgramSize;
    lPixelParams.miShaderType        = 1;

    const rw::ResourceDescriptor lPixelDescriptor = renderengine::ProgramBuffer::GetResourceDescriptor(lPixelParams);

    rw::Resource lPixelLayout = lpAllocator->Allocate(lPixelDescriptor, 0);
    mapPixelProgramBuffer[li8ProgramIndex] =
        reinterpret_cast<renderengine::ProgramBuffer*>(
            renderengine::ProgramBuffer::Initialize(&lPixelLayout, &lPixelParams));

    return li8ProgramIndex;
}
