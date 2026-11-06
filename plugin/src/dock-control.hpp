#pragma once

#include <QString>
#include <QStringList>

class QDockWidget;
class QPushButton;

QString current_obs_profile_name();
QStringList load_aitum_output_names();
// True when Aitum's config.json and its Multistream dock both exist.
bool aitum_multistream_found();
// Aitum Multistream's config.json (read-only for this plugin).
QString aitum_config_file_path();
QDockWidget *find_aitum_multistream_dock();
// The output's stream button in the Aitum Multistream dock (Main or Vertical), or nullptr. No log.
QPushButton *find_aitum_stream_button(const QString &output_name);
bool click_aitum_output_button(const QString &output_name, bool start);
