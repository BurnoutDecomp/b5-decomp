import json, pathlib, subprocess
p=pathlib.Path('D:/Reverse/BP-X360-Experimental-Artifacts/wave30/protossl-native/rsa-provider');cases=[]
for modulus in [0x8001,0xfffd,0xffff,0x80000001,0xfffffffb]:
 n=(modulus.bit_length()+7)//8
 for number in [0,1,modulus-1,modulus-2]:
  for exponent in [3,17,65537,7]:
   data=number.to_bytes(n,'big');m=modulus.to_bytes(n,'big');e=exponent.to_bytes((exponent.bit_length()+7)//8,'big')
   actual=subprocess.check_output([str(p/'host-kat.exe'),data.hex(),m.hex(),e.hex()],text=True).strip()
   expected=pow(number,exponent,modulus).to_bytes(n,'big').hex()
   cases.append({'modulus':hex(modulus),'input':hex(number),'exponent':exponent,'actual':actual,'expected':expected,'ok':actual==expected})
report={'host_only':True,'cases':cases,'passed':sum(c['ok'] for c in cases),'failed':sum(not c['ok'] for c in cases)}
(p/'host-edge-results.json').write_text(json.dumps(report,indent=2)+'\n');print('passed',report['passed'],'failed',report['failed']);assert not report['failed']
