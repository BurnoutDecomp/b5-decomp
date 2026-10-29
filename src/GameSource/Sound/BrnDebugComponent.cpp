#include "GameSource/Sound/BrnDebugComponent.h"
#include "BrnCommonTypes.h"
#include "GameShared/GameClasses/Core/CgsStringUtils.h"
#include "GameShared/GameClasses/Development/DebugSystem/Interface/CgsDebugInterface.h"
#include "GameShared/GameClasses/Development/DebugSystem/Render/CgsDebugRender.h"
#include "GameShared/GameClasses/Sound/Playback/RWAC/CgsGenericRwacFactory.h"
#include "GameShared/GameClasses/Sound/Playback/Splicer/CgsSpliceBankStatistics.h"
#include "GameShared/GameClasses/Sound/Playback/Splicer/CgsSplicerContent.h"
#include "GameShared/GameClasses/Sound/Playback/Splicer/internal/SpliceObjects.h"
#include "rw/audio/core/PlugIn.h"
#include <new>
#include "GameShared/GameClasses/Development/DebugSystem/Core/UI/CgsDebugUI.h"
#include "GameShared/GameClasses/Development/Log/CgsLog.h"
#include "GameShared/GameClasses/Sound/CgsSoundUtils.h"
#include "GameSource/Physics/PhysicsUtilities/ExternallySimulatedBody.h"
#include "GameSource/Sound/Collision/BrnCollisionStateManager.h"
#include "GameSource/Sound/Module/LogicModule/BrnSubmixesEffect.h"
#include "GameSource/Sound/World/BrnEmitterEffect.h"
#include "GameSource/Sound/Vehicles/Environment/BrnEnclosureControl.h"
#include "GameSource/Sound/Vehicles/Wheels/BrnInAirEffect.h"
#include "GameShared/GameClasses/Sound/Logic/Cgs3dEffectControl.h"

extern s32 KI_DEBUG_PRINT_AUDIO_ALLOCATIONS; // RootSoundModule::Update @0x826FB238, X360 dword_82FFB814
extern s32 KI_DEBUG_PRINT_ENGINE_INFO; // DecFIGS BrnPhysicsControl.cpp:78; X360 dword_82FFB854
extern s32 KI_PRINT_CAR_LOADING_STATES; // DecFIGS BrnVehicleState.cpp:4; X360 dword_82FFB830

namespace BrnSound
{
namespace Vehicles
{
extern bool KI_DEBUG_PRINT_CLOSEST_AI_STATES;
extern bool KI_DEBUG_PRINT_CLOSEST_AI_SKIDS;

namespace Engines
{
extern s32 KI_DEBUG_SPEW_AI_ENGINE_INFO;
extern s32 KI_DEBUG_PRINT_RPM_OUTPUT;
extern s32 KI_DEBUG_PRINT_LOOP_OUTPUT;
extern s32 KI_DEBUG_DUMP_LOOP_DATA;
extern s32 KI_DEBUG_PITCH_BELOW_IDLE_DECEL_GINSU;
extern s32 KI_DEBUG_SPEW_BOOST_INFO;
extern f32 KF_DEBUG_ACCEL_GINSU_VOL;
extern f32 KF_DEBUG_DECEL_GINSU_VOL;
extern f32 KF_DEBUG_LOOP_VOL;
extern f32 KF_DEBUG_ENGINE_VOLUME;
extern f32 KF_DEBUG_EXHAUST_VOLUME;
extern f32 KF_HARDCODE_RPM;
extern f32 KF_DEBUG_SHUFFLE_PITCH;
extern bool KB_TURN_ON_RPM_RAMPING;
extern bool KF_PANNING_ON;
extern f32 KF_PANNING_DISTANCE;
extern f32 KF_PANNING_SIZE;
extern f32 KF_PANNING_TWIST;
extern f32 KF_PANNING_CENTRELEVEL;
extern f32 KF_PANNING_MAINLEVEL;
extern f32 KF_PANNING_LFELEVEL;
extern f32 KF_DECEL_GINSU_FADE_OUT;
extern f32 KF_DECEL_GINSU_FADE_IN;
extern f32 KF_DEBUG_IDLE_RPM_INTERPOLATION;
extern f32 KF_CLUTCH_MODEL_RPM_DELTA;
extern f32 KF_CLUTCH_MODEL_RPM_THRESHOLD;
extern f32 KF_MAX_REVERSE_SPEED;
extern f32 KF_MIN_REVERSE_PITCH;
extern f32 KF_MAX_REVERSE_PITCH;
extern f32 KF_REDLINING_RPM;
extern f32 KF_REDLINING_RPM_OFFSET;
extern f32 KF_REDLINING_DUCK_DURATION_HIGH;
extern f32 KF_REDLINING_DUCK_DURATION_LOW;
extern f32 KF_REDLINING_VOLUME_BOOST;
extern f32 KF_REDLINING_VOLUME_DUCK;
extern s32 KI_DEBUG_ROLL_INFO;
extern s32 KI_DEBUG_ROLL_INFO_VERBOSE;
extern s32 KI_DEBUG_CRASH_STREAMS;
}

namespace Wheels
{
extern bool KB_DEBUG_SPEW_RAMP_LAUNCH;
extern f32 gfFORCE_RAMP_LUNCH;
extern CgsSound::Utils::SlopeParams K_RAMP_LAUNCH_PARAMS;
extern u32 KI_WHEEL_SPEW;
extern u32 KI_SKID_PRINT;
extern u32 KI_WHEELSIDE_SPEW;
extern bool KB_SURFACE_TYPE_DRAW;
}

namespace Environment
{
extern bool KB_DEBUG_REVERB_ZONE;
extern bool KB_SPEW_WHOOSH_INFO;
extern f32 KF_WHOOSH_MIN_VELOCITY_THRESHOLD;
extern f32 KF_MIN_TIME_BETWEEN_TRIGGERS;
extern f32 KF_STATIC_PASSBY_VELOCITY_THRESHOLD;
extern bool KB_AMBIENCE_DRAW_REGION;
extern f32 KF_SPEED_STREAM_THRESHOLD_MPH;
extern f32 KF_SPEED_STREAM_CROSSFADE_WIDTH;
}

namespace Deformation
{
extern s32 KI_DEBUG_SPEW_DEFORM_INFO;
}
}

namespace Logic
{
namespace Traffic
{
extern u32 KI_SPEW_TRAFFIC;
extern u32 KI_DRAW_ATTACHED;
extern s32 KI_SPEW_TRAFFIC_MANAGEMENT;
extern s32 KI_SPEW_TRAFFIC_PASS_CULLS;
extern bool KB_HORN_DRAW_CAR_SIZES;
extern bool KB_HORN_DRAW_CAR_HOOTING;
}

namespace Passby
{
extern s32 KI_SPEW_PASSBY_INFO;
extern s32 KI_SPEW_PASSBY_INFO_VERBOSE;
extern s32 KI_SPEW_PASSBY_STATE_INFO;
extern bool KB_DRAW_PASSBY_INFO;
}

extern bool KB_HUD_SPEW_MESSAGES;
extern bool KB_SPEW_PRESENTATION_MESSAGES;
extern bool KB_MUSIC_MUTE;
extern f32 KF_JUMP_HPF_OPEN_FREQUENCY;
extern f32 KF_JUMP_HPF_OPEN_TIME;
extern f32 KF_JUMP_HPF_CLOSE_TIME;
extern f32 KF_MUSIC_HIGH_PASS_Q;
extern f32 KF_MUSIC_LOW_PASS_Q;
extern f32 KF_MUSIC_HIGH_PASS_ORDER;
extern f32 KF_MUSIC_LOW_PASS_ORDER;
}
}

