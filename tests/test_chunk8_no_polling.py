"""Chunk 8 (PLAN.md): test_no_timer_polling.

The list refresh is event-driven (QFileSystemWatcher + event filter). The only timers
allowed in the plugin are the two that already exist in plugin-main.cpp for the start
delay (1500 ms) and the gap between outputs (400 ms), which PLAN.md keeps as they are.
"""
import re
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
SRC = ROOT / "plugin" / "src"

# Recorded from plugin-main.cpp before Chunk 8 (start delay + output gap timers only).
ALLOWED_PLUGIN_MAIN_TIMER_LINES = [
    "#include <QTimer>",
    "QTimer *g_start_timer = nullptr;",
    "QTimer *g_output_timer = nullptr;",
    "g_output_timer = new QTimer(window);",
    "QObject::connect(g_output_timer, &QTimer::timeout, [] { click_next_output(); });",
    "g_start_timer = new QTimer(window);",
    "QObject::connect(g_start_timer, &QTimer::timeout, [] {",
]
POLLING = re.compile(r"QTimer|QBasicTimer|startTimer\s*\(|QThread::(m|u)?sleep|this_thread::sleep|\bSleep\s*\(|timerEvent")


def test_no_timer_polling():
    found = {}
    for path in sorted(SRC.glob("*")):
        if path.suffix.lower() not in {".cpp", ".hpp", ".h"}:
            continue
        lines = [line.strip() for line in path.read_text(encoding="utf-8").splitlines()
                 if POLLING.search(line)]
        if lines:
            found[path.name] = lines
    assert found == {"plugin-main.cpp": ALLOWED_PLUGIN_MAIN_TIMER_LINES}
