#!/usr/bin/env python3
"""G2 cache/身份写的只读Domain证据；由技能run.py提供db后调用collect。

输出含原机器码，仅存ignored目录，不保存数据库。验证复用严格LE字节核对，
不是按伪C或相邻函数猜测原Load合同。
"""
import hashlib
import json
from pathlib import Path

from g1_preparation_evidence import function_record, verify_bytes

TARGETS = (0x10010, 0x11019, 0x2548C, 0x34DD0)


def collect(db, output, strict_image):
    report = {"schema": "fd2-g2-cache-load-domain-v1",
              "functions": [function_record(db, ea) for ea in TARGETS], "tables": []}
    report["strict_check"] = verify_bytes(report, Path(strict_image).read_bytes(), TARGETS)
    output = Path(output)
    output.parent.mkdir(parents=True, exist_ok=True)
    output.write_text(json.dumps(report, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
    return {"output": str(output), "sha256": hashlib.sha256(output.read_bytes()).hexdigest(),
            "strict_check": report["strict_check"]}
