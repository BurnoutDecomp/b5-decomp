import json,pathlib,re,sys
from cryptography.hazmat.primitives.asymmetric import rsa,padding
from cryptography.hazmat.primitives import hashes,serialization
p=pathlib.Path('D:/Reverse/BP-X360-Experimental-Artifacts/wave30/protossl-native/cert-parser')
src=pathlib.Path('vendor/dirtysdk/src/x360/protossl_cert_data.inc').read_text()
key=rsa.generate_private_key(public_exponent=65537,key_size=1024);mod=key.public_key().public_numbers().n.to_bytes(128,'big')
lines=['static const u8 kCAModulus0[128] = {']
for i in range(0,128,16):lines.append('    '+', '.join('0x%02x'%b for b in mod[i:i+16])+',')
lines.append('};')
src=re.sub(r'static const u8 kCAModulus0\[128\] = \{.*?\};','\n'.join(lines),src,count=1,flags=re.S)
(p/'protossl_cert_data.inc').write_text('// TEST ONLY: first CA modulus replaced by generated local key.\n'+src)
(p/'test-key.pem').write_bytes(key.private_bytes(serialization.Encoding.PEM,serialization.PrivateFormat.PKCS8,serialization.NoEncryption()))
(p/'test-modulus.hex').write_text(mod.hex())
