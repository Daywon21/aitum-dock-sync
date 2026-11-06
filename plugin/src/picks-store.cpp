#include "picks-store.hpp"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QSaveFile>

namespace {

QJsonObject load_file(const QString &path)
{
	QFile file(path);
	if (!file.open(QIODevice::ReadOnly))
		return {};
	return QJsonDocument::fromJson(file.readAll()).object();
}

bool save_file(const QString &path, const QJsonObject &picks)
{
	QDir().mkpath(QFileInfo(path).absolutePath());
	QSaveFile file(path);
	if (!file.open(QIODevice::WriteOnly))
		return false;
	file.write(QJsonDocument(picks).toJson(QJsonDocument::Indented));
	return file.commit();
}

} // namespace

PicksStore::PicksStore(const QString &file_path) : file_path_(file_path), picks_(load_file(file_path)) {}

bool PicksStore::is_picked(const QString &profile, const QString &output) const
{
	return picks_.value(profile).toObject().value(output).toBool(false);
}

void PicksStore::set_picked(const QString &profile, const QString &output, bool picked)
{
	QJsonObject profile_picks = picks_.value(profile).toObject();
	profile_picks.insert(output, picked);
	picks_.insert(profile, profile_picks);
	save_file(file_path_, picks_);
}
