#include "style-dump.hpp"

#include <obs-frontend-api.h>
#include <obs-module.h>

#include <QCheckBox>
#include <QDockWidget>
#include <QGroupBox>
#include <QLabel>
#include <QPushButton>
#include <QWidget>

namespace {

QString font_text(const QWidget *w)
{
	const QFont f = w->font();
	return f.pointSizeF() > 0 ? QString::number(f.pointSizeF()) + QStringLiteral("pt")
				  : QString::number(f.pixelSize()) + QStringLiteral("px");
}

QString describe(const QWidget *w, const char *text_key, const QString &text)
{
	return QStringLiteral("%1 objectName='%2' %3='%4' class='%5' font=%6")
		.arg(QString::fromUtf8(w->metaObject()->className()), w->objectName(), QString::fromUtf8(text_key), text,
		     w->property("class").toString(), font_text(w));
}

void log_line(const QString &line)
{
	blog(LOG_INFO, "[aitum-dock-sync] Style dump: %s", line.toUtf8().constData());
}

// The Audio Mixer's source-name label: a QLabel inside OBS's VolumeName widget, or the first label.
QLabel *find_mixer_label(QWidget *main_window)
{
	auto *mixer = main_window ? main_window->findChild<QDockWidget *>(QStringLiteral("mixerDock")) : nullptr;
	if (!mixer)
		return nullptr;
	const auto labels = mixer->findChildren<QLabel *>();
	for (QLabel *l : labels)
		if (l->parentWidget() && QString::fromUtf8(l->parentWidget()->metaObject()->className()) == "VolumeName")
			return l;
	return labels.isEmpty() ? nullptr : labels.first();
}

} // namespace

void log_style_dump(QWidget *dock)
{
	log_line(QStringLiteral("module=") + QString::fromUtf8(obs_get_module_binary_path(obs_current_module())));

	auto *main_window = static_cast<QWidget *>(obs_frontend_get_main_window());
	QLabel *mixer_label = find_mixer_label(main_window);
	if (mixer_label)
		log_line(QStringLiteral("Audio Mixer ") + describe(mixer_label, "text", mixer_label->text()));
	else
		log_line(QStringLiteral("Audio Mixer not found"));

	auto *controls = main_window ? main_window->findChild<QDockWidget *>(QStringLiteral("controlsDock")) : nullptr;
	QPushButton *button = controls ? controls->findChild<QPushButton *>() : nullptr;
	if (button)
		log_line(QStringLiteral("Controls ") + describe(button, "text", button->text()));

	bool all_match = mixer_label != nullptr;
	for (QGroupBox *group : dock->findChildren<QGroupBox *>()) {
		log_line(QStringLiteral("group ") + describe(group, "title", group->title()));
		for (QCheckBox *box : group->findChildren<QCheckBox *>()) {
			QString line = QStringLiteral("row ") + describe(box, "text", box->text());
			QLabel *live = box->parentWidget()->findChild<QLabel *>(QStringLiteral("liveIndicator"));
			if (live)
				line += QStringLiteral("; LIVE QLabel objectName='%1' class='%2' font=%3")
						.arg(live->objectName(), live->property("class").toString(), font_text(live));
			log_line(line);
			if (mixer_label && box->font().pointSizeF() != mixer_label->font().pointSizeF())
				all_match = false;
		}
	}
	log_line(QStringLiteral("rows match Audio Mixer font: ") +
		 (mixer_label ? (all_match ? QStringLiteral("yes") : QStringLiteral("no"))
			      : QStringLiteral("unknown (Audio Mixer not found)")));
}
