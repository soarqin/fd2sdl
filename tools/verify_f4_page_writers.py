"""A14-C01 finite normal 24x24 writer oracle and separate actual-C comparison.

original saves new full before/after/write/read masks from five unmodified pure
original leaves. verify never runs original code and requires that artifact SHA.
Neither operation executes the game, performs I/O in guest code, or qualifies
heap, clipping outside the original owner, clocks, UI or natural reachability.
"""
from __future__ import annotations
import argparse
import hashlib
import json
from pathlib import Path
import struct
import sys

ROOT = Path(__file__).resolve().parents[1]
if not (ROOT / 'tools/static_data/audit_stage_data.py').is_file():
    ROOT = Path.cwd()  # ignored candidate is executed from the project root
sys.path.insert(0, str(ROOT / 'tools/static_data'))
from audit_stage_data import archive_entries

PAGE = 456 * 336
HEAD = struct.Struct('<8sII')
MAGIC = b'F4PW0001'
IMAGE_SHA = '8c40cac1a4eb947007740c90a4bae8acdb49b3922a58f565eda0454bf4d238d1'
EXE_SHA = 'bb35004c06fc483e68f869bb7eb14dde9f9b7e585af29501af5ad1004c8861cd'
OTHER_SHA = '882580dfa31bd22e069eb151b7a86a2b8dbcd8194ec755b00036c96a4a0e0667'
PIXEL_SHA = 'ca9c1187e0777087214f25d1c5e9e7db9b2a766ffb4597d97559257e141a522b'
SOLID_RECEIPT_SHA = '7f0cbfa9af8ff0e7df439c6d3d22ae8a5a9099198d302df08aac2a0c043965b9'
LUT_RECEIPT_SHA = '069d892d34f5035af9be9143e62e13d14fae2f56040179c2d084092858538286'
# kind: complete canonical function range and independently frozen source hash.
KERNELS = {
    0: (0x4e22a, 0x4e29c, '8227b32c6e4b324ed8d8309aa36278e2057f2380be196a35fc5cbb29936911f3'),
    1: (0x4e016, 0x4e0a2, 'e8f64ea788b023e894ee89411555e03e74f497fcd486ca5e958a2ba723160c0b'),
    2: (0x4e0a2, 0x4e127, '56ba96794467e8db8b7b7d0c1d647716a96b1d5c3c6cb4a4cbea52cf65287147'),
    3: (0x4e127, 0x4e1a6, 'a466fe4c6e5b855925dc413ea0adb227686799bfe0aa2c186a8d6237bd3dcb09'),
    4: (0x4e1a6, 0x4e22a, '83c06fd8bf0818bf9914ab8bb8650011821360ed47cef73bb0743f0854835d44'),
}

def sha(raw):
    return hashlib.sha256(raw).hexdigest()

def frozen(path, expected=None):
    path = Path(path).resolve()
    before = path.stat()
    raw = path.read_bytes()
    after = path.stat()
    fact = dict(path=str(path), sha256=sha(raw), size=len(raw), mtime_ns=before.st_mtime_ns)
    if (before.st_size, before.st_mtime_ns) != (after.st_size, after.st_mtime_ns) or (expected and fact['sha256'] != expected):
        raise ValueError(f'source identity mismatch: {path}')
    return raw, fact

def finish(facts, primary=None):
    after, errors = [], []
    for old in facts:
        try:
            _, current = frozen(old['path'])
            after.append(current)
            if current != old:
                errors.append(dict(path=old['path'], error='late SHA/size/mtime mismatch'))
        except (OSError, ValueError) as exc:
            after.append(dict(path=old['path'], available=False))
            errors.append(dict(path=old['path'], error=str(exc)))
    result = dict(before=facts, after=after, unchanged=not errors, late_errors=errors)
    if primary or errors:
        error = primary or ValueError(errors[0]['error'])
        error.receipt = dict(passed=False, primary_error=str(error), identity=result)
        raise error
    return result

def stream():
    data = bytearray()
    for row in range(11):
        data += bytes([0x97]) + bytes((row * 24 + col) & 255 for col in range(24))
    data += bytes([0xd7]) * 11
    data += bytes([1,0,0xc2,0x44,193,0x88,0,1,127,128,192,193,254,255,0,23,0])
    assert len(data) == 303
    return bytes(data)

def luts(resource):
    data = archive_entries(resource)[3][1]
    if data[:4] != b'LMI1' or struct.unpack_from('<H', data, 4)[0] != 23:
        raise ValueError('FDOTHER[3] LUT owner/count mismatch')
    offsets = struct.unpack_from('<23I', data, 6)
    ends = offsets[1:] + (len(data),)
    if offsets[0] < 98 or any(end - start != 256 for start, end in zip(offsets, ends)):
        raise ValueError('FDOTHER[3] LUT extent mismatch')
    return [data[a:b] for a, b in zip(offsets, ends)]

