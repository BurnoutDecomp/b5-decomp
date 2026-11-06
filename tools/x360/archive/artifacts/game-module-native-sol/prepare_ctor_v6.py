from pathlib import Path
p=Path(r'D:/Reverse/BP-X360-Experimental-Artifacts/game-module-native-sol/validate_replay_ctor_v5.py');s=p.read_text(encoding='utf-8-sig');s=s.replace("objects=[PackedCoff", "extra=json.loads((OUT/'icetake-provider-v6/compile-results.json').read_text())['results'][0];rows['ICEDataICETake.cpp']=extra\nobjects=[PackedCoff",1).replace("rows['ICEData.cpp']['object']","rows['ICEDataICETake.cpp']['object']").replace("'replay-ctor-v5-comparison.json'","'replay-ctor-v6-comparison.json'")
p.with_name('validate_replay_ctor_v6.py').write_text(s,encoding='utf-8')
