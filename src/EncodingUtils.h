#pragma once

#include <QByteArray>
#include <QMetaType>
#include <QString>
#include <QStringList>
#include <QVector>

enum class TextEncoding
{
    Utf8 = 0,
    Gbk = 1,
    Ascii = 2,
    Utf8Bom = 3,
    Unknown = 4
};
Q_DECLARE_METATYPE(TextEncoding)

struct FileEncodingInfo
{
    QString filePath;
    TextEncoding encoding = TextEncoding::Unknown;
};
Q_DECLARE_METATYPE(FileEncodingInfo)
Q_DECLARE_METATYPE(QVector<FileEncodingInfo>)

struct ConvertResult
{
    QString filePath;
    bool success = false;
    QString message;
};
Q_DECLARE_METATYPE(ConvertResult)
Q_DECLARE_METATYPE(QVector<ConvertResult>)

QString encodingToString(TextEncoding encoding);
bool matchesTargetEncoding(TextEncoding detected, TextEncoding target);
TextEncoding detectEncoding(const QByteArray& bytes);

QStringList parseExtensionPatterns(const QString& text);
QStringList defaultExcludePatterns();
bool isDirExcluded(const QString& filePath, const QString& rootDir, const QStringList& excludePatterns);
bool fileMatchesPatterns(const QString& fileName, const QStringList& patterns);

bool convertFileEncoding(const QString& filePath, TextEncoding target, QString* errorMessage);

