#pragma once

#include "EncodingUtils.h"

#include <QObject>
#include <QStringList>

class ConvertWorker : public QObject
{
    Q_OBJECT
public:
    explicit ConvertWorker(QObject* parent = nullptr);

public slots:
    void process(const QStringList& filePaths, TextEncoding targetEncoding);

signals:
    void progress(int current, int total);
    void finished(const QVector<ConvertResult>& results);
};

