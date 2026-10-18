// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QVector>
#include <QString>
#include <optional>

struct PhotoEntry {
    int number = 0;
    QString name;
    QString path;
};

class PhotoCdAlbum {
public:
    QString photoCdDirectory;
    QString imagesDirectory;
    QString discRootDirectory;
    QString volumeName;
    QString identity;
    QString startupPath;
    QString overviewPath;
    QString infoPath;
    QString optionsPath;
    QVector<PhotoEntry> photos;

    static std::optional<PhotoCdAlbum> open(const QString &path, QString *error = nullptr);
    static std::optional<PhotoCdAlbum> discoverMounted(QString *error = nullptr);
};
