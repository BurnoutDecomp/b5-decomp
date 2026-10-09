$case = & (Join-Path $PSScriptRoot 'AnimatedSignsLive.ps1')
$case.Name = 'animated_signs_junkyard'
# TRK_UNIT119 instance model 27CA53BC places the neon sign at (3012.21,7.35,-1956.05).
# Look at its north face from the road; the lettering/bulbs are above the instance origin.
$case.Run.Teleport = '3040.7,-5.8,-1910,210'
$case.Run.MaxSeconds = 65
$case.Run.FrameEvery = 3
$case.DiagEnv = 'BRN_MATERIAL_ANIM_DIAG=1,BRN_FRAME_DUMP_START=5000,BRN_FRAME_DUMP_MAX=240'
$case.Checks += @{ Kind='LogMatch'; Name='car actually seats by the default junkyard'; Pattern='\[teleport\] ProcessResetEvents car 0 road \(3040\.' }
$case
