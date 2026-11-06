from pathlib import Path
root=Path(r'D:/Reverse/BP-X360-Experimental-Worktrees/fma_scalar_luna_526adc')
BOM=b'\xef\xbb\xbf'
def replace_exact(rel, pairs):
 p=root/rel; raw=p.read_bytes(); has_bom=raw.startswith(BOM); enc='utf-8'; text=raw[3:].decode(enc) if has_bom else raw.decode(enc)
 for old,new,count_expected,label in pairs:
  n=text.count(old)
  if n!=count_expected: raise SystemExit(f'{rel}: {label}: expected {count_expected} matches, found {n}')
  text=text.replace(old,new)
 out=text.encode(enc)
 p.write_bytes((BOM+out) if has_bom else out)
 print(rel,[(label,count_expected) for _,_,count_expected,label in pairs])
replace_exact('src/GameShared/GameClasses/System/Input/CgsInputPads.cpp',[
 ('#include <cmath>     // std::fmaf (UpdateJoltEnvelope\'s two fmadds)\r\n','',1,'remove now-unused cmath include'),
 ('    // so they round once (std::fmaf) (crash parity FX-GATE).\r\n','    // so the native XDK must keep each scalar expression fused to one fmadds.\r\n',1,'document native contraction requirement'),
 ('return std::fmaf((lfTime - lfAttack) / lfDecay, lfLevel - lfPeak, lfPeak);','return ((lfTime - lfAttack) / lfDecay) * (lfLevel - lfPeak) + lfPeak;',1,'attack/decay fused expression'),
 ('return std::fmaf((lfTime - lfSustainEnd) / lfRelease, -lfLevel, lfLevel);','return ((lfTime - lfSustainEnd) / lfRelease) * (-lfLevel) + lfLevel;',1,'release fused expression'),
])
replace_exact('src/GameShared/GameClasses/Development/PerfMon/DebugComponent/CgsDebugComponentPerfMonGpu.cpp',[
 ('#include <cmath>  // std::fma preserves the X360 fused scale-bar endpoint\r\n','',1,'remove now-unused cmath include'),
 ('std::fma( -lfRowHeight, KF_TABLETITLEHEIGHT,\r\n                                               lrMetrics.mfScreenBorderTop ),','lrMetrics.mfScreenBorderTop - lfRowHeight * KF_TABLETITLEHEIGHT,',1,'table negative product plus top'),
 ('std::fma( static_cast<f32>( luLine ), KF_GRAPHCOLUMNPITCH,\r\n                                          KF_GRAPHORIGINX )','static_cast<f32>( luLine ) * KF_GRAPHCOLUMNPITCH + KF_GRAPHORIGINX',1,'graph grid x fused expression'),
 ('std::fma( mfMaxGpu, KF_GRAPHREFERENCEBARSCALE,\r\n                                             KF_GRAPHORIGINX )','mfMaxGpu * KF_GRAPHREFERENCEBARSCALE + KF_GRAPHORIGINX',1,'graph scale bar fused expression'),
])
