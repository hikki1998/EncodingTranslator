#include "EncodingUtils.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QRegularExpression>
#include <QTextCodec>

#include <windows.h>

namespace
{
QString normalizePattern(QString pattern)
{
    pattern = pattern.trimmed().toLower();
    if (pattern.isEmpty())
    {
        return {};
    }

    if (pattern.startsWith("*."))
    {
        return pattern;
    }
    if (pattern.startsWith("."))
    {
        return QStringLiteral("*") + pattern;
    }
    if (pattern.contains('*'))
    {
        return pattern;
    }

    return QStringLiteral("*.") + pattern;
}

bool isLikelyUtf8(const QByteArray& data)
{
    int i = 0;
    const int len = data.size();
    while (i < len)
    {
        const uchar c = static_cast<uchar>(data.at(i));
        if (c <= 0x7F)
        {
            ++i;
            continue;
        }

        int followCount = 0;
        if ((c & 0xE0) == 0xC0)
        {
            followCount = 1;
            if (c < 0xC2)
            {
                return false;
            }
        }
        else if ((c & 0xF0) == 0xE0)
        {
            followCount = 2;
        }
        else if ((c & 0xF8) == 0xF0)
        {
            followCount = 3;
            if (c > 0xF4)
            {
                return false;
            }
        }
        else
        {
            return false;
        }

        if (i + followCount >= len)
        {
            return false;
        }
        for (int j = 1; j <= followCount; ++j)
        {
            const uchar cc = static_cast<uchar>(data.at(i + j));
            if ((cc & 0xC0) != 0x80)
            {
                return false;
            }
        }
        i += followCount + 1;
    }
    return true;
}
}

QString encodingToString(TextEncoding encoding)
{
    switch (encoding)
    {
    case TextEncoding::Utf8:
        return QStringLiteral("UTF-8");
    case TextEncoding::Gbk:
        return QStringLiteral("GBK");
    case TextEncoding::Ascii:
        return QStringLiteral("ASCII");
    default:
        return QStringLiteral("Unknown");
    }
}

bool matchesTargetEncoding(TextEncoding detected, TextEncoding target)
{
    return detected == target;
}

TextEncoding detectEncoding(const QByteArray& bytes)
{
    if (bytes.isEmpty())
    {
        return TextEncoding::Ascii;
    }

    bool allAscii = true;
    for (const char c : bytes)
    {
        if (static_cast<uchar>(c) > 0x7F)
        {
            allAscii = false;
            break;
        }
    }
    if (allAscii)
    {
        return TextEncoding::Ascii;
    }

    if (bytes.size() >= 3
        && static_cast<uchar>(bytes[0]) == 0xEF
        && static_cast<uchar>(bytes[1]) == 0xBB
        && static_cast<uchar>(bytes[2]) == 0xBF)
    {
        return TextEncoding::Utf8;
    }

    if (isLikelyUtf8(bytes))
    {
        return TextEncoding::Utf8;
    }

    int wideLen = MultiByteToWideChar(936, MB_ERR_INVALID_CHARS, bytes.constData(), bytes.size(), nullptr, 0);
    if (wideLen > 0)
    {
        return TextEncoding::Gbk;
    }

    return TextEncoding::Unknown;
}

QStringList parseExtensionPatterns(const QString& text)
{
    QStringList rawParts = text.split(QRegularExpression(QStringLiteral("[,;\\s]+")), Qt::SkipEmptyParts);
    QStringList patterns;
    patterns.reserve(rawParts.size());
    for (QString part : rawParts)
    {
        part = normalizePattern(part);
        if (!part.isEmpty() && !patterns.contains(part))
        {
            patterns.push_back(part);
        }
    }
    return patterns;
}

bool fileMatchesPatterns(const QString& fileName, const QStringList& patterns)
{
    if (patterns.isEmpty())
    {
        return false;
    }
    const QString lower = fileName.toLower();
    for (const QString& pattern : patterns)
    {
        QRegularExpression re(QRegularExpression::wildcardToRegularExpression(pattern), QRegularExpression::CaseInsensitiveOption);
        if (re.match(lower).hasMatch())
        {
            return true;
        }
    }
    return false;
}

