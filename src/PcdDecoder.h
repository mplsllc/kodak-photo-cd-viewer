// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QImage>
#include <QString>

struct DecodeResult {
    QImage image;
    QString error;
    QString warning;

    bool ok() const { return !image.isNull(); }
};

class PcdDecoder {
public:
    enum Resolution {
        Base16 = 0,
        Base4 = 1,
        Base = 2,
        FourBase = 3,
        SixteenBase = 4
    };

    static DecodeResult decode(const QString &path, Resolution resolution);
};
