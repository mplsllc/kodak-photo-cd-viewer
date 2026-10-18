// SPDX-License-Identifier: GPL-3.0-or-later
#include "PcdDecoder.h"

#include "pcdDecode.h"

#include <QColorSpace>
#include <QFileInfo>
#include <QByteArray>
#include <cstdint>
#include <limits>
#include <vector>

DecodeResult PcdDecoder::decode(const QString &path, Resolution resolution)
{
    DecodeResult result;
    if (!QFileInfo::exists(path)) {
        result.error = QStringLiteral("The Photo CD file is unavailable. The disc may have been removed.");
        return result;
    }

    const QByteArray encodedPath = QFileInfo(path).absoluteFilePath().toUtf8();
    pcdDecode decoder;
    decoder.setColorSpace(kPCDsRGBColorSpace);
    decoder.setWhiteBalance(kPCDD65White);
    decoder.setInterpolation(kUpResIterpolate);
    if (!decoder.parseFile(encodedPath.constData(), nullptr, static_cast<unsigned int>(resolution))) {
        result.error = QString::fromLocal8Bit(decoder.getErrorString());
        if (result.error.isEmpty())
            result.error = QStringLiteral("The file is not a decodable Photo CD image.");
        return result;
    }

    decoder.postParse();
    const size_t width = decoder.getWidth();
    const size_t height = decoder.getHeight();
    if (width == 0 || height == 0 || width > 100000 || height > 100000 ||
        width > std::numeric_limits<size_t>::max() / height / 3) {
        result.error = QStringLiteral("The decoder returned invalid image dimensions.");
        return result;
    }

    std::vector<std::uint8_t> rgb(width * height * 3);
    decoder.populateUInt8Buffers(rgb.data(), rgb.data() + 1, rgb.data() + 2, nullptr, 3);
    QImage view(rgb.data(), static_cast<int>(width), static_cast<int>(height),
                static_cast<int>(width * 3), QImage::Format_RGB888);
    result.image = view.copy();
    result.image.setColorSpace(QColorSpace::SRgb);
    result.warning = QString::fromLocal8Bit(decoder.getErrorString());
    if (result.image.isNull())
        result.error = QStringLiteral("Could not allocate the decoded image.");
    return result;
}
