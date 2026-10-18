// SPDX-License-Identifier: GPL-3.0-or-later
#include "MainWindow.h"
#include "PhotoCdExporter.h"
#include "PcdDecoder.h"
#include "PhotoCdAlbum.h"

#include <QApplication>
#include <QTextStream>
#include <QUrl>

namespace {
QString localPathFromLaunchArgument(const QString &argument)
{
    const QUrl url(argument);
    return url.isLocalFile() ? url.toLocalFile() : argument;
}

int selfCheck(const QString &path)
{
    QTextStream out(stdout);
    QString error;
    std::optional<PhotoCdAlbum> album = path.isEmpty()
        ? PhotoCdAlbum::discoverMounted(&error)
        : PhotoCdAlbum::open(path, &error);
    if (!album) {
        out << "DISC ERROR: " << error << Qt::endl;
        return 1;
    }

    out << "Album " << album->volumeName << ": " << album->photos.size()
        << " image files in numeric disc order\n";
    int decoded = 0;
    for (const PhotoEntry &photo : album->photos) {
        const DecodeResult image = PcdDecoder::decode(photo.path, PcdDecoder::Base16);
        if (!image.ok()) {
            out << "FAIL " << photo.name << ": " << image.error << Qt::endl;
            return 2;
        }
        ++decoded;
        out << "OK   " << photo.name << " " << image.image.width() << 'x'
            << image.image.height() << Qt::endl;
    }

    if (!album->startupPath.isEmpty()) {
        const DecodeResult startup = PcdDecoder::decode(album->startupPath, PcdDecoder::Base4);
        if (startup.ok())
            out << "OK   STARTUP.PCD " << startup.image.width() << 'x' << startup.image.height() << Qt::endl;
        else
            out << "WARN STARTUP.PCD: " << startup.error << Qt::endl;
    } else {
        out << "INFO no STARTUP.PCD" << Qt::endl;
    }
    if (!album->overviewPath.isEmpty())
        out << "INFO OVERVIEW.PCD present as Kodak overview pack; not decoded as an individual photo" << Qt::endl;
    if (!album->infoPath.isEmpty())
        out << "INFO INFO.PCD present" << Qt::endl;
    if (!album->optionsPath.isEmpty())
        out << "INFO OPTIONS.PCD present" << Qt::endl;
    out << "SELF-CHECK PASS: " << decoded << " of " << album->photos.size()
        << " album photographs decoded from the mounted disc." << Qt::endl;
    return 0;
}

int exportAll(const QString &sourcePath, const QString &destinationPath)
{
    QTextStream out(stdout);
    QString error;
    const auto album = PhotoCdAlbum::open(sourcePath, &error);
    if (!album) {
        out << "DISC ERROR: " << error << Qt::endl;
        return 1;
    }

    const PhotoCdExportResult result = PhotoCdExporter::exportAll(
        *album, destinationPath,
        [&out](int completed, int total, const QString &name) {
            out << "Exporting " << name << " (" << completed + 1 << '/' << total << ")" << Qt::endl;
        });
    out << "Exported " << result.exported << ", skipped " << result.skipped
        << ", failed " << result.failed << Qt::endl;
    for (const QString &message : result.errors)
        out << "ERROR " << message << Qt::endl;
    if (result.canceled)
        return 130;
    return result.failed == 0 ? 0 : 2;
}
}

int main(int argc, char **argv)
{
    QApplication app(argc, argv);
    QCoreApplication::setOrganizationName(QStringLiteral("PhotoCD"));
    QCoreApplication::setApplicationName(QStringLiteral("Photo CD Player"));
    QCoreApplication::setApplicationVersion(QStringLiteral("0.1.0"));

    QString initialPath;
    QString checkPath;
    QString exportSourcePath;
    QString exportDestinationPath;
    bool runCheck = false;
    bool runExportAll = false;
    bool help = false;
    for (int i = 1; i < argc; ++i) {
        const QString arg = QString::fromLocal8Bit(argv[i]);
        if (arg == QStringLiteral("--help") || arg == QStringLiteral("-h")) {
            help = true;
        } else if (arg == QStringLiteral("--open")) {
            if (i + 1 >= argc) {
                QTextStream(stderr) << "--open requires a mounted disc directory.\n";
                return 64;
            }
            initialPath = localPathFromLaunchArgument(QString::fromLocal8Bit(argv[++i]));
        } else if (arg == QStringLiteral("--self-check")) {
            runCheck = true;
            if (i + 1 < argc && !QString::fromLocal8Bit(argv[i + 1]).startsWith(QLatin1Char('-')))
                checkPath = QString::fromLocal8Bit(argv[++i]);
        } else if (arg == QStringLiteral("--export-all")) {
            if (i + 2 >= argc) {
                QTextStream(stderr) << "--export-all requires a disc directory and destination directory.\n";
                return 64;
            }
            runExportAll = true;
            exportSourcePath = QString::fromLocal8Bit(argv[++i]);
            exportDestinationPath = QString::fromLocal8Bit(argv[++i]);
        } else if (arg.startsWith(QLatin1Char('-'))) {
            QTextStream(stderr) << "Unknown option: " << arg << Qt::endl;
            return 64;
        } else {
            initialPath = localPathFromLaunchArgument(arg);
        }
    }
    if (help) {
        QTextStream(stdout) << "Photo CD Player\n"
                               "Usage: photo-cd-player [--open DIRECTORY] [--self-check [DIRECTORY]]\n"
                               "       photo-cd-player --export-all DISC_DIRECTORY OUTPUT_DIRECTORY\n";
        return 0;
    }
    if (runCheck)
        return selfCheck(checkPath);
    if (runExportAll)
        return exportAll(exportSourcePath, exportDestinationPath);

    MainWindow window(initialPath);
    window.show();
    return app.exec();
}
