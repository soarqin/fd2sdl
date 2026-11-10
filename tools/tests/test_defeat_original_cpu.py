"""完整原1db65 CPU工具的来源拒绝；实际CPU阳性另由带unicorn命令取得。"""
import hashlib
import importlib.util
from pathlib import Path
import sys
import unittest
from unittest.mock import patch

ROOT = Path(__file__).resolve().parents[2]
spec = importlib.util.spec_from_file_location('defeat_original_cpu', ROOT/'tools/ida/verify_fd2_defeat_flow.py')
cpu = importlib.util.module_from_spec(spec); spec.loader.exec_module(cpu)


class DefeatSourceGuardTest(unittest.TestCase):
    def test_wrong_whole_image_rejected_before_execution(self):
        with patch.object(cpu, 'execute') as execute:
            with self.assertRaisesRegex(ValueError, '镜像身份'): cpu.check(bytes(0x64000), b'')
            execute.assert_not_called()

    def test_even_whole_image_identity_does_not_replace_function_guard(self):
        image = bytes(0x64000)
        with patch.object(cpu, 'IMAGE_SHA', hashlib.sha256(image).hexdigest()), patch.object(cpu, 'execute') as execute:
            with self.assertRaisesRegex(ValueError, '完整1db65字节'): cpu.check(image, b'')
            execute.assert_not_called()

    def test_bad_container_rejected_after_both_source_guards(self):
        image = bytes(0x64000)
        with patch.object(cpu, 'IMAGE_SHA', hashlib.sha256(image).hexdigest()), \
                patch.object(cpu, 'FUNCTION_SHA', hashlib.sha256(image[0x1db65:0x1debe]).hexdigest()), \
                patch.object(cpu, 'execute') as execute:
            with self.assertRaisesRegex(ValueError, 'FDOTHER容器'): cpu.check(image, bytes(200))
            execute.assert_not_called()


if __name__ == '__main__': unittest.main()