QStringList defaultExcludePatterns()
{
    return {
        QStringLiteral("build"),
        QStringLiteral("cmake-build-*"),
        QStringLiteral("Debug"),
        QStringLiteral("Release"),
        QStringLiteral("x64"),
        QStringLiteral("x86"),
        QStringLiteral("out"),
        QStringLiteral("obj"),
        QStringLiteral(".git"),
        QStringLiteral(".svn"),
        QStringLiteral(".vs"),
        QStringLiteral(".idea"),
        QStringLiteral(".vscode"),
        QStringLiteral("node_modules"),
        QStringLiteral("__pycache__"),
        QStringLiteral("vendor"),
        QStringLiteral("third_party"),
        QStringLiteral(".qmake.stash"),
        QStringLiteral("Makefile"),
    };
}

bool isDirExcluded(const QString& filePath, const QString& rootDir, const QStringList& excludePatterns)
{
    if (excludePatterns.isEmpty())
    {
        return false;
    }
    QDir root(rootDir);
    const QString dirPath = root.relativeFilePath(QFileInfo(filePath).absoluteFilePath());
    const QStringList parts = dirPath.split(QChar('/'), Qt::SkipEmptyParts);
    for (const QString& part : parts)
    {
        for (const QString& pattern : excludePatterns)
        {
            QRegularExpression re(QRegularExpression::wildcardToRegularExpression(pattern), QRegularExpression::CaseInsensitiveOption);
            if (re.match(part).hasMatch())
            {
                return true;
            }
        }
    }
    return false;
}

bool convertFileEncoding(const QString& filePath, TextEncoding target, QString* errorMessage)
{
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly))
    {
        if (errorMessage)
        {
            *errorMessage = QStringLiteral("Open failed: %1").arg(file.errorString());
        }
        return false;
    }
    const QByteArray raw = file.readAll();
    file.close();

    const TextEncoding detected = detectEncoding(raw);
    if (matchesTargetEncoding(detected, target))
    {
        if (errorMessage)
        {
            *errorMessage = QStringLiteral("Already target encoding");
        }
        return true;
    }

    QString unicodeText;
    if (detected == TextEncoding::Utf8)
    {
        QByteArray temp = raw;
        if (temp.startsWith("\xEF\xBB\xBF"))
        {
            temp = temp.mid(3);
        }
        unicodeText = QString::fromUtf8(temp);
    }
    else if (detected == TextEncoding::Gbk)
    {
        QTextCodec* codec = QTextCodec::codecForName("GBK");
        if (!codec)
        {
            if (errorMessage)
            {
                *errorMessage = QStringLiteral("GBK codec unavailable");
            }
            return false;
        }
        unicodeText = codec->toUnicode(raw);
    }
    else if (detected == TextEncoding::Ascii)
    {
        unicodeText = QString::fromLatin1(raw);
    }
    else
    {
        if (errorMessage)
        {
            *errorMessage = QStringLiteral("Cannot detect source encoding");
        }
        return false;
    }

    QByteArray output;
    if (target == TextEncoding::Utf8)
    {
        output = unicodeText.toUtf8();
    }
    else if (target == TextEncoding::Gbk)
    {
        QTextCodec* codec = QTextCodec::codecForName("GBK");
        if (!codec)
        {
            if (errorMessage)
            {
                *errorMessage = QStringLiteral("GBK codec unavailable");
            }
            return false;
        }
        output = codec->fromUnicode(unicodeText);
    }
    else
    {
        if (errorMessage)
        {
            *errorMessage = QStringLiteral("Unsupported target encoding");
        }
        return false;
    }

    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate))
    {
        if (errorMessage)
        {
            *errorMessage = QStringLiteral("Write open failed: %1").arg(file.errorString());
        }
        return false;
    }
    const qint64 written = file.write(output);
    file.close();
    if (written != output.size())
    {
        if (errorMessage)
        {
            *errorMessage = QStringLiteral("Write failed");
        }
        return false;
    }

    if (errorMessage)
    {
        errorMessage->clear();
    }
    return true;
}

