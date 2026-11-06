// Chunk 13 (Captain Dark 2026-11-03, Chunk 9 step 3 FAIL, log 2026-11-03 00-07-16.txt):
// Aitum Vertical's "Start streaming with main" starts every Vertical output itself when the main
// stream starts (00:08:07.607), before Dock Sync's start pass (00:08:09.105). An output that is still
// connecting reads obs_output_active() == false, libobs ignores obs_output_stop() on it, and a second
// start while connecting would start it twice. Dock Sync follows each Vertical output's libobs
// signals (starting / start / stop) to know idle, connecting or live:
//  - unticked + live: stopped now; unticked + connecting: stopped as soon as it goes live;
//  - ticked + idle: started; ticked + connecting or live: left alone (no double start).
// FakeAitumVertical runs in realistic mode (libobs + Aitum Vertical behavior) for these tests.
#include "fake_obs.hpp"
#include "fake_obs_api.hpp"

#include <obs-module.h>

#include <QApplication>
#include <QCheckBox>
#include <QDir>
#include <QFile>
#include <QGroupBox>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QtTest>

#include <memory>

namespace {

void write_picks(const QByteArray &json)
{
	QDir().mkpath(FakeObs::config_dir());
	QFile f(QDir(FakeObs::config_dir()).filePath(QStringLiteral("picks.json")));
	QVERIFY(f.open(QIODevice::WriteOnly | QIODevice::Truncate));
	f.write(json);
}

QCheckBox *vertical_box(const QString &name)
{
	for (const FakeObsDock &d : FakeObs::docks())
		if (d.title == QStringLiteral("Aitum Dock Sync"))
			for (QGroupBox *g : d.widget->findChildren<QGroupBox *>())
				if (g->title() == QStringLiteral("Vertical"))
					for (QCheckBox *b : g->findChildren<QCheckBox *>())
						if (b->text() == name)
							return b;
	return nullptr;
}

bool wait_for_log(QSignalSpy &spy, const QString &line, int timeout_ms = 5000)
{
	while (!FakeObs::log().lines.contains(line))
		if (!spy.wait(timeout_ms))
			return false;
	return true;
}

const QString kConnecting = QStringLiteral("[aitum-dock-sync] Vertical '%1' is connecting; it will be stopped when it goes live.");
const QString kAlreadyStarting = QStringLiteral("[aitum-dock-sync] Vertical '%1' is already starting.");

} // namespace

class TestVerticalLifecycle : public QObject {
	Q_OBJECT

	std::unique_ptr<QTemporaryDir> tmp_;
	QMainWindow *window_ = nullptr;
	bool loaded_ = false;
	FakeAitumVertical vertical_;
	FakeAitumMultistreamVendor vendor_;
	FakeAitumDock *aitum_ = nullptr;

	// Fresh OBS session: Aitum Vertical knows both outputs but has not created them yet.
	void setup(const QByteArray &picks, bool match_main)
	{
		FakeAitumConfig::write(tmp_->filePath("FakeAppData"), {{"Main", {"Kick Output"}}});
		write_picks(picks);
		aitum_ = new FakeAitumDock(window_);
		aitum_->add_output("Kick Output", false);
		aitum_->add_output("YouTube Vertical", false);
		aitum_->add_output("Instagram", false);
		vertical_.install();
		vertical_.set_realistic(true);
		vertical_.add_lazy_output("YouTube Vertical");
		vertical_.add_lazy_output("Instagram");
		vendor_.install();
		vendor_.set_outputs({{"Kick Output", false}, {"YouTube Vertical", true}, {"Instagram", true}});

		QVERIFY(obs_module_load());
		loaded_ = true;
		vertical_.set_streaming_match_main(match_main); // Aitum Vertical loads after Dock Sync
		FakeObs::fire(OBS_FRONTEND_EVENT_FINISHED_LOADING);
		QCoreApplication::processEvents();
	}

	void start_stream()
	{
		FakeObs::streaming_active() = true;
		FakeObs::fire(OBS_FRONTEND_EVENT_STREAMING_STARTED);
		QCoreApplication::processEvents();
	}

private slots:
	void init()
	{
		tmp_ = std::make_unique<QTemporaryDir>();
		QVERIFY(tmp_->isValid());
		FakeObs::reset();
		FakeObsApi::reset();
		qputenv("APPDATA", tmp_->filePath("FakeAppData").toUtf8());
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
		aitum_ = nullptr;
		FakeObsApi::reset();
		tmp_.reset();
	}

