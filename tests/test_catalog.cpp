// SPDX-License-Identifier: GPL-3.0-or-later
#include "PhotoCdAlbum.h"
#include "PhotoCdExporter.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QTemporaryDir>
#include <QFileInfo>
#include <iostream>

static bool writeFile(const QString &path)
{
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly))
        return false;
    file.write("fixture");
    return true;
}

int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);
    QTemporaryDir temp;
    if (!temp.isValid()) {
        std::cerr << "could not create temporary test directory\n";
        return 1;
    }
    QDir root(temp.path());
    if (!root.mkpath(QStringLiteral("photo_cd/images")) ||
        !writeFile(root.filePath(QStringLiteral("photo_cd/images/IMG0010.PCd"))) ||
        !writeFile(root.filePath(QStringLiteral("photo_cd/images/img0002.pcd"))) ||
        !writeFile(root.filePath(QStringLiteral("photo_cd/images/cover.pcd"))) ||
        !writeFile(root.filePath(QStringLiteral("photo_cd/startUp.Pcd"))) ||
        !writeFile(root.filePath(QStringLiteral("photo_cd/OPTIONS.PCD")))) {
        std::cerr << "could not prepare album fixture\n";
        return 1;
    }

    QString error;
    auto album = PhotoCdAlbum::open(temp.path(), &error);
    if (!album) {
        std::cerr << "album discovery failed: " << error.toStdString() << '\n';
        return 1;
    }
    if (album->photos.size() != 2 || album->photos.at(0).number != 2 || album->photos.at(1).number != 10) {
        std::cerr << "numeric ordering or IMG filename filtering failed\n";
        return 1;
    }
    if (album->startupPath.isEmpty() || album->optionsPath.isEmpty()) {
        std::cerr << "case-insensitive special-file lookup failed\n";
        return 1;
    }
    if (album->discRootDirectory != QDir(temp.path()).canonicalPath()) {
        std::cerr << "disc root was not retained for safe export destination checks: got "
                  << album->discRootDirectory.toStdString() << ", expected "
                  << QDir(temp.path()).canonicalPath().toStdString() << '\n';
        return 1;
    }
    auto fromImages = PhotoCdAlbum::open(album->imagesDirectory, &error);
    if (!fromImages || fromImages->photos.size() != 2) {
        std::cerr << "open from IMAGES folder failed: " << error.toStdString() << '\n';
        return 1;
    }
    if (PhotoCdAlbum::open(temp.path() + QStringLiteral("/missing"), &error)) {
        std::cerr << "invalid location was incorrectly accepted\n";
        return 1;
    }

    if (!root.mkpath(QStringLiteral("inside-disc-export"))) {
        std::cerr << "could not prepare in-disc export destination\n";
        return 1;
    }
    const PhotoCdExportResult rejected = PhotoCdExporter::exportAll(
        *album, root.filePath(QStringLiteral("inside-disc-export")));
    if (rejected.failed != 1 || rejected.errors.isEmpty() ||
        !rejected.errors.first().contains(QStringLiteral("outside the Photo CD"))) {
        std::cerr << "export into source disc was not rejected\n";
        return 1;
    }

    QTemporaryDir exportTemp;
    if (!exportTemp.isValid()) {
        std::cerr << "could not create external export test directory\n";
        return 1;
    }
    const QString firstExisting = QDir(exportTemp.path()).filePath(QStringLiteral("img0002.jpg"));
    const QString secondExisting = QDir(exportTemp.path()).filePath(QStringLiteral("IMG0010.jpg"));
    if (!writeFile(firstExisting) || !writeFile(secondExisting)) {
        std::cerr << "could not prepare existing export files\n";
        return 1;
    }
    const PhotoCdExportResult skipped = PhotoCdExporter::exportAll(*album, exportTemp.path());
    if (skipped.exported != 0 || skipped.skipped != 2 || skipped.failed != 0 ||
        QFileInfo(firstExisting).size() != 7 || QFileInfo(secondExisting).size() != 7) {
        std::cerr << "existing export files were not safely skipped\n";
        return 1;
    }

    std::cout << "catalog/export tests passed: case-insensitive paths, numeric order, special files, source-disc protection, skip-existing\n";
    return 0;
}
