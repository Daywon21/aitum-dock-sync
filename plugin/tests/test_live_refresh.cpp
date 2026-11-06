// Chunk 8 (PLAN.md): live list refresh, event-driven. A QFileSystemWatcher on Aitum's
// config.json (re-armed after each change) plus an event filter on the Aitum dock that
// reacts to a new output QGroupBox. Whichever fires first refreshes; refresh is idempotent.
// The plugin logs "[aitum-dock-sync] Dock outputs for profile '<p>': <a, b>" when its list
// changes; the tests wait on that real log line (QSignalSpy), never on a timer.
#include "fake_obs.hpp"

#include <obs-module.h>

#include <QApplication>
#include <QCheckBox>
#include <QDir>
#include <QFile>
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

QString list_line(const QString &outputs)
{
	return QStringLiteral("[aitum-dock-sync] Dock outputs for profile 'Main': ") + outputs;
}

// Waits (event-driven) for `line` to be logged at or after index `from`.
bool wait_for_log(QSignalSpy &spy, const QString &line, int from, int timeout_ms = 5000)
{
	for (;;) {
		const QStringList &lines = FakeObs::log().lines;
		for (int i = from; i < lines.size(); ++i)
			if (lines[i] == line)
				return true;
		if (!spy.wait(timeout_ms))
			return false;
	}
}

bool wait_for(QSignalSpy &spy, int count, int timeout_ms = 5000)
{
	while (spy.count() < count)
		if (!spy.wait(timeout_ms))
			return false;
	return true;
}

} // namespace

class TestLiveRefresh : public QObject {
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

	void test_config_change_adds_new_output_unticked()
	{
		FakeAitumConfig::write(appdata(), {{"Main", {"YouTube"}}});
		write_picks(R"({"Main": {"YouTube": true}})");
		FakeAitumDock aitum(window_);
		aitum.add_output("YouTube", false);
		QSignalSpy clicks(&aitum, &FakeAitumDock::clicked);
		QSignalSpy log(&FakeObs::log(), &FakeObsLog::line_added);
		load();
		QCOMPARE(box_texts(), QStringList({"YouTube"}));

		// Live: YouTube starts.
		FakeObs::streaming_active() = true;
		FakeObs::fire(OBS_FRONTEND_EVENT_STREAMING_STARTED);
		QVERIFY(wait_for(clicks, 1));

		// Aitum saves a new output (file replaced): it appears unticked, nothing starts.
		int from = FakeObs::log().lines.size();
		FakeAitumConfig::write(appdata(), {{"Main", {"YouTube", "Kick"}}});
		QVERIFY(wait_for_log(log, list_line("YouTube, Kick"), from));
		QCOMPARE(box_texts(), QStringList({"YouTube", "Kick"}));
		QCOMPARE(box_states(), QList<bool>({true, false}));
		QCOMPARE(aitum.clicked_order, QStringList({"YouTube"}));

		// Second replace: the watcher was re-armed.
		from = FakeObs::log().lines.size();
		FakeAitumConfig::write(appdata(), {{"Main", {"YouTube", "Kick", "TikTok"}}});
		QVERIFY(wait_for_log(log, list_line("YouTube, Kick, TikTok"), from));
		QCOMPARE(box_texts(), QStringList({"YouTube", "Kick", "TikTok"}));
		QCOMPARE(box_states(), QList<bool>({true, false, false}));

		// Ticking the new output while live starts it.
		aitum.add_output("Kick", false);
		QCoreApplication::processEvents();
		QVERIFY(box("Kick"));
		box("Kick")->click();
		QCOMPARE(aitum.clicked_order, QStringList({"YouTube", "Kick"}));
		QCOMPARE(aitum.button("Kick")->isChecked(), true);
		QCOMPARE(read_picks(), json(R"({"Main": {"YouTube": true, "Kick": true}})"));
	}

	void test_new_aitum_groupbox_adds_output_unticked()
	{
		FakeAitumConfig::write(appdata(), {{"Main", {"YouTube"}}});
		write_picks(R"({"Main": {"YouTube": true}})");
		FakeAitumDock aitum(window_);
		aitum.add_output("YouTube", false);
		load();
		QCOMPARE(box_texts(), QStringList({"YouTube"}));

		// Aitum saves its config and adds the output's QGroupBox. Only one event-loop pass
		// follows, so the list has to come from the dock's event filter.
		FakeAitumConfig::write(appdata(), {{"Main", {"YouTube", "Kick"}}});
		aitum.add_output("Kick", false);
		QCoreApplication::processEvents();

		QCOMPARE(box_texts(), QStringList({"YouTube", "Kick"}));
		QCOMPARE(box_states(), QList<bool>({true, false}));
		QCOMPARE(aitum.clicked_order, QStringList());
	}

	void test_removed_output_leaves_dock_keeps_pick()
	{
		FakeAitumConfig::write(appdata(), {{"Main", {"YouTube", "Kick"}}});
		write_picks(R"({"Main": {"YouTube": true, "Kick": true}})");
		FakeAitumDock aitum(window_);
		aitum.add_output("YouTube", false);
		aitum.add_output("Kick", false);
		QSignalSpy log(&FakeObs::log(), &FakeObsLog::line_added);
		load();
		QCOMPARE(box_texts(), QStringList({"YouTube", "Kick"}));

		// Kick removed in Aitum: leaves the dock, its pick stays saved.
		int from = FakeObs::log().lines.size();
		FakeAitumConfig::write(appdata(), {{"Main", {"YouTube"}}});
		aitum.remove_output("Kick");
		QVERIFY(wait_for_log(log, list_line("YouTube"), from));
		QCOMPARE(box_texts(), QStringList({"YouTube"}));
		QCOMPARE(read_picks(), json(R"({"Main": {"YouTube": true, "Kick": true}})"));

		// Kick comes back: listed again, still ticked.
		from = FakeObs::log().lines.size();
		FakeAitumConfig::write(appdata(), {{"Main", {"YouTube", "Kick"}}});
		aitum.add_output("Kick", false);
		QVERIFY(wait_for_log(log, list_line("YouTube, Kick"), from));
		QCOMPARE(box_texts(), QStringList({"YouTube", "Kick"}));
		QCOMPARE(box_states(), QList<bool>({true, true}));
		QCOMPARE(aitum.clicked_order, QStringList());
	}
};

QTEST_MAIN(TestLiveRefresh)
#include "test_live_refresh.moc"
