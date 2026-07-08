#include "dock-control.hpp"

#include <obs-frontend-api.h>
#include <obs-module.h>

#include <functional>

#include <QMetaObject>
#include <QTimer>
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
QStringList g_pending_outputs;
bool g_pending_start = true;

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

	const QString name = g_pending_outputs.takeFirst();
	click_aitum_output_button(name, g_pending_start);

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
	g_pending_outputs = load_aitum_output_names();
	if (g_pending_outputs.isEmpty()) {
		blog(LOG_WARNING, "[aitum-dock-sync] No Aitum outputs found for the current profile.");
		return;
	}

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

void frontend_event(enum obs_frontend_event event, void *)
{
	if (event == OBS_FRONTEND_EVENT_FINISHED_LOADING) {
		g_obs_ready = true;
		blog(LOG_INFO, "[aitum-dock-sync] OBS finished loading.");
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
	g_main_stream_active = obs_frontend_streaming_active();
	obs_frontend_add_event_callback(frontend_event, nullptr);
	blog(LOG_INFO, "[aitum-dock-sync] Loaded.");
	return true;
}

void obs_module_unload(void)
{
	clear_start_timer();
	clear_output_timer();
	obs_frontend_remove_event_callback(frontend_event, nullptr);
}

const char *obs_module_name(void)
{
	return obs_module_text("AitumDockSync");
}

const char *obs_module_description(void)
{
	return "Starts and stops Aitum Multistream outputs with the OBS main stream.";
}
