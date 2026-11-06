// Chunk 11 (Captain Dark 2026-11-02): dock restyled to match OBS's native docks, plus a
// per-row live indicator. Expected values come from OBS 33.0.1's own source:
//  - frontend/forms/OBSBasicControls.ui: outer layout margins 1,0,1,1 spacing 0; row layouts spacing 2.
//  - OBS marks muted info text with the theme class "text-muted" (SourceTreeItem.cpp, AudioMixer.cpp);
//    Yami (and its Light/Classic/... variants) and System define the classes used here.
//  - No custom fonts or colors: every widget keeps the theme font (Qt::WA_SetFont not set).
// Live indicator: a "LIVE" QLabel per row (objectName "liveIndicator", classes
// "text-small text-bold text-danger"), shown while that output's Aitum Multistream stream
// button is checked (followed through its toggled signal), hidden otherwise.
#include "fake_obs.hpp"
#include "fake_obs_api.hpp"

#include <obs-module.h>

#include <QApplication>
#include <QBoxLayout>
#include <QCheckBox>
#include <QDir>
#include <QGroupBox>
#include <QLabel>
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

QGroupBox *group(const QString &title)
{
	if (QWidget *w = our_dock())
		for (QGroupBox *g : w->findChildren<QGroupBox *>())
			if (g->title() == title)
				return g;
	return nullptr;
}

QList<QCheckBox *> rows(const QString &title)
{
	QGroupBox *g = group(title);
	return g ? g->findChildren<QCheckBox *>() : QList<QCheckBox *>();
}

QLabel *live_label(QCheckBox *box)
{
	return box && box->parentWidget() ? box->parentWidget()->findChild<QLabel *>(QStringLiteral("liveIndicator"))
					  : nullptr;
}

// One entry per row: true = LIVE shown, false = hidden; "missing" rows make the list wrong.
QStringList live_states(const QString &title)
{
	QStringList out;
	for (QCheckBox *b : rows(title)) {
		QLabel *l = live_label(b);
		out.push_back(b->text() + (l ? (l->isHidden() ? ":off" : ":LIVE") : ":missing"));
	}
	return out;
}

QStringList custom_font_widgets()
{
	QStringList out;
	if (QWidget *w = our_dock()) {
		if (w->testAttribute(Qt::WA_SetFont))
			out << w->metaObject()->className();
		for (QWidget *c : w->findChildren<QWidget *>())
			if (c->testAttribute(Qt::WA_SetFont))
				out << QString::fromUtf8(c->metaObject()->className()) + ":" + c->objectName();
	}
	return out;
}

} // namespace

class TestDockStyle : public QObject {
	Q_OBJECT

	std::unique_ptr<QTemporaryDir> tmp_;
	QMainWindow *window_ = nullptr;
	bool loaded_ = false;
	FakeAitumVertical vertical_;
	FakeAitumMultistreamVendor vendor_;

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

	void test_live_indicator_follows_output_state()
	{
		FakeAitumConfig::write(appdata(), {{"Main", {"YouTube Horizontal", "Kick"}}});
		FakeAitumDock aitum(window_);
		aitum.add_output("YouTube Horizontal", true); // already live
		aitum.add_output("Kick", false);
		// Multistream hosts the vertical output's group box and button in its dock too.
		aitum.add_output("TikTok", false);
		vertical_.install();
		vendor_.install();
		vendor_.set_outputs({{"YouTube Horizontal", false}, {"Kick", false}, {"TikTok", true}});
		vertical_.add_output("TikTok", false);
		load();

		QCOMPARE(live_states("Main"), QStringList({"YouTube Horizontal:LIVE", "Kick:off"}));
		QCOMPARE(live_states("Vertical"), QStringList({"TikTok:off"}));

		aitum.button("Kick")->setChecked(true);
		aitum.button("YouTube Horizontal")->setChecked(false);
		aitum.button("TikTok")->setChecked(true);
		QCOMPARE(live_states("Main"), QStringList({"YouTube Horizontal:off", "Kick:LIVE"}));
		QCOMPARE(live_states("Vertical"), QStringList({"TikTok:LIVE"}));

		// Multistream rebuilds the TikTok group box (new button, already live): the row follows it.
		aitum.remove_output("TikTok");
		aitum.add_output("TikTok", true);
		QCoreApplication::processEvents();
		QCOMPARE(live_states("Vertical"), QStringList({"TikTok:LIVE"}));
		aitum.button("TikTok")->setChecked(false);
		QCOMPARE(live_states("Vertical"), QStringList({"TikTok:off"}));

		// The indicator never changes the tick.
		QCOMPARE(rows("Main")[1]->isChecked(), false);
	}

	void test_dock_uses_obs_theme_classes_and_margins()
	{
		// Not found first.
		load();
		QWidget *dock = our_dock();
		QVERIFY(dock);
		QLabel *not_found = nullptr;
		for (QLabel *l : dock->findChildren<QLabel *>())
			if (l->text() == QStringLiteral("Aitum Multistream not found"))
				not_found = l;
		QVERIFY(not_found);
		QCOMPARE(not_found->isHidden(), false);
		QCOMPARE(not_found->property("class").toString(), QStringLiteral("text-muted"));

		auto *outer = qobject_cast<QBoxLayout *>(dock->layout());
		QVERIFY(outer);
		QCOMPARE(outer->contentsMargins(), QMargins(1, 0, 1, 1));
		QCOMPARE(outer->spacing(), 0);

		// Aitum appears with one Main and one Vertical output.
		FakeAitumConfig::write(appdata(), {{"Main", {"Kick"}}});
		FakeAitumDock aitum(window_);
		aitum.add_output("Kick", false);
		aitum.add_output("TikTok", false);
		vertical_.install();
		vendor_.install();
		vendor_.set_outputs({{"Kick", false}, {"TikTok", true}});
		vertical_.add_output("TikTok", false);
		FakeObs::fire(OBS_FRONTEND_EVENT_PROFILE_CHANGED);
		QCoreApplication::processEvents();

		QStringList titles;
		for (QGroupBox *g : dock->findChildren<QGroupBox *>())
			titles << g->title();
		QCOMPARE(titles, QStringList({"Main", "Vertical"}));
		QCOMPARE(not_found->isHidden(), true);

		for (const QString &t : {QStringLiteral("Main"), QStringLiteral("Vertical")}) {
			const auto boxes = rows(t);
			QCOMPARE(boxes.size(), 1);
			auto *row = qobject_cast<QBoxLayout *>(boxes[0]->parentWidget()->layout());
			QVERIFY(row);
			QCOMPARE(row->contentsMargins(), QMargins(0, 0, 0, 0));
			QCOMPARE(row->spacing(), 2);
			QLabel *live = live_label(boxes[0]);
			QVERIFY(live);
			QCOMPARE(live->text(), QStringLiteral("LIVE"));
			QCOMPARE(live->property("class").toString(), QStringLiteral("text-small text-bold text-danger"));
			QCOMPARE(live->isHidden(), true);
		}

		QCOMPARE(custom_font_widgets(), QStringList());
	}
};

QTEST_MAIN(TestDockStyle)
#include "test_dock_style.moc"
