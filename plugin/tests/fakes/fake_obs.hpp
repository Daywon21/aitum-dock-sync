#pragma once
// Fakes at the OBS boundary (PLAN.md rule 5). FakeObs supplies the libobs and
// obs-frontend-api functions the plugin calls; FakeAitumDock / FakeAitumConfig stand in
// for the Aitum Multistream plugin (real QDockWidget / QGroupBox / QPushButton widgets and
// a real config.json file).
#include <obs-frontend-api.h>

#include <QDockWidget>
#include <QGroupBox>
#include <QMainWindow>
#include <QObject>
#include <QPair>
#include <QPushButton>
#include <QString>
#include <QStringList>
#include <QVBoxLayout>
#include <QVector>

class FakeObsLog : public QObject {
	Q_OBJECT
public:
	QStringList lines;
	void add(const QString &line)
	{
		lines.push_back(line);
		emit line_added(line);
	}
signals:
	void line_added(const QString &line);
};

struct FakeObsDock {
	QString id;
	QString title;
	QWidget *widget = nullptr;
};

namespace FakeObs {

void reset();
QMainWindow *&main_window();
QString &profile();
QString &config_dir();
bool &streaming_active();
FakeObsLog &log();
QVector<FakeObsDock> &docks();
// Calls every registered frontend event callback, like OBS does.
void fire(enum obs_frontend_event event);
int callback_count();

} // namespace FakeObs

// Aitum Multistream's dock: QDockWidget "AitumMultistreamDock", one QGroupBox per output
// (objectName = output name) with a checkable QPushButton "canvasStream".
class FakeAitumDock : public QObject {
	Q_OBJECT
public:
	explicit FakeAitumDock(QMainWindow *window);
	QPushButton *add_output(const QString &name, bool active);
	void remove_output(const QString &name);
	QPushButton *button(const QString &name) const;
	QDockWidget *dock() const { return dock_; }

	// Every click on a stream button, in order, with the time since start_clock().
	QStringList clicked_order;
	QVector<qint64> clicked_ms;
	void start_clock();

signals:
	void clicked(const QString &name);

private:
	QDockWidget *dock_;
	QWidget *body_;
	QVBoxLayout *layout_;
	qint64 clock_start_ = 0;
};

// Writes <appdata>/obs-studio/plugin_config/aitum-multistream/config.json.
namespace FakeAitumConfig {
QString path(const QString &appdata);
void write(const QString &appdata, const QVector<QPair<QString, QStringList>> &profiles);
} // namespace FakeAitumConfig
