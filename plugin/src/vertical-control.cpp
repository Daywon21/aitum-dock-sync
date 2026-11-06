#include "vertical-control.hpp"

#include <cassert>
#include <cstring>

#include <obs-module.h>
#include <obs.h>

#include "obs-websocket-api.h"

#include <QHash>
#include <QJsonArray>
#include <QMetaObject>
#include <QObject>
#include <QJsonDocument>
#include <QJsonObject>

namespace {

constexpr unsigned int kRequestSuccess = 100; // obs-websocket RequestStatus::Success

// Calls an Aitum Vertical proc for one output on the first vertical canvas (width/height 0).
// Returns false when Aitum Vertical is not loaded. *output (if asked for) must be released.
bool call_vertical_proc(const char *proc, const QString &output_name, obs_output_t **output)
{
	proc_handler_t *ph = obs_get_proc_handler();
	if (!ph)
		return false;

	const QByteArray name = output_name.toUtf8();
	calldata_t cd;
	calldata_init(&cd);
	calldata_set_int(&cd, "width", 0);
	calldata_set_int(&cd, "height", 0);
	calldata_set_string(&cd, "name", name.constData());
	const bool called = proc_handler_call(ph, proc, &cd);
	if (output)
		*output = called ? static_cast<obs_output_t *>(calldata_ptr(&cd, "output")) : nullptr;
	calldata_free(&cd);
	return called;
}

} // namespace

QString vertical_pick_key(const QString &output_name)
{
	return QStringLiteral("[Vertical] ") + output_name;
}

QStringList load_vertical_output_names()
{
	obs_data_t *request =
		obs_data_create_from_json(R"({"vendorName": "aitum-multistream", "requestType": "get_outputs"})");
	obs_websocket_request_response *response = obs_websocket_call_request("CallVendorRequest", request);
	obs_data_release(request);
	if (!response)
		return {};

	QStringList names;
	if (response->status_code == kRequestSuccess && response->response_data) {
		const QJsonObject root = QJsonDocument::fromJson(QByteArray(response->response_data)).object();
		const QJsonArray outputs =
			root.value(QStringLiteral("responseData")).toObject().value(QStringLiteral("outputs")).toArray();
		for (const QJsonValue &value : outputs) {
			const QJsonObject output = value.toObject();
			if (!output.value(QStringLiteral("vertical")).toBool(false))
				continue;
			const QString name = output.value(QStringLiteral("name")).toString();
			if (!name.isEmpty())
				names.push_back(name);
		}
	}
	obs_websocket_request_response_free(response);
	return names;
}

