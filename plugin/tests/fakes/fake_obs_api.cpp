#include "fake_obs_api.hpp"

#include <cassert>
#include <obs.h>
#include <callback/calldata.h>
#include <callback/proc.h>

#include <QByteArray>
#include <QMetaObject>
#include <QSet>
#include <obs-frontend-api.h>
#include <callback/signal.h>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMap>

#include <cstdlib>
#include <cstring>
#include <string>

#include "obs-websocket-api.h"

struct proc_handler {
	QMap<QString, QPair<proc_handler_proc_t, void *>> procs;
};

struct signal_handler {
	struct Slot {
		QString signal;
		signal_callback_t callback;
		void *data;
	};
	QVector<Slot> entries;
};

struct obs_output {
	QString name;
	bool active = false;
	int refs = 1;
	bool connecting = false;   // Chunk 13 (realistic mode)
	signal_handler handlers;   // Chunk 13
};

struct obs_weak_output {
	obs_output *output;
	int refs = 1;
};

struct obs_data {
	QByteArray json;
};

namespace {

proc_handler g_global_ph;
proc_handler g_ws_ph;
QMap<QString, obs_output *> g_vertical_outputs;
FakeAitumVertical *g_vertical = nullptr;
FakeAitumMultistreamVendor *g_vendor = nullptr;
QVector<QPair<QString, bool>> g_vendor_outputs;
QSet<QString> g_lazy_names;               // Chunk 13
QSet<obs_output *> g_live_objects;        // Chunk 13: outputs not yet destroyed (for weak refs)

void emit_signal(obs_output *o, const char *sig)
{
	calldata_t cd = {0, 0, 0, false};
	calldata_set_ptr(&cd, "output", o);
	const auto entries = o->handlers.entries;
	for (const auto &s : entries)
		if (s.signal == QString::fromUtf8(sig))
			s.callback(s.data, &cd);
	std::free(cd.stack);
}

obs_output *create_output(const QString &name, bool active)
{
	auto *o = new obs_output;
	o->name = name;
	o->active = active;
	g_vertical_outputs.insert(name, o);
	g_live_objects.insert(o);
	return o;
}

// Realistic start/stop (Chunk 13).
void realistic_start(const QString &name)
{
	obs_output *out = g_vertical_outputs.value(name, nullptr);
	if (!out && g_lazy_names.contains(name))
		out = create_output(name, false);
	if (!out)
		return;
	if (out->active || out->connecting) {
		if (g_vertical)
			g_vertical->double_starts++;
		return;
	}
	out->connecting = true;
	emit_signal(out, "starting");
}

void realistic_stop(const QString &name)
{
	obs_output *out = g_vertical_outputs.value(name, nullptr);
	if (!out || !out->active) {
		if (g_vertical)
			g_vertical->ignored_stops++;
		return;
	}
	out->active = false;
	emit_signal(out, "stopping");
	emit_signal(out, "deactivate");
	emit_signal(out, "stop");
}

void match_main_event(enum obs_frontend_event event, void *)
{
	if (!g_vertical || !g_vertical->match_main)
		return;
	if (event == OBS_FRONTEND_EVENT_STREAMING_STARTED) {
		QMetaObject::invokeMethod(
			g_vertical,
			[] {
				if (!g_vertical)
					return;
				QStringList names = g_lazy_names.values();
				for (auto it = g_vertical_outputs.begin(); it != g_vertical_outputs.end(); ++it)
					if (!names.contains(it.key()))
						names << it.key();
				names.sort();
				for (const QString &n : names) {
					g_vertical->auto_calls << "auto-start:" + n;
					realistic_start(n);
				}
			},
			Qt::QueuedConnection);
	} else if (event == OBS_FRONTEND_EVENT_STREAMING_STOPPED) {
		QMetaObject::invokeMethod(
			g_vertical,
			[] {
				if (!g_vertical)
					return;
				for (auto it = g_vertical_outputs.begin(); it != g_vertical_outputs.end(); ++it) {
					if (it.value()->active) {
						g_vertical->auto_calls << "auto-stop:" + it.key();
						realistic_stop(it.key());
					}
				}
			},
			Qt::QueuedConnection);
	}
}

QString proc_name(const char *decl)
{
	// "void name(in int a, ...)" or "bool name(...)"
	QString d = QString::fromUtf8(decl);
	const int open = d.indexOf('(');
	const int space = d.lastIndexOf(' ', open);
	return d.mid(space + 1, open - space - 1);
}

char *dup(const QByteArray &s)
{
	char *out = static_cast<char *>(std::malloc(size_t(s.size()) + 1));
	std::memcpy(out, s.constData(), size_t(s.size()) + 1);
	return out;
}

// calldata storage: repeated [u32 name_len][name][u32 size][bytes] in data->stack.
struct Entry {
	size_t name_off, name_len, data_off, data_len, end;
};

bool find_entry(const calldata_t *data, const char *name, Entry &e)
{
	size_t pos = 0;
	const size_t want = std::strlen(name);
	while (data->stack && pos + 4 <= data->size) {
		uint32_t nl, dl;
		std::memcpy(&nl, data->stack + pos, 4);
		const size_t noff = pos + 4;
		std::memcpy(&dl, data->stack + noff + nl, 4);
		const size_t doff = noff + nl + 4;
		if (nl == want && std::memcmp(data->stack + noff, name, nl) == 0) {
			e = {noff, nl, doff, dl, doff + dl};
			return true;
		}
		pos = doff + dl;
	}
	return false;
}

void ensure_capacity(calldata_t *data, size_t need)
{
	if (need <= data->capacity)
		return;
	size_t cap = data->capacity ? data->capacity : 128;
	while (cap < need)
		cap *= 2;
	data->stack = static_cast<uint8_t *>(std::realloc(data->stack, cap));
	data->capacity = cap;
}

// ---- Aitum Vertical procs ----
void vertical_get_stream_output(void *, calldata_t *cd)
{
	if (g_vertical)
		g_vertical->state_queries++;
	const char *name = calldata_string(cd, "name");
	obs_output *out = g_vertical_outputs.value(QString::fromUtf8(name ? name : ""), nullptr);
	if (out)
		out->refs++;
	calldata_set_ptr(cd, "output", out);
}

void vertical_start(void *, calldata_t *cd)
{
	const QString name = QString::fromUtf8(calldata_string(cd, "name"));
	if (g_vertical && g_vertical->realistic)
		realistic_start(name);
	else if (obs_output *out = g_vertical_outputs.value(name, nullptr))
		out->active = true;
	if (g_vertical) {
		g_vertical->calls.push_back("start:" + name);
		emit g_vertical->called("start:" + name);
	}
}

void vertical_stop(void *, calldata_t *cd)
{
	const QString name = QString::fromUtf8(calldata_string(cd, "name"));
	if (g_vertical && g_vertical->realistic)
		realistic_stop(name);
	else if (obs_output *out = g_vertical_outputs.value(name, nullptr))
		out->active = false;
	if (g_vertical) {
		g_vertical->calls.push_back("stop:" + name);
		emit g_vertical->called("stop:" + name);
	}
}

void vertical_get_stream_settings(void *, calldata_t *cd)
{
	if (g_vertical)
		g_vertical->settings_calls++;
	calldata_set_ptr(cd, "outputs", nullptr);
}

// ---- obs-websocket ----
void ws_get_ph(void *, calldata_t *cd)
{
	calldata_set_ptr(cd, "ph", &g_ws_ph);
	calldata_set_bool(cd, "success", true);
}

void ws_call_request(void *, calldata_t *cd)
{
	const char *type = calldata_string(cd, "request_type");
	const char *data = calldata_string(cd, "request_data");
	auto *response = static_cast<obs_websocket_request_response *>(std::calloc(1, sizeof(obs_websocket_request_response)));
	const QJsonObject req = QJsonDocument::fromJson(QByteArray(data ? data : "{}")).object();
	if (QString::fromUtf8(type) == "CallVendorRequest" && req.value("vendorName").toString() == "aitum-multistream" &&
	    req.value("requestType").toString() == "get_outputs") {
		if (g_vendor)
			g_vendor->get_outputs_calls++;
		QJsonArray outputs;
		for (const auto &o : g_vendor_outputs)
			outputs.push_back(QJsonObject{{"name", o.first},
						     {"vertical", o.second},
						     {"stream_key", QString::fromUtf8(FakeAitumMultistreamVendor::kSecret)}});
		const QJsonObject body{{"vendorName", "aitum-multistream"},
				       {"requestType", "get_outputs"},
				       {"responseData", QJsonObject{{"outputs", outputs}, {"success", true}}}};
		response->status_code = 100;
		response->response_data = dup(QJsonDocument(body).toJson(QJsonDocument::Compact));
	} else {
		response->status_code = 600;
		response->comment = dup("No vendor was found by that name.");
	}
	calldata_set_ptr(cd, "response", response);
	calldata_set_bool(cd, "success", true);
}

} // namespace

