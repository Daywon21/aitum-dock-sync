#pragma once

#include <QString>
#include <QStringList>

QStringList load_aitum_output_names();
bool click_aitum_output_button(const QString &output_name, bool start);
