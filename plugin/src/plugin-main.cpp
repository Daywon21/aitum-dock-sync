#include "dock-control.hpp"
#include "picks-store.hpp"
#include "platform-dock.hpp"
#include "vertical-control.hpp"

#include <obs-frontend-api.h>
#include <obs-module.h>

#include <functional>
#include <memory>

#include <QMetaObject>
#include <QPointer>
#include <QTimer>
#include <QVector>
#include <QWidget>

OBS_DECLARE_MODULE()
OBS_MODULE_USE_DEFAULT_LOCALE("aitum-dock-sync", "en-US")

namespace {

constexpr int kStartDelayMs = 1500;
constexpr int kOutputGapMs = 400;

bool g_main_stream_active = false;
bool g_obs_ready = false;
QTimer *g_start_timer = nullptr;
QTimer *g_output_timer = nullptr;
struct PendingOutput {
	QString name;
	bool vertical;
	bool start;
	bool only_if_unticked; // start-pass stop of a Vertical that Aitum Vertical auto-started
};
QVector<PendingOutput> g_pending_outputs;
// Vertical outputs already live when the main stream started (started by hand): left alone.
QStringList g_vertical_live_at_start;
bool g_pending_start = true;
std::unique_ptr<PicksStore> g_picks;
QPointer<PlatformDock> g_dock;

constexpr auto kDockId = "aitum-dock-sync";
constexpr auto kDockTitle = "Aitum Dock Sync";

QWidget *main_window()
{
	return static_cast<QWidget *>(obs_frontend_get_main_window());
}

void clear_output_timer()
{
	if (!g_output_timer)
		return;

	g_output_timer->stop();
	delete g_output_timer;
	g_output_timer = nullptr;
}

void clear_start_timer()
{
	if (!g_start_timer)
		return;

	g_start_timer->stop();
	delete g_start_timer;
	g_start_timer = nullptr;
}

void run_on_ui_thread(const std::function<void()> &fn)
{
	QWidget *window = main_window();
	if (!window) {
		blog(LOG_WARNING, "[aitum-dock-sync] Main window is not available.");
		return;
	}

	QMetaObject::invokeMethod(window, [fn] { fn(); }, Qt::QueuedConnection);
}

void click_next_output()
{
	if (g_pending_outputs.isEmpty()) {
		clear_output_timer();
		return;
	}

	const PendingOutput next = g_pending_outputs.takeFirst();
	const bool ticked_since = next.only_if_unticked && g_picks &&
				  g_picks->is_picked(current_obs_profile_name(), vertical_pick_key(next.name));
	if (ticked_since)
		blog(LOG_INFO, "[aitum-dock-sync] Vertical '%s' was ticked meanwhile; not stopping it.",
		     next.name.toUtf8().constData());
	else if (next.vertical)
		set_vertical_output(next.name, next.start);
	else
		click_aitum_output_button(next.name, next.start);

	if (g_pending_outputs.isEmpty()) {
		clear_output_timer();
		return;
	}

	if (!g_output_timer) {
		QWidget *window = main_window();
		if (!window)
			return;

		g_output_timer = new QTimer(window);
		g_output_timer->setSingleShot(true);
		QObject::connect(g_output_timer, &QTimer::timeout, [] { click_next_output(); });
	}

	g_output_timer->start(kOutputGapMs);
}

void queue_output_clicks(bool start)
{
	g_pending_start = start;
	const QStringList main_outputs = load_aitum_output_names();
	const QStringList vertical_outputs = load_vertical_output_names();
	if (main_outputs.isEmpty() && vertical_outputs.isEmpty()) {
		blog(LOG_WARNING, "[aitum-dock-sync] No Aitum outputs found for the current profile.");
		return;
	}

	// Start: ticked outputs start (Main first, then Vertical). Unticked Vertical outputs that Aitum
	// Vertical started itself with the main stream ("Start streaming with main") are stopped; ones that
	// were already live before the main stream started are left alone.
	// Stop: every output stops (Main first, then Vertical).
	QVector<PendingOutput> pending;
	if (start) {
		const QString profile = current_obs_profile_name();
		bool any_ticked = false;
		for (const QString &name : main_outputs) {
			if (g_picks && g_picks->is_picked(profile, name)) {
				pending.push_back({name, false, true, false});
				any_ticked = true;
			}
		}
		bool auto_ticked = false;
		for (const QString &name : vertical_outputs) {
			if (g_picks && g_picks->is_picked(profile, vertical_pick_key(name))) {
				pending.push_back({name, true, true, false});
				any_ticked = true;
			} else if (!g_vertical_live_at_start.contains(name)) {
				pending.push_back({name, true, false, true});
			} else if (g_picks) {
				// Started by hand before going live: keep it running and tick it, so the dock matches.
				g_picks->set_picked(profile, vertical_pick_key(name), true);
				auto_ticked = true;
				blog(LOG_INFO,
				     "[aitum-dock-sync] Vertical '%s' was already live when the stream started; ticked it.",
				     name.toUtf8().constData());
			}
		}
		if (!any_ticked)
			blog(LOG_INFO, "[aitum-dock-sync] No ticked Aitum outputs for profile '%s'.",
			     profile.toUtf8().constData());
		if (auto_ticked && g_dock)
			g_dock->refresh();
		if (pending.isEmpty())
			return;
	} else {
		for (const QString &name : main_outputs)
			pending.push_back({name, false, false, false});
		for (const QString &name : vertical_outputs)
			pending.push_back({name, true, false, false});
	}
	g_pending_outputs = pending;

	clear_output_timer();
	run_on_ui_thread([] { click_next_output(); });
}

void on_streaming_started()
{
	if (!g_obs_ready) {
		blog(LOG_WARNING, "[aitum-dock-sync] Ignoring stream start before OBS finished loading.");
		return;
	}

	if (g_main_stream_active)
		return;

	g_main_stream_active = true;
	clear_start_timer();

	// Runs before Aitum Vertical's own queued auto-start: remember what was live already, then look
	// at the outputs again right after Aitum Vertical has started them.
	const QStringList vertical_outputs = load_vertical_output_names();
	g_vertical_live_at_start = vertical_live_outputs(vertical_outputs);
	run_on_ui_thread([vertical_outputs] { vertical_watch(vertical_outputs); });

	QWidget *window = main_window();
	if (!window)
		return;

	g_start_timer = new QTimer(window);
	g_start_timer->setSingleShot(true);
	QObject::connect(g_start_timer, &QTimer::timeout, [] {
		clear_start_timer();
		blog(LOG_INFO, "[aitum-dock-sync] Starting Aitum Multistream outputs.");
		queue_output_clicks(true);
	});

	blog(LOG_INFO, "[aitum-dock-sync] Main stream started. Waiting %d ms before starting outputs.",
	     kStartDelayMs);
	g_start_timer->start(kStartDelayMs);
}

void on_streaming_stopped()
{
	clear_start_timer();
	clear_output_timer();
	g_pending_outputs.clear();

	if (!g_main_stream_active)
		return;

	g_main_stream_active = false;
	blog(LOG_INFO, "[aitum-dock-sync] Main stream stopped. Stopping Aitum Multistream outputs.");
	queue_output_clicks(false);
}

void refresh_dock()
{
	if (g_dock)
		g_dock->refresh();
}

void frontend_event(enum obs_frontend_event event, void *)
{
	if (event == OBS_FRONTEND_EVENT_FINISHED_LOADING) {
		g_obs_ready = true;
		blog(LOG_INFO, "[aitum-dock-sync] OBS finished loading.");
		run_on_ui_thread([] { refresh_dock(); });
		return;
	}

	if (event == OBS_FRONTEND_EVENT_PROFILE_CHANGED) {
		run_on_ui_thread([] { refresh_dock(); });
		return;
	}

	if (event == OBS_FRONTEND_EVENT_STREAMING_STARTED) {
		run_on_ui_thread([] { on_streaming_started(); });
		return;
	}

	if (event == OBS_FRONTEND_EVENT_STREAMING_STOPPED)
		run_on_ui_thread([] { on_streaming_stopped(); });
}

} // namespace

