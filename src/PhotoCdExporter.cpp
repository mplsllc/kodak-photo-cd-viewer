// SPDX-License-Identifier: GPL-3.0-or-later
#include "PhotoCdExporter.h"

#include "PcdDecoder.h"

#include <QDir>
#include <QFileInfo>
#include <QImageWriter>
#include <QSaveFile>

namespace {
QString canonicalOrAbsolute(const QString &path)
{
    if (path.isEmpty())
        return {};
    const QString canonical = QFileInfo(path).canonicalFilePath();
    return QDir::cleanPath(canonical.isEmpty() ? QFileInfo(path).absoluteFilePath() : canonical);
}

bool isSameOrChildPath(const QString &path, const QString &parent)
{
    if (path == parent)
        return true;
    const QString prefix = parent.endsWith(QDir::separator())
        ? parent : parent + QDir::separator();
    return path.startsWith(prefix, Qt::CaseSensitive);
}
}

PhotoCdExportResult PhotoCdExporter::exportAll(const PhotoCdAlbum &album,
                                                const QString &destinationDirectory,
                                                ProgressCallback progress,
                                                CancelCallback canceled)
{
    PhotoCdExportResult result;
    const QFileInfo destinationInfo(destinationDirectory);
    const QString destination = destinationInfo.canonicalFilePath();
    if (!destinationInfo.exists() || !destinationInfo.isDir() || destination.isEmpty()) {
        result.failed = 1;
        result.errors.push_back(QStringLiteral("Choose an existing destination folder."));
        return result;
    }

    const QString discRoot = canonicalOrAbsolute(album.discRootDirectory.isEmpty()
                                                   ? album.photoCdDirectory
                                                   : album.discRootDirectory);
    if (!discRoot.isEmpty() && isSameOrChildPath(destination, discRoot)) {
        result.failed = 1;
        result.errors.push_back(QStringLiteral("Choose a folder outside the Photo CD. The disc is read-only and will not be changed."));
        return result;
    }

    if (album.photos.isEmpty()) {
        result.failed = 1;
        result.errors.push_back(QStringLiteral("This Photo CD contains no photographs to export."));
        return result;
    }

    for (int index = 0; index < album.photos.size(); ++index) {
        const PhotoEntry &photo = album.photos.at(index);
        if (canceled && canceled()) {
            result.canceled = true;
            break;
        }
        if (progress)
            progress(index, album.photos.size(), photo.name);
        if (canceled && canceled()) {
            result.canceled = true;
            break;
        }

        const QString baseName = QFileInfo(photo.name).completeBaseName();
        const QString outputPath = QDir(destination).filePath(baseName + QStringLiteral(".jpg"));
        if (QFileInfo::exists(outputPath)) {
            ++result.skipped;
            continue;
        }

        const DecodeResult decoded = PcdDecoder::decode(photo.path, PcdDecoder::SixteenBase);
        if (!decoded.ok()) {
            ++result.failed;
            result.errors.push_back(QStringLiteral("%1: %2").arg(photo.name, decoded.error));
            continue;
        }

        QSaveFile output(outputPath);
        if (!output.open(QIODevice::WriteOnly)) {
            ++result.failed;
            result.errors.push_back(QStringLiteral("%1: %2").arg(baseName + QStringLiteral(".jpg"), output.errorString()));
            continue;
        }
        QImageWriter writer(&output, "JPEG");
        writer.setQuality(95);
        if (!writer.write(decoded.image)) {
            ++result.failed;
            result.errors.push_back(QStringLiteral("%1: %2").arg(baseName + QStringLiteral(".jpg"), writer.errorString()));
            output.cancelWriting();
            continue;
        }
        if (!output.commit()) {
            ++result.failed;
            result.errors.push_back(QStringLiteral("%1: %2").arg(baseName + QStringLiteral(".jpg"), output.errorString()));
            continue;
        }
        ++result.exported;
    }
    return result;
}
