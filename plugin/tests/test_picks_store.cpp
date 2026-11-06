// Chunk 5 (PLAN.md): picks store. Real PicksStore against real files in a temp folder.
#include "picks-store.hpp"

#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTemporaryDir>
#include <QtTest>

namespace {

QJsonObject read_json(const QString &path)
{
	QFile f(path);
	if (!f.open(QIODevice::ReadOnly))
		return {};
	return QJsonDocument::fromJson(f.readAll()).object();
}

void write_raw(const QString &path, const QByteArray &bytes)
{
	QDir().mkpath(QFileInfo(path).absolutePath());
	QFile f(path);
	QVERIFY(f.open(QIODevice::WriteOnly | QIODevice::Truncate));
	QCOMPARE(f.write(bytes), qint64(bytes.size()));
}

} // namespace

class TestPicksStore : public QObject {
	Q_OBJECT

private slots:
	void test_picks_saved_per_profile()
	{
		QTemporaryDir tmp;
		QVERIFY(tmp.isValid());
		// The plugin's config folder may not exist yet on first use.
		const QString path = tmp.filePath("plugin_config/aitum-dock-sync/picks.json");

		PicksStore store(path);
		store.set_picked("Main", "YouTube", true);
		store.set_picked("Vertical", "YouTube", false);
		store.set_picked("Vertical", "TikTok", true);

		QCOMPARE(store.is_picked("Main", "YouTube"), true);
		QCOMPARE(store.is_picked("Main", "TikTok"), false);
		QCOMPARE(store.is_picked("Vertical", "YouTube"), false);
		QCOMPARE(store.is_picked("Vertical", "TikTok"), true);

		const QJsonObject expected = QJsonDocument::fromJson(
			R"({"Main": {"YouTube": true}, "Vertical": {"YouTube": false, "TikTok": true}})").object();
		QCOMPARE(read_json(path), expected);
	}

	void test_picks_survive_reload()
	{
		QTemporaryDir tmp;
		QVERIFY(tmp.isValid());
		const QString path = tmp.filePath("picks.json");

		{
			PicksStore first(path);
			first.set_picked("Main", "YouTube", true);
			first.set_picked("Main", "Kick", true);
			first.set_picked("Vertical", "TikTok", true);
		}
		{
			PicksStore second(path);
			QCOMPARE(second.is_picked("Main", "YouTube"), true);
			QCOMPARE(second.is_picked("Main", "Kick"), true);
			QCOMPARE(second.is_picked("Vertical", "TikTok"), true);
			QCOMPARE(second.is_picked("Vertical", "YouTube"), false);
			second.set_picked("Main", "Kick", false);
		}
		PicksStore third(path);
		QCOMPARE(third.is_picked("Main", "YouTube"), true);
		QCOMPARE(third.is_picked("Main", "Kick"), false);
		QCOMPARE(third.is_picked("Vertical", "TikTok"), true);

		const QJsonObject expected = QJsonDocument::fromJson(
			R"({"Main": {"YouTube": true, "Kick": false}, "Vertical": {"TikTok": true}})").object();
		QCOMPARE(read_json(path), expected);
	}

	void test_unknown_output_defaults_unticked()
	{
		QTemporaryDir tmp;
		QVERIFY(tmp.isValid());

		// No file yet: everything unticked, and reading does not create the file.
		const QString missing = tmp.filePath("missing/picks.json");
		PicksStore empty(missing);
		QCOMPARE(empty.is_picked("Main", "YouTube"), false);
		QCOMPARE(QFile::exists(missing), false);

		// Saved file: known pick loads ticked; other outputs and other profiles stay unticked.
		const QString path = tmp.filePath("picks.json");
		const QByteArray saved = R"({"Main": {"YouTube": true}})";
		write_raw(path, saved);
		PicksStore store(path);
		QCOMPARE(store.is_picked("Main", "YouTube"), true);
		QCOMPARE(store.is_picked("Main", "Twitch"), false);
		QCOMPARE(store.is_picked("Vertical", "YouTube"), false);

		QFile f(path);
		QVERIFY(f.open(QIODevice::ReadOnly));
		QCOMPARE(f.readAll(), saved);
	}
};

QTEST_GUILESS_MAIN(TestPicksStore)
#include "test_picks_store.moc"
