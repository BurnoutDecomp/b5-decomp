#pragma once

// ============================================================================
// SDKs/Packages/Lion/Final/eauk_lion/Dev/LionRuntime/include/ParticleWaveForm.h
//
// cParticleWaveForm     -- one Lion (eauk_lion) particle wave-form descriptor.
//   Carries a (base, phase, freq, amp, clampMin, clampMax) parameter set plus a
//   per-parameter variance, and an mType selecting one of the precomputed
//   wave-form lookup tables. Evaluate(time) samples the selected table and maps
//   it through amp/base then clamps.
//
// cParticleWaveFormTable -- a process-wide singleton holding six 512-entry FP32
//   lookup tables (noise / sawtooth / sawtoothInv / sin / square / triangle),
//   built once at AppInit and sampled by cParticleWaveForm::Evaluate.
//
// LAYOUT AUTHORITY: member set / order / types are from the DecFIGS DWARF
// (ParticleWaveForm.h:25 / :114). Offsets used by the runtime are verified
// against the X360 asm:
//   cParticleWaveForm::Evaluate @0x82909DE8 reads
//     mType   @0x04, mBase @0x08, mPhase @0x0C, mFreq @0x10, mAmp @0x14,
//     mClampMin @0x18, mClampMax @0x1C   (the +variance fields follow).
//   cParticleWaveFormTable build loops write the six tables at byte offsets
//     mNoiseTable @0x000, mSawToothTable @0x800, mSawToothInvTable @0x1000,
//     mSinTable @0x1800, mSquareTable @0x2000, mTriangleTable @0x2800
//   (Init / NoiseTableBuild / SawToothInvTableBuild / SinTableBuild /
//    TriangleTableBuild @0x8290E7F0 / 0x82909F68 / 0x82909FC0 / 0x8290A008 /
//    0x8290A0A8). X360 pointers are 32-bit; members are accessed BY NAME only.
//
// Reuses the committed Lion sibling type cParticleRandomSeed (LionBindings.h /
// ParticleBucket.h home) by forward-declaration; the two-arg Evaluate overload
// references it by reference only.
//
// HONEST PLACEHOLDER (flagged -- see ParticleWaveForm.cpp dep_flags):
// cLionSerialiser is the Lion serialiser; its sibling home LionSerialiser.h is
// committed and is forward-declared here (serialise methods are declared, not
// bodied in this TU).
// ============================================================================

#include "types.hpp"

class cLionSerialiser;          // LionSerialiser.h (sibling home)
struct cParticleRandomSeed;     // LionBindings.h / ParticleBucket.h (sibling home)

// ParticleWaveForm.h:25
struct cParticleWaveForm
{
public:
    // Wave-form type selectors (mType). 0 == constant (default 1.0); 1..7 select
    // a lookup table / interpolation mode -- see Evaluate's switch.
    enum E_Type
    {
        E_TYPE_CONSTANT     = 0,
        E_TYPE_NOISE        = 1,    // nearest-sample noise table
        E_TYPE_NOISE_LERP   = 2,    // linearly-interpolated noise table
        E_TYPE_SQUARE       = 3,
        E_TYPE_SAWTOOTH     = 4,
        E_TYPE_SAWTOOTH_INV = 5,
        E_TYPE_SIN          = 6,
        E_TYPE_TRIANGLE     = 7
    };

    void Init();
    void Lerp(const cParticleWaveForm& aWave0, const cParticleWaveForm& aWave1, f32 afWeight);
    f32  Evaluate(f32 afTime) const;
    f32  Evaluate(f32 afTime, cParticleRandomSeed& arSeed) const;

    void Delocate(u32 auEndianTwiddleFlag);
    void Relocate();

    void               GetSerialiseSize(cLionSerialiser& arSer) const;
    cParticleWaveForm* Serialise(cLionSerialiser& arSer) const;
    void               Remap(cLionSerialiser& arSer);

    u32  GetType() const               { return mType; }
    void SetType(u32 auType)           { mType = auType; }

    f32  GetBase() const               { return mBase; }
    void SetBase(f32 afBase)           { mBase = afBase; }

    f32  GetPhase() const              { return mPhase; }
    void SetPhase(f32 afPhase)         { mPhase = afPhase; }

    f32  GetFreq() const               { return mFreq; }
    void SetFreq(f32 afFreq)           { mFreq = afFreq; }

    f32  GetAmp() const                { return mAmp; }
    void SetAmp(f32 afAmp)             { mAmp = afAmp; }

    f32  GetClampMin() const           { return mClampMin; }
    void SetClampMin(f32 afMin)        { mClampMin = afMin; }

