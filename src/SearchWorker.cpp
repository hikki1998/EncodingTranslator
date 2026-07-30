#include "SearchWorker.h"

#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QFutureWatcher>
#include <QMutex>
#include <QMutexLocker>
#include <QThreadPool>
#include <QTimer>
#include <QtConcurrent/QtConcurrent>

namespace {
constexpr qint64 kMaxReadSize = 256 * 1024;
}

SearchWorker::SearchWorker(QObject* parent)
    : QObject(parent)
{
}

void SearchWorker::process(const QString& rootDir, const QStringList& patterns,
                           TextEncoding targetEncoding, const QStringList& excludePatterns,
                           bool skipAsciiFiles, int threadCount)
{
    QFileInfo rootInfo(rootDir);
    if (!rootInfo.exists() || !rootInfo.isDir())
    {
        emit failed(QStringLiteral("Invalid directory: %1").arg(rootDir));
        return;
    }
    if (patterns.isEmpty())
    {
        emit failed(QStringLiteral("No extension pattern configured"));
        return;
    }

    auto* dirTasks = new QVector<QString>();
    dirTasks->push_back(rootDir);

    {
        QDirIterator dirIt(rootDir, QDir::Dirs | QDir::NoDotAndDotDot, QDirIterator::Subdirectories);
        while (dirIt.hasNext())
        {
            const QString d = dirIt.next();
            if (!isDirExcluded(d, rootDir, excludePatterns))
            {
                dirTasks->push_back(d);
            }
        }
    }

    if (dirTasks->isEmpty())
    {
        emit progress(0, 0);
        emit finished(QVector<FileEncodingInfo>(), 0);
        delete dirTasks;
        return;
    }

    if (threadCount > 0)
    {
        QThreadPool::globalInstance()->setMaxThreadCount(threadCount);
    }

    auto* result = new QVector<FileEncodingInfo>();
    auto* resultMutex = new QMutex();
    auto* scannedCount = new QAtomicInt(0);

    auto* watcher = new QFutureWatcher<void>(this);

    connect(watcher, &QFutureWatcher<void>::finished, this, [=]() {
        const int total = scannedCount->loadRelaxed();
        emit finished(*result, total);
        delete result;
        delete resultMutex;
        delete scannedCount;
        delete dirTasks;
        watcher->deleteLater();
    });

    auto* timer = new QTimer(this);
    connect(timer, &QTimer::timeout, this, [this, scannedCount]() {
        emit progress(scannedCount->loadRelaxed(), 0);
    });
    connect(watcher, &QFutureWatcher<void>::finished, timer, &QTimer::stop);
    connect(watcher, &QFutureWatcher<void>::finished, timer, &QObject::deleteLater);
    timer->start(150);

    QFuture<void> future = QtConcurrent::map(*dirTasks, [=](const QString& dir) {
        QDirIterator it(dir, QDir::Files);
        while (it.hasNext())
        {
            const QString path = it.next();
            if (isDirExcluded(path, rootDir, excludePatterns))
            {
                continue;
            }
            const QFileInfo fi(path);
            if (!fileMatchesPatterns(fi.fileName(), patterns))
            {
                continue;
            }
            QFile f(path);
            if (f.open(QIODevice::ReadOnly))
            {
                const QByteArray raw = f.read(kMaxReadSize);
                f.close();
                const TextEncoding enc = detectEncoding(raw);
                if ((!skipAsciiFiles || enc != TextEncoding::Ascii) && !matchesTargetEncoding(enc, targetEncoding))
                {
                    QMutexLocker lock(resultMutex);
                    result->push_back(FileEncodingInfo{path, enc});
                }
            }
            scannedCount->fetchAndAddRelaxed(1);
        }
    });

    watcher->setFuture(future);
}