def cases(real_luts):
    result = []
    for kind in range(5):
        for corner in range(2):
            for seed in range(2):
                x, y = ((432,312) if corner else (0,0)) if kind < 3 else ((384,282) if corner else (48,42))
                result.append(dict(case=len(result),kind=kind,seed=seed,x=x,y=y,lut_index=-1))
    for index in range(23):
        for kind in (1,2):
            result.append(dict(case=len(result),kind=kind,seed=0,x=72,y=72,lut_index=index))
    for row in result:
        row['rle'] = stream().hex()
        row['lut'] = (real_luts[row['lut_index']] if row['lut_index'] >= 0 else
            bytes((i * 73 + 19) & 255 for i in range(256))).hex() if row['kind'] in (1,2) else ''
    return result

def before_page(row):
    page = bytearray((i * 13 + row['seed'] * 29 + 7) & 255 for i in range(PAGE))
    for y in range(11,22):
        for x in range(24):
            page[(row['y'] + y) * 456 + row['x'] + x] = ((y - 11) * 24 + x + row['seed'] * 37) & 255
    return bytes(page)

def original_leaf(image, row):
    import unicorn
    from unicorn.x86_const import UC_X86_REG_EIP, UC_X86_REG_ESP
    start, end, digest = KERNELS[row['kind']]
    if sha(image[start:end]) != digest:
        raise ValueError(f'complete original leaf mismatch: {start:#x}')
    cpu = unicorn.Uc(unicorn.UC_ARCH_X86, unicorn.UC_MODE_32)
    cpu.mem_map(0x4e000,0x1000)
    cpu.mem_write(start,image[start:end])
    source, table, page, stack, stop = 0x100000,0x101000,0x201000,0x308000,0x400000
    cpu.mem_map(source,0x2000)
    cpu.mem_map(0x200000,0x28000)
    cpu.mem_map(0x300000,0x10000)
    cpu.mem_map(stop,0x1000)
    payload, lut = bytes.fromhex(row['rle']), bytes.fromhex(row['lut'])
    initial = before_page(row)
    cpu.mem_write(source,payload)
    if lut: cpu.mem_write(table,lut)
    guard = bytes([109]) * 0x28000
    cpu.mem_write(0x200000,guard)
    cpu.mem_write(page,initial)
    target = page + row['y'] * 456 + row['x']
    # Only the already-proven normal cdecl pure-writer ABI. No guest I/O,
    # parent/game state, callback substitution or exceptional-state experiment.
    parameters = [stop,source,target,456]
    if row['kind'] in (1,2): parameters.append(table)
    elif row['kind']==3: parameters.append(200)
    cpu.mem_write(stack,struct.pack('<'+'I'*len(parameters),*parameters))
    cpu.reg_write(UC_X86_REG_ESP,stack)
    writes, reads = bytearray(PAGE), bytearray(PAGE)
    data_reads = set()
    def code(machine,address,size,data):
        if not start <= address < end:
            raise ValueError(f'pure leaf escaped code owner: {address:#x}')
    def memory(machine,access,address,size,value,data):
        if page <= address and address + size <= page + PAGE:
            marks = writes if access == unicorn.UC_MEM_WRITE else reads
            marks[address-page:address-page+size] = bytes([1]) * size
            return
        if stack-0x100 <= address and address+size <= stack+0x100:
            return
        if access == unicorn.UC_MEM_READ and source <= address and address+size <= source+len(payload):
            data_reads.update(range(address-source,address-source+size)); return
        if access == unicorn.UC_MEM_READ and lut and table <= address and address+size <= table+256:
            return
        raise ValueError(f'pure leaf touched undeclared data owner {address:#x}/{size}')
    def interrupt(machine,number,data):
        raise ValueError(f'guest interrupt is outside pure writer contract: {number}')
    cpu.hook_add(unicorn.UC_HOOK_CODE,code)
    cpu.hook_add(unicorn.UC_HOOK_MEM_READ | unicorn.UC_HOOK_MEM_WRITE,memory)
    cpu.hook_add(unicorn.UC_HOOK_INTR,interrupt)
    cpu.emu_start(start,stop,count=20000)
    if cpu.reg_read(UC_X86_REG_EIP) != stop or cpu.reg_read(UC_X86_REG_ESP) != stack+4:
        raise ValueError('pure writer did not return through its normal ABI')
    full = bytes(cpu.mem_read(0x200000,0x28000))
    offset = page - 0x200000
    if full[:offset] != guard[:offset] or full[offset+PAGE:] != guard[offset+PAGE:]:
        raise ValueError('original destination guard/padding changed')
    if bytes(cpu.mem_read(source,len(payload))) != payload or (lut and bytes(cpu.mem_read(table,256)) != lut):
        raise ValueError('original changed source/LUT owner')
    actual = full[offset:offset+PAGE]
    audit = dict(code_owner=[start,end],kernel_sha256=digest,original_write_bytes=sum(writes),
        original_destination_read_bytes=sum(reads),destination_read_values=sorted({initial[i] for i,v in enumerate(reads) if v}),
        source_bytes_read=len(data_reads),before_sha256=sha(initial),after_sha256=sha(actual),
        original_write_mask_sha256=sha(writes),original_destination_read_mask_sha256=sha(reads),
        source_lut_destination_guards_unchanged=True,guest_IO_executed=False)
    return (initial,actual,bytes(writes),bytes(reads)), audit

