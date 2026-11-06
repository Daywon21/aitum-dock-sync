// Chunk 7 (PLAN.md): the "Aitum Dock Sync" dock and live toggling.
// Real plugin code driven through FakeObs and FakeAitumDock / FakeAitumConfig.
#include "fake_obs.hpp"

#include <obs-module.h>

#include <QApplication>
#include <QCheckBox>
#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLabel>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QtTest>

#include <memory>

namespace {

const char *kNotFound = "Aitum Multistream not found";

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

QJsonObject read_picks()
{
	QFile f(picks_path());
	if (!f.open(QIODevice::ReadOnly))
		return {};
	return QJsonDocument::fromJson(f.readAll()).object();
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

QList<QCheckBox *> boxes()
{
	QWidget *w = our_dock();
	return w ? w->findChildren<QCheckBox *>() : QList<QCheckBox *>();
}

QStringList box_texts()
{
	QStringList out;
	for (QCheckBox *b : boxes())
		out.push_back(b->text());
	return out;
}

QList<bool> box_states()
{
	QList<bool> out;
	for (QCheckBox *b : boxes())
		out.push_back(b->isChecked());
	return out;
}

QCheckBox *box(const QString &name)
{
	for (QCheckBox *b : boxes())
		if (b->text() == name)
			return b;
	return nullptr;
}

bool not_found_shown()
{
	QWidget *w = our_dock();
	if (!w)
		return false;
	for (QLabel *l : w->findChildren<QLabel *>())
		if (l->text() == QString::fromUtf8(kNotFound) && !l->isHidden())
			return true;
	return false;
}

bool wait_for(QSignalSpy &spy, int count, int timeout_ms = 5000)
{
	while (spy.count() < count)
		if (!spy.wait(timeout_ms))
			return false;
	return true;
}

} // namespace

class TestPlatformDock : public QObject {
	Q_OBJECT

	std::unique_ptr<QTemporaryDir> tmp_;
	QMainWindow *window_ = nullptr;
	bool loaded_ = false;

	QString appdata() const { return tmp_->filePath("FakeAppData"); }

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
		tmp_.reset();
	}

	void test_dock_lists_aitum_outputs_for_current_profile()
	{
		FakeAitumConfig::write(appdata(), {{"Main", {"YouTube", "Kick", "TikTok"}}, {"Vertical", {"Shorts", "Reels"}}});
		write_picks(R"({"Main": {"YouTube": true, "Kick": false}, "Vertical": {"Shorts": true}})");
		FakeAitumDock aitum(window_);
		aitum.add_output("YouTube", false);
		aitum.add_output("Kick", false);
		aitum.add_output("TikTok", false);

		load();

		int ours = 0;
		for (const FakeObsDock &d : FakeObs::docks())
			if (d.title == QStringLiteral("Aitum Dock Sync"))
				++ours;
		QCOMPARE(ours, 1);
		QCOMPARE(box_texts(), QStringList({"YouTube", "Kick", "TikTok"}));
		QCOMPARE(box_states(), QList<bool>({true, false, false}));
		QCOMPARE(not_found_shown(), false);
		QCOMPARE(aitum.clicked_order, QStringList());
	}

	void test_tick_while_live_starts_output()
	{
		FakeAitumConfig::write(appdata(), {{"Main", {"YouTube", "Kick", "TikTok"}}});
		write_picks(R"({"Main": {"YouTube": true}})");
		FakeAitumDock aitum(window_);
		aitum.add_output("YouTube", false);
		aitum.add_output("Kick", false);
		aitum.add_output("TikTok", false);
		QSignalSpy clicks(&aitum, &FakeAitumDock::clicked);
		load();

		FakeObs::streaming_active() = true;
		FakeObs::fire(OBS_FRONTEND_EVENT_STREAMING_STARTED);
		QVERIFY(wait_for(clicks, 1));
		QCOMPARE(aitum.clicked_order, QStringList({"YouTube"}));

		QVERIFY(box("Kick"));
		box("Kick")->click();

		QCOMPARE(aitum.clicked_order, QStringList({"YouTube", "Kick"}));
		QCOMPARE(aitum.button("Kick")->isChecked(), true);
		QCOMPARE(aitum.button("TikTok")->isChecked(), false);
		QCOMPARE(read_picks(), json(R"({"Main": {"YouTube": true, "Kick": true}})"));
	}

