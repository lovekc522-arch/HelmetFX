import hashlib, os, struct, sys

root = os.path.abspath(os.path.join(os.path.dirname(os.path.abspath(__file__)), '..'))
src = os.path.join(root, 'arma_addon')
out_dir = os.path.abspath(sys.argv[1]) if len(sys.argv) > 1 else os.path.join(root, 'build', 'HelmetFX', 'addons')
out = os.path.join(out_dir, 'helmetfx_main.pbo')
prefix = 'x\\helmetfx\\addons\\main'

if not os.path.isdir(src):
    sys.exit('arma_addon folder not found: ' + src)

files = sorted(f for f in os.listdir(src)
               if os.path.isfile(os.path.join(src, f)) and not f.startswith('.'))
if 'config.cpp' not in files:
    sys.exit('config.cpp not found in ' + src)

hdr = b'\0' + struct.pack('<5I', 0x56657273, 0, 0, 0, 0)
hdr += b'prefix\0' + prefix.encode() + b'\0' + b'\0'
data = b''
for f in files:
    with open(os.path.join(src, f), 'rb') as fh:
        d = fh.read()
    hdr += f.encode() + b'\0' + struct.pack('<5I', 0, len(d), 0, 0, len(d))
    data += d
hdr += b'\0' + struct.pack('<5I', 0, 0, 0, 0, 0)
body = hdr + data

os.makedirs(out_dir, exist_ok=True)
with open(out, 'wb') as fh:
    fh.write(body + b'\0' + hashlib.sha1(body).digest())
print('wrote', out, '(%d files)' % len(files))
