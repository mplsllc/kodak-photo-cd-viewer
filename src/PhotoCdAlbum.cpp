// SPDX-License-Identifier: GPL-3.0-or-later
#include "PhotoCdAlbum.h"

#include <QCryptographicHash>
#include <QDir>
#include <QFileInfo>
#include <QRegularExpression>
#include <QStorageInfo>
#include <QByteArray>
#include <QStringList>
#include <algorithm>

namespace {
QString namedChildDirectory(const QString &parent, const QString &wanted)
{
    const QDir dir(parent);
    const auto entries = dir.entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name);
    for (const QFileInfo &entry : entries) {
        if (entry.fileName().compare(wanted, Qt::CaseInsensitive) == 0)
            return entry.absoluteFilePath();
    }
    return {};
}

QString namedChildFile(const QString &parent, const QString &wanted)
{
    const QDir dir(parent);
    const auto entries = dir.entryInfoList(QDir::Files | QDir::NoDotAndDotDot, QDir::Name);
    for (const QFileInfo &entry : entries) {
        if (entry.fileName().compare(wanted, Qt::CaseInsensitive) == 0)
            return entry.absoluteFilePath();
    }
    return {};
}

QString locatePhotoCdDirectory(const QString &input)
{
    QFileInfo given(input);
    QString candidate = given.isFile() ? given.absolutePath() : given.absoluteFilePath();
    if (!QFileInfo(candidate).isDir())
        return {};

    const QString leaf = QFileInfo(candidate).fileName();
    QStringList possibilities;
    possibilities << candidate;
    if (leaf.compare(QStringLiteral("IMAGES"), Qt::CaseInsensitive) == 0)
        possibilities << QFileInfo(candidate).dir().absolutePath();
    possibilities << QFileInfo(candidate).dir().absolutePath();

    for (const QString &possible : possibilities) {
        if (QFileInfo(possible).fileName().compare(QStringLiteral("PHOTO_CD"), Qt::CaseInsensitive) == 0) {
            if (!namedChildDirectory(possible, QStringLiteral("IMAGES")).isEmpty())
                return QDir(possible).canonicalPath();
        }
        const QString child = namedChildDirectory(possible, QStringLiteral("PHOTO_CD"));
        if (!child.isEmpty() && !namedChildDirectory(child, QStringLiteral("IMAGES")).isEmpty())
            return QDir(child).canonicalPath();
    }
    return {};
}

QString canonicalOrAbsolute(const QString &path)
{
    const QString canonical = QFileInfo(path).canonicalFilePath();
    return QDir::cleanPath(canonical.isEmpty() ? QFileInfo(path).absoluteFilePath() : canonical);
}

QString selectedDiscRoot(const QString &input)
{
    QFileInfo given(input);
    QString root = given.isFile() ? given.absolutePath() : given.absoluteFilePath();
    const QString leaf = QFileInfo(root).fileName();
    if (leaf.compare(QStringLiteral("IMAGES"), Qt::CaseInsensitive) == 0)
        root = QFileInfo(root).dir().absolutePath();
    if (QFileInfo(root).fileName().compare(QStringLiteral("PHOTO_CD"), Qt::CaseInsensitive) == 0)
        root = QFileInfo(root).dir().absolutePath();
    return canonicalOrAbsolute(root);
}
}

