import pathlib, subprocess, json
p=pathlib.Path('D:/Reverse/BP-X360-Experimental-Artifacts/wave30/protossl-native/rsa-provider');cases=[]
for seed in [0x12345678,11]:
 tick=seed;buf=[int(bool(seed & (1 << (i & 31)))) for i in range(64)]; retries=0
 for i in range(64):
  value=buf[i]
  while True:
   tick=(69069*(tick+1)) & 0xffffffff;value=(value ^ tick) & 255
   if value: break
   retries+=1
  buf[i]=value
 buf[0]=0;buf[1]=2;buf[47]=0;buf[48:]=range(16)
 actual=subprocess.check_output([str(p/'padding-kat.exe'),str(seed)],text=True).strip();expected=bytes(buf).hex()
 cases.append({'ok':actual==expected,'seed':hex(seed),'zero_retries':retries,'actual':actual,'expected':expected})
(p/'padding-kat-results.json').write_text(json.dumps({'cases':cases},indent=2)+'\n');print(cases);assert all(c['ok'] for c in cases) and cases[1]['zero_retries']>0
