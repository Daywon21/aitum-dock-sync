#pragma once

#include <QString>
#include <QStringList>

// Aitum Vertical outputs.
//  - List: Aitum Multistream's obs-websocket vendor request aitum-multistream/get_outputs
//    (entries with "vertical": true), called in-process through obs-websocket-api.h.
//    Only each entry's "name" and "vertical" fields are read.
//  - Control and state: Aitum Vertical's procs aitum_vertical_start_stream_output,
//    aitum_vertical_stop_stream_output and aitum_vertical_get_stream_output.

// picks.json key for a vertical output: "[Vertical] <name>".
QString vertical_pick_key(const QString &output_name);
// Empty when Aitum Multistream / obs-websocket is not available.
QStringList load_vertical_output_names();
bool vertical_output_active(const QString &output_name);
// Starts or stops the vertical output, following its real state (idle / connecting / live):
//  - start: only when idle (an output Aitum Vertical is already connecting is left alone);
//  - stop: now when live; when still connecting (libobs ignores a stop then), as soon as it goes live.
bool set_vertical_output(const QString &output_name, bool start);

// Vertical output tracking through libobs output signals (starting / start / stop), using weak
// references. Started in obs_module_load, stopped in obs_module_unload.
void vertical_tracker_start();
void vertical_tracker_stop();
// Looks up the outputs (Aitum Vertical creates them on first start) and follows their signals.
void vertical_watch(const QStringList &output_names);
// The outputs that are live right now.
QStringList vertical_live_outputs(const QStringList &output_names);