namespace FakeObsApi {
void reset()
{
	g_global_ph.procs.clear();
	g_ws_ph.procs.clear();
	for (obs_output *o : g_vertical_outputs)
		delete o;
	g_vertical_outputs.clear();
	g_lazy_names.clear();
	g_live_objects.clear();
	g_vendor_outputs.clear();
	g_vertical = nullptr;
	g_vendor = nullptr;
}
} // namespace FakeObsApi

void FakeAitumVertical::install()
{
	g_vertical = this;
	proc_handler_add(&g_global_ph, "void aitum_vertical_get_stream_output(in int width, in int height, in string name, out ptr output)",
			 vertical_get_stream_output, nullptr);
	proc_handler_add(&g_global_ph, "void aitum_vertical_start_stream_output(in int width, in int height, in string name)",
			 vertical_start, nullptr);
	proc_handler_add(&g_global_ph, "void aitum_vertical_stop_stream_output(in int width, in int height, in string name)",
			 vertical_stop, nullptr);
	proc_handler_add(&g_global_ph, "void aitum_vertical_get_stream_settings(in int width, in int height, out ptr outputs)",
			 vertical_get_stream_settings, nullptr);
}

void FakeAitumVertical::add_output(const QString &name, bool active)
{
	create_output(name, active);
}

