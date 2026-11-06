// Chunk 10 (Captain Dark 2026-11-02, after Chunk 9 step 2 FAIL): Aitum Vertical outputs.
//  - List: Aitum Multistream's official obs-websocket vendor request aitum-multistream/get_outputs
//    (outputs with "vertical": true), called in-process through obs-websocket-api.h.
//  - Control/state: Aitum Vertical's official procs aitum_vertical_start_stream_output /
//    aitum_vertical_stop_stream_output / aitum_vertical_get_stream_output (+ obs_output_active).
//  - Dock shows two groups, "Main" and "Vertical". Vertical picks are saved as "[Vertical] <name>".
//  - Refresh when Multistream adds a vertical output's QGroupBox to its dock.
//  - stream_key is never read or logged.
#include "fake_obs.hpp"
#include "fake_obs_api.hpp"

#include <obs-module.h>

#include <QApplication>
#include <QCheckBox>
#include <QDir>
#include <QFile>
#include <QGroupBox>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QtTest>

#include <memory>

namespace {

const QStringList kMain = {"YouTube Horizontal", "Kick", "Facebook", "X"};

QString picks_path()
{
	return QDir(FakeObs::config_dir()).filePath(QStringLiteral("picks.json"));
}

void write_picks(const QByteArray &json)
{
	QDir().mkpath(FakeObs::config_dir());
	QFile f(picks_path());
	QVERIFY(f.open(QIODevice::WriteOnly | QIODevice::Truncate));
	f.write(json);
}

QByteArray picks_bytes()
{
	QFile f(picks_path());
	return f.open(QIODevice::ReadOnly) ? f.readAll() : QByteArray();
}

QJsonObject read_picks()
{
	return QJsonDocument::fromJson(picks_bytes()).object();
}

QJsonObject json(const char *text)
{
	return QJsonDocument::fromJson(text).object();
}

QWidget *our_dock()
{
	for (const FakeObsDock &d : FakeObs::docks())
		if (d.title == QStringLiteral("Aitum Dock Sync"))
			return d.widget;
	return nullptr;
}

QStringList visible_groups()
{
	QStringList out;
	if (QWidget *w = our_dock())
		for (QGroupBox *g : w->findChildren<QGroupBox *>())
			if (!g->isHidden())
				out.push_back(g->title());
	return out;
}

QGroupBox *group(const QString &title)
{
	if (QWidget *w = our_dock())
		for (QGroupBox *g : w->findChildren<QGroupBox *>())
			if (g->title() == title)
				return g;
	return nullptr;
}

QStringList texts(const QString &title)
{
	QStringList out;
	if (QGroupBox *g = group(title))
		for (QCheckBox *b : g->findChildren<QCheckBox *>())
			out.push_back(b->text());
	return out;
}

QList<bool> states(const QString &title)
{
	QList<bool> out;
	if (QGroupBox *g = group(title))
		for (QCheckBox *b : g->findChildren<QCheckBox *>())
			out.push_back(b->isChecked());
	return out;
}

QCheckBox *box(const QString &title, const QString &name)
{
	if (QGroupBox *g = group(title))
		for (QCheckBox *b : g->findChildren<QCheckBox *>())
			if (b->text() == name)
				return b;
	return nullptr;
}

bool wait_for(QSignalSpy &spy, int count, int timeout_ms = 5000)
{
	while (spy.count() < count)
		if (!spy.wait(timeout_ms))
			return false;
	return true;
}

bool wait_for_log(QSignalSpy &spy, const QString &line, int timeout_ms = 5000)
{
	while (!FakeObs::log().lines.contains(line))
		if (!spy.wait(timeout_ms))
			return false;
	return true;
}

QVector<QPair<QString, bool>> vendor_list(const QStringList &main, const QStringList &vertical)
{
	QVector<QPair<QString, bool>> out;
	for (const QString &n : main)
		out.push_back({n, false});
	for (const QString &n : vertical)
		out.push_back({n, true});
	return out;
}

} // namespace

class TestVerticalOutputs : public QObject {
	Q_OBJECT

	std::unique_ptr<QTemporaryDir> tmp_;
	QMainWindow *window_ = nullptr;
	bool loaded_ = false;
	FakeAitumVertical vertical_;
	FakeAitumMultistreamVendor vendor_;

	QString appdata() const { return tmp_->filePath("FakeAppData"); }

	// Aitum Multistream (config.json + dock with the Main groupboxes) and Aitum Vertical.
	void setup_aitum(FakeAitumDock &aitum, const QStringList &vertical, const QList<bool> &vertical_active)
	{
		FakeAitumConfig::write(appdata(), {{"Main", kMain}});
		for (const QString &n : kMain)
			aitum.add_output(n, false);
		vertical_.install();
		vendor_.install();
		vendor_.set_outputs(vendor_list(kMain, vertical));
		for (int i = 0; i < vertical.size(); ++i)
			vertical_.add_output(vertical[i], vertical_active.value(i, false));
	}

