import sys,types,json,hashlib
from pathlib import Path
sys.dont_write_bytecode=True
_r6_objects={'CgsPerfMonGpuPS3': 'D:\\Reverse\\BP-X360-Experimental-Artifacts\\wave8\\repair-r6\\selected-stage1\\CgsPerfMonGpuPS3-17e6ca3b.obj', 'CgsPerfMonGpu': 'D:\\Reverse\\BP-X360-Experimental-Artifacts\\wave8\\repair-r6\\selected-stage1\\CgsPerfMonGpu-a8ed9d0a.obj', 'CgsStrStream': 'D:\\Reverse\\BP-X360-Experimental-Artifacts\\wave8\\repair-r6\\selected-stage1\\CgsStrStream-95facce1.obj', 'CgsPerfMonCpu': 'D:\\Reverse\\BP-X360-Experimental-Artifacts\\wave8\\repair-r6\\named-native-gates\\CgsPerfMonCpu.obj', 'actual-branch-v1': 'D:\\Reverse\\BP-X360-Experimental-Artifacts\\wave8\\repair-r6\\named-native-gates\\ObservedMainFrequencyBranch.obj', 'sdk-u64tod': 'D:\\Reverse\\BP-X360-Experimental-Artifacts\\perfmon-gpu-native-r4\\sdk-u64tod.obj'}
_r6_views={'gpu': 'D:\\Reverse\\BP-X360-Experimental-Artifacts\\wave8\\repair-r6\\current-gpu-witnesses\\gpu-actual-row-selection.json', 'frequency': 'D:\\Reverse\\BP-X360-Experimental-Artifacts\\wave8\\repair-r6\\current-gpu-witnesses\\frequency-actual-row-selection.json'}
_r6_out=Path('D:\\Reverse\\BP-X360-Experimental-Artifacts\\wave8\\repair-r6\\current-gpu-witnesses\\gpu-native')
def _r6_output(p):return _r6_out/Path(p).name
def _r6_object(p):
 stem=Path(p).stem;hits=[v for k,v in _r6_objects.items()if stem==k or stem.startswith(k+'-')]
 assert len(hits)==1,(str(p),hits)
 return Path(hits[0])
def _r6_report(p):
 return Path(_r6_views['frequency' if 'successor-native-v1' in str(p) else 'gpu'])
sys.path.insert(0,'D:\\Reverse\\BP-X360-Experimental-Artifacts\\perfmon-gpu-native-r4')
_helper=types.ModuleType("gpu_fixture");_helper.__dict__.update({k:v for k,v in globals().items()if k.startswith("_r6_")});_helper.__file__='D:\\Reverse\\BP-X360-Experimental-Artifacts\\perfmon-gpu-native-r4\\gpu_fixture.py'
exec(compile(Path('D:\\Reverse\\BP-X360-Experimental-Artifacts\\wave8\\repair-r6\\current-gpu-witnesses\\gpu-native\\gpu_fixture_adapted.py').read_text(encoding="utf-8"),_helper.__file__,"exec"),_helper.__dict__);sys.modules["gpu_fixture"]=_helper
__file__='D:\\Reverse\\BP-X360-Experimental-Artifacts\\perfmon-gpu-native-r4\\validate_gpu_native.py'
exec(compile(Path('D:\\Reverse\\BP-X360-Experimental-Artifacts\\wave8\\repair-r6\\current-gpu-witnesses\\gpu-native\\model_adapted.py').read_text(encoding="utf-8"),__file__,"exec"),globals())
