// Added test (Captain Dark 2026-11-02), PLAN.md Part B: "If the Aitum dock or config is
// missing, the dock shows "Aitum Multistream not found" and nothing starts."
#include "fake_obs.hpp"

#include <obs-module.h>

#include <QApplication>
#include <QCheckBox>
#include <QDir>
#include <QFile>
#include <QLabel>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QtTest>

#include <memory>

namespace {

QWidget *our_dock()
{
	for (const FakeObsDock &d : FakeObs::docks())
		if (d.title == QStringLiteral("Aitum Dock Sync"))
			return d.widget;
	return nullptr;
}

QStringList box_texts()
{
	QStringList out;
	if (QWidget *w = our_dock())
		for (QCheckBox *b : w->findChildren<QCheckBox *>())
			out.push_back(b->text());
	return out;
}

QList<bool> box_states()
{
	QList<bool> out;
	if (QWidget *w = our_dock())
		for (QCheckBox *b : w->findChildren<QCheckBox *>())
			out.push_back(b->isChecked());
	return out;
}

// The visible labels' texts in the dock.
QStringList shown_labels()
{
	QStringList out;
	if (QWidget *w = our_dock())
		for (QLabel *l : w->findChildren<QLabel *>())
			if (!l->isHidden())
				out.push_back(l->text());
	return out;
}

bool wait_for_log(QSignalSpy &spy, const QString &line, int timeout_ms = 5000)
{
	while (!FakeObs::log().lines.contains(line))
		if (!spy.wait(timeout_ms))
			return false;
	return true;
}

} // namespace

class TestDockNotFound : public QObject {
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

	void test_dock_shows_not_found_without_aitum()
	{
		const QString appdata = tmp_->filePath("FakeAppData");
		QSignalSpy log(&FakeObs::log(), &FakeObsLog::line_added);

		// Saved pick from an earlier session (the plugin reads picks.json once, at load).
		QDir().mkpath(FakeObs::config_dir());
		{
			QFile f(QDir(FakeObs::config_dir()).filePath("picks.json"));
			QVERIFY(f.open(QIODevice::WriteOnly | QIODevice::Truncate));
			f.write(R"({"Main": {"YouTube": true}})");
		}

		// 1. No Aitum config and no Aitum dock.
		QVERIFY(obs_module_load());
		loaded_ = true;
		FakeObs::fire(OBS_FRONTEND_EVENT_FINISHED_LOADING);
		QCoreApplication::processEvents();
		QCOMPARE(shown_labels(), QStringList({"Aitum Multistream not found"}));
		QCOMPARE(box_texts(), QStringList());

		FakeObs::streaming_active() = true;
		FakeObs::fire(OBS_FRONTEND_EVENT_STREAMING_STARTED);
		QVERIFY(wait_for_log(log, "[aitum-dock-sync] No Aitum outputs found for the current profile."));
		FakeObs::streaming_active() = false;
		FakeObs::fire(OBS_FRONTEND_EVENT_STREAMING_STOPPED);
		QCoreApplication::processEvents();

		// 2. Config with a ticked output, but the Aitum dock is still missing.
		FakeAitumConfig::write(appdata, {{"Main", {"YouTube"}}});
		FakeObs::fire(OBS_FRONTEND_EVENT_PROFILE_CHANGED);
		QCoreApplication::processEvents();
		QCOMPARE(shown_labels(), QStringList({"Aitum Multistream not found"}));
		QCOMPARE(box_texts(), QStringList());

		// 3. Aitum dock appears: the message goes away and the output is listed with its pick.
		FakeAitumDock aitum(window_);
		aitum.add_output("YouTube", false);
		FakeObs::fire(OBS_FRONTEND_EVENT_PROFILE_CHANGED);
		QCoreApplication::processEvents();
		QCOMPARE(shown_labels(), QStringList());
		QCOMPARE(box_texts(), QStringList({"YouTube"}));
		QCOMPARE(box_states(), QList<bool>({true}));
		QCOMPARE(aitum.clicked_order, QStringList());
	}
};

QTEST_MAIN(TestDockNotFound)
#include "test_dock_not_found.moc"