void FakeAitumVertical::set_realistic(bool on)
{
	realistic = on;
}

void FakeAitumVertical::add_lazy_output(const QString &name)
{
	g_lazy_names.insert(name);
}

void FakeAitumVertical::connect_output(const QString &name)
{
	obs_output *o = g_vertical_outputs.value(name, nullptr);
	if (!o || !o->connecting)
		return;
	o->connecting = false;
	o->active = true;
	emit_signal(o, "activate");
	emit_signal(o, "start");
}

void FakeAitumVertical::set_streaming_match_main(bool on)
{
	if (on && !match_main)
		obs_frontend_add_event_callback(match_main_event, nullptr);
	match_main = on;
}

bool FakeAitumVertical::exists(const QString &name) const
{
	return g_vertical_outputs.contains(name);
}

bool FakeAitumVertical::connecting(const QString &name) const
{
	obs_output *o = g_vertical_outputs.value(name, nullptr);
	return o && o->connecting;
}

bool FakeAitumVertical::active(const QString &name) const
{
	obs_output *o = g_vertical_outputs.value(name, nullptr);
	return o && o->active;
}

int FakeAitumVertical::refs(const QString &name) const
{
	obs_output *o = g_vertical_outputs.value(name, nullptr);
	return o ? o->refs : -1;
}

void FakeAitumMultistreamVendor::install()
{
	g_vendor = this;
	proc_handler_add(&g_global_ph, "bool obs_websocket_api_get_ph(out ptr ph)", ws_get_ph, nullptr);
	proc_handler_add(&g_ws_ph, "bool call_request(in string request_type, in string request_data, out ptr response)",
			 ws_call_request, nullptr);
}