namespace {

enum class State { Idle, Connecting, Live };

struct Entry {
	obs_weak_output_t *weak = nullptr;
	const void *identity = nullptr; // the output's address: only compared, never dereferenced
	bool known_null = false;        // seen without an output object at the last look (not: never looked)
	bool connecting = false;
	bool want_stopped = false;
};

struct Hook {
	QString name;
};

QObject *g_context = nullptr;
QHash<QString, Entry> g_entries;
QHash<QString, Hook *> g_hooks;

void on_signal(const QString &name, const char *signal);

void post_signal(void *data, const char *signal)
{
	QObject *context = g_context;
	if (!context || !data)
		return;
	const QString name = static_cast<Hook *>(data)->name;
	// libobs emits from the output's thread; handle it on the UI thread.
	QMetaObject::invokeMethod(context, [name, signal] { on_signal(name, signal); }, Qt::QueuedConnection);
}

void signal_starting(void *data, calldata_t *)
{
	post_signal(data, "starting");
}

void signal_start(void *data, calldata_t *)
{
	post_signal(data, "start");
}

void signal_stop(void *data, calldata_t *)
{
	post_signal(data, "stop");
}

void connect_signals(obs_output_t *output, Hook *hook, bool connect)
{
	signal_handler_t *handler = obs_output_get_signal_handler(output);
	if (!handler)
		return;
	if (connect) {
		signal_handler_connect(handler, "starting", signal_starting, hook);
		signal_handler_connect(handler, "start", signal_start, hook);
		signal_handler_connect(handler, "stop", signal_stop, hook);
	} else {
		signal_handler_disconnect(handler, "starting", signal_starting, hook);
		signal_handler_disconnect(handler, "start", signal_start, hook);
		signal_handler_disconnect(handler, "stop", signal_stop, hook);
	}
}

void unhook(const QString &name, Entry &entry)
{
	if (!entry.weak)
		return;
	if (obs_output_t *output = obs_weak_output_get_output(entry.weak)) {
		connect_signals(output, g_hooks.value(name, nullptr), false);
		obs_output_release(output);
	}
	obs_weak_output_release(entry.weak);
	entry.weak = nullptr;
	entry.identity = nullptr;
}

// Looks the output up and hooks its signals when it is new. Returns whether it is live.
// *available is false when Aitum Vertical is not loaded.
bool discover(const QString &name, bool *available = nullptr)
{
	obs_output_t *output = nullptr;
	const bool called = call_vertical_proc("aitum_vertical_get_stream_output", name, &output);
	if (available)
		*available = called;
	Entry &entry = g_entries[name];
	if (!output) {
		unhook(name, entry);
		entry.known_null = true;
		entry.connecting = false;
		return false;
	}

	const bool active = obs_output_active(output);
	if (entry.identity != output) {
		unhook(name, entry);
		if (!g_hooks.contains(name))
			g_hooks.insert(name, new Hook{name});
		entry.weak = obs_output_get_weak_output(output);
		entry.identity = output;
		if (g_context)
			connect_signals(output, g_hooks.value(name), true);
		// Aitum Vertical creates an output only when it starts it: new and not live yet = connecting.
		entry.connecting = !active && entry.known_null;
	}
	if (active)
		entry.connecting = false;
	entry.known_null = false;
	obs_output_release(output);
	return active;
}

State state_of(const QString &name, bool *available)
{
	if (discover(name, available))
		return State::Live;
	return g_entries.value(name).connecting ? State::Connecting : State::Idle;
}

void stop_now(const QString &name)
{
	call_vertical_proc("aitum_vertical_stop_stream_output", name, nullptr);
	Entry &entry = g_entries[name];
	entry.connecting = false;
	entry.want_stopped = false;
	blog(LOG_INFO, "[aitum-dock-sync] Stopped vertical '%s' (previous state: active).", name.toUtf8().constData());
}

void on_signal(const QString &name, const char *signal)
{
	Entry &entry = g_entries[name];
	if (std::strcmp(signal, "starting") == 0) {
		entry.connecting = true;
	} else if (std::strcmp(signal, "start") == 0) {
		entry.connecting = false;
		if (entry.want_stopped)
			stop_now(name);
	} else {
		entry.connecting = false;
		entry.want_stopped = false;
	}
}

} // namespace

void vertical_tracker_start()
{
	if (!g_context)
		g_context = new QObject();
}

void vertical_tracker_stop()
{
	for (auto it = g_entries.begin(); it != g_entries.end(); ++it)
		unhook(it.key(), it.value());
	g_entries.clear();
	delete g_context;
	g_context = nullptr;
	qDeleteAll(g_hooks);
	g_hooks.clear();
}

void vertical_watch(const QStringList &output_names)
{
	for (const QString &name : output_names)
		discover(name);
}

QStringList vertical_live_outputs(const QStringList &output_names)
{
	QStringList live;
	for (const QString &name : output_names)
		if (discover(name))
			live.push_back(name);
	return live;
}

bool vertical_output_active(const QString &output_name)
{
	return discover(output_name);
}

bool set_vertical_output(const QString &output_name, bool start)
{
	const QByteArray name = output_name.toUtf8();
	bool available = false;
	const State state = state_of(output_name, &available);
	if (!available) {
		blog(LOG_WARNING, "[aitum-dock-sync] Aitum Vertical not found; cannot %s vertical '%s'.",
		     start ? "start" : "stop", name.constData());
		return false;
	}
	Entry &entry = g_entries[output_name];

	if (start) {
		entry.want_stopped = false;
		if (state == State::Live) {
			blog(LOG_INFO, "[aitum-dock-sync] Vertical '%s' is already active.", name.constData());
			return true;
		}
		if (state == State::Connecting) {
			blog(LOG_INFO, "[aitum-dock-sync] Vertical '%s' is already starting.", name.constData());
			return true;
		}
		call_vertical_proc("aitum_vertical_start_stream_output", output_name, nullptr);
		if (!discover(output_name))
			g_entries[output_name].connecting = true;
		blog(LOG_INFO, "[aitum-dock-sync] Started vertical '%s' (previous state: inactive).", name.constData());
		return true;
	}

	if (state == State::Live) {
		stop_now(output_name);
	} else if (state == State::Connecting) {
		entry.want_stopped = true;
		blog(LOG_INFO, "[aitum-dock-sync] Vertical '%s' is connecting; it will be stopped when it goes live.",
		     name.constData());
	} else {
		blog(LOG_INFO, "[aitum-dock-sync] Vertical '%s' is already inactive.", name.constData());
	}
	return true;
}