	void load()
	{
		QVERIFY(obs_module_load());
		loaded_ = true;
		FakeObs::fire(OBS_FRONTEND_EVENT_FINISHED_LOADING);
		QCoreApplication::processEvents();
	}

private slots:
	void init()
	{
		tmp_ = std::make_unique<QTemporaryDir>();
		QVERIFY(tmp_->isValid());
		FakeObs::reset();
		FakeObsApi::reset();
		qputenv("APPDATA", appdata().toUtf8());
		FakeObs::config_dir() = tmp_->filePath("FakeAppData/obs-studio/plugin_config/aitum-dock-sync");
		FakeObs::profile() = QStringLiteral("Main");
		window_ = new QMainWindow;
		FakeObs::main_window() = window_;
	}

	void cleanup()
	{
		if (loaded_)
			obs_module_unload();
		loaded_ = false;
		delete window_;
		window_ = nullptr;
		FakeObsApi::reset();
		tmp_.reset();
	}

	void test_dock_groups_main_and_vertical_outputs()
	{
		write_picks(R"({"Main": {"Kick": true, "[Vertical] TikTok": true}})");
		FakeAitumDock aitum(window_);
		setup_aitum(aitum, {"YouTube Vertical", "TikTok"}, {false, false});
		load();

		QCOMPARE(visible_groups(), QStringList({"Main", "Vertical"}));
		QCOMPARE(texts("Main"), kMain);
		QCOMPARE(states("Main"), QList<bool>({false, true, false, false}));
		QCOMPARE(texts("Vertical"), QStringList({"YouTube Vertical", "TikTok"}));
		QCOMPARE(states("Vertical"), QList<bool>({false, true}));
		QVERIFY(FakeObs::log().lines.contains(
			"[aitum-dock-sync] Dock vertical outputs for profile 'Main': YouTube Vertical, TikTok"));
		QCOMPARE(vertical_.calls, QStringList());
		QCOMPARE(vertical_.settings_calls, 0);
	}

	void test_stream_start_starts_ticked_vertical_outputs()
	{
		write_picks(R"({"Main": {"Kick": true, "[Vertical] TikTok": true}})");
		FakeAitumDock aitum(window_);
		setup_aitum(aitum, {"YouTube Vertical", "TikTok"}, {false, false});
		QStringList events;
		QObject::connect(&aitum, &FakeAitumDock::clicked, [&events](const QString &n) { events << "main:" + n; });
		QObject::connect(&vertical_, &FakeAitumVertical::called, [&events](const QString &c) { events << "vertical:" + c; });
		QSignalSpy vcalls(&vertical_, &FakeAitumVertical::called);
		load();

		FakeObs::streaming_active() = true;
		FakeObs::fire(OBS_FRONTEND_EVENT_STREAMING_STARTED);
		QVERIFY(wait_for(vcalls, 1));

		QCOMPARE(events, QStringList({"main:Kick", "vertical:start:TikTok"}));
		QCOMPARE(vertical_.active("TikTok"), true);
		QCOMPARE(vertical_.active("YouTube Vertical"), false);
		QCOMPARE(vertical_.refs("TikTok"), 1);
		QCOMPARE(vertical_.refs("YouTube Vertical"), 1);
	}

	void test_stream_stop_stops_all_vertical_outputs()
	{
		// Nothing ticked; both vertical outputs were started by hand.
		FakeAitumDock aitum(window_);
		setup_aitum(aitum, {"YouTube Vertical", "TikTok"}, {true, true});
		QSignalSpy vcalls(&vertical_, &FakeAitumVertical::called);
		QSignalSpy log(&FakeObs::log(), &FakeObsLog::line_added);
		load();

		FakeObs::streaming_active() = true;
		FakeObs::fire(OBS_FRONTEND_EVENT_STREAMING_STARTED);
		QVERIFY(wait_for_log(log, "[aitum-dock-sync] No ticked Aitum outputs for profile 'Main'."));
		QCOMPARE(vertical_.calls, QStringList());

		FakeObs::streaming_active() = false;
		FakeObs::fire(OBS_FRONTEND_EVENT_STREAMING_STOPPED);
		QVERIFY(wait_for(vcalls, 2));

		QCOMPARE(vertical_.calls, QStringList({"stop:YouTube Vertical", "stop:TikTok"}));
		QCOMPARE(vertical_.active("YouTube Vertical"), false);
		QCOMPARE(vertical_.active("TikTok"), false);
		QCOMPARE(vertical_.refs("YouTube Vertical"), 1);
		QCOMPARE(vertical_.refs("TikTok"), 1);
	}

