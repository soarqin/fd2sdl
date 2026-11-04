#!/usr/bin/env python3
"""只读移动成本oracle：复用既有LE loader，按canonical VA提取已确认580字节。

field_movement_profile_get @0x4e8a5；object3 canonical 0x61646..0x6188a。
不应用fixup、不写镜像或原版；不将历史runtime/file地址当提取入口。
"""
import hashlib
import struct
from pathlib import Path

from rebuild_fd2_analysis import build_relbase_image, parse_le, parse_objects, page_number_from_map_entry

MOVEMENT_VA = 0x61646
PROFILE_COUNT = 29
COST_CLASS_COUNT = 20
TABLE_SIZE = PROFILE_COUNT * COST_CLASS_COUNT
# 已发布FD2 canonical objects最末segment页界；只限制本oracle，不改通用loader。
MAX_IMAGE_SIZE = 0x64000
TABLE_SHA256 = "4002a997ddd7c1e036461c7ee2b915560587f1653aae6c5978bd47d84f533935"


def movement_table(executable):
    data = Path(executable).read_bytes()
    try:
        le = parse_le(data)
        if not 0 < le.page_size <= MAX_IMAGE_SIZE or le.num_pages <= 0 or le.objcnt <= 0:
            raise ValueError("LE对象/分页尺寸无效")
        if data[le.le_off + 2:le.le_off + 4] != b"\0\0":
            raise ValueError("移动表oracle只支持小端LE")
        if (le.le_off + le.objtab_off + le.objcnt * 24 > len(data) or
                le.le_off + le.objmap_off + le.num_pages * 4 > len(data)):
            raise ValueError("LE对象/页目录截断")
        objects = parse_objects(data, le)
        for obj in objects:
            if obj.relbase + obj.vsize > MAX_IMAGE_SIZE:
                raise ValueError("LE对象越出本作已确认canonical镜像界，拒绝大分配")
            if obj.page_count and not 1 <= obj.page_map_index <= obj.page_map_index + obj.page_count - 1 <= le.num_pages:
                raise ValueError("LE对象文件页范围越界")
        owners = [obj for obj in objects if obj.relbase <= MOVEMENT_VA and MOVEMENT_VA + TABLE_SIZE <= obj.relbase + obj.vsize]
        if len(owners) != 1:
            raise ValueError("移动表不在唯一LE对象的完整virtual范围内")
        owner = owners[0]
        start = MOVEMENT_VA - owner.relbase
        first_page, last_page = start // le.page_size, (start + TABLE_SIZE - 1) // le.page_size
        if first_page < 0 or last_page >= owner.page_count:
            raise ValueError("移动表跨出LE文件映射页，不能把BSS当成本表")
        pages = []
        for page in range(first_page, last_page + 1):
            map_index = owner.page_map_index - 1 + page
            entry_at = le.le_off + le.objmap_off + map_index * 4
            entry = data[entry_at:entry_at + 4]
            if not 0 <= map_index < le.num_pages or len(entry) != 4 or entry[3] != 0:
                raise ValueError("移动表引用无效/未支持的LE页")
            page_number = page_number_from_map_entry(entry)
            if not 1 <= page_number <= le.num_pages:
                raise ValueError("移动表必须来自已初始化LE文件页")
            offset_in_page = start % le.page_size if page == first_page else 0
            file_offset = le.module_file_off + le.data_pages_off + (page_number - 1) * le.page_size + offset_in_page
            pages.append({"object_page": page, "page_number": page_number, "file_offset": file_offset})
        image = build_relbase_image(data, le, objects)
        table = bytes(image[MOVEMENT_VA:MOVEMENT_VA + TABLE_SIZE])
    except (IndexError, OverflowError, struct.error) as error:
        raise ValueError("移动表LE结构截断或越界") from error
    digest = hashlib.sha256(table).hexdigest()
    if len(table) != TABLE_SIZE or digest != TABLE_SHA256:
        raise ValueError("移动表580字节与已确认canonical表SHA256不符")
    return table, {"canonical_va": MOVEMENT_VA, "size": TABLE_SIZE, "sha256": digest,
                   "executable_sha256": hashlib.sha256(data).hexdigest(), "object": owner.index,
                   "object_relbase": owner.relbase, "object_offset": start, "pages": pages,
                   "source": "parse_le/parse_objects/build_relbase_image raw-unrelocated"}


def movement_profile(executable, profile_id):
    if not isinstance(profile_id, int) or not 0 <= profile_id < PROFILE_COUNT:
        raise ValueError("移动profile索引必须在0..28")
    table, evidence = movement_table(executable)
    start = profile_id * COST_CLASS_COUNT
    return table[start:start + COST_CLASS_COUNT], dict(evidence, selected_profile=profile_id)
