// Chunk 14 (Captain Dark 2026-11-03): an UNTICKED Vertical output that is already live when the main
// stream starts (started by hand) is left running AND ticked in the dock, so the checkbox matches.
// Unticked Vertical outputs that Aitum Vertical auto-starts are still stopped (Chunk 13).
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

QString picks_path()
{
	return QDir(FakeObs::config_dir()).filePath(QStringLiteral("picks.json"));
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

} // namespace

class TestHandStartedVertical : public QObject {
	Q_OBJECT

	std::unique_ptr<QTemporaryDir> tmp_;
	QMainWindow *window_ = nullptr;
	bool loaded_ = false;
	FakeAitumVertical vertical_;
	FakeAitumMultistreamVendor vendor_;

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
		FakeObsApi::reset();
		tmp_.reset();
	}

	void test_hand_started_vertical_left_running_and_ticked_at_start()
	{
		FakeAitumConfig::write(tmp_->filePath("FakeAppData"), {{"Main", {"Kick Output"}}});
		QDir().mkpath(FakeObs::config_dir());
		{
			QFile f(picks_path());
			QVERIFY(f.open(QIODevice::WriteOnly | QIODevice::Truncate));
			f.write(R"({"Main": {"Kick Output": true}})");
		}
		auto *aitum = new FakeAitumDock(window_);
		aitum->add_output("Kick Output", false);
		aitum->add_output("YouTube Vertical", true);
		aitum->add_output("Instagram", false);
		vertical_.install();
		vertical_.set_realistic(true);
		vertical_.add_output("YouTube Vertical", true); // started by hand before going live, unticked
		vertical_.add_lazy_output("Instagram");         // unticked, not started
		vendor_.install();
		vendor_.set_outputs({{"Kick Output", false}, {"YouTube Vertical", true}, {"Instagram", true}});
		QSignalSpy log(&FakeObs::log(), &FakeObsLog::line_added);
		QVERIFY(obs_module_load());
		loaded_ = true;
		FakeObs::fire(OBS_FRONTEND_EVENT_FINISHED_LOADING);
		QCoreApplication::processEvents();
		QVERIFY(vertical_box("YouTube Vertical"));
		QCOMPARE(vertical_box("YouTube Vertical")->isChecked(), false);

		FakeObs::streaming_active() = true;
		FakeObs::fire(OBS_FRONTEND_EVENT_STREAMING_STARTED);
		// Start pass: Kick Output, then Instagram (the last entry). Wait until it has finished.
		QVERIFY(wait_for_log(log, "[aitum-dock-sync] Vertical 'Instagram' is already inactive."));
		QCoreApplication::processEvents();

		QCOMPARE(aitum->clicked_order, QStringList({"Kick Output"}));
		QCOMPARE(vertical_.calls, QStringList());
		QCOMPARE(vertical_.active("YouTube Vertical"), true);
		QVERIFY(FakeObs::log().lines.contains(
			"[aitum-dock-sync] Vertical 'YouTube Vertical' was already live when the stream started; ticked it."));
		QCOMPARE(vertical_box("YouTube Vertical")->isChecked(), true);
		QCOMPARE(vertical_box("Instagram")->isChecked(), false);
		QFile f(picks_path());
		QVERIFY(f.open(QIODevice::ReadOnly));
		QCOMPARE(QJsonDocument::fromJson(f.readAll()).object(),
			 QJsonDocument::fromJson(R"({"Main": {"Kick Output": true, "[Vertical] YouTube Vertical": true}})").object());
	}
};

QTEST_MAIN(TestHandStartedVertical)
#include "test_hand_started_vertical.moc"
