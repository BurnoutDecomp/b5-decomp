#define private public
#include "D:/Reverse/BP-X360-Experimental-Worktrees/compressor-native-3534/vendor/renderware/src/rw/audio/core/CompressorLimiter1.cpp"
#undef private
extern "C" void NativeGainPowerProbe(void* output, const void* base, const void* exponent)
{
    const __vector4 one = __vspltw(__vupkd3d(__vspltisw(0), VPACK_NORMSHORT2), 3);
    const __vector4 normalizedBase = __vmulfp(__lvx(base, 0), one);
    __stvx(rw::audio::core::GainPower(normalizedBase, __lvx(exponent, 0)), output, 0);
}
extern "C" void NativeLinkedGainProbe(void* output, const void* previous, const void* current, const void* firstMask)
{
    const __vector4 oldGain = __lvx(previous, 0);
    const __vector4 gain = __lvx(current, 0);
    const __vector4 candidate = __vsel(oldGain, gain, __vcmpgtuw(oldGain, gain));
    __stvx(__vsel(candidate, gain, __lvx(firstMask, 0)), output, 0);
}
extern "C" const unsigned NativeCompressorLayout[] = {
 sizeof(rw::audio::core::CompressorLimiter1),__alignof(rw::audio::core::CompressorLimiter1),
 sizeof(rw::audio::core::CompressorLimiter1::History),
 offsetof(rw::audio::core::CompressorLimiter1,mChannelHistory),
 offsetof(rw::audio::core::CompressorLimiter1,mThresholdOn),
 offsetof(rw::audio::core::CompressorLimiter1,mThresholdOff),
 offsetof(rw::audio::core::CompressorLimiter1,mCompExponent),
 offsetof(rw::audio::core::CompressorLimiter1,mAttackSamples),
 offsetof(rw::audio::core::CompressorLimiter1,mReleaseSamples),
 offsetof(rw::audio::core::CompressorLimiter1,mCompExponentStepOn),
 offsetof(rw::audio::core::CompressorLimiter1,mCompExponentStepOff),
 offsetof(rw::audio::core::CompressorLimiter1,mGroupChannels)
};
