#include "fake_obs.hpp"

#include <obs-module.h>
#include <util/base.h>
#include <util/bmem.h>
#include <util/text-lookup.h>

#include <QDateTime>
#include <QDir>
#include <QElapsedTimer>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace {

QMainWindow *g_window = nullptr;
QString g_profile = QStringLiteral("Untitled");
QString g_config_dir;
bool g_streaming = false;
FakeObsLog *g_log = nullptr;
QVector<FakeObsDock> g_docks;
QVector<QPair<obs_frontend_event_cb, void *>> g_callbacks;
QByteArray g_binary_path = "FakeObs/plugins/aitum-dock-sync/aitum-dock-sync.dll";
QByteArray g_data_path = "FakeObs/plugins/aitum-dock-sync/data";

char *dup_cstr(const QByteArray &s)
{
	char *out = static_cast<char *>(std::malloc(size_t(s.size()) + 1));
	std::memcpy(out, s.constData(), size_t(s.size()) + 1);
	return out;
}

QElapsedTimer &fake_clock()
{
	static QElapsedTimer t;
	if (!t.isValid())
		t.start();
	return t;
}

} // namespace

namespace FakeObs {

void reset()
{
	g_window = nullptr;
	g_profile = QStringLiteral("Untitled");
	g_config_dir.clear();
	g_streaming = false;
	if (g_log)
		g_log->lines.clear();
	g_docks.clear();
	g_callbacks.clear();
}

QMainWindow *&main_window() { return g_window; }
QString &profile() { return g_profile; }
QString &config_dir() { return g_config_dir; }
bool &streaming_active() { return g_streaming; }
FakeObsLog &log()
{
	if (!g_log)
		g_log = new FakeObsLog;
	return *g_log;
}
QVector<FakeObsDock> &docks() { return g_docks; }
int callback_count() { return g_callbacks.size(); }

void fire(enum obs_frontend_event event)
{
	const auto callbacks = g_callbacks;
	for (const auto &cb : callbacks)
		cb.first(event, cb.second);
}

} // namespace FakeObs

// ---- libobs ----
extern "C" {

void blogva(int, const char *format, va_list args)
{
	char buf[4096];
	std::vsnprintf(buf, sizeof(buf), format, args);
	std::fprintf(stderr, "[FakeObs log] %s\n", buf);
	FakeObs::log().add(QString::fromUtf8(buf));
}

void blog(int level, const char *format, ...)
{
	va_list args;
	va_start(args, format);
	blogva(level, format, args);
	va_end(args);
}

void bfree(void *ptr)
{
	std::free(ptr);
}

const char *obs_get_module_binary_path(obs_module_t *)
{
	return g_binary_path.constData();
}

const char *obs_get_module_data_path(obs_module_t *)
{
	return g_data_path.constData();
}

char *obs_module_get_config_path(obs_module_t *, const char *file)
{
	return dup_cstr(QDir(g_config_dir).filePath(QString::fromUtf8(file)).toUtf8());
}

lookup_t *obs_module_load_locale(obs_module_t *, const char *, const char *)
{
	return nullptr;
}

void text_lookup_destroy(lookup_t *) {}

bool text_lookup_getstr(lookup_t *, const char *, const char **)
{
	return false;
}

// ---- obs-frontend-api ----
void *obs_frontend_get_main_window(void)
{
	return g_window;
}

char *obs_frontend_get_current_profile(void)
{
	return dup_cstr(g_profile.toUtf8());
}

bool obs_frontend_streaming_active(void)
{
	return g_streaming;
}

void obs_frontend_add_event_callback(obs_frontend_event_cb callback, void *private_data)
{
	g_callbacks.push_back({callback, private_data});
}

void obs_frontend_remove_event_callback(obs_frontend_event_cb callback, void *private_data)
{
	for (int i = 0; i < g_callbacks.size(); ++i) {
		if (g_callbacks[i].first == callback && g_callbacks[i].second == private_data) {
			g_callbacks.remove(i);
			return;
		}
	}
}

bool obs_frontend_add_dock_by_id(const char *id, const char *title, void *widget)
{
	for (const auto &d : g_docks)
		if (d.id == QString::fromUtf8(id))
			return false;
	g_docks.push_back({QString::fromUtf8(id), QString::fromUtf8(title), static_cast<QWidget *>(widget)});
	return true;
}

void obs_frontend_remove_dock(const char *id)
{
	for (int i = 0; i < g_docks.size(); ++i) {
		if (g_docks[i].id == QString::fromUtf8(id)) {
			delete g_docks[i].widget;
			g_docks.remove(i);
			return;
		}
	}
}

} // extern "C"

// ---- Aitum Multistream stand-ins ----
FakeAitumDock::FakeAitumDock(QMainWindow *window) : QObject(window)
{
	dock_ = new QDockWidget(QStringLiteral("Multistream"), window);
	dock_->setObjectName(QStringLiteral("AitumMultistreamDock"));
	body_ = new QWidget(dock_);
	layout_ = new QVBoxLayout(body_);
	dock_->setWidget(body_);
	window->addDockWidget(Qt::RightDockWidgetArea, dock_);
}

QPushButton *FakeAitumDock::add_output(const QString &name, bool active)
{
	auto *group = new QGroupBox(name);
	group->setObjectName(name);
	auto *row = new QVBoxLayout(group);
	auto *button = new QPushButton(QStringLiteral("Stream"), group);
	button->setObjectName(QStringLiteral("canvasStream"));
	button->setCheckable(true);
	button->setChecked(active);
	row->addWidget(button);
	QObject::connect(button, &QPushButton::clicked, this, [this, name] {
		clicked_order.push_back(name);
		clicked_ms.push_back(fake_clock().elapsed() - clock_start_);
		emit clicked(name);
	});
	layout_->addWidget(group);
	return button;
}

void FakeAitumDock::remove_output(const QString &name)
{
	if (auto *group = dock_->findChild<QGroupBox *>(name))
		delete group;
}

QPushButton *FakeAitumDock::button(const QString &name) const
{
	auto *group = dock_->findChild<QGroupBox *>(name);
	return group ? group->findChild<QPushButton *>(QStringLiteral("canvasStream")) : nullptr;
}

void FakeAitumDock::start_clock()
{
	clock_start_ = fake_clock().elapsed();
}

namespace FakeAitumConfig {

QString path(const QString &appdata)
{
	return QDir(appdata).filePath(QStringLiteral("obs-studio/plugin_config/aitum-multistream/config.json"));
}

void write(const QString &appdata, const QVector<QPair<QString, QStringList>> &profiles)
{
	QJsonArray profile_array;
	for (const auto &p : profiles) {
		QJsonArray outputs;
		for (const QString &name : p.second)
			outputs.push_back(QJsonObject{{QStringLiteral("name"), name}});
		profile_array.push_back(QJsonObject{{QStringLiteral("name"), p.first}, {QStringLiteral("outputs"), outputs}});
	}
	const QString file_path = path(appdata);
	QDir().mkpath(QFileInfo(file_path).absolutePath());
	// Write a new file and move it over the old one, like editors and Aitum do.
	const QString tmp = file_path + QStringLiteral(".new");
	QFile f(tmp);
	f.open(QIODevice::WriteOnly | QIODevice::Truncate);
	f.write(QJsonDocument(QJsonObject{{QStringLiteral("profiles"), profile_array}}).toJson());
	f.close();
	QFile::remove(file_path);
	QFile::rename(tmp, file_path);
}

} // namespace FakeAitumConfig
