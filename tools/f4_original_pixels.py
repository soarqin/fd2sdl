"""Finite source-qualified original pixel kernels, extracted from the F4 harness.

No decoder/model/IO stubs. The caller selects only declared pure normal writer
ABIs. Hash-only manifest binds complete source chunks and instruction heads to
user-provided strict image. Original code bytes and images are never embedded.
Old resource batches need not be rerun to import this reusable helper.
"""
import hashlib
import struct
from collections import Counter

INPUT, PAGE, PALETTE, BASE, STACK_BASE, STACK, RET = (
    0x100000, 0x300000, 0x400000, 0x410000, 0x500000, 0x508000, 0x700000)
IMAGE_SHA="8c40cac1a4eb947007740c90a4bae8acdb49b3922a58f565eda0454bf4d238d1"
MANIFEST_SHA="f13de1d584849a9edb18937267e15a2da985dc88525906d9fe80ba47b54975dc"

def u16(data, at=0): return struct.unpack_from("<H",data,at)[0]
def u32(data, at=0): return struct.unpack_from("<I",data,at)[0]
def sha(data): return hashlib.sha256(data).hexdigest()

def validate_manifest(image, raw):
    import json
    if sha(image)!=IMAGE_SHA or sha(raw)!=MANIFEST_SHA: raise ValueError("original image/kernel manifest identity")
    document=json.loads(raw)
    if document["schema"]!="fd2-f4-pixel-kernel-identities-v1" or document["image_sha256"]!=IMAGE_SHA:
        raise ValueError("original kernel manifest schema")
    functions={}
    for f in document["functions"]:
        if f["va"] in functions: raise ValueError("duplicate kernel owner")
        covered,decoded=set(),set()
        for c in f["chunks"]:
            a,b=c["start"],c["end"]
            if not 0x10000<=a<b<=0x64000 or sha(image[a:b])!=c["sha256"]:
                raise ValueError("complete kernel chunk identity")
            if covered.intersection(range(a,b)): raise ValueError("duplicate chunk bytes")
            covered.update(range(a,b))
        for i in f["instructions"]:
            a,n=i["ea"],i["size"]
            if not 1<=n<=15 or decoded.intersection(range(a,a+n)):
                raise ValueError("duplicate/invalid kernel head")
            decoded.update(range(a,a+n))
        if not covered or covered!=decoded: raise ValueError("kernel instruction coverage")
        functions[f["va"]]=f
    return functions

SEED_INPUTS = {}


