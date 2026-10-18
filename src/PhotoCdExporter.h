// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "PhotoCdAlbum.h"

#include <QStringList>
#include <functional>

struct PhotoCdExportResult {
    int exported = 0;
    int skipped = 0;
    int failed = 0;
    bool canceled = false;
    QStringList errors;
};

class PhotoCdExporter {
public:
    using ProgressCallback = std::function<void(int completed, int total, const QString &currentName)>;
    using CancelCallback = std::function<bool()>;

    static PhotoCdExportResult exportAll(const PhotoCdAlbum &album,
                                         const QString &destinationDirectory,
                                         ProgressCallback progress = {},
                                         CancelCallback canceled = {});
};
