#include "GameSource/Jobs/ParticleRender/ParticleRender.h"
#include "GameShared/Jobs/CgsJobAssertBuffer.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"
#include "GameShared/GameClasses/Development/CgsStrStream.h"
#include "SDKs/EATech/eathread/BrnEAThreadX360.h"

namespace
{
    ParticleRenderJob gaParticleRenderJobs[6];
    const u32 kPreLionTypes[10] = { 3, 4, 5, 6, 7, 8, 9, 10, 11, 12 };
    const u32 kPostLionTypes[2] = { 1, 2 };
}
CGS_STATIC_ASSERT(sizeof(ParticleRenderJob) == 1, "ARTIST particle worker stride");

// ARTIST 8291DD58. The local job's second Param holds the submitted record.
void ParticleRenderEntry(EA::Jobs::Param, EA::Jobs::Param param, EA::Jobs::Param, EA::Jobs::Param)
{
    const u64 id = static_cast<u64>(reinterpret_cast<uintptr_t>(EA::Thread::GetThreadId()));
    const u64 index = (id - 0xF8000088ull) >> 2;
    if (index >= 6)
    {
        CgsDev::StrStream message(byte_83015F20, sizeof(byte_83015F20));
        const u64 reported = static_cast<u64>(reinterpret_cast<uintptr_t>(EA::Thread::GetThreadId()));
        message << "SPU Id out of range: " << reported << "\n";
        const char* const file = "D:\\P4\\B5_MAIN\\Burnout\\MAIN\\Code\\GameSource\\Jobs\\ParticleRender\\ParticleRender.cpp";
        CgsDev::Assert::PrintStringed(file, 0, 54, message.GetBuffer());
        __asm { mfmsr r12 }
        __asm { ori r12, r12, 0x400 }
        __asm { mtmsr r12 }
        CgsDev::Assert::PrintStringed(file, 0, 54, message.GetBuffer());
        __asm { mfmsr r12 }
        __asm { ori r12, r12, 0x400 }
        __asm { mtmsr r12 }
    }
    gaParticleRenderJobs[static_cast<u32>(index)].Execute(static_cast<ParticleRenderJobData*>(param.mpValue));
}

// ARTIST 8291DE88: split the simple batches around the Lion draw.
void ParticleRenderJob::RenderSimpleParticles(ParticleRenderJobData* data)
{
    BrnParticle::Native::SimpleParticleBatchArray& batches = data->mSimpleParticleBatchArray;
    batches.Clear();
    BrnParticle::Native::BrnSimpleParticleArray* arrays =
        const_cast<BrnParticle::Native::BrnSimpleParticleArray*>(data->mpaSimpleParticleArrays);
    BrnParticle::Native::SimpleParticleVertexBufferBuilder::BuildDispatchData(
        &data->mDestinationVertexBuffer, batches, arrays, kPreLionTypes, 10,
        data->mfCurrentTime, data->mCamera, data->mfWhiteLevel, data->mbIsCrash);
    batches.SetPreLionCount();
    BrnParticle::Native::SimpleParticleVertexBufferBuilder::BuildDispatchData(
        &data->mDestinationVertexBuffer, batches, arrays, kPostLionTypes, 2,
        data->mfCurrentTime, data->mCamera, data->mfWhiteLevel, data->mbIsCrash);
}

// ARTIST 8291DF38: the two flags independently select the worker arms.
void ParticleRenderJob::Execute(ParticleRenderJobData* data)
{
    if (data->mbRenderParticles)
        RenderSimpleParticles(data);
    if (data->mbRenderSparks)
    {
        data->mSparkBatchArray.Clear();
        BrnParticle::Native::SparkVertexBufferBuilder::BuildDispatchData(
            &data->mDestinationVertexBuffer, data->mSparkBatchArray,
            const_cast<BrnParticle::Native::SparkArray*>(data->mpaSparkArrays), 4,
            data->mfTimeMultiplier, data->mSparkFrameDataSet, data->mfWhiteLevel, data->mbIsCrash);
    }
}