void FakeAitumMultistreamVendor::set_outputs(const QVector<QPair<QString, bool>> &outputs)
{
	g_vendor_outputs = outputs;
}

// ---- libobs functions ----
extern "C" {

proc_handler_t *obs_get_proc_handler(void)
{
	return &g_global_ph;
}

void proc_handler_add(proc_handler_t *handler, const char *decl_string, proc_handler_proc_t proc, void *data)
{
	handler->procs.insert(proc_name(decl_string), {proc, data});
}

bool proc_handler_call(proc_handler_t *handler, const char *name, calldata_t *params)
{
	if (!handler)
		return false;
	auto it = handler->procs.find(QString::fromUtf8(name));
	if (it == handler->procs.end())
		return false;
	it->first(it->second, params);
	return true;
}

bool calldata_get_data(const calldata_t *data, const char *name, void *out, size_t size)
{
	Entry e;
	if (!find_entry(data, name, e) || e.data_len != size)
		return false;
	std::memcpy(out, data->stack + e.data_off, size);
	return true;
}

void calldata_set_data(calldata_t *data, const char *name, const void *in, size_t new_size)
{
	Entry e;
	if (find_entry(data, name, e)) {
		// remove the old entry
		const size_t start = e.name_off - 4;
		std::memmove(data->stack + start, data->stack + e.end, data->size - e.end);
		data->size -= e.end - start;
	}
	const uint32_t nl = uint32_t(std::strlen(name));
	const uint32_t dl = uint32_t(new_size);
	ensure_capacity(data, data->size + 8 + nl + dl);
	uint8_t *p = data->stack + data->size;
	std::memcpy(p, &nl, 4);
	std::memcpy(p + 4, name, nl);
	std::memcpy(p + 4 + nl, &dl, 4);
	if (dl)
		std::memcpy(p + 8 + nl, in, dl);
	data->size += 8 + nl + dl;
}

bool calldata_get_string(const calldata_t *data, const char *name, const char **str)
{
	Entry e;
	if (!find_entry(data, name, e))
		return false;
	*str = e.data_len ? reinterpret_cast<const char *>(data->stack + e.data_off) : nullptr;
	return true;
}

bool obs_output_active(const obs_output_t *output)
{
	return output && output->active;
}

void obs_output_release(obs_output_t *output)
{
	if (output)
		output->refs--;
}

// ---- Chunk 13: output signals and weak references ----
signal_handler_t *obs_output_get_signal_handler(const obs_output_t *output)
{
	return output ? &const_cast<obs_output_t *>(output)->handlers : nullptr;
}

void signal_handler_connect(signal_handler_t *handler, const char *signal, signal_callback_t callback, void *data)
{
	if (handler)
		handler->entries.push_back({QString::fromUtf8(signal), callback, data});
}

void signal_handler_disconnect(signal_handler_t *handler, const char *signal, signal_callback_t callback, void *data)
{
	if (!handler)
		return;
	for (int i = 0; i < handler->entries.size(); ++i) {
		const auto &s = handler->entries[i];
		if (s.signal == QString::fromUtf8(signal) && s.callback == callback && s.data == data) {
			handler->entries.remove(i);
			return;
		}
	}
}

obs_weak_output_t *obs_output_get_weak_output(obs_output_t *output)
{
	if (!output)
		return nullptr;
	return new obs_weak_output{output};
}

obs_output_t *obs_weak_output_get_output(obs_weak_output_t *weak)
{
	if (!weak || !g_live_objects.contains(weak->output))
		return nullptr;
	weak->output->refs++;
	return weak->output;
}

void obs_weak_output_release(obs_weak_output_t *weak)
{
	delete weak;
}

obs_data_t *obs_data_create_from_json(const char *json_string)
{
	auto *d = new obs_data;
	d->json = QByteArray(json_string ? json_string : "{}");
	return d;
}

const char *obs_data_get_json(obs_data_t *data)
{
	return data ? data->json.constData() : nullptr;
}

void obs_data_release(obs_data_t *data)
{
	delete data;
}

} // extern "C"
