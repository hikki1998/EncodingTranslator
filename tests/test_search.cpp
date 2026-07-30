#include "../src/EncodingUtils.h"

#include <QCoreApplication>
#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QMutex>
#include <QMutexLocker>
#include <QTemporaryDir>
#include <QTextCodec>
#include <QThreadPool>
#include <QVector>
#include <QtConcurrent/QtConcurrent>

#include <cstdio>

namespace
{
constexpr qint64 kMaxReadSize = 256 * 1024;

void log(const QString& message)
{
    fprintf(stdout, "%s\n", qPrintable(message));
    fflush(stdout);
}

bool writeTextFile(const QString& path, const QByteArray& content)
{
    QDir().mkpath(QFileInfo(path).absolutePath());
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate))
    {
        return false;
    }
    return file.write(content) == content.size();
}

QByteArray gbkText(const QString& text)
{
    QTextCodec* codec = QTextCodec::codecForName("GBK");
    return codec ? codec->fromUnicode(text) : QByteArray();
}
}

int main(int argc, char* argv[])
{
    QCoreApplication app(argc, argv);
    Q_UNUSED(app)

    QTemporaryDir tempDir;
    if (!tempDir.isValid())
    {
        log(QStringLiteral("Failed to create temporary directory"));
        return 1;
    }

    const QString rootDir = tempDir.path();
    const QByteArray gbk = gbkText(QStringLiteral("int value = 1; // 中文注释\n"));
    if (gbk.isEmpty())
    {
        log(QStringLiteral("GBK codec unavailable"));
        return 1;
    }

    const QStringList gbkFiles = {
        QStringLiteral("main.cpp"),
        QStringLiteral("utils.cpp"),
        QStringLiteral("include/header0.h"),
        QStringLiteral("include/header1.h"),
        QStringLiteral("include/header2.h"),
        QStringLiteral("src/file0.cpp"),
        QStringLiteral("src/file1.cpp"),
    };

    for (const QString& relativePath : gbkFiles)
    {
        if (!writeTextFile(rootDir + QLatin1Char('/') + relativePath, gbk))
        {
            log(QStringLiteral("Failed to write %1").arg(relativePath));
            return 1;
        }
    }

    writeTextFile(rootDir + QStringLiteral("/src/file2.cpp"), QByteArray("int utf8 = 1;\n"));
    writeTextFile(rootDir + QStringLiteral("/src/bom.cpp"), QByteArray("\xEF\xBB\xBFint bom = 1;\n"));
    writeTextFile(rootDir + QStringLiteral("/readme.txt"), QByteArray("plain text\n"));
    writeTextFile(rootDir + QStringLiteral("/build/generated.cpp"), gbk);

    const QStringList patterns = parseExtensionPatterns(QStringLiteral(".cpp,.h,.hpp,.c"));
    const TextEncoding target = TextEncoding::Utf8;
    const QStringList exclude = defaultExcludePatterns();
    const bool skipAsciiFiles = true;

    QVector<QString> dirTasks;
    dirTasks.push_back(rootDir);

    QDirIterator dirIt(rootDir, QDir::Dirs | QDir::NoDotAndDotDot, QDirIterator::Subdirectories);
    while (dirIt.hasNext())
    {
        const QString directory = dirIt.next();
        if (!isDirExcluded(directory, rootDir, exclude))
        {
            dirTasks.push_back(directory);
        }
    }

    QThreadPool::globalInstance()->setMaxThreadCount(4);

    QVector<FileEncodingInfo> allResults;
    QMutex resultMutex;
    QAtomicInt scannedCount(0);

    QFuture<void> future = QtConcurrent::map(dirTasks, [&](const QString& directory) {
        QDirIterator it(directory, QDir::Files);
        while (it.hasNext())
        {
            const QString path = it.next();
            if (isDirExcluded(path, rootDir, exclude))
            {
                continue;
            }

            const QFileInfo fileInfo(path);
            if (!fileMatchesPatterns(fileInfo.fileName(), patterns))
            {
                continue;
            }

            QFile file(path);
            if (file.open(QIODevice::ReadOnly))
            {
                const QByteArray raw = file.read(kMaxReadSize);
                const TextEncoding encoding = detectEncoding(raw);
                if ((!skipAsciiFiles || encoding != TextEncoding::Ascii) && !matchesTargetEncoding(encoding, target))
                {
                    QMutexLocker lock(&resultMutex);
                    allResults.push_back(FileEncodingInfo{path, encoding});
                }
            }
            scannedCount.fetchAndAddRelaxed(1);
        }
    });

    future.waitForFinished();

    int gbkCount = 0;
    int utf8BomCount = 0;
    int asciiCount = 0;
    bool foundBuild = false;
    for (const FileEncodingInfo& result : allResults)
    {
        if (result.encoding == TextEncoding::Gbk)
        {
            ++gbkCount;
        }
        if (result.encoding == TextEncoding::Utf8Bom)
        {
            ++utf8BomCount;
        }
        if (result.encoding == TextEncoding::Ascii)
        {
            ++asciiCount;
        }
        if (result.filePath.contains(QStringLiteral("/build/")))
        {
            foundBuild = true;
        }
    }

    log(QStringLiteral("Scanned: %1").arg(scannedCount.loadRelaxed()));
    log(QStringLiteral("GBK files: %1").arg(gbkCount));
    log(QStringLiteral("UTF-8 BOM files: %1").arg(utf8BomCount));
    log(QStringLiteral("ASCII files: %1").arg(asciiCount));
    log(QStringLiteral("build excluded: %1").arg(foundBuild ? "no" : "yes"));

    if (gbkCount != 7 || utf8BomCount != 1 || asciiCount != 0 || foundBuild || scannedCount.loadRelaxed() < 9)
    {
        log(QStringLiteral("SOME TESTS FAILED"));
        return 1;
    }

    QString convertError;
    const QString bomPath = rootDir + QStringLiteral("/src/bom.cpp");
    if (!convertFileEncoding(bomPath, TextEncoding::Utf8, &convertError))
    {
        log(QStringLiteral("BOM conversion failed: %1").arg(convertError));
        return 1;
    }

    QFile convertedFile(bomPath);
    if (!convertedFile.open(QIODevice::ReadOnly))
    {
        log(QStringLiteral("Failed to read converted BOM file"));
        return 1;
    }
    const QByteArray convertedBytes = convertedFile.readAll();
    if (convertedBytes.startsWith("\xEF\xBB\xBF") || detectEncoding(convertedBytes) != TextEncoding::Ascii)
    {
        log(QStringLiteral("BOM was not removed"));
        return 1;
    }

    log(QStringLiteral("ALL TESTS PASSED"));
    return 0;
}
