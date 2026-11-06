#pragma once

#include <QJsonObject>
#include <QString>

// Which Aitum outputs Captain Dark ticked, per OBS profile.
// Stored in picks.json as {"<profile>": {"<output>": true|false}}.
// An output with no saved pick is unticked.
class PicksStore {
public:
	explicit PicksStore(const QString &file_path);

	bool is_picked(const QString &profile, const QString &output) const;
	// Saves to the file immediately.
	void set_picked(const QString &profile, const QString &output, bool picked);

private:
	QString file_path_;
	QJsonObject picks_;
};