std::optional<PhotoCdAlbum> PhotoCdAlbum::open(const QString &path, QString *error)
{
    auto fail = [error](const QString &message) -> std::optional<PhotoCdAlbum> {
        if (error)
            *error = message;
        return std::nullopt;
    };

    if (path.trimmed().isEmpty())
        return fail(QStringLiteral("Choose a mounted Photo CD or a folder containing PHOTO_CD/IMAGES."));

    const QString photoCd = locatePhotoCdDirectory(path);
    if (photoCd.isEmpty())
        return fail(QStringLiteral("No PHOTO_CD/IMAGES directory was found at that location."));

    PhotoCdAlbum album;
    album.photoCdDirectory = photoCd;
    album.imagesDirectory = namedChildDirectory(photoCd, QStringLiteral("IMAGES"));
    if (album.imagesDirectory.isEmpty())
        return fail(QStringLiteral("PHOTO_CD exists, but its IMAGES folder is missing."));

    const QDir imageDir(album.imagesDirectory);
    const auto entries = imageDir.entryInfoList(QDir::Files | QDir::NoDotAndDotDot, QDir::Name);
    static const QRegularExpression imagePattern(QStringLiteral("^IMG(\\d+)\\.PCD$"),
                                                  QRegularExpression::CaseInsensitiveOption);
    for (const QFileInfo &entry : entries) {
        const auto match = imagePattern.match(entry.fileName());
        if (!match.hasMatch())
            continue;
        bool ok = false;
        const int number = match.captured(1).toInt(&ok);
        if (ok)
            album.photos.push_back({number, entry.fileName(), entry.absoluteFilePath()});
    }
    std::sort(album.photos.begin(), album.photos.end(), [](const PhotoEntry &a, const PhotoEntry &b) {
        if (a.number != b.number)
            return a.number < b.number;
        return a.name.compare(b.name, Qt::CaseInsensitive) < 0;
    });
    if (album.photos.isEmpty())
        return fail(QStringLiteral("The Photo CD contains no IMG####.PCD photographs."));

    album.startupPath = namedChildFile(photoCd, QStringLiteral("STARTUP.PCD"));
    album.overviewPath = namedChildFile(photoCd, QStringLiteral("OVERVIEW.PCD"));
    album.infoPath = namedChildFile(photoCd, QStringLiteral("INFO.PCD"));
    album.optionsPath = namedChildFile(photoCd, QStringLiteral("OPTIONS.PCD"));

    QStorageInfo storage(photoCd);
    const QString mountedRoot = storage.isValid() && storage.isReady()
        ? canonicalOrAbsolute(storage.rootPath()) : QString();
    const QByteArray fileSystemType = storage.fileSystemType().toLower();
    const bool opticalDiscFileSystem = fileSystemType == QByteArrayLiteral("iso9660") ||
        fileSystemType == QByteArrayLiteral("udf") || fileSystemType == QByteArrayLiteral("cd9660");
    album.discRootDirectory = opticalDiscFileSystem && !mountedRoot.isEmpty() && mountedRoot != QStringLiteral("/")
        ? mountedRoot : selectedDiscRoot(path);
    if (album.discRootDirectory.isEmpty())
        album.discRootDirectory = canonicalOrAbsolute(QFileInfo(photoCd).dir().absolutePath());
    album.volumeName = opticalDiscFileSystem && storage.isValid() && !storage.name().isEmpty()
        ? storage.name() : QFileInfo(album.discRootDirectory).fileName();
    if (album.volumeName.isEmpty())
        album.volumeName = QFileInfo(photoCd).dir().dirName();

    QCryptographicHash hash(QCryptographicHash::Sha256);
    hash.addData(album.volumeName.toUtf8());
    for (const PhotoEntry &photo : album.photos) {
        const QFileInfo file(photo.path);
        hash.addData(photo.name.toUtf8());
        hash.addData(QByteArray::number(file.size()));
    }
    album.identity = QString::fromLatin1(hash.result().toHex().left(24));
    return album;
}

std::optional<PhotoCdAlbum> PhotoCdAlbum::discoverMounted(QString *error)
{
    QStringList errors;
    for (const QStorageInfo &storage : QStorageInfo::mountedVolumes()) {
        if (!storage.isValid() || !storage.isReady())
            continue;
        QString localError;
        auto album = open(storage.rootPath(), &localError);
        if (album)
            return album;
        if (!localError.isEmpty())
            errors.push_back(localError);
    }
    if (error)
        *error = errors.isEmpty()
            ? QStringLiteral("No mounted Photo CD was found. Use Open Disc to choose a mounted disc.")
            : QStringLiteral("No mounted Photo CD was found.");
    return std::nullopt;
}
