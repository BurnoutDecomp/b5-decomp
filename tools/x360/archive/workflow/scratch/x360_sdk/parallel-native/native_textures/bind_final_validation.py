from pathlib import Path
base=Path(r'C:/Users/adri1/.codex/worktrees/fbaf/BP-Decomp_Workflow/scratch/x360_sdk/parallel-native/native_textures')
for name in ['validate_getters.py','validate_resources.py','validate_loading_state.py','validate_loading_setup.py','validate_loading_render.py','validate_layout.py']:
    p=base/name;s=p.read_text(encoding='utf-8').replace('/native_textures/native2','/native_textures/final').replace('/native_textures/native3','/native_textures/final')
    if name=='validate_layout.py':s=s.replace("p=Path(j['results'][0]['object'])", "p=Path(next(r['object'] for r in j['results'] if r['source'].endswith('native_texture_layout.cpp')))")
    p.write_text(s,encoding='utf-8')
