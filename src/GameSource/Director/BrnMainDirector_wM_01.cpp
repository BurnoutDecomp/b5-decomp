// ============================================================================
// GameSource/Director/BrnMainDirector_wM_01.cpp
//
// BrnDirector::MainDirector::UpdateICE @0x82238FC0 -- the director's once-a-frame ICE tick. A
// PARTFILE of BrnMainDirector.cpp (OWNERLIST 2026-09-27, lane L5), kept apart only so the body could
// land without a second lane's hunk in that file; fold it back at will.
//
// ⭐ WHY IT MATTERS. It is the ONLY caller of BrnDirector::ICEWrapper::Update @0x82540180, i.e. the only
// thing that advances an ICE MOVIE (ICEWrapper::PlayMovie's take) and drives the ICE camera mover that
// turns it into a camera. Every director movie runs on it -- above all the pause menu's camera:
// ArbStateCrashNav loops the shared pause playlist through its ICEMoviePlayer and shows
// ICEWrapper::GetCamera, which only this tick moves. Without it the PC's pause kept the frozen gameplay
// camera. MainDirector::Update calls it right after UpdateMoments, inside the live-player-car arm,
// unless the director's debug zero-timestep byte is up (0x8227434C..0x8227436C:
// `lbzx this+0x3543C ; bne` -- maStateFlagTail[E_FLAG_TAIL_DEBUG_ZERO_TIMESTEP]).
//
// The console body, in order:
//   0x82238FE4  car = (GameState +0x144 meTargetVehicleRefType != 0)
//                         ? AllVehicleData::GetRaceCar(GameState +0x140 meTargetRaceCarIndex)
//                         : AllVehicleData::GetPlayer()                   (bne 0x82238FFC / fall-through)
//   0x8223900C  lCarToWorld = car +0x1F0 (its mRaceCarState.mTransform), copied to the stack
//   0x82239054  GetRaceCar(GetNearestRaceCarIndexToPlayer(1))  -> its +0x1F0 is the heading-2 space
//   0x82239064..0x82239134  the loose-heading (+0x80), heading (+0x40) and impact (+0x00) spaces of
//               AllVehicleData, copied to the stack
//   0x82239138  GetRaceCar(GetNearestRaceCarIndexToPlayer(1)) AGAIN -> its +0x1F0 is the car-2 space
//   0x82239170  CameraSpaceHandler::Construct(car, car2, GameState +0x10 (the traffic-light space),
//               this +0x12170 (mICESceneSpace), impact, heading, loose heading, heading 2,
//               this +0x166A4 (the shared gameplay-external camera))
//   0x8223918C  ICEWrapper::Update(this +0x50, InputBuffer::GetTimerStatusInterface(lpIO->mpInputBuffer),
//               the handler)
// The player-car index argument (r5) is not read. This is BuildBehaviourSharedInfo's handler build
// (0x82250074) with the first space taken from the ICE target car instead of the player; the nearest
// car's transform is read twice, as there.
// ============================================================================

#include "GameSource/Director/BrnMainDirector.h"
#include "GameSource/Director/DirectorModule/BrnDirectorInputOutput.h" // DirectorInputOutput (lpIO->mpInputBuffer)
#include "GameSource/Director/DirectorModule/BrnDirectorModuleIO.h"   // DirectorIO::InputBuffer::GetTimerStatusInterface
#include "SDKs/Packages/ICE/ICECameraSpaceHandler.hpp"                // ICE::CameraSpaceHandler::Construct

namespace BrnDirector
{
    void MainDirector::UpdateICE(const DirectorInputOutput* lpIO, s32 /*liPlayerCarIndex*/)
    {
        const Camera::VehicleInfo& lrTargetCar =
            (maGameState.meTargetVehicleRefType != VehicleRef::E_PLAYER_CAR)
                ? mAllVehicleData.GetRaceCar(maGameState.meTargetRaceCarIndex)
                : mAllVehicleData.GetPlayer();
        const Matrix44Affine lCarToWorld = lrTargetCar.mRaceCarState.mTransform;

        const Matrix44Affine& lrHeading2ToWorld =
            mAllVehicleData.GetRaceCar(mAllVehicleData.GetNearestRaceCarIndexToPlayer(1u)).mRaceCarState.mTransform;
        const Matrix44Affine lLooseHeadingToWorld = mAllVehicleData.GetPlayerLooseHeadingSpace();
        const Matrix44Affine lHeadingToWorld      = mAllVehicleData.GetPlayerHeadingSpace();
        const Matrix44Affine lImpactToWorld       = mAllVehicleData.GetPlayerImpactSpace();
        const Matrix44Affine& lrCar2ToWorld =
            mAllVehicleData.GetRaceCar(mAllVehicleData.GetNearestRaceCarIndexToPlayer(1u)).mRaceCarState.mTransform;

        ICE::CameraSpaceHandler lCameraSpaces;
        lCameraSpaces.Construct(lCarToWorld,
                                lrCar2ToWorld,
                                maGameState.mTrafficLightSpace,
                                mICESceneSpace,
                                lImpactToWorld,
                                lHeadingToWorld,
                                lLooseHeadingToWorld,
                                lrHeading2ToWorld,
                                &mArbitrator.GetSharedCameras().mGameplayExternal);

        mICEWrapper.Update(lpIO->mpInputBuffer->GetTimerStatusInterface(), lCameraSpaces);
    }
}
