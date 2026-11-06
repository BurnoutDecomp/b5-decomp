import json, pathlib, random, subprocess, sys
root=pathlib.Path('D:/Reverse/BP-X360-Experimental-Artifacts/wave30/protossl-native/rsa-provider')
rng=random.Random(0x82b41ef0)
results=[]
for n in [2,3,4,8,16,32,128]:
 for e in [3,17,65537,7,0]:
  if n==128 and e in [7,0]: continue
  modulus=(1 << (8*n-1)) | rng.getrandbits(8*n-1) | 1
  number=rng.randrange(0,modulus)
  data=number.to_bytes(n,'big'); mod=modulus.to_bytes(n,'big'); ex=e.to_bytes(max(1,(e.bit_length()+7)//8),'big')
  p=subprocess.run([str(root/'host-kat.exe'),data.hex(),mod.hex(),ex.hex()],capture_output=True,text=True,timeout=90)
  value=pow(number,e,modulus) if e else 1
  expected=(value.to_bytes(n,'big') if n%2==0 else (value & ((1<<(8*(n-1)))-1)).to_bytes(n-1,'big')+data[-1:]).hex()
  results.append({'bytes':n,'exponent':e,'input':data.hex(),'modulus':mod.hex(),'actual':p.stdout,'expected':expected,'ok':p.returncode==0 and p.stdout==expected,'returncode':p.returncode})
report={'host_only':True,'seed':'0x82b41ef0','cases':results,'passed':sum(x['ok'] for x in results),'failed':sum(not x['ok'] for x in results)}
(root/'host-kat-results.json').write_text(json.dumps(report,indent=2))
print('passed',report['passed'],'failed',report['failed'])
for x in results:
 if not x['ok']:print(x)
sys.exit(report['failed']!=0)
