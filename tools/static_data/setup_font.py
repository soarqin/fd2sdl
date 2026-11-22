"""Build the launcher's bitmap atlas from the licensed GNU Unifont HEX subset."""
import argparse
from pathlib import Path
import re


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("font", type=Path)
    parser.add_argument("output", type=Path)
    parser.add_argument("sources", nargs="+", type=Path)
    parser.add_argument("--hex", action="store_true", help="extract a new source subset")
    args = parser.parse_args()
    needed = set(range(32, 127))
    tokens = re.compile(r'//[^\n]*|/\*.*?\*/|"(?:\\.|[^"\\])*"', re.S)
    for source in args.sources:
        for match in tokens.finditer(source.read_text(encoding="utf-8")):
            token = match.group()
            if token.startswith('"'):
                needed.update(ord(c) for c in token if ord(c) >= 128)
    glyphs = {}
    for line in args.font.read_text(encoding="ascii").splitlines():
        code, bitmap = line.split(":")
        key = int(code, 16)
        if key in needed:
            if len(bitmap) not in (32, 64) or not re.fullmatch("[0-9a-fA-F]+", bitmap):
                raise ValueError(f"invalid glyph U+{key:04X}")
            glyphs[key] = bitmap
    if missing := needed - glyphs.keys():
        raise ValueError("missing setup characters: " + "".join(map(chr, sorted(missing))))
    lines = []
    for code, bitmap in sorted(glyphs.items()):
        if args.hex:
            lines.append(f"{code:04X}:{bitmap}")
        else:
            n = len(bitmap) // 16
            rows = ", ".join("0x" + bitmap[i:i+n] for i in range(0, len(bitmap), n))
            lines.append(f"{{0x{code:04x}, {n*4}, {{{rows}}}}},")
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_bytes(("\n".join(lines) + "\n").encode("ascii"))


if __name__ == "__main__":
    main()
