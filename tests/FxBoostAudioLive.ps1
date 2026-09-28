# Run with tests/run_rival_organic.py --ai-pad pursuit --slot 5 --no-frames.
$case = & (Join-Path $PSScriptRoot 'FxAudioStreamsLive.ps1')
$case.Name = 'fx_boost_audio'
$case.DiagEnv += ',BRN_GINSU_DIAG=1,BRN_AI_PAD_PLAYER=pursuit'
$case.Bug = 'Original boost controls must activate the authored boost bank and produce real sample PCM.'
$case.Checks += @(
 @{Kind='Script';Name='boost audio receives elapsed time and scaled road speed';Script={
   param($ctx)
   $s=Get-Content -LiteralPath (Join-Path $ctx.RunDir 'reverb_pcm_trace.txt')
   $good=0;$bad=0
   foreach($l in $s){
     if($l -match '^boost-input stage=6 boosting=1 speed=([^ ]+) time=([^ ]+)'){
       $speed=[double]::Parse($Matches[1],[Globalization.CultureInfo]::InvariantCulture)
       $time=[double]::Parse($Matches[2],[Globalization.CultureInfo]::InvariantCulture)
       if($speed -gt 0 -and $speed -le 1024 -and $time -lt 1000){$good++}else{$bad++}
     }
   }
   @{Pass=($good -gt 0 -and $bad -eq 0);Detail="$good boost-in edges with original controls; $bad incorrect"}
 }}
 @{Kind='Script';Name='boost bank emits actual sample PCM';Script={
   param($ctx)
   $banks=@{};$samples=@{};$n=0;$mixed=0
   foreach($l in Get-Content -LiteralPath (Join-Path $ctx.RunDir 'reverb_pcm_trace.txt')){
     if($l -match '^aems-bank bank=(\d+) .* path=.*Boost.*\.abi'){$banks[$Matches[1]]=$true}
     elseif($l -match '^aems-select bank=(\d+) sample=(\w+)'){
       if($banks.ContainsKey($Matches[1])){$samples[$Matches[2]]=$true}
     }elseif($l -match '^aems-pcm .*sample=(\w+) '){
       if($samples.ContainsKey($Matches[1])){$n++}
     }elseif($l -match '^aems-mix-pcm .*sample=(\w+) '){
       if($samples.ContainsKey($Matches[1])){$mixed++}
     }
   }
   @{Pass=($n -gt 0 -and $mixed -gt 0);Detail="$n decoded sample players attributed to a boost bank; $mixed nonzero send contributions"}
 }}
)
$case