	// The log's case: both Verticals unticked, Aitum Vertical starts both with the main stream.
	void test_unticked_vertical_auto_started_is_stopped_once_live()
	{
		setup(R"({"Main": {"Kick Output": true}})", true);
		QSignalSpy log(&FakeObs::log(), &FakeObsLog::line_added);
		start_stream();
		QCOMPARE(vertical_.auto_calls, QStringList({"auto-start:Instagram", "auto-start:YouTube Vertical"}));
		QCOMPARE(vertical_.connecting("YouTube Vertical"), true);
		QCOMPARE(vertical_.connecting("Instagram"), true);

		vertical_.connect_output("YouTube Vertical"); // connected before the start pass, like 00:08:07.956
		QCoreApplication::processEvents();

		QVERIFY(wait_for_log(log, kConnecting.arg("Instagram")));
		QCOMPARE(aitum_->clicked_order, QStringList({"Kick Output"}));
		QCOMPARE(vertical_.calls, QStringList({"stop:YouTube Vertical"}));
		QCOMPARE(vertical_.active("YouTube Vertical"), false);
		QCOMPARE(vertical_.ignored_stops, 0);

		vertical_.connect_output("Instagram"); // connected after the start pass, like 00:08:09.124
		QCoreApplication::processEvents();
		QCOMPARE(vertical_.calls, QStringList({"stop:YouTube Vertical", "stop:Instagram"}));
		QCOMPARE(vertical_.active("Instagram"), false);
		QCOMPARE(vertical_.ignored_stops, 0);
		QCOMPARE(vertical_.double_starts, 0);
	}

	void test_ticked_vertical_already_connecting_is_not_started_again()
	{
		setup(R"({"Main": {"[Vertical] Instagram": true}})", true);
		QSignalSpy log(&FakeObs::log(), &FakeObsLog::line_added);
		start_stream();

		QVERIFY(wait_for_log(log, kAlreadyStarting.arg("Instagram")));
		QVERIFY(wait_for_log(log, kConnecting.arg("YouTube Vertical")));
		QCOMPARE(vertical_.calls, QStringList());

		vertical_.connect_output("Instagram");
		vertical_.connect_output("YouTube Vertical");
		QCoreApplication::processEvents();
		QCOMPARE(vertical_.calls, QStringList({"stop:YouTube Vertical"}));
		QCOMPARE(vertical_.active("Instagram"), true);
		QCOMPARE(vertical_.active("YouTube Vertical"), false);
		QCOMPARE(vertical_.double_starts, 0);
		QCOMPARE(vertical_.ignored_stops, 0);
	}

	void test_ticked_vertical_started_when_aitum_does_not_auto_start()
	{
		setup(R"({"Main": {"[Vertical] Instagram": true}})", false);
		QSignalSpy vcalls(&vertical_, &FakeAitumVertical::called);
		start_stream();
		QVERIFY(vcalls.wait(5000));
		QCOMPARE(vertical_.calls, QStringList({"start:Instagram"}));
		QCOMPARE(vertical_.connecting("Instagram"), true);
		vertical_.connect_output("Instagram");
		QCoreApplication::processEvents();
		QCOMPARE(vertical_.active("Instagram"), true);
		QCOMPARE(vertical_.exists("YouTube Vertical"), false);
		QCOMPARE(vertical_.calls, QStringList({"start:Instagram"}));
		QCOMPARE(vertical_.double_starts, 0);
		QCOMPARE(vertical_.ignored_stops, 0);
	}

	void test_untick_while_connecting_stops_once_live()
	{
		setup(R"({"Main": {"[Vertical] Instagram": true}})", false);
		QSignalSpy vcalls(&vertical_, &FakeAitumVertical::called);
		QSignalSpy log(&FakeObs::log(), &FakeObsLog::line_added);
		start_stream();
		QVERIFY(vcalls.wait(5000));
		QCOMPARE(vertical_.calls, QStringList({"start:Instagram"}));

		QVERIFY(vertical_box("Instagram"));
		vertical_box("Instagram")->click(); // untick while it is still connecting
		QVERIFY(wait_for_log(log, kConnecting.arg("Instagram")));
		QCOMPARE(vertical_.calls, QStringList({"start:Instagram"}));

		vertical_.connect_output("Instagram");
		QCoreApplication::processEvents();
		QCOMPARE(vertical_.calls, QStringList({"start:Instagram", "stop:Instagram"}));
		QCOMPARE(vertical_.active("Instagram"), false);
		QCOMPARE(vertical_.ignored_stops, 0);
	}

	void test_stop_pass_waits_for_connecting_vertical()
	{
		setup(R"({"Main": {"[Vertical] Instagram": true}})", false);
		QSignalSpy vcalls(&vertical_, &FakeAitumVertical::called);
		QSignalSpy log(&FakeObs::log(), &FakeObsLog::line_added);
		start_stream();
		QVERIFY(vcalls.wait(5000));

		// Main stream stopped while Instagram is still connecting.
		FakeObs::streaming_active() = false;
		FakeObs::fire(OBS_FRONTEND_EVENT_STREAMING_STOPPED);
		QVERIFY(wait_for_log(log, kConnecting.arg("Instagram")));
		QCOMPARE(vertical_.calls, QStringList({"start:Instagram"}));

		vertical_.connect_output("Instagram");
		QCoreApplication::processEvents();
		QCOMPARE(vertical_.calls, QStringList({"start:Instagram", "stop:Instagram"}));
		QCOMPARE(vertical_.active("Instagram"), false);
		QCOMPARE(vertical_.ignored_stops, 0);
	}
};

QTEST_MAIN(TestVerticalLifecycle)
#include "test_vertical_lifecycle.moc"
