// SPDX-License-Identifier: LicenseRef-AGPL-3.0-only-OpenSSL
// RemoteShift portable mode: a "portable.txt" beside the executable keeps all data next to it.
#pragma once

#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QStandardPaths>
#include <QString>

inline QString RsPortableDataDir()
{
	static const QString dir = [] {
		const QString exe = QCoreApplication::applicationDirPath();
		if(!exe.isEmpty() && QFileInfo::exists(exe + "/portable.txt"))
		{
			const QString d = exe + "/data";
			QDir().mkpath(d);
			return d;
		}
		return QString();
	}();
	return dir;
}

// Where RemoteShift keeps its own files (input profiles).
inline QString RsDataDir()
{
	const QString portable = RsPortableDataDir();
	return portable.isEmpty() ? QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) : portable;
}
