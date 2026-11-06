#include "platform-dock.hpp"

#include "dock-control.hpp"
#include "picks-store.hpp"
#include "style-dump.hpp"
#include "vertical-control.hpp"

#include <obs-module.h>

#include <QCheckBox>
#include <QChildEvent>
#include <QDir>
#include <QDockWidget>
#include <QFileInfo>
#include <QFileSystemWatcher>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QMetaObject>
#include <QPushButton>
#include <QVBoxLayout>

namespace {

// Layout values from OBS 33.0.1's own Controls dock (frontend/forms/OBSBasicControls.ui).
constexpr int kOuterSide = 1;
constexpr int kOuterBottom = 1;
constexpr int kRowSpacing = 2;

QGroupBox *make_group(const QString &title, QWidget *parent, QVBoxLayout **layout)
{
	auto *group = new QGroupBox(title, parent);
	*layout = new QVBoxLayout(group);
	(*layout)->setContentsMargins(kRowSpacing, 0, kRowSpacing, 0);
	(*layout)->setSpacing(0);
	return group;
}

} // namespace

PlatformDock::PlatformDock(PicksStore *picks, std::function<bool()> is_live, QWidget *parent)
	: QWidget(parent), picks_(picks), is_live_(std::move(is_live))
{
	auto *layout = new QVBoxLayout(this);
	layout->setContentsMargins(kOuterSide, 0, kOuterSide, kOuterBottom);
	layout->setSpacing(0);

	// Muted info text, like OBS's own (theme class "text-muted").
	not_found_ = new QLabel(QStringLiteral("Aitum Multistream not found"), this);
	not_found_->setProperty("class", "text-muted");
	not_found_->setWordWrap(true);
	not_found_->setHidden(true);
	layout->addWidget(not_found_);

	main_group_ = make_group(QStringLiteral("Main"), this, &main_layout_);
	layout->addWidget(main_group_);
	vertical_group_ = make_group(QStringLiteral("Vertical"), this, &vertical_layout_);
	vertical_group_->setHidden(true);
	layout->addWidget(vertical_group_);
	layout->addStretch(1);
}

void PlatformDock::detach()
{
	picks_ = nullptr;
}

void PlatformDock::rebuild(QVBoxLayout *layout, const QStringList &outputs, bool vertical)
{
	for (int i = rows_.size() - 1; i >= 0; --i) {
		if (rows_[i].vertical != vertical)
			continue;
		delete rows_[i].box->parentWidget();
		rows_.remove(i);
	}

	QWidget *group = layout->parentWidget();
	for (const QString &name : outputs) {
		auto *row = new QWidget(group);
		auto *row_layout = new QHBoxLayout(row);
		row_layout->setContentsMargins(0, 0, 0, 0);
		row_layout->setSpacing(kRowSpacing);

		auto *box = new QCheckBox(name, row);
		// Same theme size as OBS's Audio Mixer names (Yami: .text-small = --font_small).
		box->setProperty("class", "text-small");
		auto *live = new QLabel(QStringLiteral("LIVE"), row);
		live->setObjectName(QStringLiteral("liveIndicator"));
		live->setProperty("class", "text-small text-bold text-danger");
		live->setToolTip(QStringLiteral("Streaming"));
		QSizePolicy policy = live->sizePolicy();
		policy.setRetainSizeWhenHidden(true);
		live->setSizePolicy(policy);
		live->setHidden(true);

		row_layout->addWidget(box, 1);
		row_layout->addWidget(live, 0);
		layout->addWidget(row);

		QObject::connect(box, &QCheckBox::clicked, this,
				 [this, name, vertical](bool checked) { on_clicked(name, vertical, checked); });
		rows_.push_back({name, vertical, box, live});
	}
}

// Each row's LIVE label follows that output's stream button in the Aitum Multistream dock
// (Multistream checks it while the output is streaming). Re-bound on every refresh, because
// Multistream rebuilds its group boxes.
void PlatformDock::bind_live()
{
	for (const QMetaObject::Connection &c : live_connections_)
		QObject::disconnect(c);
	live_connections_.clear();

	for (const Row &row : rows_) {
		QPushButton *button = find_aitum_stream_button(row.name);
		row.live->setHidden(!(button && button->isChecked()));
		if (!button)
			continue;
		QLabel *live = row.live;
		live_connections_.push_back(
			QObject::connect(button, &QPushButton::toggled, live, [live](bool on) { live->setHidden(!on); }));
	}
}

