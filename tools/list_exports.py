import struct, sys

def main(path):
    d = open(path, 'rb').read()
    pe = struct.unpack_from('<I', d, 0x3C)[0]
    if d[pe:pe+4] != b'PE\0\0':
        sys.exit('not a PE file')
    machine, nsec, _, _, _, optsize, _ = struct.unpack_from('<HHIIIHH', d, pe + 4)
    print('machine:', {0x8664: 'x64', 0x14c: 'x86 (32-bit!)', 0xaa64: 'arm64'}.get(machine, hex(machine)))
    opt = pe + 24
    magic = struct.unpack_from('<H', d, opt)[0]
    ddir = opt + (112 if magic == 0x20b else 96)
    exp_rva, exp_size = struct.unpack_from('<II', d, ddir)
    secs = []
    sp = opt + optsize
    for i in range(nsec):
        vsize, va, rawsize, rawptr = struct.unpack_from('<IIII', d, sp + i*40 + 8)
        secs.append((va, max(vsize, rawsize), rawptr))
    def off(rva):
        for va, sz, raw in secs:
            if va <= rva < va + sz:
                return rva - va + raw
        raise ValueError('bad rva')
    if not exp_rva:
        print('NO EXPORTS: TeamSpeak cannot use this DLL')
        return
    e = off(exp_rva)
    nfunc, nnames, _, names_rva, _ = struct.unpack_from('<IIIII', d, e + 20)
    names = []
    for i in range(nnames):
        r = struct.unpack_from('<I', d, off(names_rva) + 4*i)[0]
        o = off(r)
        names.append(d[o:d.index(b'\0', o)].decode())
    print('exports (%d):' % len(names))
    for n in sorted(names):
        print('  ', n)
    need = ['ts3plugin_name', 'ts3plugin_version', 'ts3plugin_apiVersion', 'ts3plugin_author',
            'ts3plugin_description', 'ts3plugin_setFunctionPointers', 'ts3plugin_init', 'ts3plugin_shutdown']
    missing = [n for n in need if n not in names]
    print('MISSING required exports:', missing if missing else 'none')

if len(sys.argv) < 2:
    sys.exit('usage: python tools/list_exports.py <dll>')
main(sys.argv[1])