bool obs_module_load(void)
{
	vertical_tracker_start();
	char *picks_path = obs_module_config_path("picks.json");
	g_picks = std::make_unique<PicksStore>(QString::fromUtf8(picks_path));
	bfree(picks_path);

	auto *dock = new PlatformDock(g_picks.get(), [] { return g_main_stream_active; });
	if (obs_frontend_add_dock_by_id(kDockId, kDockTitle, dock)) {
		g_dock = dock;
		g_dock->refresh();
		g_dock->start_watching();
	} else {
		blog(LOG_WARNING, "[aitum-dock-sync] Could not add the dock.");
		delete dock;
	}

	g_main_stream_active = obs_frontend_streaming_active();
	obs_frontend_add_event_callback(frontend_event, nullptr);
	blog(LOG_INFO, "[aitum-dock-sync] Loaded.");
	blog(LOG_INFO, "[aitum-dock-sync] Module: %s, data: %s",
	     obs_get_module_binary_path(obs_current_module()),
	     obs_get_module_data_path(obs_current_module()));
	return true;
}

void obs_module_unload(void)
{
	clear_start_timer();
	clear_output_timer();
	obs_frontend_remove_event_callback(frontend_event, nullptr);
	if (g_dock)
		g_dock->detach();
	g_picks.reset();
	vertical_tracker_stop();
}

const char *obs_module_name(void)
{
	return obs_module_text("AitumDockSync");
}

const char *obs_module_description(void)
{
	return "Starts and stops Aitum Multistream outputs with the OBS main stream.";
}
