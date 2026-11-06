"""Chunk 10 (Captain Dark 2026-11-02): never read or log stream_key.

The plugin source must not name the stream_key field, and must not call
aitum_vertical_get_stream_settings (that proc returns every vertical output's stream key).
"""
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
SRC = ROOT / "plugin" / "src"


def test_plugin_source_never_reads_stream_key():
    hits = []
    for path in sorted(SRC.glob("*")):
        if path.suffix.lower() not in {".cpp", ".hpp", ".h"}:
            continue
        for n, line in enumerate(path.read_text(encoding="utf-8").splitlines(), 1):
            low = line.lower()
            if "stream_key" in low or "streamkey" in low or "aitum_vertical_get_stream_settings" in low:
                hits.append(f"{path.name}:{n}: {line.strip()}")
    assert hits == []
