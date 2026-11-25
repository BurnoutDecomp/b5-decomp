// SubMixConnector links a Send or Route into a SubMix. LegacySrc submix.h
// and the Xbox 360 audio PDB give the node and connector declarations.

#include "rw/audio/core/SubMixConnector.h"
#include "rw/audio/core/Route.h"
#include "rw/audio/core/SubMix.h" // SubMix (the real class; moved out of SubMixConnector.h)

namespace rw
{
namespace audio
{
namespace core
{

#if defined(_XBOX) && defined(_MSC_VER)
// ARTIST and the original Xbox 360 plug-in object use the scalar size form here.
#pragma optimize("s", on)
#endif
// ARTIST 0x82B9C3C0
void SubMixConnector::Disconnect(f32 *foldBackGains)
{
    if (!mpSubMix)
        return;

    mpSubMix->mSendList.Remove(&mListNode);
    if (foldBackGains)
    {
        mpSubMix->mDeClickRequired = 1;
        for (int channel = 0; channel < mpSubMix->mOutputChannels; ++channel)
            mpSubMix->mDeClickValueTotal[channel] += foldBackGains[channel];
    }
    mpSubMix = 0;
    mpSubMixBuffer = 0;
    mNumSubMixChannels = 0;
}

// The original Xbox 360 rwAudio plug-in object owns these Route callers and
// Disconnect. Keeping them together lets XDK track the callee's register use.
void Route::ReleaseEvent()
{
    mSubMixConnector.Disconnect(mDeClickValue);
    for (unsigned channel = 0; channel < 6; ++channel)
        mDeClickValue[channel] = 0.0f;
}

int Route::ConnectByPointerHandler(Command *command)
{
    RouteConnectCommand *params = static_cast<RouteConnectCommand *>(command);
    Route *route = static_cast<Route *>(params->pObject);
    route->mSubMixConnector.Disconnect(route->mDeClickValue);
    for (unsigned channel = 0; channel < 6; ++channel)
        route->mDeClickValue[channel] = 0.0f;

    SubMix *submix = params->mpSubMix;
    if (submix)
    {
        route->mSubMixConnector.Connect(route->mBase.mpVoice, submix);
        route->mSourceStartChannel = static_cast<u8>(params->mfGain0);
        route->mTargetStartChannel = static_cast<u8>(params->mfGain1);
        route->mNumChannels = static_cast<u8>(params->mfGain2);
    }
    return sizeof(RouteConnectCommand);
}

#if defined(_XBOX) && defined(_MSC_VER)
#pragma optimize("s", off)
#endif

} // namespace core
} // namespace audio
} // namespace rw
