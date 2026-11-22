"""Run file and junction checks in a fresh, disposable working directory."""
import os
from pathlib import Path
import subprocess
import sys
import tempfile

with tempfile.TemporaryDirectory() as directory:
    root = Path(directory)
    (root / "resources").mkdir()
    (root / "resources" / "keep").write_bytes(b"original")
    (root / "original_game").mkdir()
    if os.name == "nt":
        subprocess.run(["cmd", "/d", "/c", "mklink", "/J", "alias", "resources"],
                       cwd=root, check=True)
    else:
        (root / "alias").symlink_to("resources", target_is_directory=True)
    subprocess.run([sys.argv[1]], cwd=root, check=True)
