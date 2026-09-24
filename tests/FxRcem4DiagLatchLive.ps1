# FX-RCEM4 (crash parity 2026-09-24, reviewer A on 65eadffe item 3) -- the race-car module's
# [crash-exit] / [persist-damage] witnesses now print only under BRN_CRASH_EXIT_DIAG and its
# [intro-timer] witnesses only under BRN_INTRO_TIMER_DIAG. One Road Rage run (RivalOrganic: the
# drive-by start arms and releases the intro timer) with ONLY the intro latch set proves both sides:
#   * ON:  `[intro-timer] armed` and `[intro-timer] expired` print;
#   * OFF: the PostSceneUpdate one-shot SLICE banner -- printed on EVERY run before the latch -- and the
#          other race-car-module [crash-exit] / [persist-damage] lines stay silent.
# (CrashModule's own `[crash-exit] OPENED` / `CRASH COMPLETE posted` lines belong to another file and
#  are not latched here, so they are not counted.)
#   python b5-decomp/tests/run_rival_organic.py --case b5-decomp/tests/FxRcem4DiagLatchLive.ps1 --run-name fxrcem4_diag_latch
$case = & (Join-Path $PSScriptRoot 'RivalOrganic.ps1')
$case.Name = 'fxrcem4_diag_latch'
$case.Bug = 'The race-car module witnesses [crash-exit] / [persist-damage] / [intro-timer] must print only under their BRN_*_DIAG latch.'
$case.DiagEnv += ',BRN_INTRO_TIMER_DIAG=1'
$case.Run.MaxSeconds = 150
$case.Checks = @(
    @{ Kind = 'NewAsserts'; Name = 'no new assertions' }
    @{ Kind = 'LogCount'; Name = 'no exceptions'; Pattern = '\[EXCEPTION\]'; Max = 0 }
    @{ Kind = 'Mark'; Name = 'reached driving'; Phase = 'DRIVING' }
    @{ Kind = 'LogMatch'; Name = 'latch ON: [intro-timer] armed'; Pattern = '\[intro-timer\] armed'; Expect = $true }
    @{ Kind = 'LogMatch'; Name = 'latch ON: [intro-timer] expired'; Pattern = '\[intro-timer\] expired -> SetAllCarsOnStartLine'; Expect = $true }
    @{ Kind = 'LogCount'; Name = 'latch OFF: no PostSceneUpdate SLICE banner'; Pattern = '\[crash-exit\] RaceCarEntityModule::PostSceneUpdate SLICE'; Max = 0 }
    @{ Kind = 'LogCount'; Name = 'latch OFF: no race-car-module crash-exit lines'
       Pattern = '\[crash-exit\] (CRASH COMPLETE received|reset-on-track active race car)'; Max = 0 }
    @{ Kind = 'LogCount'; Name = 'latch OFF: no [persist-damage] lines'; Pattern = '\[persist-damage\]'; Max = 0 }
)
$case
