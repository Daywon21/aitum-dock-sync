// Chunk 12 (Captain Dark 2026-11-02): after installing Chunk 11, the rows were still larger than
// OBS's Audio Mixer text. Cause, from OBS 33.0.1's Yami.obt: every widget gets
// "QWidget { font-size: var(--font_base) }", while the Audio Mixer's names get
// "VolumeName QLabel { font-size: var(--font_small) }". Fix: rows use OBS's own theme class
// "text-small" (".text-small { font-size: var(--font_small) }"), still no custom font.
// Font sizes are the requested sizes (QFont::pointSizeF) after the style sheet is applied.
// Also: a one-time style dump in the OBS log (rows vs. Audio Mixer), so the runtime result is visible.
//
// The style sheet below reproduces those Yami rules with Yami's default sizes (obsFontScale 10:
// font_base 10pt, font_small max(7pt, 0.8 * 10pt) = 8pt). The Audio Mixer stand-in is a QLabel
// inside a QDockWidget named "mixerDock" (OBSBasic.ui); "#mixerDock QLabel" stands in for
// "VolumeName QLabel".
#include "fake_obs.hpp"
#include "fake_obs_api.hpp"

#include <obs-module.h>

#include <QApplication>
#include <QCheckBox>
#include <QDockWidget>
#include <QFontInfo>
#include <QGroupBox>
#include <QLabel>
#include <QPushButton>
#include <QTemporaryDir>
#include <QtTest>

#include <memory>

namespace {

const char *kYamiRules = R"(
QWidget { font-size: 10pt; }
#mixerDock QLabel { font-size: 8pt; }
[class~="text-small"] { font-size: 8pt; }
)";

QWidget *our_dock()
{
	for (const FakeObsDock &d : FakeObs::docks())
		if (d.title == QStringLiteral("Aitum Dock Sync"))
			return d.widget;
	return nullptr;
}

QStringList dump_lines()
{
	QStringList out;
	for (const QString &l : FakeObs::log().lines)
		if (l.startsWith(QStringLiteral("[aitum-dock-sync] Style dump: ")))
			out << l;
	return out;
}

} // namespace

class TestStyleMatch : public QObject {
	Q_OBJECT

	std::unique_ptr<QTemporaryDir> tmp_;
	QMainWindow *window_ = nullptr;
	bool loaded_ = false;
	FakeAitumVertical vertical_;
	FakeAitumMultistreamVendor vendor_;

	void setup()
	{
		const QString appdata = tmp_->filePath("FakeAppData");
		FakeAitumConfig::write(appdata, {{"Main", {"Kick Output", "X (Twitter) Output"}}});
		auto *aitum = new FakeAitumDock(window_);
		aitum->add_output("Kick Output", false);
		aitum->add_output("X (Twitter) Output", false);
		aitum->add_output("Instagram", false);
		vertical_.install();
		vendor_.install();
		vendor_.set_outputs({{"Kick Output", false}, {"X (Twitter) Output", false}, {"Instagram", true}});
		vertical_.add_output("Instagram", false);

		// OBS's Audio Mixer and Controls docks.
		auto *mixer = new QDockWidget(QStringLiteral("Audio Mixer"), window_);
		mixer->setObjectName(QStringLiteral("mixerDock"));
		mixer->setWidget(new QLabel(QStringLiteral("Mic/Aux"), mixer));
		auto *controls = new QDockWidget(QStringLiteral("Controls"), window_);
		controls->setObjectName(QStringLiteral("controlsDock"));
		controls->setWidget(new QPushButton(QStringLiteral("Start Streaming"), controls));
		window_->show();

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
		qApp->setStyleSheet(QString::fromUtf8(kYamiRules));
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
		qApp->setStyleSheet(QString());
		tmp_.reset();
	}

	void test_rows_match_audio_mixer_font_under_yami_rules()
	{
		setup();
		QWidget *dock = our_dock();
		QVERIFY(dock);
		dock->show();
		QCoreApplication::processEvents();

		auto *mixer_label = window_->findChild<QDockWidget *>("mixerDock")->findChild<QLabel *>();
		const qreal mixer_pt = mixer_label->font().pointSizeF();
		QCOMPARE(mixer_pt, 8.0);

		QStringList rows;
		for (QCheckBox *b : dock->findChildren<QCheckBox *>())
			rows << b->text() + "=" + QString::number(b->font().pointSizeF()) + "pt class=" +
					b->property("class").toString();
		QCOMPARE(rows, QStringList({"Kick Output=8pt class=text-small", "X (Twitter) Output=8pt class=text-small",
					    "Instagram=8pt class=text-small"}));
	}

	void test_style_dump_logged_once()
	{
		setup();
		QWidget *dock = our_dock();
		QVERIFY(dock);
		QCOMPARE(dump_lines(), QStringList()); // not shown yet: nothing dumped

		dock->show();
		QCoreApplication::processEvents();
		const QStringList first = dump_lines();
		QCOMPARE(first,
			 QStringList({
				 "[aitum-dock-sync] Style dump: module=FakeObs/plugins/aitum-dock-sync/aitum-dock-sync.dll",
				 "[aitum-dock-sync] Style dump: Audio Mixer QLabel objectName='' text='Mic/Aux' class='' font=8pt",
				 "[aitum-dock-sync] Style dump: Controls QPushButton objectName='' text='Start Streaming' class='' font=10pt",
				 "[aitum-dock-sync] Style dump: group QGroupBox objectName='' title='Main' class='' font=10pt",
				 "[aitum-dock-sync] Style dump: row QCheckBox objectName='' text='Kick Output' class='text-small' font=8pt; LIVE QLabel objectName='liveIndicator' class='text-small text-bold text-danger' font=8pt",
				 "[aitum-dock-sync] Style dump: row QCheckBox objectName='' text='X (Twitter) Output' class='text-small' font=8pt; LIVE QLabel objectName='liveIndicator' class='text-small text-bold text-danger' font=8pt",
				 "[aitum-dock-sync] Style dump: group QGroupBox objectName='' title='Vertical' class='' font=10pt",
				 "[aitum-dock-sync] Style dump: row QCheckBox objectName='' text='Instagram' class='text-small' font=8pt; LIVE QLabel objectName='liveIndicator' class='text-small text-bold text-danger' font=8pt",
				 "[aitum-dock-sync] Style dump: rows match Audio Mixer font: yes",
			 }));

		// Hidden and shown again, or refreshed: no second dump.
		dock->hide();
		dock->show();
		FakeObs::fire(OBS_FRONTEND_EVENT_PROFILE_CHANGED);
		QCoreApplication::processEvents();
		QCOMPARE(dump_lines(), first);
	}
};

QTEST_MAIN(TestStyleMatch)
#include "test_style_match.moc"
