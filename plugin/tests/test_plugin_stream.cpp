// Chunk 6 (PLAN.md): only ticked outputs start with the main stream; stopping stops all.
// Real plugin-main.cpp / dock-control.cpp / picks-store.cpp, driven through FakeObs frontend
// events and a FakeAitumDock with real QGroupBox / QPushButton widgets.
#include "fake_obs.hpp"

#include <obs-module.h>

#include <QApplication>
#include <QDir>
#include <QFile>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QtTest>

#include <memory>

namespace {

void write_picks(const QString &config_dir, const QByteArray &json)
{
	QDir().mkpath(config_dir);
	QFile f(QDir(config_dir).filePath(QStringLiteral("picks.json")));
	QVERIFY(f.open(QIODevice::WriteOnly | QIODevice::Truncate));
	f.write(json);
}

// Waits on the signal (event-driven) until it has fired `count` times in total.
bool wait_for(QSignalSpy &spy, int count, int timeout_ms = 5000)
{
	while (spy.count() < count)
		if (!spy.wait(timeout_ms))
			return false;
	return true;
}

} // namespace

class TestPluginStream : public QObject {
	Q_OBJECT

	std::unique_ptr<QTemporaryDir> tmp_;
	QMainWindow *window_ = nullptr;
	bool loaded_ = false;

private slots:
	void init()
	{
		tmp_ = std::make_unique<QTemporaryDir>();
		QVERIFY(tmp_->isValid());
		FakeObs::reset();
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
		tmp_.reset();
	}

	void test_stream_start_clicks_only_ticked_outputs()
	{
		const QString appdata = tmp_->filePath("FakeAppData");
		FakeAitumConfig::write(appdata, {{"Main", {"YouTube", "Kick", "TikTok"}}, {"Vertical", {"Shorts"}}});
		write_picks(FakeObs::config_dir(),
			    R"({"Main": {"YouTube": true, "Kick": false, "TikTok": true}, "Vertical": {"Shorts": true}})");
		FakeAitumDock aitum(window_);
		aitum.add_output("YouTube", false);
		aitum.add_output("Kick", false);
		aitum.add_output("TikTok", false);
		QSignalSpy clicks(&aitum, &FakeAitumDock::clicked);

		QVERIFY(obs_module_load());
		loaded_ = true;
		FakeObs::fire(OBS_FRONTEND_EVENT_FINISHED_LOADING);
		aitum.start_clock();
		FakeObs::streaming_active() = true;
		FakeObs::fire(OBS_FRONTEND_EVENT_STREAMING_STARTED);

		QVERIFY(wait_for(clicks, 2));
		// Kick sits between YouTube and TikTok, so a wrong Kick click would already be here.
		QCOMPARE(aitum.clicked_order, QStringList({"YouTube", "TikTok"}));
		QCOMPARE(aitum.button("YouTube")->isChecked(), true);
		QCOMPARE(aitum.button("Kick")->isChecked(), false);
		QCOMPARE(aitum.button("TikTok")->isChecked(), true);
		// Start delay 1500 ms and gap 400 ms stay as they are (Qt coarse timers may fire 5% early).
		QVERIFY2(aitum.clicked_ms[0] >= 1425, qPrintable(QString::number(aitum.clicked_ms[0])));
		QVERIFY2(aitum.clicked_ms[1] - aitum.clicked_ms[0] >= 380,
			 qPrintable(QString::number(aitum.clicked_ms[1] - aitum.clicked_ms[0])));
	}

	void test_stream_stop_stops_all_outputs()
	{
		const QString appdata = tmp_->filePath("FakeAppData");
		FakeAitumConfig::write(appdata, {{"Main", {"YouTube", "Kick", "TikTok"}}});
		write_picks(FakeObs::config_dir(), R"({"Main": {"YouTube": true}})");
		FakeAitumDock aitum(window_);
		// Kick and TikTok were started by hand in Aitum; they are unticked here.
		aitum.add_output("YouTube", true);
		aitum.add_output("Kick", true);
		aitum.add_output("TikTok", true);
		QSignalSpy clicks(&aitum, &FakeAitumDock::clicked);
		QSignalSpy log(&FakeObs::log(), &FakeObsLog::line_added);

		QVERIFY(obs_module_load());
		loaded_ = true;
		FakeObs::fire(OBS_FRONTEND_EVENT_FINISHED_LOADING);
		FakeObs::streaming_active() = true;
		FakeObs::fire(OBS_FRONTEND_EVENT_STREAMING_STARTED);

		// Start pass finished: YouTube was already active, nothing clicked.
		while (!FakeObs::log().lines.contains("[aitum-dock-sync] 'YouTube' is already active."))
			QVERIFY(log.wait(5000));
		QCOMPARE(aitum.clicked_order, QStringList());

		FakeObs::streaming_active() = false;
		FakeObs::fire(OBS_FRONTEND_EVENT_STREAMING_STOPPED);

		QVERIFY(wait_for(clicks, 3));
		QCOMPARE(aitum.clicked_order, QStringList({"YouTube", "Kick", "TikTok"}));
		QCOMPARE(aitum.button("YouTube")->isChecked(), false);
		QCOMPARE(aitum.button("Kick")->isChecked(), false);
		QCOMPARE(aitum.button("TikTok")->isChecked(), false);
	}
};

QTEST_MAIN(TestPluginStream)
#include "test_plugin_stream.moc"
