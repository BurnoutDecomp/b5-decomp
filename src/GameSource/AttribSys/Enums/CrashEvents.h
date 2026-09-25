#ifndef ATTRIBSYS_ENUMS_CRASH_EVENTS_H
#define ATTRIBSYS_ENUMS_CRASH_EVENTS_H

namespace AttribSys
{
namespace Enums
{
namespace CrashEvents
{

// DecFIGS CrashEvents.h / CrashEvents.cpp:9. The authored crash-event bits a shot is "suitable for"
// (Attrib iceanim / proceduralshot SuitableFor) and that BrnDirector::CrashAnalyser::Update raises
// into CrashAnalysis::mxEventFlags for the shot selector to match against.
//
// The DWARF carries only KI_NUM_ENUMS / KI_MAX_VALUE and `const char* gaNames[9]`; the names are
// read out of the ARTIST image. gaNames @0x82CDA518 (x360rd) points at, in order:
//     "None" 0x8200169C, "HardStop" 0x82001634, "LeftImpact" 0x82001628, "RightImpact" 0x8200161C,
//     "WorldImpact" 0x82001610, "CarImpact" 0x82001604, "CrashStart" 0x820015F8,
//     "FrontImpact" 0x820015EC, "RearImpact" 0x820015E0
// and entry [k >= 1] names bit k-1 -- ShotSelector's PrintShotEventFlags @0x821F6EA0 walks the
// table from entry [1] against the flag word one bit at a time. The values are confirmed by the
// one producer, CrashAnalyser::Update @0x82209290:
//     li 0x20 @0x8220931C                    -> CrashStart                          (the crash's first frame)
//     li 0x29 @0x82209330                    -> CrashStart | WorldImpact | HardStop (PlayerCrashInfo::mbHardstopVsWall)
//     ori 0x11 @0x82209348                   -> CarImpact | HardStop                (PlayerCrashInfo::mbHardStopVsAI)
//     ori 2 / 4 @0x82209458 / @0x82209490     -> LeftImpact / RightImpact            (the car-space X test)
//     ori 0x40 / 0x80 @0x822094C4 / @0x822094F8 -> FrontImpact / RearImpact         (the car-space Z test)
// Like eAction / eImpactTime these are authored BIT values, not a zero-based index.
enum CrashEvents
{
    None        = 0,
    HardStop    = 1,
    LeftImpact  = 2,
    RightImpact = 4,
    WorldImpact = 8,
    CarImpact   = 16,
    CrashStart  = 32,
    FrontImpact = 64,
    RearImpact  = 128,
};

const int KI_NUM_ENUMS = 9;
const int KI_MAX_VALUE = 128;

} // namespace CrashEvents
} // namespace Enums
} // namespace AttribSys

#endif // ATTRIBSYS_ENUMS_CRASH_EVENTS_H
