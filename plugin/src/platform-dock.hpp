#pragma once

#include <QMetaObject>
#include <QPointer>
#include <QStringList>
#include <QVector>
#include <QWidget>

#include <functional>

class PicksStore;
class QCheckBox;
class QDockWidget;
class QFileSystemWatcher;
class QGroupBox;
class QLabel;
class QVBoxLayout;

// The "Aitum Dock Sync" dock: Aitum outputs of the current OBS profile in two groups, "Main" and
// "Vertical", one row each (checkbox + LIVE indicator). Ticking/unticking saves the pick; while the
// main stream is live it also starts/stops that output right away. Styled only through OBS theme
// classes, so it follows the active theme (Yami, Light, ...).
class PlatformDock : public QWidget {
	Q_OBJECT

public:
	PlatformDock(PicksStore *picks, std::function<bool()> is_live, QWidget *parent = nullptr);

	// Re-reads the outputs and picks for the current profile. Safe to call any number of times.
	void refresh();
	// Called on module unload: the picks store is going away.
	void detach();
	// Watches Aitum's config.json (and its folder, so a replaced file is seen) for changes.
	void start_watching();

protected:
	// Event filter on the Aitum Multistream dock: a new output QGroupBox triggers a refresh.
	bool eventFilter(QObject *watched, QEvent *event) override;
	void showEvent(QShowEvent *event) override;

private:
	struct Row {
		QString name;
		bool vertical;
		QCheckBox *box;
		QLabel *live;
	};

	void on_clicked(const QString &output, bool vertical, bool checked);
	void rebuild(QVBoxLayout *layout, const QStringList &outputs, bool vertical);
	void bind_live();
	void arm_watcher();
	void on_config_event();
	void queue_refresh();
	void hook_aitum_dock(QDockWidget *dock);
	void filter_tree(QObject *object);
	void try_style_dump();

	PicksStore *picks_;
	std::function<bool()> is_live_;
	QLabel *not_found_;
	QGroupBox *main_group_;
	QVBoxLayout *main_layout_;
	QGroupBox *vertical_group_;
	QVBoxLayout *vertical_layout_;
	QVector<Row> rows_;
	QVector<QMetaObject::Connection> live_connections_;
	QString shown_profile_;
	QStringList shown_outputs_;
	QStringList shown_vertical_;
	bool shown_found_ = false;
	bool built_ = false;
	QFileSystemWatcher *watcher_ = nullptr;
	QPointer<QDockWidget> aitum_dock_;
	bool refresh_queued_ = false;
	bool style_dumped_ = false;
};
