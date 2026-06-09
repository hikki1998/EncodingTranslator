#include "ConvertWorker.h"

ConvertWorker::ConvertWorker(QObject* parent)
    : QObject(parent)
{
}

void ConvertWorker::process(const QStringList& filePaths, TextEncoding targetEncoding)
{
    QVector<ConvertResult> results;
    results.reserve(filePaths.size());

    const int total = filePaths.size();
    emit progress(0, total == 0 ? 1 : total);

    int current = 0;
    for (const QString& path : filePaths)
    {
        QString error;
        const bool ok = convertFileEncoding(path, targetEncoding, &error);

        ConvertResult item;
        item.filePath = path;
        item.success = ok;
        item.message = ok ? QStringLiteral("OK") : error;
        results.push_back(item);

        ++current;
        emit progress(current, total == 0 ? 1 : total);
    }

    emit finished(results);
}