class OriginalPixels:
    """Only the finite source-checked pixel kernel PCs, not arbitrary binaries."""
    def __init__(self, image, functions, owners, source, width, height, stride, seed, family, initial=None):
        import unicorn
        import unicorn.x86_const as regs
        self.regs = regs
        self.cpu = unicorn.Uc(unicorn.UC_ARCH_X86, unicorn.UC_MODE_32)
        self.cpu.mem_map(0, 0x64000)
        self.cpu.mem_write(0, image[:0x64000])
        self.cpu.mem_map(INPUT, (max(len(source), 1)+4095)//4096*4096)
        self.cpu.mem_write(INPUT, source)
        self.source = source
        self.size, self.stride = stride*height, stride
        self.cpu.mem_map(PAGE, (self.size+4095)//4096*4096)
        seed_key = (self.size, stride, seed, family == "ani")
        if seed_key not in SEED_INPUTS:
            SEED_INPUTS[seed_key] = bytes([83])*self.size if family == "ani" else bytes((i*17+i//stride+seed)&255 for i in range(self.size))
        self.initial = SEED_INPUTS[seed_key] if initial is None else bytes(initial)
        if len(self.initial) != self.size: raise ValueError("explicit initial owner size")
        self.cpu.mem_write(PAGE, self.initial)
        self.cpu.mem_map(STACK_BASE, 0x10000)
        self.cpu.mem_map(PALETTE, 0x1000)
        self.cpu.mem_map(BASE, 0x1000)
        self.cpu.mem_write(PALETTE, bytes([0xa5])*768)
        self.cpu.mem_write(BASE, bytes([7])*768)
        self.cpu.mem_write(0x53a65, struct.pack("<I", BASE))
        self.mask, self.palette_mask, self.visited = bytearray(self.size), bytearray(768), set()
        allowed = {ins["ea"]: ins for owner in owners for ins in functions[owner]["instructions"]}
        io_ops = {"in", "out", "int", "into", "ins", "insb", "insw", "insd", "outs", "outsb", "outsw", "outsd", "iret", "iretd", "syscall", "sysenter"}
        self.io_instruction_count = 0
        self.abi_calls = 0
        def code(cpu, pc, size, _):
            assert pc in allowed, ("undeclared original PC", hex(pc))
            tokens = allowed[pc]["disasm"].lower().split()
            mnemonic = tokens[1] if tokens[0] in {"rep", "repe", "repne", "repz", "repnz", "lock"} else tokens[0]
            assert mnemonic not in io_ops, ("hardware/DOS I/O opcode is outside pixel oracle", hex(pc), mnemonic)
            self.visited.add(pc)
        def write(cpu, access, address, size, value, _):
            if PAGE <= address and address+size <= PAGE+self.size:
                self.mask[address-PAGE:address-PAGE+size] = bytes([1])*size
            elif PALETTE <= address and address+size <= PALETTE+768:
                assert family == "ani"
                self.palette_mask[address-PALETTE:address-PALETTE+size] = bytes([1])*size
            elif STACK_BASE <= address and address+size <= STACK_BASE+0x10000:
                pass
            elif family == "figani" and 0x627b4 <= address and address+size <= 0x627b8:
                pass
            elif family == "glyph" and 0x627a3 <= address and address+size <= 0x627b4:
                pass
            elif family == "raw" and 0x627a3 <= address and address+size <= 0x627a5:
                pass
            elif family == "ani" and 0x52760 <= address and address+size <= 0x5276a:
                pass
            else:
                raise AssertionError(("write outside declared page/temp/scratch/stack", hex(address), size))
        self.hooks = [self.cpu.hook_add(unicorn.UC_HOOK_CODE, code),
                      self.cpu.hook_add(unicorn.UC_HOOK_MEM_WRITE, write)]

    def call(self, entry, args):
        self.abi_calls += 1
        self.cpu.mem_write(STACK, struct.pack("<"+"I"*(len(args)+1), RET, *args))
        self.cpu.reg_write(self.regs.UC_X86_REG_ESP, STACK)
        self.cpu.reg_write(self.regs.UC_X86_REG_EFLAGS, 2)
        self.cpu.emu_start(entry, RET, count=10000000)
        assert self.cpu.reg_read(self.regs.UC_X86_REG_EIP) == RET
        assert bytes(self.cpu.mem_read(INPUT, len(self.source))) == self.source
        assert bytes(self.cpu.mem_read(BASE, 768)) == bytes([7])*768
        assert u32(bytes(self.cpu.mem_read(0x53a65, 4))) == BASE

    def payload(self):
        return bytes(self.cpu.mem_read(PAGE, self.size)), bytes(self.cpu.mem_read(PALETTE, 768))


    def close(self):
        if self.cpu is not None:
            for hook in self.hooks:
                self.cpu.hook_del(hook)
            self.hooks = []
            self.cpu = None



def rle_extent(data, width, height):
    # Extent/mode inventory only. No output-pixel or expected-mask construction.
    cursor = 0
    modes = Counter()
    for row in range(height):
        column = 0
        while column < width:
            assert cursor < len(data), (row, column, "truncated RLE")
            control = data[cursor]
            cursor += 1
            count, mode = (control & 63)+1, control >> 6
            modes[str(mode)] += 1
            advance = count*2 if mode == 1 else count
            assert column+advance <= width, (row, column, control, width)
            source = count if mode == 2 else 1 if mode < 2 else 0
            assert cursor+source <= len(data)
            cursor += source
            column += advance
    assert cursor == len(data), (cursor, len(data), "unconsumed RLE source")
    return dict(modes)


def lmi_extent(data):
    width, height = u16(data), u16(data, 2)
    assert width > 0 and height > 0
    cursor, emitted, total = 4, 0, width*height
    while emitted < total:
        assert cursor < len(data)
        control = data[cursor]
        cursor += 1
        count = control-0xc0 if control > 0xc0 else 1
        if control > 0xc0:
            assert cursor < len(data)
            cursor += 1
        emitted += count
        assert emitted <= total
    assert cursor == len(data), (cursor, len(data), "unconsumed LMI source")
    return width, height
