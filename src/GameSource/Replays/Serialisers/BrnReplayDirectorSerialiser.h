#ifndef GAMESOURCE_REPLAYS_SERIALISERS_BRNREPLAYDIRECTORSERIALISER_H
#define GAMESOURCE_REPLAYS_SERIALISERS_BRNREPLAYDIRECTORSERIALISER_H

#include "GameShared/GameClasses/Core/CgsAssert.h"
#include "GameSource/Replays/BrnReplayBaseSerialiser.h"
#include "GameSource/Director/Camera/Camera.h"
#include "GameSource/Director/Camera/Utils/BrnTempBoostResponder.h"
#include "GameSource/Director/Camera/Utils/BrnSpeedResponder.h"

namespace BrnReplays
{
    // ARTIST DirectorModule::Update 82275478..82275518 writes the camera,
    // car key (+0x160), boost responder (+0x168), speed responder (+0x170),
    // and handbrake timer (+0x174), in this order. ReplayDirector::FillInSharedInfo
    // reads the same tail at 82250C68..82250C94. Camera pointer members widen on
    // PC, so allocation and stream sizes follow this native record's sizeof.
    // Member names other than mCarBaseAssetAttribsKey are inferred from those
    // already-homed producers; the latter name is in the 8225BD28 assertion.
    struct DirectorSerialiserStaticLayout
    {
        BrnDirector::Camera::Camera mCamera;
        u64 mCarBaseAssetAttribsKey;
        BrnDirector::Camera::Utils::TempCameraBoostResponder mTempCameraBoostResponder;
        BrnDirector::Camera::Utils::SpeedResponder mSpeedResponder;
        f32 mfLastHandbrakeTime;
    };

    class DirectorSerialiser : public BaseSerialiser
    {
    public:
        static const s32 KI_STATIC_LAYOUT_SIZE = sizeof(DirectorSerialiserStaticLayout);

        // Inlined in DirectorModule::Construct 8225C698..8225C6C4: the
        // BaseSerialiser initialiser followed by the derived +0x5C latch reset.
        void Construct();

        // ARTIST 821F5C58; original header lines 107/108.
        DirectorSerialiserStaticLayout* GetStaticLayout()
        {
            CGS_ASSERT(GetStaticBufferSize() >= KI_STATIC_LAYOUT_SIZE,
                       "Static buffer size is too small\n");
            CGS_ASSERT(GetStaticBuffer() != 0, "GetStaticBuffer()");
            return static_cast<DirectorSerialiserStaticLayout*>(GetStaticBuffer());
        }

        void Read();   // ARTIST 82650340
        void Write();  // ARTIST 82650438

    private:
        bool mbStaticCameraConstructed;  // ARTIST +0x5C
    };
}

#endif
