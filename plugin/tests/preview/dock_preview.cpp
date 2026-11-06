// Preview only (not a test, not shipped): renders the real Aitum Dock Sync dock, driven by the
// test fakes, inside a QDockWidget, with an APPROXIMATION of OBS 33.0.1's Yami and Yami Light
// themes (values copied from frontend/data/themes/Yami.obt / Yami_Light.ovt; OBS's own theme
// engine is not available outside OBS). Usage: dock_preview <output folder>
#include "fake_obs.hpp"
#include "fake_obs_api.hpp"

#include <obs-module.h>

#include <QApplication>
#include <QDir>
#include <QDockWidget>
#include <QFile>
#include <QHBoxLayout>
#include <QLabel>
#include <QMainWindow>
#include <QSettings>
#include <QTemporaryDir>

#include <algorithm>
#include <cstdio>

namespace {

struct Theme {
	const char *name;
	const char *window, *base, *text, *text_light, *muted, *red, *input, *border, *primary;
};

// Yami sizes computed with OBS 33.0.1's formulas (Yami.obt, OBSApp_Themes.cpp) from the user's
// real Appearance settings in %APPDATA%\obs-studio\user.ini (read-only).
struct Metrics {
	int font_scale = 10;
	int density = 1;
	double padding = 4;
	double font_base, font_small, spacing_base, spacing_large, padding_large, input_height, icon;
};

Metrics read_metrics()
{
	Metrics m;
	const QString dir = QDir(qEnvironmentVariable("APPDATA")).filePath("obs-studio");
	for (const char *file : {"user.ini", "global.ini"}) {
		const QString path = QDir(dir).filePath(QString::fromLatin1(file));
		if (!QFile::exists(path))
			continue;
		QSettings ini(path, QSettings::IniFormat);
		if (!ini.contains("Appearance/FontScale") && !ini.contains("Appearance/Density"))
			continue;
		m.font_scale = ini.value("Appearance/FontScale", 10).toInt();
		m.density = ini.value("Appearance/Density", 1).toInt();
		std::printf("Appearance from %s: FontScale=%d Density=%d\n", qPrintable(path), m.font_scale, m.density);
		break;
	}
	m.padding = m.density == -2 ? 0.25 : m.density == -3 ? 2 : m.density == -5 ? 6 : 4;
	const double sbv = 2 + m.padding / 2;
	m.font_base = m.font_scale;
	m.font_small = std::max(7.0, 0.8 * m.font_scale);
	m.spacing_base = std::min(std::max(1.0, 0.4 * sbv), 2.0);
	m.spacing_large = std::min(std::max(2.0, sbv), 4.0);
	m.padding_large = std::min(std::max(1.0, m.padding), 5.0);
	m.input_height = std::max(m.font_scale * 2.2 + m.padding * 2, 24.0);
	m.icon = std::max(2.0, m.padding) + 12;
	return m;
}

QString qss(const Theme &t, const Metrics &m)
{
	return QString(R"(
QWidget { background: %1; color: %3; font-size: %10pt; font-family: 'Open Sans', '.AppleSystemUIFont', Helvetica, Arial, 'MS Shell Dlg', sans-serif; }
QDockWidget { font-size: %11pt; font-weight: bold; color: %4; }
QGroupBox { background: %2; border-radius: 4px; padding-top: %12px; padding-bottom: %13px; font-weight: bold; margin-bottom: %14px; }
QGroupBox::title { subcontrol-origin: margin; left: 4px; top: 4px; }
QCheckBox { background: transparent; margin-top: %15px; margin-bottom: %15px; spacing: %16px; }
QCheckBox::indicator { width: %17px; height: %17px; border: 1px solid %8; border-radius: 3px; background: %7; margin-right: %14px; }
QCheckBox::indicator:checked { background: %9; border-color: %9; }
QLabel { background: transparent; }
#mixerSample QLabel { font-size: %11pt; }
[class~="text-muted"] { color: %5; }
[class~="text-small"] { font-size: %11pt; }
[class~="text-bold"] { font-weight: bold; }
[class~="text-danger"] { color: %6; }
)")
		.arg(t.window, t.base, t.text, t.text_light, t.muted, t.red, t.input, t.border, t.primary)
		.arg(m.font_base)
		.arg(m.font_small)
		.arg(m.input_height)
		.arg(m.padding_large)
		.arg(m.spacing_large)
		.arg(m.spacing_base)
		.arg(std::max(1.0, 0.25 * (2 + m.padding / 2)))
		.arg(m.icon);
}

void render(const QString &path, bool with_aitum)
{
	QTemporaryDir tmp;
	FakeObs::reset();
	FakeObsApi::reset();
	qputenv("APPDATA", tmp.filePath("FakeAppData").toUtf8());
	FakeObs::config_dir() = tmp.filePath("FakeAppData/obs-studio/plugin_config/aitum-dock-sync");
	FakeObs::profile() = QStringLiteral("Main");
	QMainWindow window;
	FakeObs::main_window() = &window;

	FakeAitumVertical vertical;
	FakeAitumMultistreamVendor vendor;
	FakeAitumDock *aitum = nullptr;
	const QStringList main = {"YouTube Horizontal", "Kick", "Facebook", "X"};
	const QStringList vert = {"YouTube Vertical", "Vertical 2"};
	if (with_aitum) {
		FakeAitumConfig::write(tmp.filePath("FakeAppData"), {{"Main", main}});
		QDir().mkpath(FakeObs::config_dir());
		QFile picks(QDir(FakeObs::config_dir()).filePath("picks.json"));
		picks.open(QIODevice::WriteOnly);
		picks.write(R"({"Main": {"YouTube Horizontal": true, "Kick": true, "[Vertical] YouTube Vertical": true}})");
		picks.close();
		aitum = new FakeAitumDock(&window);
		for (const QString &n : main)
			aitum->add_output(n, n == "YouTube Horizontal" || n == "Kick");
		for (const QString &n : vert)
			aitum->add_output(n, n == "YouTube Vertical");
		vertical.install();
		vendor.install();
		QVector<QPair<QString, bool>> list;
		for (const QString &n : main)
			list.push_back({n, false});
		for (const QString &n : vert)
			list.push_back({n, true});
		vendor.set_outputs(list);
		for (const QString &n : vert)
			vertical.add_output(n, n == "YouTube Vertical");
	}

	obs_module_load();
	FakeObs::fire(OBS_FRONTEND_EVENT_FINISHED_LOADING);
	QCoreApplication::processEvents();

	QWidget *ours = nullptr;
	for (const FakeObsDock &d : FakeObs::docks())
		if (d.title == QStringLiteral("Aitum Dock Sync"))
			ours = d.widget;
	{
		QWidget frame;
		auto *row = new QHBoxLayout(&frame);
		row->setContentsMargins(0, 0, 0, 0);
		auto *mixer = new QDockWidget(QStringLiteral("Audio Mixer (sample)"), &frame);
		mixer->setObjectName(QStringLiteral("mixerSample"));
		auto *names = new QWidget(mixer);
		auto *names_layout = new QVBoxLayout(names);
		for (const char *n : {"Desktop Audio", "Mic/Aux", "Music"})
			names_layout->addWidget(new QLabel(QString::fromUtf8(n), names));
		names_layout->addStretch(1);
		mixer->setWidget(names);
		auto *host = new QDockWidget(QStringLiteral("Aitum Dock Sync"), &frame);
		host->setWidget(ours);
		row->addWidget(host, 3);
		row->addWidget(mixer, 2);
		frame.resize(460, with_aitum ? 360 : 110);
		frame.ensurePolished();
		QCoreApplication::processEvents();
		frame.grab().save(path);
		std::printf("wrote %s\n", qPrintable(path));
		obs_module_unload();
	}
	delete aitum;
	FakeObsApi::reset();
}

} // namespace

int main(int argc, char **argv)
{
	QApplication app(argc, argv);
	const QString out = argc > 1 ? QString::fromLocal8Bit(argv[1]) : QStringLiteral(".");
	QDir().mkpath(out);
	const Theme themes[] = {
		{"yami", "#1D1F26", "#272A33", "#FFFFFF", "#FFFFFF", "#969696", "#C01C37", "#3C404D", "#5B6273", "#284CB8"},
		{"yami-light", "rgb(211,211,211)", "rgb(229,229,229)", "#0A0A0A", "#414141", "#646464", "#C01C37",
		 "rgb(245,245,245)", "rgb(140,140,140)", "rgb(140,181,255)"},
	};
	const Metrics m = read_metrics();
	std::printf("font_base=%gpt font_small=%gpt padding=%g input_height=%gpx icon=%gpx\n", m.font_base, m.font_small,
		    m.padding, m.input_height, m.icon);
	for (const Theme &t : themes) {
		app.setStyleSheet(qss(t, m));
		render(QDir(out).filePath(QString("dock-%1.png").arg(t.name)), true);
		render(QDir(out).filePath(QString("dock-%1-not-found.png").arg(t.name)), false);
	}
	return 0;
}
