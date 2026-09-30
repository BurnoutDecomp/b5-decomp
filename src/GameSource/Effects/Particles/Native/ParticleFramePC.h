#pragma once

#include "GameSource/Effects/Particles/Native/BrnSimpleParticleArray.h"
#include "GameShared/GameClasses/Numeric/CgsRandom.h"
#include <cstring>
#include <mutex>
#include <vector>

namespace BrnParticle { namespace Native {

// FLAG PC-platform leaf: the native CPU vertex builder consumes a completed
// frame while update can overwrite the original spawn rings. Publish only at
// the joined frame boundary; storage is allocated during particle preparation.
class SimpleParticleFramePC
{
public:
    void Prepare(const BrnSimpleParticleArray* lpSource)
    {
        for (u32 luType = 0; luType < eParticleArray_Max; ++luType)
        {
            maRegular[luType].resize(lpSource[luType].mBankRegular.muNumParticles);
            maCrash[luType].resize(lpSource[luType].mBankCrash.muNumParticles);
        }
        mbPrepared = true;
        Publish(lpSource);
    }

    void Publish(const BrnSimpleParticleArray* lpSource)
    {
        if (!mbPrepared) return;
        for (u32 luType = 0; luType < eParticleArray_Max; ++luType)
        {
            maArrays[luType] = lpSource[luType];
            if (lpSource[luType].mpStandardParams)
            {
                maParams[luType] = *lpSource[luType].mpStandardParams;
                maArrays[luType].mpStandardParams = &maParams[luType];
            }
            CopyBank(maArrays[luType].mBankRegular, maRegular[luType]);
            CopyBank(maArrays[luType].mBankCrash, maCrash[luType]);
        }
    }

    BrnSimpleParticleArray* GetArrays() { return maArrays; }

private:
    static void CopyBank(BrnSimpleParticleArray::CB4ParticleBank& lrBank,
                         std::vector<CB4Particle>& lrStorage)
    {
        if (!lrStorage.empty())
            std::memcpy(lrStorage.data(), lrBank.mpaParticles,
                        lrStorage.size() * sizeof(CB4Particle));
        lrBank.mpaParticles = lrStorage.data();
    }

    bool mbPrepared = false;
    BrnSimpleParticleArray maArrays[eParticleArray_Max] = {};
    CB4ParticleArrayStandardParams maParams[eParticleArray_Max] = {};
    std::vector<CB4Particle> maRegular[eParticleArray_Max];
    std::vector<CB4Particle> maCrash[eParticleArray_Max];
};

// Keep the original shared RNG stream. Lock only arithmetic draws, never an
// assertion, rendering operation or frame wait, which can require the other
// thread to make progress.
class ParticleRandomAccessPC
{
public:
    template<class Draw> decltype(auto) Execute(CgsNumeric::Random& lrRandom, Draw lfDraw)
    {
        std::lock_guard<std::mutex> lLock(mMutex);
        return lfDraw(lrRandom);
    }
private:
    std::mutex mMutex;
};

} }
