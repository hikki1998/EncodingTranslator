#pragma once

#include "EncodingUtils.h"

#include <QObject>
#include <QString>
#include <QStringList>

class SearchWorker : public QObject
{
    Q_OBJECT
public:
    explicit SearchWorker(QObject* parent = nullptr);

public slots:
    void process(const QString& rootDir, const QStringList& patterns, TextEncoding targetEncoding, const QStringList& excludePatterns, bool skipAsciiFiles, int threadCount);

signals:
    void progress(int current, int total);
    void finished(const QVector<FileEncodingInfo>& foundFiles, int scannedTotal);
    void failed(const QString& message);
};