	void test_vertical_tick_while_live_starts_untick_stops()
	{
		FakeAitumDock aitum(window_);
		setup_aitum(aitum, {"YouTube Vertical", "TikTok"}, {false, false});
		QSignalSpy log(&FakeObs::log(), &FakeObsLog::line_added);
		load();

		FakeObs::streaming_active() = true;
		FakeObs::fire(OBS_FRONTEND_EVENT_STREAMING_STARTED);
		QVERIFY(wait_for_log(log, "[aitum-dock-sync] No ticked Aitum outputs for profile 'Main'."));

		QVERIFY(box("Vertical", "YouTube Vertical"));
		box("Vertical", "YouTube Vertical")->click();
		QCOMPARE(vertical_.calls, QStringList({"start:YouTube Vertical"}));
		QCOMPARE(vertical_.active("YouTube Vertical"), true);
		QCOMPARE(read_picks(), json(R"({"Main": {"[Vertical] YouTube Vertical": true}})"));

		box("Vertical", "YouTube Vertical")->click();
		QCOMPARE(vertical_.calls, QStringList({"start:YouTube Vertical", "stop:YouTube Vertical"}));
		QCOMPARE(vertical_.active("YouTube Vertical"), false);
		QCOMPARE(vertical_.active("TikTok"), false);
		QCOMPARE(read_picks(), json(R"({"Main": {"[Vertical] YouTube Vertical": false}})"));
		QCOMPARE(aitum.clicked_order, QStringList());
		QCOMPARE(vertical_.refs("YouTube Vertical"), 1);
	}

	void test_vertical_toggle_while_offline_only_saves()
	{
		FakeAitumDock aitum(window_);
		setup_aitum(aitum, {"YouTube Vertical", "TikTok"}, {false, false});
		load();

		QVERIFY(box("Vertical", "TikTok") && box("Main", "Kick"));
		box("Vertical", "TikTok")->click();
		box("Main", "Kick")->click();
		QCoreApplication::processEvents();

		QCOMPARE(states("Vertical"), QList<bool>({false, true}));
		QCOMPARE(states("Main"), QList<bool>({false, true, false, false}));
		QCOMPARE(vertical_.calls, QStringList());
		QCOMPARE(aitum.clicked_order, QStringList());
		QCOMPARE(read_picks(), json(R"({"Main": {"[Vertical] TikTok": true, "Kick": true}})"));
	}

	void test_new_vertical_output_appears_unticked()
	{
		write_picks(R"({"Main": {"[Vertical] YouTube Vertical": true}})");
		FakeAitumDock aitum(window_);
		setup_aitum(aitum, {"YouTube Vertical"}, {false});
		load();
		QCOMPARE(texts("Vertical"), QStringList({"YouTube Vertical"}));

		// Added in Aitum Multistream: its vertical list changes and it adds the output's
		// QGroupBox to its dock. One event-loop pass only.
		vendor_.set_outputs(vendor_list(kMain, {"YouTube Vertical", "TikTok"}));
		vertical_.add_output("TikTok", false);
		aitum.add_output("TikTok", false);
		QCoreApplication::processEvents();

		QCOMPARE(texts("Vertical"), QStringList({"YouTube Vertical", "TikTok"}));
		QCOMPARE(states("Vertical"), QList<bool>({true, false}));
		QCOMPARE(texts("Main"), kMain);
		QVERIFY(FakeObs::log().lines.contains(
			"[aitum-dock-sync] Dock vertical outputs for profile 'Main': YouTube Vertical, TikTok"));
		QCOMPARE(vertical_.calls, QStringList());
	}

	void test_stream_key_never_read_or_logged()
	{
		write_picks(R"({"Main": {"[Vertical] TikTok": true}})");
		FakeAitumDock aitum(window_);
		setup_aitum(aitum, {"YouTube Vertical", "TikTok"}, {false, false});
		QSignalSpy vcalls(&vertical_, &FakeAitumVertical::called);
		load();

		FakeObs::streaming_active() = true;
		FakeObs::fire(OBS_FRONTEND_EVENT_STREAMING_STARTED);
		QVERIFY(wait_for(vcalls, 1));
		box("Vertical", "YouTube Vertical")->click();
		FakeObs::streaming_active() = false;
		FakeObs::fire(OBS_FRONTEND_EVENT_STREAMING_STOPPED);
		QVERIFY(wait_for(vcalls, 4));

		QCOMPARE(vertical_.calls, QStringList({"start:TikTok", "start:YouTube Vertical", "stop:YouTube Vertical", "stop:TikTok"}));
		QVERIFY(vendor_.get_outputs_calls > 0);
		QCOMPARE(vertical_.settings_calls, 0);
		QStringList leaked;
		for (const QString &line : FakeObs::log().lines)
			if (line.contains(FakeAitumMultistreamVendor::kSecret) || line.contains("stream_key"))
				leaked << line;
		QCOMPARE(leaked, QStringList());
		QCOMPARE(picks_bytes().contains(FakeAitumMultistreamVendor::kSecret), false);
	}

	void test_main_only_without_vertical_api()
	{
		// Aitum Multistream present, but no obs-websocket vendor and no Aitum Vertical.
		FakeAitumConfig::write(appdata(), {{"Main", kMain}});
		FakeAitumDock aitum(window_);
		for (const QString &n : kMain)
			aitum.add_output(n, false);
		load();

		QCOMPARE(visible_groups(), QStringList({"Main"}));
		QCOMPARE(texts("Main"), kMain);
		QCOMPARE(texts("Vertical"), QStringList());
	}
};

QTEST_MAIN(TestVerticalOutputs)
#include "test_vertical_outputs.moc"
