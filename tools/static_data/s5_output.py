"""S5显式授权的本地证据盘；仍拒覆盖、原资源和junction穿越。"""
from pathlib import Path
from stage_source_ledger import validate_output as project_output


def validate_output(output,original):
    output=Path(output).resolve()
    evidence=Path('F:/fd2sdl-evidence/s5-20261114').resolve()
    source=Path(original).resolve()
    if output!=evidence and output.is_relative_to(evidence):
        if (output.exists() or output.is_relative_to(source) or source.is_relative_to(output)):
            raise ValueError('S5 evidence requires new output outside original source')
        return output
    return project_output(output,original=original)
