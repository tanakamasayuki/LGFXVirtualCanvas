"""Shared pytest hooks for LGFXVirtualCanvas tests.

Wipes the per-test `output/` directory before each test so host-profile
artifacts (PNG captures) don't leak across runs and a stale file can't
make a failing sketch look like it passed.

Note: this is only the output-wipe hook. The host-arduino-core conftest
also has a session-scoped symlink fixture for its develop-against-the-
working-tree workflow; we intentionally do NOT copy that here.
"""

import shutil
from pathlib import Path


def pytest_runtest_setup(item):
    output_dir = Path(item.fspath).parent / "output"
    if output_dir.exists():
        shutil.rmtree(output_dir)
