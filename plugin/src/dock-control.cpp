#include "dock-control.hpp"

#include <obs-frontend-api.h>
#include <obs-module.h>

#include <QDir>
#include <QDockWidget>
#include <QFile>
#include <QGroupBox>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QPushButton>
#include <QWidget>

namespace {

constexpr auto kAitumDockObjectName = "AitumMultistreamDock";
constexpr auto kAitumStreamButtonObjectName = "canvasStream";

QString current_obs_profile_name()
{
	char *profile = obs_frontend_get_current_profile();
	if (!profile)
		return QStringLiteral("Untitled");

	const QString profile_name = QString::fromUtf8(profile);
	bfree(profile);
	return profile_name;
}

QJsonObject load_aitum_config()
{
	const QString path = QDir(qEnvironmentVariable("APPDATA"))
				 .filePath(QStringLiteral(
					 "obs-studio/plugin_config/aitum-multistream/config.json"));

	QFile file(path);
	if (!file.open(QIODevice::ReadOnly)) {
		blog(LOG_WARNING, "[aitum-dock-sync] Failed to open Aitum config: %s",
		     path.toUtf8().constData());
		return {};
	}

	return QJsonDocument::fromJson(file.readAll()).object();
}

QDockWidget *find_aitum_dock()
{
	auto *main_window = static_cast<QWidget *>(obs_frontend_get_main_window());
	if (!main_window)
		return nullptr;

	if (auto *dock =
		    main_window->findChild<QDockWidget *>(QString::fromUtf8(kAitumDockObjectName)))
		return dock;

	const QStringList names = load_aitum_output_names();
	const auto docks = main_window->findChildren<QDockWidget *>();
	for (QDockWidget *dock : docks) {
		for (const QString &name : names) {
			if (dock->findChild<QGroupBox *>(name, Qt::FindChildrenRecursively))
				return dock;
		}
	}

	return nullptr;
}

QPushButton *find_output_stream_button(QDockWidget *dock, const QString &output_name)
{
	if (!dock)
		return nullptr;

	auto *group = dock->findChild<QGroupBox *>(output_name, Qt::FindChildrenRecursively);
	if (!group) {
		blog(LOG_WARNING, "[aitum-dock-sync] Output group not found for '%s'.",
		     output_name.toUtf8().constData());
		return nullptr;
	}

	auto *button =
		group->findChild<QPushButton *>(QString::fromUtf8(kAitumStreamButtonObjectName));
	if (!button) {
		blog(LOG_WARNING, "[aitum-dock-sync] Stream button not found for '%s'.",
		     output_name.toUtf8().constData());
	}

	return button;
}

} // namespace

QStringList load_aitum_output_names()
{
	const QJsonObject config = load_aitum_config();
	const QString profile_name = current_obs_profile_name();
	const QJsonArray profiles = config.value(QStringLiteral("profiles")).toArray();

	for (const QJsonValue &profile_value : profiles) {
		const QJsonObject profile = profile_value.toObject();
		if (profile.value(QStringLiteral("name")).toString() != profile_name)
			continue;

		QStringList names;
		const QJsonArray outputs = profile.value(QStringLiteral("outputs")).toArray();
		for (const QJsonValue &output_value : outputs) {
			const QString name =
				output_value.toObject().value(QStringLiteral("name")).toString();
			if (!name.isEmpty())
				names.push_back(name);
		}
		return names;
	}

	blog(LOG_WARNING, "[aitum-dock-sync] No Aitum profile match for '%s'.",
	     profile_name.toUtf8().constData());
	return {};
}

bool click_aitum_output_button(const QString &output_name, bool start)
{
	QDockWidget *dock = find_aitum_dock();
	if (!dock) {
		blog(LOG_WARNING, "[aitum-dock-sync] Aitum Multistream dock was not found.");
		return false;
	}

	QPushButton *button = find_output_stream_button(dock, output_name);
	if (!button)
		return false;

	const bool was_active = button->isChecked();
	if (start && was_active) {
		blog(LOG_INFO, "[aitum-dock-sync] '%s' is already active.",
		     output_name.toUtf8().constData());
		return true;
	}

	if (!start && !was_active) {
		blog(LOG_INFO, "[aitum-dock-sync] '%s' is already inactive.",
		     output_name.toUtf8().constData());
		return true;
	}

	button->click();
	blog(LOG_INFO, "[aitum-dock-sync] %s '%s' (previous state: %s).",
	     start ? "Started" : "Stopped", output_name.toUtf8().constData(),
	     was_active ? "active" : "inactive");
	return true;
}