void PlatformDock::refresh()
{
	const QString profile = current_obs_profile_name();
	const bool found = aitum_multistream_found();
	const QStringList outputs = found ? load_aitum_output_names() : QStringList();
	const QStringList vertical = found ? load_vertical_output_names() : QStringList();
	vertical_watch(vertical);

	const bool main_changed = !built_ || profile != shown_profile_ || outputs != shown_outputs_ || found != shown_found_;
	const bool vertical_changed = !built_ || profile != shown_profile_ || vertical != shown_vertical_ ||
				      found != shown_found_;

	if (main_changed) {
		rebuild(main_layout_, outputs, false);
		blog(LOG_INFO, "[aitum-dock-sync] Dock outputs for profile '%s': %s", profile.toUtf8().constData(),
		     outputs.isEmpty() ? "(none)" : outputs.join(QStringLiteral(", ")).toUtf8().constData());
	}
	if (vertical_changed) {
		rebuild(vertical_layout_, vertical, true);
		if (!vertical.isEmpty() || !shown_vertical_.isEmpty())
			blog(LOG_INFO, "[aitum-dock-sync] Dock vertical outputs for profile '%s': %s",
			     profile.toUtf8().constData(),
			     vertical.isEmpty() ? "(none)" : vertical.join(QStringLiteral(", ")).toUtf8().constData());
	}
	shown_profile_ = profile;
	shown_outputs_ = outputs;
	shown_vertical_ = vertical;
	shown_found_ = found;
	built_ = true;

	not_found_->setHidden(found);
	main_group_->setHidden(!found);
	vertical_group_->setHidden(vertical.isEmpty());
	for (const Row &row : rows_) {
		const QSignalBlocker block(row.box);
		const QString key = row.vertical ? vertical_pick_key(row.name) : row.name;
		row.box->setChecked(picks_ && picks_->is_picked(profile, key));
	}

	if (found)
		hook_aitum_dock(find_aitum_multistream_dock());
	bind_live();
	if (isVisible() && !style_dumped_)
		QMetaObject::invokeMethod(this, [this] { try_style_dump(); }, Qt::QueuedConnection);
}

void PlatformDock::showEvent(QShowEvent *event)
{
	QWidget::showEvent(event);
	if (!style_dumped_)
		QMetaObject::invokeMethod(this, [this] { try_style_dump(); }, Qt::QueuedConnection);
}

// Once, after OBS's theme has styled the visible rows.
void PlatformDock::try_style_dump()
{
	if (style_dumped_ || !isVisible() || rows_.isEmpty())
		return;
	style_dumped_ = true;
	log_style_dump(this);
}

void PlatformDock::on_clicked(const QString &output, bool vertical, bool checked)
{
	if (!picks_)
		return;

	const QString profile = current_obs_profile_name();
	picks_->set_picked(profile, vertical ? vertical_pick_key(output) : output, checked);
	blog(LOG_INFO, "[aitum-dock-sync] %s'%s' %s for profile '%s'.", vertical ? "Vertical " : "",
	     output.toUtf8().constData(), checked ? "ticked" : "unticked", profile.toUtf8().constData());

	if (is_live_ && is_live_()) {
		if (vertical)
			set_vertical_output(output, checked);
		else
			click_aitum_output_button(output, checked);
	}
}

void PlatformDock::start_watching()
{
	if (watcher_)
		return;
	watcher_ = new QFileSystemWatcher(this);
	QObject::connect(watcher_, &QFileSystemWatcher::fileChanged, this, [this] { on_config_event(); });
	QObject::connect(watcher_, &QFileSystemWatcher::directoryChanged, this, [this] { on_config_event(); });
	arm_watcher();
}

// Editors and Aitum replace config.json, which drops it from the watcher: add it back.
void PlatformDock::arm_watcher()
{
	if (!watcher_)
		return;
	const QString file = aitum_config_file_path();
	const QString dir = QFileInfo(file).absolutePath();
	if (QFileInfo::exists(dir) && !watcher_->directories().contains(dir))
		watcher_->addPath(dir);
	if (QFileInfo::exists(file) && !watcher_->files().contains(file))
		watcher_->addPath(file);
}

void PlatformDock::on_config_event()
{
	arm_watcher();
	queue_refresh();
}

// Several events can arrive together (file + folder + new widgets): one refresh for all.
void PlatformDock::queue_refresh()
{
	if (refresh_queued_)
		return;
	refresh_queued_ = true;
	QMetaObject::invokeMethod(
		this,
		[this] {
			refresh_queued_ = false;
			refresh();
		},
		Qt::QueuedConnection);
}

void PlatformDock::hook_aitum_dock(QDockWidget *dock)
{
	if (!dock || dock == aitum_dock_)
		return;
	aitum_dock_ = dock;
	filter_tree(dock);
}

void PlatformDock::filter_tree(QObject *object)
{
	object->installEventFilter(this);
	for (QObject *child : object->children())
		if (child->isWidgetType())
			filter_tree(child);
}

bool PlatformDock::eventFilter(QObject *watched, QEvent *event)
{
	if (event->type() == QEvent::ChildAdded) {
		QObject *child = static_cast<QChildEvent *>(event)->child();
		if (child && child->isWidgetType()) {
			filter_tree(child);
			// A group box added whole, or a group box getting its first widgets while Aitum builds it.
			if (qobject_cast<QGroupBox *>(child) || qobject_cast<QGroupBox *>(watched))
				queue_refresh();
		}
	}
	return QWidget::eventFilter(watched, event);
}
