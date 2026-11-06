import sys,json
from pathlib import Path
sys.dont_write_bytecode=True
sys.path.insert(0,'C:/Users/adri1/.codex/worktrees/fbaf/BP-Decomp_Workflow/scratch/x360_sdk')
sys.path.insert(0,'D:\\Reverse\\BP-X360-Experimental-Artifacts\\parallel\\wave8_scene_terra\\gpcylinder-maximum-feature-sol')
_r5_objects={'GPCylinder': 'D:\\Reverse\\BP-X360-Experimental-Artifacts\\wave8\\repair-r7\\selected-successor1\\GPCylinder-2c424d06.obj'}
_r5_output_dir=Path('D:\\Reverse\\BP-X360-Experimental-Artifacts\\wave8\\repair-r7\\current-world-auxiliary-witnesses\\cylinder-feature')
def _r5_output(p):
    return _r5_output_dir/Path(p).name
def _r5_object(p):
    p=Path(p)
    stem=p.stem
    found=[v for k,v in _r5_objects.items() if stem==k or stem.startswith(k+'-')]
    assert len(found)==1,(str(p),found)
    return Path(found[0])
__file__='D:\\Reverse\\BP-X360-Experimental-Artifacts\\parallel\\wave8_scene_terra\\gpcylinder-maximum-feature-sol\\validate_features_v1.py'
sys.argv=['D:\\Reverse\\BP-X360-Experimental-Artifacts\\parallel\\wave8_scene_terra\\gpcylinder-maximum-feature-sol\\validate_features_v1.py', 'r7-selected']
_r7_previous_object = _r5_object
_r7_aliases={'CgsFine_wSQ1': 'D:\\Reverse\\BP-X360-Experimental-Artifacts\\wave8\\repair-r7\\selected-successor1\\CgsFineIntersectionTestModule_wSQ1-253da880.obj', 'CgsFine-hit-v2': 'D:\\Reverse\\BP-X360-Experimental-Artifacts\\wave8\\repair-r7\\selected-successor1\\CgsFineIntersectionTestModule_wSQ1-253da880.obj'}
def _r5_object(p):
 p=Path(p)
 return Path(_r7_aliases[p.stem]) if p.stem in _r7_aliases else _r7_previous_object(p)
exec(compile(Path('D:\\Reverse\\BP-X360-Experimental-Artifacts\\wave8\\repair-r7\\current-world-auxiliary-witnesses\\cylinder-feature\\adapted_model.py').read_text(encoding="utf-8"),__file__,"exec"),globals())