	void test_untick_while_live_stops_output()
	{
		FakeAitumConfig::write(appdata(), {{"Main", {"YouTube", "Kick", "TikTok"}}});
		write_picks(R"({"Main": {"YouTube": true, "Kick": true}})");
		FakeAitumDock aitum(window_);
		aitum.add_output("YouTube", false);
		aitum.add_output("Kick", false);
		aitum.add_output("TikTok", false);
		QSignalSpy clicks(&aitum, &FakeAitumDock::clicked);
		load();

		FakeObs::streaming_active() = true;
		FakeObs::fire(OBS_FRONTEND_EVENT_STREAMING_STARTED);
		QVERIFY(wait_for(clicks, 2));
		QCOMPARE(aitum.clicked_order, QStringList({"YouTube", "Kick"}));

		QVERIFY(box("Kick"));
		box("Kick")->click();

		QCOMPARE(aitum.clicked_order, QStringList({"YouTube", "Kick", "Kick"}));
		QCOMPARE(aitum.button("Kick")->isChecked(), false);
		QCOMPARE(aitum.button("YouTube")->isChecked(), true);
		QCOMPARE(read_picks(), json(R"({"Main": {"YouTube": true, "Kick": false}})"));
	}

	void test_toggle_while_offline_only_saves()
	{
		FakeAitumConfig::write(appdata(), {{"Main", {"YouTube", "Kick", "TikTok"}}});
		FakeAitumDock aitum(window_);
		aitum.add_output("YouTube", false);
		aitum.add_output("Kick", false);
		aitum.add_output("TikTok", false);
		load();

		QCOMPARE(box_states(), QList<bool>({false, false, false}));
		QVERIFY(box("YouTube") && box("Kick"));
		box("YouTube")->click();
		box("Kick")->click();
		box("Kick")->click();
		QCoreApplication::processEvents();

		QCOMPARE(box_states(), QList<bool>({true, false, false}));
		QCOMPARE(aitum.clicked_order, QStringList());
		QCOMPARE(aitum.button("YouTube")->isChecked(), false);
		QCOMPARE(aitum.button("Kick")->isChecked(), false);
		QCOMPARE(read_picks(), json(R"({"Main": {"YouTube": true, "Kick": false}})"));
	}

	void test_profile_change_reloads_list_and_picks()
	{
		FakeAitumConfig::write(appdata(), {{"Main", {"YouTube", "Kick", "TikTok"}}, {"Vertical", {"Shorts", "Reels"}}});
		write_picks(R"({"Main": {"YouTube": true}, "Vertical": {"Reels": true}})");
		FakeAitumDock aitum(window_);
		aitum.add_output("YouTube", false);
		aitum.add_output("Kick", false);
		aitum.add_output("TikTok", false);
		load();
		QCOMPARE(box_texts(), QStringList({"YouTube", "Kick", "TikTok"}));
		QCOMPARE(box_states(), QList<bool>({true, false, false}));

		// Switching OBS profile: Aitum rebuilds its dock for the new profile.
		FakeObs::profile() = QStringLiteral("Vertical");
		aitum.remove_output("YouTube");
		aitum.remove_output("Kick");
		aitum.remove_output("TikTok");
		aitum.add_output("Shorts", false);
		aitum.add_output("Reels", false);
		FakeObs::fire(OBS_FRONTEND_EVENT_PROFILE_CHANGED);
		QCoreApplication::processEvents();

		QCOMPARE(box_texts(), QStringList({"Shorts", "Reels"}));
		QCOMPARE(box_states(), QList<bool>({false, true}));

		QVERIFY(box("Shorts"));
		box("Shorts")->click();
		QCOMPARE(read_picks(), json(R"({"Main": {"YouTube": true}, "Vertical": {"Reels": true, "Shorts": true}})"));
		QCOMPARE(aitum.clicked_order, QStringList());
	}
};

QTEST_MAIN(TestPlatformDock)
#include "test_platform_dock.moc"