def make_original(args):
    facts = []
    def get(path, expected=None):
        raw, info = frozen(path,expected); facts.append(info); return raw
    primary, result = None, None
    try:
        import unicorn
        get(__file__)
        get(archive_entries.__code__.co_filename)
        get(sys.executable)
        get(unicorn.__file__)
        for library in (Path(unicorn.__file__).parent/'lib').glob('*.dll'):
            get(library)
        get(ROOT/'tools/ida/verify_fd2_page_lut.py','133bd23bb5fcecd94bf44898c89518a8b72f61919dd5ce43b9b6045f4f1a0eb1')
        get(ROOT/'tools/ida/verify_fd2_solid_blit.py','3a6b12b058b8f9950d4cdfb47ec8dc6931856148a1173a71d0c1a78d286789ca')
        get(args.original/'FD2.EXE',EXE_SHA)
        resource = get(args.original/'FDOTHER.DAT',OTHER_SHA)
        image = get(args.image,IMAGE_SHA)
        pixel = json.loads(get(args.pixel_source,PIXEL_SHA))
        old_lut = json.loads(get(args.lut_receipt,LUT_RECEIPT_SHA))
        old_solid = json.loads(get(args.solid_receipt,SOLID_RECEIPT_SHA))
        if not old_lut['ok'] or old_lut['case_count'] != 2 or not old_solid['ok'] or old_solid['vectors'] != 180:
            raise ValueError('inherited summary qualification mismatch')
        for kind in (0,4):
            start,end,digest = KERNELS[kind]
            found = [f for f in pixel['functions'] if f['va']==start]
            if len(found)!=1 or len(found[0]['chunks'])!=1:
                raise ValueError('missing complete pixel-source function')
            chunk = found[0]['chunks'][0]
            raw = bytes.fromhex(chunk.get('bytes',chunk.get('raw_hex','')))
            if chunk['start']!=start or chunk['end']!=end or raw!=image[start:end] or sha(raw)!=digest:
                raise ValueError('strict pixel-source chunk mismatch')
        rows, records, audits = cases(luts(resource)), [], []
        for row in rows:
            record,audit = original_leaf(image,row)
            records.append(record); audits.append(audit)
        result = dict(schema='fd2-f4-page-writer-original-v1',cases=rows,audits=audits,unicorn_version=unicorn.__version__,
            owner_bytes=PAGE,record_pages=['before','after','original_write_mask','original_destination_read_mask'],
            new_original_leaf_cases=66,inherited_solid180_reexecuted=False,
            scope='fixed valid 24x24 inputs/full page; no heap/clipping/clock/game/parent/natural qualification')
    except Exception as exc:  # include bounded-emulator/ABI failures, then late source guard
        primary=exc
    identity = finish(facts,primary)
    result['identity'] = identity
    metadata=json.dumps(result,separators=(',',':')).encode('utf8')
    destination = args.output.resolve()
    original = args.original.resolve()
    if destination==original or original in destination.parents or any(p.name.casefold()=='original_game' for p in destination.parents):
        raise ValueError('output must be outside original resources')
    with destination.open('xb') as stream_out:
        stream_out.write(HEAD.pack(MAGIC,66,len(metadata)))
        stream_out.write(metadata)
        for record in records:
            for page in record: stream_out.write(page)
    _, output = frozen(destination)
    return dict(passed=True,expected=output,identity=identity,new_original_leaf_cases=66,
        original_game_executed=False,GUI_executed=False,
        scope=result['scope'],C_actual_executed=False)

