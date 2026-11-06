import json,os,subprocess,pathlib,sys
root=pathlib.Path('D:/Reverse/BP-X360-Experimental-Artifacts/wave11/sound_remaining_terra'); (root/'temp').mkdir(parents=True,exist_ok=True)
d=json.load(open('D:/Reverse/BP-X360-Experimental-Artifacts/wave8/repair-r13-prep-20261103/composite-effective.json'))
r=next(x for x in d['results'] if x['source'].endswith('BrnAIPhysicsControl.cpp'))
incs=[x for x in r['command'] if x.startswith('/IC:/Users/')]
sources=['src/GameSource/Sound/Collision/BrnCollisionStateManager.cpp']+['src/GameSource/Sound/Vehicles/Engines/'+n+'.cpp' for n in ['BrnAIPhysicsControl','BrnHybridExhaustControl','BrnPhysicsControl']]
cmd=['python','tools/build_x360.py','--sdk',d['sdk'],'--out',str(root/sys.argv[1]),'--jobs','1','--compiler-arg=/showIncludes','--quiet']
for i in incs: cmd.append('--compiler-arg='+i)
for s in sources: cmd += ['--source',s]
env=os.environ.copy(); env['TEMP']=env['TMP']=str(root/'temp'); subprocess.run(cmd,env=env)