namespace BrnSound
{
namespace Debug
{
CgsDev::Log::LogCombined DebugComponent::mLogCombined; // ARTIST 0x82F2F9BC

// ARTIST/DecFIGS menu globals; the max-RPM value cell is read-only in the X360
// exports (image value 0.0; no writer), so do not derive a live maximum here.
bool gbShowStatistics = false;             // byte_82FFB93D
bool gbResetStatistics = false;            // byte_82FFB93E
bool gbStatisticsDrawDacGraph = false;      // byte_82FFB93F
s32 KI_SPEW_SPLICE_USAGES = 0;             // dword_82FFB950

// ARTIST 0x827E0720: only base/member construction; Construct seeds state.
DebugComponent::DebugComponent()
{
}

// ARTIST 0x826A1230..0x826A12BC (export hole; tools/re/ppcdis.py).
void DebugComponent::Construct(CgsSound::Playback::Module::Module* lpPlaybackModule,
                               BrnSound::Module::SoundLogicModule* lpLogicModule)
{
    // The original's call at 0x826A1248 is the shared blr body 0x8284CB38;
    // it adds no stores (the current host base Construct resets the list link).
    mStatistics.Construct(lpPlaybackModule, lpLogicModule);
    mpPlaybackModule = lpPlaybackModule;
    mpLogicModule = lpLogicModule;
    meOutput = E_AUDIO;
    mbSpliceAuditionAutoCycle = false;
    mbSpliceAudition = false;
    miSpliceBankIndex = 0;
    miSpliceIndexTens = 0;
    miSpliceIndexUnits = 0;
    mpSpliceAuditionVoice = 0;
}

// ARTIST 0x826893A0: r3=value pointer, r4=user data; load enum then tail call.
void ConsoleChangeCallback(void* lpValue, void* lpUserData)
{
    static_cast<DebugComponent*>(lpUserData)->InstallConsole(
        static_cast<DebugComponent::EConsole>(*static_cast<s32*>(lpValue)));
}

// ARTIST 0x826893B0. Its enum argument is unused in this build; the live
// registered member is read at 0x82689470, including when invoked by callback.
void DebugComponent::InstallConsole(EConsole)
{
    static bool sbConsoleRegistered = false; // byte_8300C6A4, process lifetime
    if (!sbConsoleRegistered)
    {
        RegisterVariable(&meOutput, "Console");
        SetRange(&meOutput, 0, 2);
        maConsoleNames[0].miValue = E_AUDIO; // table 0x82F2FCF0
        maConsoleNames[0].mpcName = "Audio Only";
        maConsoleNames[1].miValue = E_GAME; // 0x82F2FCF4
        maConsoleNames[1].mpcName = "Game Only";
        maConsoleNames[2].miValue = E_AUDIO_AND_GAME; // 0x82F2FCF8
        maConsoleNames[2].mpcName = "Audio AND Game";
        maConsoleNames[3].miValue = E_MAX_CONSOLES; // 0x82F2FCFC
        maConsoleNames[3].mpcName = 0;
        SetOptions(&meOutput, maConsoleNames);
        SetChangeCallback(&meOutput, &ConsoleChangeCallback, this);
        sbConsoleRegistered = true;
    }

    CgsDev::Log::gpDebugPrint = meOutput == E_GAME || meOutput == E_AUDIO_AND_GAME
        ? static_cast<CgsDev::StrStreamBase*>(&mLogTty)
        : static_cast<CgsDev::StrStreamBase*>(&mLogNull);
    mLogCombined.RemoveAllStreams();
    if (meOutput != E_GAME)
    {
        mLogCombined.AddStream(&mLogTty);
        mLogCombined.AddStream(&GetUI().GetConsole().mLog);
    }
}

// ARTIST 0x826D5CB0 / DecFIGS 0x8359A4: register the complete Sound menu in
// console call order. The bool-tag sites that point at DecFIGS s32/u32 storage
// intentionally alias the same cell through its native-endian first byte; do not
// replace those with separate bool backing variables.
void DebugComponent::OnActivate()
{
    CgsDev::DebugComponent::OnActivate(); // ICF-folded blr at 0x8284CB38
    CgsSound::Logic::Cgs3dEffectControl::spDebugRenderingFunction =
        &DebugComponent::Render3dControl; // X360 dword_82FFBC10
    InstallConsole(E_AUDIO); // original E_AUDIO argument; InstallConsole reads meOutput

    // Engines (0x826D5D08..0x826D612C).
    RegisterVariable(reinterpret_cast<bool*>(&BrnSound::Vehicles::Engines::KI_MONITOR_MAX_RPM),
                     "Engines", "Max Rpm Display");
    RegisterVariable(reinterpret_cast<bool*>(&KI_DEBUG_PRINT_ENGINE_INFO),
                     "Engines", "Engine Info");
    RegisterVariable(&BrnSound::Vehicles::KI_DEBUG_PRINT_CLOSEST_AI_STATES,
                     "Engines", "Closest AI");
    RegisterVariable(&BrnSound::Vehicles::Engines::KI_DEBUG_SPEW_AI_ENGINE_INFO,
                     "Engines", "Spew AI info");
    SetRange(&BrnSound::Vehicles::Engines::KI_DEBUG_SPEW_AI_ENGINE_INFO, -1, 7);
    SetStep(&BrnSound::Vehicles::Engines::KI_DEBUG_SPEW_AI_ENGINE_INFO, 1);
    RegisterVariable(reinterpret_cast<bool*>(&BrnSound::Vehicles::Engines::KI_DEBUG_PRINT_RPM_OUTPUT),
                     "Engines", "Rpm Info");
    RegisterVariable(reinterpret_cast<bool*>(&BrnSound::Vehicles::Engines::KI_DEBUG_PRINT_LOOP_OUTPUT),
                     "Engines", "Loop Info");
    RegisterVariable(reinterpret_cast<bool*>(&BrnSound::Vehicles::Engines::KI_DEBUG_DUMP_LOOP_DATA),
                     "Engines", "Loop Data Info");
    RegisterVariable(reinterpret_cast<bool*>(&BrnSound::Vehicles::Engines::KI_DEBUG_PITCH_BELOW_IDLE_DECEL_GINSU),
                     "Engines", "Pitch Below Decel Min");
    RegisterVariable(reinterpret_cast<bool*>(&BrnSound::Vehicles::Engines::KI_DEBUG_SPEW_BOOST_INFO),
                     "Engines", "Boost Info");
    RegisterVariable(reinterpret_cast<bool*>(&BrnPhysics::KI_HOLD_CAR_IN_PLACE),
                     "Engines", "Hold Car in Place");
    RegisterVariable(reinterpret_cast<bool*>(&BrnPhysics::KI_HOLD_CAR_SPINING_IN_AIR),
                     "Engines", "Hold Car SPINNING in AIR");
    RegisterVariable(&BrnPhysics::KF_SPINNING_SPEED_X, "Engines", "Spin Speed X");
    RegisterVariable(&BrnPhysics::KF_SPINNING_SPEED_Y, "Engines", "Spin Speed Y");
    RegisterVariable(&BrnPhysics::KF_SPINNING_SPEED_Z, "Engines", "Spin Speed Z");
    SetStep(&BrnPhysics::KF_SPINNING_SPEED_X, 0.1f);
    SetStep(&BrnPhysics::KF_SPINNING_SPEED_Y, 0.1f);
    SetStep(&BrnPhysics::KF_SPINNING_SPEED_Z, 0.1f);

    RegisterVariable(&BrnSound::Vehicles::Engines::KF_DEBUG_ACCEL_GINSU_VOL,
                     "Engines", "Hard Code Ginsu Accel Vol");
    SetRange(&BrnSound::Vehicles::Engines::KF_DEBUG_ACCEL_GINSU_VOL, 0.0f, 1.0f);
    SetStep(&BrnSound::Vehicles::Engines::KF_DEBUG_ACCEL_GINSU_VOL, 0.05f);
    RegisterVariable(&BrnSound::Vehicles::Engines::KF_DEBUG_DECEL_GINSU_VOL,
                     "Engines", "Hard Code Ginsu Decel Vol");
    SetRange(&BrnSound::Vehicles::Engines::KF_DEBUG_DECEL_GINSU_VOL, 0.0f, 1.0f);
    SetStep(&BrnSound::Vehicles::Engines::KF_DEBUG_DECEL_GINSU_VOL, 0.05f);
    RegisterVariable(&BrnSound::Vehicles::Engines::KF_DEBUG_LOOP_VOL,
                     "Engines", "Hard Code Loop Model Vol");
    SetRange(&BrnSound::Vehicles::Engines::KF_DEBUG_LOOP_VOL, 0.0f, 1.0f);
    SetStep(&BrnSound::Vehicles::Engines::KF_DEBUG_LOOP_VOL, 0.05f);
    RegisterVariable(&BrnSound::Vehicles::Engines::KF_DEBUG_ENGINE_VOLUME,
                     "Engines", "Hard code ENGINE Vol");
    SetRange(&BrnSound::Vehicles::Engines::KF_DEBUG_ENGINE_VOLUME, 0.0f, 1.0f);
    SetStep(&BrnSound::Vehicles::Engines::KF_DEBUG_ENGINE_VOLUME, 0.05f);
    RegisterVariable(&BrnSound::Vehicles::Engines::KF_DEBUG_EXHAUST_VOLUME,
                     "Engines", "Hard Code EXHAUST Vol");
    SetRange(&BrnSound::Vehicles::Engines::KF_DEBUG_EXHAUST_VOLUME, 0.0f, 1.0f);
    SetStep(&BrnSound::Vehicles::Engines::KF_DEBUG_EXHAUST_VOLUME, 0.05f);
    RegisterVariable(&BrnSound::Vehicles::Engines::KF_HARDCODE_RPM,
                     "Engines", "Hard Code RPM");
    SetRange(&BrnSound::Vehicles::Engines::KF_HARDCODE_RPM, 0.0f, 10000.0f);
    SetStep(&BrnSound::Vehicles::Engines::KF_HARDCODE_RPM, 500.0f);
    RegisterVariable(&BrnSound::Vehicles::Engines::KF_DEBUG_SHUFFLE_PITCH,
                     "Engines", "Shuffle Pitch");
    SetRange(&BrnSound::Vehicles::Engines::KF_DEBUG_SHUFFLE_PITCH, 0.0f, 100.0f);
    SetStep(&BrnSound::Vehicles::Engines::KF_DEBUG_SHUFFLE_PITCH, 1.0f);
    RegisterVariable(&BrnSound::Vehicles::Engines::KB_TURN_ON_RPM_RAMPING,
                     "Engines", "Rpm Ramping");
    RegisterVariable(reinterpret_cast<bool*>(&KI_PRINT_CAR_LOADING_STATES),
                     "Engines", "Engine Loading");

    // Panning (0x826D6154..0x826D62D8).
    RegisterVariable(&BrnSound::Vehicles::Engines::KF_PANNING_ON,
                     "Engines Pan", "Panning Distance");
    RegisterVariable(&BrnSound::Vehicles::Engines::KF_PANNING_DISTANCE,
                     "Engines Pan", "Panning Distance");
    SetRange(&BrnSound::Vehicles::Engines::KF_PANNING_DISTANCE, 0.0f, 3.0f);
    SetStep(&BrnSound::Vehicles::Engines::KF_PANNING_DISTANCE, 0.05f);
    RegisterVariable(&BrnSound::Vehicles::Engines::KF_PANNING_SIZE,
                     "Engines Pan", "Panning Size");
    SetRange(&BrnSound::Vehicles::Engines::KF_PANNING_SIZE, 0.0f, 3.0f);
    SetStep(&BrnSound::Vehicles::Engines::KF_PANNING_SIZE, 0.05f);
    RegisterVariable(&BrnSound::Vehicles::Engines::KF_PANNING_TWIST,
                     "Engines Pan", "Panning Twist");
    SetRange(&BrnSound::Vehicles::Engines::KF_PANNING_TWIST, 0.0f, 360.0f);
    SetStep(&BrnSound::Vehicles::Engines::KF_PANNING_TWIST, 5.0f);
    RegisterVariable(&BrnSound::Vehicles::Engines::KF_PANNING_CENTRELEVEL,
                     "Engines Pan", "Panning CentreLvl");
    SetRange(&BrnSound::Vehicles::Engines::KF_PANNING_CENTRELEVEL, 0.0f, 1.0f);
    SetStep(&BrnSound::Vehicles::Engines::KF_PANNING_CENTRELEVEL, 0.05f);
    RegisterVariable(&BrnSound::Vehicles::Engines::KF_PANNING_MAINLEVEL,
                     "Engines Pan", "Panning MainLvl");
    SetRange(&BrnSound::Vehicles::Engines::KF_PANNING_MAINLEVEL, 0.0f, 1.0f);
    SetStep(&BrnSound::Vehicles::Engines::KF_PANNING_MAINLEVEL, 0.05f);
    RegisterVariable(&BrnSound::Vehicles::Engines::KF_PANNING_LFELEVEL,
                     "Engines Pan", "Panning LFElvl");
    SetRange(&BrnSound::Vehicles::Engines::KF_PANNING_LFELEVEL, 0.0f, 1.0f);
    SetStep(&BrnSound::Vehicles::Engines::KF_PANNING_LFELEVEL, 0.05f);

    RegisterVariable(&BrnSound::Vehicles::Engines::KF_DECEL_GINSU_FADE_OUT,
                     "Engines", "Decel Ginsu Fade Out");
    SetRange(&BrnSound::Vehicles::Engines::KF_DECEL_GINSU_FADE_OUT, 0.0f, 1.0f);
    SetStep(&BrnSound::Vehicles::Engines::KF_DECEL_GINSU_FADE_OUT, 0.01f);
    RegisterVariable(&BrnSound::Vehicles::Engines::KF_DECEL_GINSU_FADE_IN,
                     "Engines", "Decel Ginsu Fade In");
    SetRange(&BrnSound::Vehicles::Engines::KF_DECEL_GINSU_FADE_IN, 0.0f, 1.0f);
    SetStep(&BrnSound::Vehicles::Engines::KF_DECEL_GINSU_FADE_IN, 0.01f);
    RegisterVariable(&BrnSound::Vehicles::Engines::KF_DEBUG_IDLE_RPM_INTERPOLATION,
                     "Engines", "Idle RPM Interp");
    SetRange(&BrnSound::Vehicles::Engines::KF_DEBUG_IDLE_RPM_INTERPOLATION,
             0.0f, 10000.0f);
    SetStep(&BrnSound::Vehicles::Engines::KF_DEBUG_IDLE_RPM_INTERPOLATION, 100.0f);
    RegisterVariable(&BrnSound::Vehicles::Engines::KF_CLUTCH_MODEL_RPM_DELTA,
                     "Engines", "Clutch: RpmDelta");
    SetRange(&BrnSound::Vehicles::Engines::KF_CLUTCH_MODEL_RPM_DELTA, 0.0f, 1000.0f);
    SetStep(&BrnSound::Vehicles::Engines::KF_CLUTCH_MODEL_RPM_DELTA, 10.0f);
    RegisterVariable(&BrnSound::Vehicles::Engines::KF_CLUTCH_MODEL_RPM_THRESHOLD,
                     "Engines", "Clutch: Thresh");
    SetRange(&BrnSound::Vehicles::Engines::KF_CLUTCH_MODEL_RPM_THRESHOLD, 0.0f, 5000.0f);
    SetStep(&BrnSound::Vehicles::Engines::KF_CLUTCH_MODEL_RPM_THRESHOLD, 100.0f);
    RegisterVariable(&BrnSound::Vehicles::Engines::KF_CLUTCH_MODEL_RPM_THRESHOLD,
                     "Engines", "Clutch: Thresh");
    SetRange(&BrnSound::Vehicles::Engines::KF_CLUTCH_MODEL_RPM_THRESHOLD, 0.0f, 5000.0f);
    SetStep(&BrnSound::Vehicles::Engines::KF_CLUTCH_MODEL_RPM_THRESHOLD, 100.0f);
    RegisterVariable(&BrnSound::Vehicles::Engines::KF_MAX_REVERSE_SPEED,
                     "Engines", "Reverse Max Speed");
    SetRange(&BrnSound::Vehicles::Engines::KF_MAX_REVERSE_SPEED, 0.0f, 5000.0f);
    SetStep(&BrnSound::Vehicles::Engines::KF_MAX_REVERSE_SPEED, 1.0f);
    RegisterVariable(&BrnSound::Vehicles::Engines::KF_MIN_REVERSE_PITCH,
                     "Engines", "Reverse Pitch at 0");
    SetRange(&BrnSound::Vehicles::Engines::KF_MIN_REVERSE_PITCH, 0.0f, 4.0f);
    SetStep(&BrnSound::Vehicles::Engines::KF_MIN_REVERSE_PITCH, 0.1f);
    RegisterVariable(&BrnSound::Vehicles::Engines::KF_MAX_REVERSE_PITCH,
                     "Engines", "Reverse Pitch at max speed");
    SetRange(&BrnSound::Vehicles::Engines::KF_MAX_REVERSE_PITCH, 0.0f, 4.0f);
    SetStep(&BrnSound::Vehicles::Engines::KF_MAX_REVERSE_PITCH, 0.1f);

    // RedLining (0x826D65A8..0x826D66FC).
    RegisterVariable(&BrnSound::Vehicles::Engines::KF_REDLINING_RPM,
                     "RedLining", "RPM");
    SetRange(&BrnSound::Vehicles::Engines::KF_REDLINING_RPM, 0.0f, 10000.0f);
    SetStep(&BrnSound::Vehicles::Engines::KF_REDLINING_RPM, 10.0f);
    RegisterVariable(&BrnSound::Vehicles::Engines::KF_REDLINING_RPM_OFFSET,
                     "RedLining", "Rpm Offset");
    SetRange(&BrnSound::Vehicles::Engines::KF_REDLINING_RPM_OFFSET, 0.0f, 10000.0f);
    SetStep(&BrnSound::Vehicles::Engines::KF_REDLINING_RPM_OFFSET, 10.0f);
    RegisterVariable(&BrnSound::Vehicles::Engines::KF_REDLINING_DUCK_DURATION_HIGH,
                     "RedLining", "Duck Duration HI");
    SetRange(&BrnSound::Vehicles::Engines::KF_REDLINING_DUCK_DURATION_HIGH, 0.0f, 1.0f);
    SetStep(&BrnSound::Vehicles::Engines::KF_REDLINING_DUCK_DURATION_HIGH, 0.01f);
    RegisterVariable(&BrnSound::Vehicles::Engines::KF_REDLINING_DUCK_DURATION_LOW,
                     "RedLining", "Duck Duration LOW");
    SetRange(&BrnSound::Vehicles::Engines::KF_REDLINING_DUCK_DURATION_LOW, 0.0f, 1.0f);
    SetStep(&BrnSound::Vehicles::Engines::KF_REDLINING_DUCK_DURATION_LOW, 0.01f);
    RegisterVariable(&BrnSound::Vehicles::Engines::KF_REDLINING_VOLUME_BOOST,
                     "RedLining", "Volume Boost");
    SetRange(&BrnSound::Vehicles::Engines::KF_REDLINING_VOLUME_BOOST, 0.0f, 10.0f);
    SetStep(&BrnSound::Vehicles::Engines::KF_REDLINING_VOLUME_BOOST, 0.01f);
    RegisterVariable(&BrnSound::Vehicles::Engines::KF_REDLINING_VOLUME_DUCK,
                     "RedLining", "Volume Duck");
    SetRange(&BrnSound::Vehicles::Engines::KF_REDLINING_VOLUME_DUCK, 0.0f, 10.0f);
    SetStep(&BrnSound::Vehicles::Engines::KF_REDLINING_VOLUME_DUCK, 0.01f);

    // InAir and Wheels.
    RegisterVariable(&BrnSound::Vehicles::Wheels::KB_DEBUG_SPEW_RAMP_LAUNCH,
                     "InAir", "Debug Ramp Launch");
    RegisterVariable(&BrnSound::Vehicles::Wheels::gfFORCE_RAMP_LUNCH,
                     "InAir", "Force Launch from Ramp");
    SetRange(&BrnSound::Vehicles::Wheels::gfFORCE_RAMP_LUNCH, 0.0f, 10.0f);
    SetStep(&BrnSound::Vehicles::Wheels::gfFORCE_RAMP_LUNCH, 0.1f);
    RegisterVariable(&BrnSound::Vehicles::Wheels::K_RAMP_LAUNCH_PARAMS.mfMinInput,
                     "InAir", "Min Speed");
    SetRange(&BrnSound::Vehicles::Wheels::K_RAMP_LAUNCH_PARAMS.mfMinInput, 0.0f, 180.0f);
    SetStep(&BrnSound::Vehicles::Wheels::K_RAMP_LAUNCH_PARAMS.mfMinInput, 5.0f);
    RegisterVariable(&BrnSound::Vehicles::Wheels::K_RAMP_LAUNCH_PARAMS.mfMaxInput,
                     "InAir", "Max Speed");
    SetRange(&BrnSound::Vehicles::Wheels::K_RAMP_LAUNCH_PARAMS.mfMaxInput, 0.0f, 180.0f);
    SetStep(&BrnSound::Vehicles::Wheels::K_RAMP_LAUNCH_PARAMS.mfMaxInput, 5.0f);
    RegisterVariable(&BrnSound::Vehicles::Wheels::K_RAMP_LAUNCH_PARAMS.mfMinOutput,
                     "InAir", "Slowest Ramp(ms)");
    SetRange(&BrnSound::Vehicles::Wheels::K_RAMP_LAUNCH_PARAMS.mfMinOutput, 0.0f, 2000.0f);
    SetStep(&BrnSound::Vehicles::Wheels::K_RAMP_LAUNCH_PARAMS.mfMinOutput, 25.0f);
    RegisterVariable(&BrnSound::Vehicles::Wheels::K_RAMP_LAUNCH_PARAMS.mfMaxOutput,
                     "InAir", "Fastest Ramp(ms)");
    SetRange(&BrnSound::Vehicles::Wheels::K_RAMP_LAUNCH_PARAMS.mfMaxOutput, 0.0f, 2000.0f);
    SetStep(&BrnSound::Vehicles::Wheels::K_RAMP_LAUNCH_PARAMS.mfMaxOutput, 25.0f);
    RegisterVariable(reinterpret_cast<bool*>(&BrnSound::Vehicles::Wheels::KI_WHEEL_SPEW),
                     "Wheels", "Wheel Info");
    RegisterVariable(reinterpret_cast<bool*>(&BrnSound::Vehicles::Wheels::KI_SKID_PRINT),
                     "Wheels", "Skid Info");
    RegisterVariable(reinterpret_cast<bool*>(&BrnSound::Vehicles::Wheels::KI_WHEELSIDE_SPEW),
                     "Wheels", "Wheel Side Info");
    RegisterVariable(&BrnSound::Vehicles::Wheels::KB_SURFACE_TYPE_DRAW,
                     "Wheels", "Surface Types");
    RegisterVariable(&BrnSound::Vehicles::KI_DEBUG_PRINT_CLOSEST_AI_SKIDS,
                     "Wheels", "Ai Skids info");

    // Collision (0x826D694C..0x826D6D00).
    RegisterVariable(reinterpret_cast<bool*>(&BrnSound::Logic::Collision::KI_DEBUG_PRINT_COLLISION_BINS),
                     "Collision", "Print Collision Bins");
    RegisterVariable(reinterpret_cast<bool*>(const_cast<s32*>(&BrnSound::Logic::Collision::KI_SPEW_COLLISION_INFO)),
                     "Collision", "Collision Info");
    RegisterVariable(&BrnSound::Logic::Collision::KI_SPEW_COLLISION_BIN_LOGIC_VERBOSE,
                     "Collision", "Collision Bin Logic Verbose");
    SetRange(&BrnSound::Logic::Collision::KI_SPEW_COLLISION_BIN_LOGIC_VERBOSE, 0, 3);
    SetStep(&BrnSound::Logic::Collision::KI_SPEW_COLLISION_BIN_LOGIC_VERBOSE, 1);
    RegisterVariable(&BrnSound::Logic::Collision::KI_DEBUG_CULL_AGAINST_PLAYING,
                     "Collision", "CollisionCulling");
    SetRange(&BrnSound::Logic::Collision::KI_DEBUG_CULL_AGAINST_PLAYING, 0, 4);
    SetStep(&BrnSound::Logic::Collision::KI_DEBUG_CULL_AGAINST_PLAYING, 1);
    RegisterVariable(reinterpret_cast<bool*>(&BrnSound::Logic::Collision::KI_DEBUG_DETACHING),
                     "Collision", "Print Detaching");
    RegisterVariable(reinterpret_cast<bool*>(&BrnSound::Logic::Collision::KI_DEBUG_DUCKING),
                     "Collision", "print Ducking");
    RegisterVariable(reinterpret_cast<bool*>(&BrnSound::Logic::Collision::KI_DEBUG_SPEW_ALL_INPUT_COLLISIONS),
                     "Collision", "Print All Inputs");
    RegisterVariable(reinterpret_cast<bool*>(&BrnSound::Logic::Collision::KI_DEBUG_FILTER_NO_COLLISION),
                     "Collision", "No Normal");
    RegisterVariable(reinterpret_cast<bool*>(&BrnSound::Logic::Collision::KI_DEBUG_FILTER_NO_PROPS),
                     "Collision", "No Props");
    RegisterVariable(reinterpret_cast<bool*>(&BrnSound::Logic::Collision::KI_DEBUG_FILTER_NO_BODY_PARTS),
                     "Collision", "No bodyparts");
    RegisterVariable(reinterpret_cast<bool*>(&BrnSound::Logic::Collision::KI_DEBUG_FILTER_NO_GLASS),
                     "Collision", "No Glass");
    RegisterVariable(&BrnSound::Logic::Collision::KF_HINGING_VELOCITY_MIN_TOLERANCE,
                     "Collision", "Hinging Tol");
    SetStep(&BrnSound::Logic::Collision::KF_HINGING_VELOCITY_MIN_TOLERANCE, 0.01f);
    RegisterVariable(&BrnSound::Logic::Collision::KF_HINGING_VELOCITY_DELTA_TOLERANCE,
                     "Collision", "Hinging Delta");
    SetStep(&BrnSound::Logic::Collision::KF_HINGING_VELOCITY_DELTA_TOLERANCE, 0.01f);
    RegisterVariable(&BrnSound::Logic::Collision::KI_DEBUG_SAMPLE_ID_TO_PLAY_LARGE,
                     "Collision", "Force ID LARGE");
    SetRange(&BrnSound::Logic::Collision::KI_DEBUG_SAMPLE_ID_TO_PLAY_LARGE, 0, 500);
    SetStep(&BrnSound::Logic::Collision::KI_DEBUG_SAMPLE_ID_TO_PLAY_LARGE, 10);
    RegisterVariable(&BrnSound::Logic::Collision::KI_DEBUG_SAMPLE_ID_TO_PLAY_SMALL,
                     "Collision", "Force ID SMALL");
    SetRange(&BrnSound::Logic::Collision::KI_DEBUG_SAMPLE_ID_TO_PLAY_SMALL, -1, 9);
    SetStep(&BrnSound::Logic::Collision::KI_DEBUG_SAMPLE_ID_TO_PLAY_SMALL, 1);
    RegisterVariable(&BrnSound::Logic::Collision::KI_DEBUG_PIPELINE_TOPLAY,
                     "Collision", "Pipeline");
    SetRange(&BrnSound::Logic::Collision::KI_DEBUG_PIPELINE_TOPLAY, 0, 1);
    SetStep(&BrnSound::Logic::Collision::KI_DEBUG_PIPELINE_TOPLAY, 1);
    RegisterVariable(&BrnSound::Logic::Collision::KI_DEBUG_SPAWN_LOTS_OF_SAMPLES,
                     "Collision", "Play Tons");
    SetRange(&BrnSound::Logic::Collision::KI_DEBUG_SPAWN_LOTS_OF_SAMPLES, 0, 1);
    SetStep(&BrnSound::Logic::Collision::KI_DEBUG_SPAWN_LOTS_OF_SAMPLES, 1);
    RegisterVariable(&BrnSound::Logic::Collision::KF_FASTEST_COLLISION,
                     "Collision", "Fastest thing");
    SetStep(&BrnSound::Logic::Collision::KF_FASTEST_COLLISION, 50.0f);
    RegisterVariable(&BrnSound::Logic::Collision::KF_BIGGEST_THING_MASS,
                     "Collision", "Heaviest thing");
    SetStep(&BrnSound::Logic::Collision::KF_BIGGEST_THING_MASS, 50.0f);
    RegisterVariable(reinterpret_cast<bool*>(&BrnSound::Logic::Collision::KI_SPEW_SCRAPE_INFO),
                     "Collision", "Scrape Info");
    RegisterVariable(reinterpret_cast<bool*>(&BrnSound::Logic::Collision::KI_DRAW_SCRAPES),
                     "Collision", "Scrapes Draw");
    RegisterVariable(&BrnSound::Logic::Collision::KI_DEBUG_FILTER_MATERIAL,
                     "Collision", "Material Filter");
    BuildMaterialNames();
    RegisterVariable(&BrnSound::Logic::Collision::KI_DEBUG_FILTER_ACTION,
                     "Collision", "Action Filter");
    BuildActionNames();
    RegisterVariable(reinterpret_cast<bool*>(&BrnSound::Logic::Collision::KI_DEBUG_RENDER_COLLISIONS),
                     "Collision", "Render Collisions");
    RegisterVariable(reinterpret_cast<bool*>(&BrnSound::Logic::Collision::KI_DEBUG_SPEW_ORIENTATION),
                     "Collision", "Orientation Spew");
    RegisterVariable(reinterpret_cast<bool*>(&BrnSound::Logic::Collision::KI_DEBUG_DRAW_ORIENATION_BOX),
                     "Collision", "Draw Orientation Box");
    RegisterVariable(reinterpret_cast<bool*>(&BrnSound::Logic::Collision::KI_DEBUG_SPEW_PROP_LOOKUP_FAULTS),
                     "Collision", "Spew Prop Resource IDs");

    // Traffic, passby, and environmental diagnostics.
    RegisterVariable(reinterpret_cast<bool*>(&BrnSound::Logic::Traffic::KI_SPEW_TRAFFIC),
                     "Traffic", "Engine Info");
    RegisterVariable(reinterpret_cast<bool*>(&BrnSound::Logic::Traffic::KI_DRAW_ATTACHED),
                     "Traffic", "Draw Attached");
    RegisterVariable(reinterpret_cast<bool*>(&BrnSound::Logic::Traffic::KI_SPEW_TRAFFIC_MANAGEMENT),
                     "Traffic", "Management Info");
    RegisterVariable(reinterpret_cast<bool*>(&BrnSound::Logic::Passby::KI_SPEW_PASSBY_INFO),
                     "Passby", "Effect Info");
    RegisterVariable(reinterpret_cast<bool*>(&BrnSound::Logic::Passby::KI_SPEW_PASSBY_INFO_VERBOSE),
                     "Passby", "Verbose Info");
    RegisterVariable(reinterpret_cast<bool*>(&BrnSound::Logic::Traffic::KI_SPEW_TRAFFIC_PASS_CULLS),
                     "Passby", "Traffic Cull Info");
    RegisterVariable(reinterpret_cast<bool*>(&BrnSound::Logic::Passby::KI_SPEW_PASSBY_STATE_INFO),
                     "Passby", "State Info");
    RegisterVariable(&BrnSound::Logic::Passby::KB_DRAW_PASSBY_INFO,
                     "Passby", "Draw Spheres");
    RegisterVariable(&BrnSound::Logic::Traffic::KB_HORN_DRAW_CAR_SIZES,
                     "Horn", "Draw Sizes");
    RegisterVariable(&BrnSound::Logic::Traffic::KB_HORN_DRAW_CAR_HOOTING,
                     "Horn", "Draw Hooting");
    RegisterVariable(reinterpret_cast<bool*>(&BrnSound::Vehicles::Engines::KI_DEBUG_ROLL_INFO),
                     "Passby", "Roll Info");
    RegisterVariable(reinterpret_cast<bool*>(&BrnSound::Vehicles::Engines::KI_DEBUG_ROLL_INFO_VERBOSE),
                     "Passby", "Roll Info Verbose");
    RegisterVariable(&BrnSound::Vehicles::Environment::KB_SPEW_WHOOSH_INFO,
                     "Passbys&Reverb", "Spew Information");
    RegisterVariable(&BrnSound::Vehicles::Environment::KF_WHOOSH_MIN_VELOCITY_THRESHOLD,
                     "Passbys&Reverb", "Min Velocity Walls");
    SetRange(&BrnSound::Vehicles::Environment::KF_WHOOSH_MIN_VELOCITY_THRESHOLD,
             0.0f, 500.0f);
    RegisterVariable(&BrnSound::Vehicles::Environment::KF_MIN_TIME_BETWEEN_TRIGGERS,
                     "Passbys&Reverb", "Min Time Between");
    RegisterVariable(&BrnSound::Vehicles::Environment::KB_SHOW_STATIC_ENVIRONMENT,
                     "Passbys&Reverb", "Show Static");
    RegisterVariable(&BrnSound::Vehicles::Environment::KF_STATIC_PASSBY_VELOCITY_THRESHOLD,
                     "Passbys&Reverb", "Min Velocity Spheres");
    RegisterVariable(&BrnSound::Vehicles::Environment::KB_DEBUG_REVERB_ZONE,
                     "Passbys&Reverb", "Debug Reverb");
    RegisterVariable(reinterpret_cast<bool*>(&BrnSound::Vehicles::Engines::KI_DEBUG_CRASH_STREAMS),
                     "Collision", "Crash Stream");
    RegisterVariable(&BrnSound::Vehicles::Environment::KB_AMBIENCE_DRAW_REGION,
                     "Ambience", "Draw Information");
    RegisterVariable(&BrnSound::Logic::KB_HUD_SPEW_MESSAGES,
                     "Presentation", "Spew Messages");
    RegisterVariable(&BrnSound::Logic::KB_SPEW_PRESENTATION_MESSAGES,
                     "Presentation", "Spew GUI Messages");

    // Streams and music.
    RegisterVariable(&BrnSound::Vehicles::Environment::KF_SPEED_STREAM_THRESHOLD_MPH,
                     "Streams", "Speed Threshold");
    RegisterVariable(&BrnSound::Vehicles::Environment::KF_SPEED_STREAM_CROSSFADE_WIDTH,
                     "Streams", "Speed Crossfade Width");
    SetRange(&BrnSound::Vehicles::Environment::KF_SPEED_STREAM_CROSSFADE_WIDTH,
             1.0f, 50.0f);
    RegisterVariable(&BrnSound::Logic::KB_MUSIC_MUTE,
                     "Streams", "Mute Music");
    RegisterVariable(&BrnSound::Logic::KF_JUMP_HPF_OPEN_FREQUENCY,
                     "Streams", "Music HPF open freq");
    SetStep(&BrnSound::Logic::KF_JUMP_HPF_OPEN_FREQUENCY, 100.0f);
    RegisterVariable(&BrnSound::Logic::KF_JUMP_HPF_OPEN_TIME,
                     "Streams", "Music HPF open time");
    SetStep(&BrnSound::Logic::KF_JUMP_HPF_OPEN_TIME, 0.1f);
    RegisterVariable(&BrnSound::Logic::KF_JUMP_HPF_CLOSE_TIME,
                     "Streams", "Music HPF close time");
    SetStep(&BrnSound::Logic::KF_JUMP_HPF_CLOSE_TIME, 0.1f);
    RegisterVariable(&BrnSound::Logic::KF_MUSIC_HIGH_PASS_Q,
                     "Streams", "Music HPF Q");
    SetStep(&BrnSound::Logic::KF_MUSIC_HIGH_PASS_Q, 0.01f);
    RegisterVariable(&BrnSound::Logic::KF_MUSIC_LOW_PASS_Q,
                     "Streams", "Music LPF Q");
    SetStep(&BrnSound::Logic::KF_MUSIC_LOW_PASS_Q, 0.01f);
    RegisterVariable(&BrnSound::Logic::KF_MUSIC_HIGH_PASS_ORDER,
                     "Streams", "Music HPF Order");
    RegisterVariable(&BrnSound::Logic::KF_MUSIC_LOW_PASS_ORDER,
                     "Streams", "Music LPF Order");

    // Remaining single-entry groups.
    RegisterVariable(&BrnSound::Logic::World::KB_DEBUG_WORLD_EMITTERS,
                     "Emitters", "Debug");
    RegisterVariable(reinterpret_cast<bool*>(&BrnSound::Vehicles::Deformation::KI_DEBUG_SPEW_DEFORM_INFO),
                     "Deformation", "Spew Info");

    // Global (0x826D713C..0x826D7378).
    RegisterVariable(&BrnSound::Logic::KF_GLOBAL_GAIN, "Global", "Master Gain");
    SetRange(&BrnSound::Logic::KF_GLOBAL_GAIN, 0.0f, 50.0f);
    SetStep(&BrnSound::Logic::KF_GLOBAL_GAIN, 0.25f);
    RegisterVariable(&BrnSound::Logic::KF_GLOBAL_LOW_SHELF_GAIN, "Global", "LF Shelf Gain");
    SetRange(&BrnSound::Logic::KF_GLOBAL_LOW_SHELF_GAIN, 0.0f, 50.0f);
    SetStep(&BrnSound::Logic::KF_GLOBAL_LOW_SHELF_GAIN, 0.1f);
    RegisterVariable(&BrnSound::Logic::KF_GLOBAL_LOW_SHELF_FREQ, "Global", "LF Shelf Freq");
    SetRange(&BrnSound::Logic::KF_GLOBAL_LOW_SHELF_FREQ, 0.0f, 120.0f);
    SetStep(&BrnSound::Logic::KF_GLOBAL_LOW_SHELF_FREQ, 2.0f);
    RegisterVariable(&BrnSound::Logic::KF_GLOBAL_LIMITER_THRESHOLD, "Global", "Limiter Thresh");
    SetRange(&BrnSound::Logic::KF_GLOBAL_LIMITER_THRESHOLD, -100.0f, 20.0f);
    SetStep(&BrnSound::Logic::KF_GLOBAL_LIMITER_THRESHOLD, 1.0f);
    RegisterVariable(&BrnSound::Logic::KF_GLOBAL_LIMITER_RELEASE, "Global", "Limiter Release");
    SetRange(&BrnSound::Logic::KF_GLOBAL_LIMITER_RELEASE, 0.0f, 1000.0f);
    SetStep(&BrnSound::Logic::KF_GLOBAL_LIMITER_RELEASE, 1.0f);
    RegisterVariable(&BrnSound::Logic::KF_GLOBAL_GAIN_ARRAY_CENTRE, "Global", "GainArray Centre");
    SetRange(&BrnSound::Logic::KF_GLOBAL_GAIN_ARRAY_CENTRE, 0.0f, 50.0f);
    SetStep(&BrnSound::Logic::KF_GLOBAL_GAIN_ARRAY_CENTRE, 0.02f);
    RegisterVariable(&BrnSound::Logic::KF_GLOBAL_GAIN_ARRAY_FRONT, "Global", "GainArray Front");
    SetRange(&BrnSound::Logic::KF_GLOBAL_GAIN_ARRAY_FRONT, 0.0f, 50.0f);
    SetStep(&BrnSound::Logic::KF_GLOBAL_GAIN_ARRAY_FRONT, 0.02f);
    RegisterVariable(&BrnSound::Logic::KF_GLOBAL_GAIN_ARRAY_REAR, "Global", "GainArray Rear");
    SetRange(&BrnSound::Logic::KF_GLOBAL_GAIN_ARRAY_REAR, 0.0f, 50.0f);
    SetStep(&BrnSound::Logic::KF_GLOBAL_GAIN_ARRAY_REAR, 0.02f);
    RegisterVariable(&BrnSound::Logic::KF_GLOBAL_GAIN_ARRAY_SUB, "Global", "GainArray Sub");
    SetRange(&BrnSound::Logic::KF_GLOBAL_GAIN_ARRAY_SUB, 0.0f, 50.0f);
    SetStep(&BrnSound::Logic::KF_GLOBAL_GAIN_ARRAY_SUB, 0.02f);

    // Splice and Statistics member/global variables (tail of the original sequence).
    RegisterVariable(&mbSpliceAudition, "Splice", "Audition");
    RegisterVariable(&mbSpliceAuditionAutoCycle, "Splice", "Auto Cycle");
    RegisterVariable(&miSpliceBankIndex, "Splice", "Bank");
    BuildSplicerBankNames();
    RegisterVariable(&miSpliceIndexUnits, "Splice", "Index x1");
    SetRange(&miSpliceIndexUnits, 0, 512);
    RegisterVariable(&miSpliceIndexTens, "Splice", "Index x10");
    SetRange(&miSpliceIndexTens, 0, 512);
    SetStep(&miSpliceIndexTens, 10);

    RegisterVariable(&gbShowStatistics, "Statistics", "Main Screen Turn On");
    RegisterVariable(&gbResetStatistics, "Statistics", "Main Screen Reset");
    RegisterVariable(&gbStatisticsDrawDacGraph, "Statistics", "Draw Mixer Graph");
    RegisterVariable(&KI_DEBUG_PRINT_AUDIO_ALLOCATIONS, "Statistics", "Memory Dump");
    RegisterVariable(&KI_SPEW_SPLICE_USAGES, "Statistics", "Splice Stats Dump");
}


namespace
{
    // ARTIST pointer table off_82F2D030. The 25 names were read from the original image and
    // match AttribSys::Enums::eMaterialType's ascending one-bit order exactly.
    const char* const KA_COLLISION_MATERIAL_NAMES[25] =
    {
        "Nothing", "PlayerCar", "AiCar", "TrafficCar", "World",
        "BodyPartSmall", "BodyPartLarge", "Mirrors", "GlassSmall", "GlassLarge",
        "Wheels", "Lights", "Crane", "Mixer", "Tipper", "NumberPlate", "Seats",
        "RoofRacks", "Extinguisher", "Ladder", "Body", "Suspension", "Exhaust",
        "TrafficCarMedium", "TrafficCarLarge"
    };

