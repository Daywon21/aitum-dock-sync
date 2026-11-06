"""Chunk 11 (Captain Dark 2026-11-02): the dock uses OBS theme colors and fonts only.

platform-dock.{hpp,cpp} must not set its own style sheet, font, palette or colors; it may
only use OBS's theme classes (setProperty("class", ...)).
"""
import re
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
FILES = [ROOT / "plugin" / "src" / "platform-dock.hpp", ROOT / "plugin" / "src" / "platform-dock.cpp"]
CUSTOM_STYLE = re.compile(
    r"setStyleSheet|setFont\s*\(|setPointSize|setPixelSize|setPalette|QPalette|QColor|"
    r"font-size|font-weight|color\s*:|rgba?\s*\(|#[0-9A-Fa-f]{6}\b|#[0-9A-Fa-f]{3}\b|Qt::(red|green|white|black|gray|darkRed)")


def test_dock_source_has_no_custom_style():
    hits = []
    for path in FILES:
        for n, line in enumerate(path.read_text(encoding="utf-8").splitlines(), 1):
            if CUSTOM_STYLE.search(line):
                hits.append(f"{path.name}:{n}: {line.strip()}")
    assert hits == []
