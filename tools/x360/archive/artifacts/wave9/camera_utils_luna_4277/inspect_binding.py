import json
from pathlib import Path
p=json.loads(Path(r"D:\Reverse\BP-X360-Experimental-Artifacts\wave8\repair-r7\native-parallel-wave8-r7-full-recovered-patch.json").read_text())
r=json.loads(Path(r"D:\Reverse\BP-X360-Experimental-Artifacts\wave8\repair-r7\full-stage1-interrupted-checkpoint.json").read_text())
targets=["src/GameSource/Director/Camera/Utils/BrnVehicleCollisionPredictor.cpp","src/GameSource/Director/Camera/Utils/BrnTextFileReadSerialiser.cpp","src/GameSource/Director/Camera/Utils/BrnTextFileWriteSerialiser.cpp","src/GameSource/Director/Camera/Utils/BrnPositionLag.cpp","src/GameSource/Director/Camera/Utils/BrnCameraShakeICEController.cpp"]
print("R7 patch base/head",p["base"],p["head"])
for t in targets:print(t,"in_patched_paths=",t in p["paths"],"snapshot_hash=",p["source_sha256"].get(t))
print("checkpoint keys",r.keys())
for k,v in r.items():
 if isinstance(v,dict):
  for t in targets:
   if t in v:print("checkpoint",k,t,v[t])