    // ARTIST pointer table off_82F2CF6C, followed by the local "Off" choice.
    const char* const KA_COLLISION_ACTION_NAMES[6] =
    {
        "Collision", "Detach", "Hinging", "HingeOpen", "HingeClose", "Cracking"
    };
}

// ARTIST 0x827020B0. No base Update call is present in the original body.
void DebugComponent::Update()
{
    UpdateMaxRpmDisplay();

    if (KI_SPEW_SPLICE_USAGES)
    {
        CgsSound::Playback::SpliceBankStatistics::DumpAllToTty();
        KI_SPEW_SPLICE_USAGES = 0;
    }

    UpdateSpliceAudition();
}

// ARTIST 0x826BF8D0. Statistics has its own gate and the DAC graph is an independent option.
void DebugComponent::RenderHUD(CgsDev::Debug2DImmediateRender* lpRender)
{
    if (gbShowStatistics)
        mStatistics.RenderHUD(lpRender, gbStatisticsDrawDacGraph);
}

// ARTIST 0x826BF8F8 / DecFIGS BrnDebugComponent.h:94. This is the callback installed in
// CgsSound::Logic::Cgs3dEffectControl::spDebugRenderingFunction and invoked from UpdateParams.
// The buffered DebugRender API takes the original packed CgsDev::RGBA word directly.
void DebugComponent::Render3dControl(
    const CgsSound::Logic::Cgs3dEffectControl* lp3dControl)
{
    CgsDev::DebugInterface lDebugInterface;
    CgsDev::DebugRender& lRender = lDebugInterface.GetRender();

    const CgsSound::Logic::Cgs3dEffectControl::DebugRendererMessage& lrMessage =
        lp3dControl->GetDebugRenderingMessage();
    Vector3 lPosition = lp3dControl->GetEmitterPosition().GetCurrent();
    lPosition.y += lrMessage.mfYOffset;

    lRender.DrawSphere(lPosition, lrMessage.mfRadius, 0xFF00FFA0u);
}

// ARTIST 0x82689200 / DecFIGS BrnPhysicsControl.cpp:42-44. The menu gate is the s32
// KI_MONITOR_MAX_RPM; gfMaxRpmMonitor is an original 0.0f cell with no writer.
void DebugComponent::UpdateMaxRpmDisplay()
{
    if (!BrnSound::Vehicles::Engines::KI_MONITOR_MAX_RPM)
        return;

    CgsDev::DebugInterface lDebugInterface;
    CgsDev::DebugRender& lRender = lDebugInterface.Get2dRender();
    static_cast<void>(lDebugInterface.GetUI()); // present in the original call sequence

    char lacText[32];
    CgsCore::SPrintf(lacText, 32, "MAX RPM [%d]",
                     static_cast<s32>(BrnSound::Vehicles::Engines::gfMaxRpmMonitor));
    const Vector2 lPosition = { 40.0f, 30.0f, 0.0f, 0.0f };
    lRender.Draw2DTextJustified(lacText, lPosition,
                                CgsDev::DebugRender::E_JUSTIFY_LEFT,
                                12.0f, 0xFFFFFFFFu);
}

// ARTIST 0x826A12C0. i=0..24 corresponds to the highest set bit of (1u << i) in
// the console loop, so the recovered pointer table can be indexed directly.
void DebugComponent::BuildMaterialNames()
{
    for (s32 liIndex = 0; liIndex < 25; ++liIndex)
        {
            maCollisionMaterialNames[liIndex].miValue = liIndex;
            maCollisionMaterialNames[liIndex].mpcName = KA_COLLISION_MATERIAL_NAMES[liIndex];
        }
    {
        maCollisionMaterialNames[25].miValue = 0;
        maCollisionMaterialNames[25].mpcName = 0;
    }

    SetOptions(&BrnSound::Logic::Collision::KI_DEBUG_FILTER_MATERIAL, maCollisionMaterialNames);
    SetRange(&BrnSound::Logic::Collision::KI_DEBUG_FILTER_MATERIAL, 0, 25);
}

// ARTIST 0x826892D8. The six collision actions retain their original values; Off is 0.
void DebugComponent::BuildActionNames()
{
    for (s32 liIndex = 0; liIndex < 6; ++liIndex)
        {
            maCollisionActionNames[liIndex].miValue = liIndex + 1;
            maCollisionActionNames[liIndex].mpcName = KA_COLLISION_ACTION_NAMES[liIndex];
        }
    {
        maCollisionActionNames[6].miValue = 0;
        maCollisionActionNames[6].mpcName = "Off";
    }
    {
        maCollisionActionNames[7].miValue = 0;
        maCollisionActionNames[7].mpcName = 0;
    }

    SetOptions(&BrnSound::Logic::Collision::KI_DEBUG_FILTER_ACTION, maCollisionActionNames);
    SetRange(&BrnSound::Logic::Collision::KI_DEBUG_FILTER_ACTION, 0, 7);
}

// ARTIST 0x826A1380. The original fallback pointer is the empty string; Name::GetCString
// returns that same value when the interned spelling cannot be found.
void DebugComponent::BuildSplicerBankNames()
{
    s32 liIndex = 0;
    for (CgsSound::Playback::SpliceBankStatistics* lpBank =
             CgsSound::Playback::SpliceBankStatistics::spHead;
         lpBank;
         lpBank = lpBank->mpNext)
    {
        maSpliceBankNames[liIndex].miValue = static_cast<s32>(lpBank->muIndex);
        maSpliceBankNames[liIndex].mpcName = lpBank->mpSpliceContent
            ->GetContentSpec().GetName().GetCString();
        ++liIndex;
    }
    {
        maSpliceBankNames[liIndex].miValue = 0;
        maSpliceBankNames[liIndex].mpcName = 0;
    }

    SetOptions(&miSpliceBankIndex, maSpliceBankNames);
    SetRange(&miSpliceBankIndex, 0, liIndex - 1);
}

// ARTIST 0x82700110. Preserve the console's lock boundary, single-sample audition values,
// retry behavior on a missing bank/allocation, and unit-index-only auto-cycle increment.
void DebugComponent::UpdateSpliceAudition()
{
    rw::audio::core::System* lpSystem = CgsSound::Playback::GetDefaultRwacSystem();
    rw::audio::core::System::Lock(lpSystem);

    if (mbSpliceAudition)
    {
        if (!mpSpliceAuditionVoice)
        {
            const s8 lcSpliceType = static_cast<s8>(miSpliceBankIndex); // ARTIST extsb
            SPLICE_Data* lpSpliceData = gpSpliceManager->FindSplice(
                static_cast<SPLICE_TYPE>(lcSpliceType),
                miSpliceIndexTens + miSpliceIndexUnits);
            if (lpSpliceData)
            {
                // The console passes System+0x24 through the shared +0x24 accessor; the
                // vendor System home names that exact field mpMasteringSubMix.
                void* lpMemory = Splice::operator new(sizeof(Splice));
                if (lpMemory)
                {
                    mpSpliceAuditionVoice = ::new (lpMemory)
                        Splice(lpSpliceData, lpSystem->mpMasteringSubMix);
                }
            }

            if (mpSpliceAuditionVoice)
                mpSpliceAuditionVoice->Play(0.5f, 1.0f, 0.0f, 1.0f);
        }
        else
        {
            mpSpliceAuditionVoice->Update(lpSystem, 0.5f, 1.0f, 0.0f,
                                          0.016666668f, 1.0f);
            if (!mpSpliceAuditionVoice->IsPlaying())
            {
                mpSpliceAuditionVoice->~Splice();
                Splice::operator delete(mpSpliceAuditionVoice);
                mpSpliceAuditionVoice = 0;
                if (mbSpliceAuditionAutoCycle)
                    ++miSpliceIndexUnits;
            }
        }
    }

    rw::audio::core::System::Unlock(lpSystem);
}
}
}
