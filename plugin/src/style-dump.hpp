#pragma once

class QWidget;

// Logs, once, how OBS's theme styled the dock at runtime: each group and row (Qt class,
// objectName, theme class, font size) next to OBS's own Audio Mixer and Controls widgets,
// and whether the rows match the Audio Mixer's font size.
void log_style_dump(QWidget *dock);
