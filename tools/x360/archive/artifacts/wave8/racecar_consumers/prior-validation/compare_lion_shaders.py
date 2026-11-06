from pathlib import Path
import json,sys,hashlib
sys.path.insert(0,'D:/Reverse/BP-Decomp_Workflow/scratch/CRASHPARITY_0922/fxcrashvfx_vmxemu');import vmxemu as V
out=Path('D:/Reverse/BP-X360-Experimental-Artifacts/wave8/racecar_consumers/prior-validation');obj=Path('D:/Reverse/BP-X360-Experimental-Artifacts/wave8/racecar_consumers/BrnLionBlendIm3d/final-d08b4c45f5a0.obj');data=obj.read_bytes();e=json.loads(Path('C:/Users/adri1/.codex/worktrees/fbaf/BP-Decomp_Workflow/scratch/parallel-native/particles_native/shader-evidence.json').read_text());rows=[]
for name in ['LionBlendVertex','LionBlendPixel','LionBlendZFadeVertex','LionBlendZFadePixel']:
 q=e[name];raw=V.x360rd.rd(int(q['address'],16),q['size']);assert hashlib.sha256(raw).hexdigest()==q['sha256'];at=data.find(raw);assert at>=0,name;rows.append({'name':name,'address':q['address'],'size':q['size'],'sha256':q['sha256'],'object_offset':at})
d={'object_sha256':hashlib.sha256(data).hexdigest(),'shader_packages':rows,'scope':'four original shader byte packages retained in actual object; not draw-constant, runtime GPU or whole BeginRendering parity'};(out/'lion-shaders-comparison.json').write_text(json.dumps(d,indent=2));print(json.dumps(d,indent=2))
