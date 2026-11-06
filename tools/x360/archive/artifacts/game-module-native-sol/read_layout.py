from pathlib import Path
import json,importlib.util
OUT=Path(r'D:/Reverse/BP-X360-Experimental-Artifacts/game-module-native-sol');p=Path(r'D:/Reverse/BP-X360-Experimental-Artifacts/racecar_containment_sol_r1/derived-evidence-v2/coff_records.py');spec=importlib.util.spec_from_file_location('gm_coff',p);m=importlib.util.module_from_spec(spec);spec.loader.exec_module(m)
row=next(r for r in json.loads((OUT/'baseline-v2/compile-results.json').read_text())['results']if r['passed']);fields=json.loads((OUT/'layout-fields.json').read_text());words=m.Records(row['object']).words('gGameModuleNativeLayout',len(fields));layout={k:{'expression':expr,'decimal':v,'hex':hex(v)}for (k,expr),v in zip(fields,words)}
(OUT/'baseline-v2/layout.json').write_text(json.dumps(layout,indent=2)+'\n')
for k in ['game_size','mGameStateModule','mDirectorModule','mInputModule','mGuiModule','mEffectsModule','mNetworkModule','BrnDirector::DirectorModule_size','BrnGui::GuiModule_size','CgsInput::InputModule_size','BrnDirector::MainDirector_size','director.mMainDirector','director.mCamera','director.mDirectorSerialiser','director.mCgsCamera','main_director.mICESceneSpace']:
 print(k,layout[k]['hex'])
