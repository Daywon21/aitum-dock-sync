#pragma once
// More fakes at the OBS boundary (Chunk 10): libobs proc handler / calldata / obs_output /
// obs_data (just what the plugin and the official obs-websocket-api.h use), plus stand-ins for
// Aitum Vertical's proc handlers and the obs-websocket in-process API with Aitum Multistream's
// "aitum-multistream" vendor (get_outputs).
#include <QObject>
#include <QPair>
#include <QString>
#include <QStringList>
#include <QVector>

namespace FakeObsApi {
// Removes every registered fake proc (global proc handler and obs-websocket's).
void reset();
} // namespace FakeObsApi

// Aitum Vertical: registers aitum_vertical_get_stream_output / start_stream_output /
// stop_stream_output / get_stream_settings on the global proc handler.
class FakeAitumVertical : public QObject {
	Q_OBJECT
public:
	void install();
	void add_output(const QString &name, bool active);
	bool active(const QString &name) const;
	int refs(const QString &name) const; // obs_output references currently held (1 = only Aitum's own)

	QStringList calls;          // "start:<name>" / "stop:<name>", in order
	int state_queries = 0;      // aitum_vertical_get_stream_output calls
	int settings_calls = 0;     // aitum_vertical_get_stream_settings calls (returns stream keys)

	// ---- Chunk 13 additions (opt-in; off by default, so earlier tests behave exactly as before) ----
	// Realistic libobs + Aitum Vertical behavior (obs-output.c, vertical-canvas.cpp):
	//  - start: the output is created on first start (lazy outputs), goes "connecting"
	//    (obs_output_active() false) and emits "starting"; a start while connecting or live is counted
	//    in double_starts (Aitum Vertical's StartStreamOutput has no guard);
	//  - stop: ignored unless live (obs_output_stop returns early), counted in ignored_stops;
	//    when live: inactive + "stop" signal.
	void set_realistic(bool on);
	void add_lazy_output(const QString &name); // Aitum Vertical knows it; obs_output created on first start
	void connect_output(const QString &name);  // connecting -> live: active + "activate" + "start" signals
	// Aitum Vertical's "Start streaming with main" (streaming_match_main): on main stream start it starts
	// every output itself (queued, like Vertical); on main stream stop it stops the live ones.
	// Registers a frontend callback, so call it after obs_module_load (Vertical loads after Dock Sync).
	void set_streaming_match_main(bool on);
	bool exists(const QString &name) const;
	bool connecting(const QString &name) const;
	QStringList auto_calls;   // Aitum Vertical's own: "auto-start:<name>" / "auto-stop:<name>"
	int double_starts = 0;
	int ignored_stops = 0;
	bool realistic = false;
	bool match_main = false;

signals:
	void called(const QString &call);
};

// obs-websocket in-process API + the "aitum-multistream" vendor's get_outputs.
// Each output entry also carries a "stream_key" field, so tests can prove the plugin
// never reads or logs it.
class FakeAitumMultistreamVendor {
public:
	static constexpr const char *kSecret = "SECRET-STREAM-KEY-7f3a";
	void install();
	void set_outputs(const QVector<QPair<QString, bool>> &outputs); // name, vertical
	int get_outputs_calls = 0;
};
