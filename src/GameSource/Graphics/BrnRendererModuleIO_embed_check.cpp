// Compile-gate embed check for RendererIO::InputBuffer / OutputBuffer.
// Instantiates each buffer by value (forcing the full member layout, the embedded camera, and the
// camera-offset pin in InputBuffer::Construct to compile) and exercises the bodied Construct funcs.

#include "GameSource/Graphics/BrnRendererModuleIO.h"

namespace
{
    void ExerciseRendererIOBuffers()
    {
        RendererIO::InputBuffer  lInput;
        RendererIO::OutputBuffer lOutput;

        lInput.Construct();    // @ 0x82400190
        lOutput.Construct();   // @ 0x824001A8
    }
}

// Reference the exerciser so it is not pruned before instantiation.
void* gpRendererIOBuffersEmbedCheck = reinterpret_cast<void*>(&ExerciseRendererIOBuffers);