    f32  GetClampMax() const           { return mClampMax; }
    void SetClampMax(f32 afMax)        { mClampMax = afMax; }

    f32  GetBaseVariance() const       { return mBaseVariance; }
    void SetBaseVariance(f32 afV)      { mBaseVariance = afV; }

    f32  GetPhaseVariance() const      { return mPhaseVariance; }
    void SetPhaseVariance(f32 afV)     { mPhaseVariance = afV; }

    f32  GetFreqVariance() const       { return mFreqVariance; }
    void SetFreqVariance(f32 afV)      { mFreqVariance = afV; }

    f32  GetAmpVariance() const        { return mAmpVariance; }
    void SetAmpVariance(f32 afV)       { mAmpVariance = afV; }

    f32  GetClampMinVariance() const   { return mClampMinVariance; }
    void SetClampMinVariance(f32 afV)  { mClampMinVariance = afV; }

    f32  GetClampMaxVariance() const   { return mClampMaxVariance; }
    void SetClampMaxVariance(f32 afV)  { mClampMaxVariance = afV; }

    // ParticleWaveForm.h:94..109 -- member set/order from DWARF.
    u32 mID;                  // @0x00
    u32 mType;                // @0x04
    f32 mBase;                // @0x08
    f32 mPhase;               // @0x0C
    f32 mFreq;                // @0x10
    f32 mAmp;                 // @0x14
    f32 mClampMin;            // @0x18
    f32 mClampMax;            // @0x1C
    f32 mBaseVariance;        // @0x20
    f32 mPhaseVariance;       // @0x24
    f32 mFreqVariance;        // @0x28
    f32 mAmpVariance;         // @0x2C
    f32 mClampMinVariance;    // @0x30
    f32 mClampMaxVariance;    // @0x34
};

// ParticleWaveForm.h:114
struct cParticleWaveFormTable
{
public:
    enum
    {
        KU_TABLE_SIZE   = 512,
        KU_HALF_SIZE    = 256
    };

    // Logical table selector indices used by GetTable(mType).
    enum E_TableIndex
    {
        E_TABLE_NOISE        = 0,
        E_TABLE_SAWTOOTH     = 1,
        E_TABLE_SAWTOOTH_INV = 2,
        E_TABLE_SIN          = 3,
        E_TABLE_SQUARE       = 4,
        E_TABLE_TRIANGLE     = 5
    };

    void Init();

    f32 GetCos(s32 aiIndex);
    f32 GetNoise(s32 aiIndex)       { return mNoiseTable[aiIndex & (KU_TABLE_SIZE - 1)]; }
    f32 GetSawTooth(s32 aiIndex)    { return mSawToothTable[aiIndex & (KU_TABLE_SIZE - 1)]; }
    f32 GetSawToothInv(s32 aiIndex) { return mSawToothInvTable[aiIndex & (KU_TABLE_SIZE - 1)]; }
    f32 GetSin(s32 aiIndex)         { return mSinTable[aiIndex & (KU_TABLE_SIZE - 1)]; }
    f32 GetSquare(s32 aiIndex)      { return mSquareTable[aiIndex & (KU_TABLE_SIZE - 1)]; }
    f32 GetTriangle(s32 aiIndex)    { return mTriangleTable[aiIndex & (KU_TABLE_SIZE - 1)]; }

    static cParticleWaveFormTable* GetMe() { return &mSingleton; }

    f32* GetNoiseTable()        { return mNoiseTable; }
    f32* GetSawToothTable()     { return mSawToothTable; }
    f32* GetSawToothInvTable()  { return mSawToothInvTable; }
    f32* GetSinTable()          { return mSinTable; }
    f32* GetSquareTable()       { return mSquareTable; }
    f32* GetTriangleTable()     { return mTriangleTable; }
    f32* GetTable(u32 auIndex);

private:
    void NoiseTableBuild();
    void SawToothTableBuild();
    void SawToothInvTableBuild();
    void SinTableBuild();
    void SquareTableBuild();
    void TriangleTableBuild();

    // ParticleWaveForm.h:183..188 -- six contiguous 512-entry tables.
    f32 mNoiseTable[KU_TABLE_SIZE];        // @0x000
    f32 mSawToothTable[KU_TABLE_SIZE];     // @0x800
    f32 mSawToothInvTable[KU_TABLE_SIZE];  // @0x1000
    f32 mSinTable[KU_TABLE_SIZE];          // @0x1800
    f32 mSquareTable[KU_TABLE_SIZE];       // @0x2000
    f32 mTriangleTable[KU_TABLE_SIZE];     // @0x2800

    // ParticleWaveForm.h:174 -- process-wide singleton instance.
    static cParticleWaveFormTable mSingleton;
};
