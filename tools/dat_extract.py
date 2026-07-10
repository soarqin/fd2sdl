#!/usr/bin/env python3
"""DAT 归档解包工具（炎龙骑士团 2）

原版 .DAT 文件共享 "LLLLLL" 容器格式：
  [0:6]   magic "LLLLLL"
  [6:]    u32 偏移流，严格递增，遇越界/非递增即终止
  末条目至文件末尾

用法:
  python dat_extract.py list <file.dat>           列出条目
  python dat_extract.py extract <file.dat> <outdir>  解包到目录
  python dat_extract.py dump <file.dat> <index>    打印某条目头部
"""
import struct
import os
import sys


def parse_archive(data: bytes):
    """解析 .DAT 容器，返回 (offsets, entries)。"""
    if data[:6] != b"LLLLLL":
        raise ValueError(f"bad magic: {data[:6]!r} (expected b'LLLLLL')")
    fsize = len(data)
    offsets = []
    i = 0
    while True:
        pos = 6 + i * 4
        if pos + 4 > fsize:
            break
        o = struct.unpack("<I", data[pos:pos + 4])[0]
        if o >= fsize:  # 越界
            break
        if offsets and o <= offsets[-1]:  # 非递增
            break
        offsets.append(o)
        i += 1
    entries = []
    for idx, o in enumerate(offsets):
        nxt = offsets[idx + 1] if idx + 1 < len(offsets) else fsize
        entries.append(data[o:nxt])
    return offsets, entries


def cmd_list(path):
    with open(path, "rb") as f:
        data = f.read()
    offsets, entries = parse_archive(data)
    print(f"{os.path.basename(path)}: {len(entries)} entries, {len(data)} bytes")
    print(f"{'idx':>4} {'offset':>10} {'size':>10}  head")
    for i, (o, e) in enumerate(zip(offsets, entries)):
        head = e[:16].hex(" ")
        print(f"{i:>4} {o:#10x} {len(e):>10}  {head}")


def cmd_extract(path, outdir):
    with open(path, "rb") as f:
        data = f.read()
    offsets, entries = parse_archive(data)
    os.makedirs(outdir, exist_ok=True)
    base = os.path.splitext(os.path.basename(path))[0]
    for i, e in enumerate(entries):
        out = os.path.join(outdir, f"{base}_{i:04d}.bin")
        with open(out, "wb") as f:
            f.write(e)
    print(f"extracted {len(entries)} entries to {outdir}/")


def cmd_dump(path, index):
    with open(path, "rb") as f:
        data = f.read()
    offsets, entries = parse_archive(data)
    if index < 0 or index >= len(entries):
        sys.exit(f"index out of range (0..{len(entries)-1})")
    e = entries[index]
    print(f"entry {index}: offset={offsets[index]:#x} size={len(e)}")
    print(f"head 32 bytes: {e[:32].hex(' ')}")
    # 尝试图像头
    if len(e) >= 4:
        w, h = struct.unpack("<HH", e[:4])
        if 0 < w <= 1024 and 0 < h <= 1024:
            ratio = len(e) / (w * h) if w and h else 0
            print(f"  image? {w}x{h}, ratio={ratio:.3f} (1.0=raw, <1.0=RLE)")
    # 尝试 IFF
    if e[:4] == b"FORM":
        print(f"  IFF container, type={e[8:16]}")


def main():
    if len(sys.argv) < 3:
        print(__doc__)
        sys.exit(1)
    cmd = sys.argv[1]
    if cmd == "list":
        cmd_list(sys.argv[2])
    elif cmd == "extract":
        cmd_extract(sys.argv[2], sys.argv[3])
    elif cmd == "dump":
        cmd_dump(sys.argv[2], int(sys.argv[3]))
    else:
        print(__doc__)
        sys.exit(1)


if __name__ == "__main__":
    main()
