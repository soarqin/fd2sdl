"""独立验证原格式存档的容器和持久队伍数量。"""

def decode_save(path):
    """仅检查已登记的 XOR/checksum 与活动快照；不构造原版状态。"""
    data = bytearray(path.read_bytes())
    if len(data) != 0x59cb:
        raise AssertionError("save size is not 0x59cb")
    state = 0xa5
    for index in range(len(data)):
        state = (state + 0x9014) & 0xffff
        state = ((state << 3) | (state >> 13)) & 0xffff
        data[index] ^= state & 0xff
    if sum(data[:-4]) != int.from_bytes(data[-4:], "little"):
        raise AssertionError("save checksum mismatch")
    return bytes(data)


def check_roster_count(plain):
    """meta[9]是独立持续队伍表数量，不是活动actor数量的副本。"""
    count = plain[0x30cc]
    if count > 32:
        raise AssertionError("persistent roster count exceeds 32")
    return count


def terminal_save_fixture(plain, outcome):
    """编码受控终态：只改已确认隐藏位和checksum；运行证据须由对应场景另验。"""
    if len(plain) != 0x59cb or sum(plain[:-4]) != int.from_bytes(plain[-4:], "little"):
        raise ValueError("terminal fixture requires a valid plaintext save")
    count = plain[0x30c4]
    if plain[0x30c5] != 0 or not 0 < count <= 64:
        raise ValueError("terminal fixture requires a supported stage 0 snapshot")
    if outcome not in ("victory", "defeat"):
        raise ValueError("unknown terminal outcome")
    data = bytearray(plain)
    targets = [index for index in range(count)
               if outcome == "victory" and data[0x12a3 + index * 0x50 + 6] == 0]
    if outcome == "defeat":
        if data[0x12a3 + 6] != 2:
            raise ValueError("terminal fixture leader is not a player actor")
        targets = [0]
    if not targets:
        raise ValueError("terminal fixture has no actors for the requested outcome")
    for index in targets:
        data[0x12a3 + index * 0x50 + 5] |= 1
    data[-4:] = sum(data[:-4]).to_bytes(4, "little")
    state = 0xa5
    for index in range(len(data)):
        state = (state + 0x9014) & 0xffff
        state = ((state << 3) | (state >> 13)) & 0xffff
        data[index] ^= state & 0xff
    return bytes(data)