def artifact(raw):
    if len(raw)<HEAD.size: raise ValueError('truncated expected artifact')
    magic,count,length=HEAD.unpack_from(raw)
    if magic!=MAGIC or count!=66 or length>1000000 or len(raw)!=HEAD.size+length+66*PAGE*4:
        raise ValueError('expected artifact extent/domain mismatch')
    metadata=json.loads(raw[HEAD.size:HEAD.size+length])
    if metadata.get('schema')!='fd2-f4-page-writer-original-v1' or len(metadata.get('audits',[]))!=66:
        raise ValueError('expected artifact schema/count mismatch')
    if not metadata['identity']['unchanged'] or metadata['identity']['before']!=metadata['identity']['after']:
        raise ValueError('expected source guard did not pass')
    records=[];at=HEAD.size+length
    for index in range(66):
        record=[]
        for owner in range(4):
            record.append(raw[at:at+PAGE]);at+=PAGE
        audit=metadata['audits'][index]
        for page,key in zip(record,['before_sha256','after_sha256','original_write_mask_sha256','original_destination_read_mask_sha256']):
            if sha(page)!=audit[key]: raise ValueError('expected page/audit hash mismatch')
        records.append(record)
    return metadata,records

def compare_row(actual,spec,record):
    for key,value in spec.items():
        if actual.get(key)!=value: raise ValueError(f"case{spec['case']} input mismatch: {key}")
    if actual.get('inputs_unchanged') is not True:
        raise ValueError(f"case{spec['case']} C source/LUT/unit owner changed")
    for key,expected in zip(('before_hex','after_hex'),record[:2]):
        measured=bytes.fromhex(actual[key])
        if len(measured)!=PAGE: raise ValueError(f"case{spec['case']} {key} exact extent mismatch")
        if measured!=expected:
            at=next(i for i,(a,b) in enumerate(zip(measured,expected)) if a!=b)
            raise ValueError(f"case{spec['case']} {key} first mismatch {at}: {measured[at]} != {expected[at]}")

def verify_actual(args):
    facts=[]; primary=None; result=None
    try:
        for path in [Path(__file__),Path(archive_entries.__code__.co_filename),args.producer]+args.producer_source:
            _,f=frozen(path);facts.append(f)
        raw,f=frozen(args.expected,args.expected_sha256);facts.append(f)
        expected_digest=f['sha256']
        metadata,records=artifact(raw)
        raw,f=frozen(args.original/'FDOTHER.DAT',OTHER_SHA);facts.append(f)
        rows=cases(luts(raw))
        if metadata['cases']!=rows: raise ValueError('expected parameters differ from independent fixed domain')
        if args.actual.stat().st_size>45000000: raise ValueError('actual JSONL exceeds fixed byte budget')
        raw,f=frozen(args.actual);facts.append(f)
        actual=[json.loads(line) for line in raw.decode('utf-8-sig').splitlines() if line.strip()]
        if len(actual)!=66: raise ValueError('actual must contain exactly 66 ordered cases')
        for row,spec,record in zip(actual,rows,records): compare_row(row,spec,record)
        result=dict(passed=True,case_count=66,full_pages_compared=132,pixel_bytes_compared=66*PAGE*2,
            expected_sha256=expected_digest,actual_C_write_mask_observed=False,
            scope='C full before/after pages match saved original leaves; C write mask has static source qualification only; no game/heap/clipping/clock/natural qualification')
    except (OSError,ValueError,KeyError,IndexError,UnicodeError) as exc:
        primary=exc
    identity=finish(facts,primary)
    result['identity']=identity
    return result

def main():
    parser=argparse.ArgumentParser(description=__doc__)
    sub=parser.add_subparsers(dest='mode',required=True)
    original=sub.add_parser('original')
    original.add_argument('--image',type=Path,required=True)
    original.add_argument('--pixel-source',type=Path,required=True)
    original.add_argument('--lut-receipt',type=Path,required=True)
    original.add_argument('--solid-receipt',type=Path,required=True)
    original.add_argument('--original',type=Path,default=ROOT/'original_game')
    original.add_argument('--output',type=Path,required=True)
    verify=sub.add_parser('verify')
    verify.add_argument('--expected',type=Path,required=True)
    verify.add_argument('--expected-sha256',required=True)
    verify.add_argument('--actual',type=Path,required=True)
    verify.add_argument('--producer',type=Path,required=True)
    verify.add_argument('--producer-source',type=Path,action='append',required=True)
    verify.add_argument('--original',type=Path,default=ROOT/'original_game')
    args=parser.parse_args()
    try:
        result=make_original(args) if args.mode=='original' else verify_actual(args)
        print(json.dumps(result,ensure_ascii=False,indent=2))
    except (OSError,ValueError,KeyError,IndexError,RuntimeError) as exc:
        print(json.dumps(getattr(exc,'receipt',dict(passed=False,primary_error=str(exc))),ensure_ascii=False,indent=2))
        print(f'F4页writer核验失败：{exc}',file=sys.stderr);return 1
    return 0

if __name__=='__main__': raise SystemExit(main())
