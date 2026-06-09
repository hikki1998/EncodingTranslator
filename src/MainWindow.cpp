#include "MainWindow.h"

#include "ConvertWorker.h"
#include "SearchWorker.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDir>
#include <QFileDialog>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QMap>
#include <QMessageBox>
#include <QProgressBar>
#include <QPushButton>
#include <QSpinBox>
#include <QThread>
#include <QTreeWidget>
#include <QVBoxLayout>
#include <QWidget>

namespace
{
constexpr int kPathRole = Qt::UserRole + 1;
}

MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent)
{
    qRegisterMetaType<TextEncoding>("TextEncoding");
    qRegisterMetaType<QVector<FileEncodingInfo>>("QVector<FileEncodingInfo>");
    qRegisterMetaType<QVector<ConvertResult>>("QVector<ConvertResult>");

    setupUi();
}

MainWindow::~MainWindow()
{
    if (m_searchThread)
    {
        m_searchThread->quit();
        m_searchThread->wait();
    }
    if (m_convertThread)
    {
        m_convertThread->quit();
        m_convertThread->wait();
    }
}

void MainWindow::setupUi()
{
    setWindowTitle(QStringLiteral("Source Encoding Translator"));
    resize(980, 680);

    auto* central = new QWidget(this);
    auto* rootLayout = new QVBoxLayout(central);
    rootLayout->setContentsMargins(12, 12, 12, 12);
    rootLayout->setSpacing(10);

    auto* pathLayout = new QHBoxLayout();
    auto* pathLabel = new QLabel(QStringLiteral("Directory:"), central);
    m_directoryEdit = new QLineEdit(central);
    m_browseButton = new QPushButton(QStringLiteral("Browse"), central);
    pathLayout->addWidget(pathLabel);
    pathLayout->addWidget(m_directoryEdit, 1);
    pathLayout->addWidget(m_browseButton);
    rootLayout->addLayout(pathLayout);

    auto* optionLayout = new QHBoxLayout();
    auto* extLabel = new QLabel(QStringLiteral("Extensions:"), central);
    m_extEdit = new QLineEdit(central);
    m_extEdit->setPlaceholderText(QStringLiteral("e.g. .cpp,.h,.hpp,.c,.cc,.cxx"));
    m_extEdit->setText(defaultExtensions().join(QStringLiteral(",")));
    auto* excludeLabel = new QLabel(QStringLiteral("Exclude Dirs:"), central);
    m_excludeEdit = new QLineEdit(central);
    m_excludeEdit->setPlaceholderText(QStringLiteral("e.g. build,Debug,.git,node_modules"));
    m_excludeEdit->setText(defaultExcludes().join(QStringLiteral(",")));
    auto* targetLabel = new QLabel(QStringLiteral("Target Encoding:"), central);
    m_targetCombo = new QComboBox(central);
    m_targetCombo->addItem(QStringLiteral("GBK"), static_cast<int>(TextEncoding::Gbk));
    m_targetCombo->addItem(QStringLiteral("UTF-8"), static_cast<int>(TextEncoding::Utf8));
    m_searchButton = new QPushButton(QStringLiteral("Search"), central);
    m_convertButton = new QPushButton(QStringLiteral("Convert Selected"), central);
    m_convertButton->setEnabled(false);

    auto* threadLabel = new QLabel(QStringLiteral("Threads:"), central);
    m_threadCountSpin = new QSpinBox(central);
    m_threadCountSpin->setRange(1, 64);
    m_threadCountSpin->setValue(QThread::idealThreadCount());
    m_threadCountSpin->setToolTip(QStringLiteral("Number of threads for search"));
    optionLayout->addWidget(extLabel);
    optionLayout->addWidget(m_extEdit, 1);
    optionLayout->addWidget(excludeLabel);
    optionLayout->addWidget(m_excludeEdit, 1);
    optionLayout->addWidget(targetLabel);
    optionLayout->addWidget(m_targetCombo);
    optionLayout->addWidget(threadLabel);
    optionLayout->addWidget(m_threadCountSpin);
    optionLayout->addWidget(m_searchButton);
    optionLayout->addWidget(m_convertButton);
    rootLayout->addLayout(optionLayout);

    auto* searchGroup = new QGroupBox(QStringLiteral("Search Progress"), central);
    auto* searchProgressLayout = new QHBoxLayout(searchGroup);
    m_searchProgress = new QProgressBar(searchGroup);
    m_searchProgress->setRange(0, 100);
    m_searchProgress->setValue(0);
    searchProgressLayout->addWidget(m_searchProgress, 1);
    rootLayout->addWidget(searchGroup);

    auto* convertGroup = new QGroupBox(QStringLiteral("Convert Progress"), central);
    auto* convertProgressLayout = new QHBoxLayout(convertGroup);
    m_convertProgress = new QProgressBar(convertGroup);
    m_convertProgress->setRange(0, 100);
    m_convertProgress->setValue(0);
    convertProgressLayout->addWidget(m_convertProgress, 1);
    rootLayout->addWidget(convertGroup);

    m_tree = new QTreeWidget(central);
    m_tree->setColumnCount(2);
    m_tree->setHeaderLabels({QStringLiteral("File"), QStringLiteral("Detected Encoding")});
    m_tree->header()->setStretchLastSection(false);
    m_tree->header()->setSectionResizeMode(0, QHeaderView::Stretch);
    m_tree->header()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    rootLayout->addWidget(m_tree, 1);

    m_statusLabel = new QLabel(QStringLiteral("Ready"), central);
    rootLayout->addWidget(m_statusLabel);

    setCentralWidget(central);

    connect(m_browseButton, &QPushButton::clicked, this, &MainWindow::onBrowse);
    connect(m_searchButton, &QPushButton::clicked, this, &MainWindow::onSearch);
    connect(m_convertButton, &QPushButton::clicked, this, &MainWindow::onConvert);
}

QStringList MainWindow::defaultExtensions() const
{
    return {
        QStringLiteral(".c"),
        QStringLiteral(".cc"),
        QStringLiteral(".cpp"),
        QStringLiteral(".cxx"),
        QStringLiteral(".h"),
        QStringLiteral(".hh"),
        QStringLiteral(".hpp"),
        QStringLiteral(".hxx"),
        QStringLiteral(".inl"),
        QStringLiteral(".ipp"),
        QStringLiteral(".ixx"),
        QStringLiteral(".tpp"),
        QStringLiteral(".txt"),
        QStringLiteral(".pro"),
        QStringLiteral(".pri"),
        QStringLiteral(".cmake")
    };
}

TextEncoding MainWindow::currentTargetEncoding() const
{
    return static_cast<TextEncoding>(m_targetCombo->currentData().toInt());
}

QStringList MainWindow::defaultExcludes() const
{
    return defaultExcludePatterns();
}

void MainWindow::onBrowse()
{
    const QString selectedDir = QFileDialog::getExistingDirectory(this, QStringLiteral("Select directory"), m_directoryEdit->text());
    if (!selectedDir.isEmpty())
    {
        m_directoryEdit->setText(QDir::toNativeSeparators(selectedDir));
    }
}

void MainWindow::setSearchUiState(bool searching)
{
    m_searchButton->setEnabled(!searching);
    m_convertButton->setEnabled(!searching && m_tree->topLevelItemCount() > 0);
    m_browseButton->setEnabled(!searching);
    m_extEdit->setEnabled(!searching);
    m_excludeEdit->setEnabled(!searching);
    m_targetCombo->setEnabled(!searching);
    m_threadCountSpin->setEnabled(!searching);
}

void MainWindow::setConvertUiState(bool converting)
{
    m_convertButton->setEnabled(!converting && m_tree->topLevelItemCount() > 0);
    m_searchButton->setEnabled(!converting);
    m_browseButton->setEnabled(!converting);
    m_extEdit->setEnabled(!converting);
    m_excludeEdit->setEnabled(!converting);
    m_targetCombo->setEnabled(!converting);
    m_threadCountSpin->setEnabled(!converting);
}

void MainWindow::clearTree()
{
    m_tree->clear();
    m_convertButton->setEnabled(false);
}

QString MainWindow::relativePathForDisplay(const QString& absolutePath) const
{
    if (m_rootDir.isEmpty())
    {
        return absolutePath;
    }
    QDir root(m_rootDir);
    return QDir::toNativeSeparators(root.relativeFilePath(absolutePath));
}

void MainWindow::onSearch()
{
    const QString rootDir = m_directoryEdit->text().trimmed();
    if (rootDir.isEmpty())
    {
        QMessageBox::warning(this, QStringLiteral("Input Error"), QStringLiteral("Please choose a directory first."));
        return;
    }

    const QStringList patterns = parseExtensionPatterns(m_extEdit->text());
    if (patterns.isEmpty())
    {
        QMessageBox::warning(this, QStringLiteral("Input Error"), QStringLiteral("Please input valid extension patterns."));
        return;
    }
    const QStringList excludePatterns = parseExtensionPatterns(m_excludeEdit->text());

    m_rootDir = QDir(rootDir).absolutePath();
    clearTree();
    setSearchUiState(true);
    m_searchProgress->setRange(0, 100);
    m_searchProgress->setValue(0);
    m_statusLabel->setText(QStringLiteral("Searching..."));

    if (m_searchThread)
    {
        m_searchThread->quit();
        m_searchThread->wait();
        m_searchThread->deleteLater();
        m_searchThread = nullptr;
        m_searchWorker = nullptr;
    }

    m_searchThread = new QThread(this);
    m_searchWorker = new SearchWorker();
    m_searchWorker->moveToThread(m_searchThread);

    connect(m_searchThread, &QThread::finished, m_searchWorker, &QObject::deleteLater);
    connect(m_searchWorker, &SearchWorker::progress, this, &MainWindow::onSearchProgress);
    connect(m_searchWorker, &SearchWorker::finished, this, &MainWindow::onSearchFinished);
    connect(m_searchWorker, &SearchWorker::failed, this, &MainWindow::onSearchFailed);
    connect(m_searchWorker, &SearchWorker::finished, m_searchThread, &QThread::quit);
    connect(m_searchWorker, &SearchWorker::failed, m_searchThread, &QThread::quit);
    QThread* const currentSearchThread = m_searchThread;
    connect(m_searchThread, &QThread::finished, this, [this, currentSearchThread]() {
        if (m_searchThread == currentSearchThread)
        {
            m_searchThread = nullptr;
            m_searchWorker = nullptr;
        }
        currentSearchThread->deleteLater();
    });

    m_searchThread->start();

    QMetaObject::invokeMethod(
        m_searchWorker,
        "process",
        Qt::QueuedConnection,
        Q_ARG(QString, m_rootDir),
        Q_ARG(QStringList, patterns),
        Q_ARG(TextEncoding, currentTargetEncoding()),
        Q_ARG(QStringList, excludePatterns),
        Q_ARG(int, m_threadCountSpin->value()));
}

void MainWindow::onSearchProgress(int current, int total)
{
    if (total <= 0)
    {
        m_searchProgress->setRange(0, 0);
        m_statusLabel->setText(QStringLiteral("Searching... scanned %1 files").arg(current));
    }
    else
    {
        m_searchProgress->setRange(0, total);
        m_searchProgress->setValue(current);
    }
}

void MainWindow::populateTree(const QVector<FileEncodingInfo>& files)
{
    clearTree();
    QMap<QString, QTreeWidgetItem*> folderMap;

    for (const FileEncodingInfo& item : files)
    {
        QFileInfo fi(item.filePath);
        QString folder = fi.absolutePath();
        folder = QDir::toNativeSeparators(QDir(m_rootDir).relativeFilePath(folder));
        if (folder.isEmpty() || folder == QStringLiteral("."))
        {
            folder = QStringLiteral("[root]");
        }

        QTreeWidgetItem* folderItem = folderMap.value(folder, nullptr);
        if (!folderItem)
        {
            folderItem = new QTreeWidgetItem(m_tree);
            folderItem->setText(0, folder);
            folderItem->setFirstColumnSpanned(true);
            folderItem->setExpanded(true);
            folderMap.insert(folder, folderItem);
        }

        auto* fileItem = new QTreeWidgetItem(folderItem);
        fileItem->setText(0, fi.fileName());
        fileItem->setToolTip(0, relativePathForDisplay(item.filePath));
        fileItem->setText(1, encodingToString(item.encoding));
        fileItem->setCheckState(0, Qt::Checked);
        fileItem->setData(0, kPathRole, item.filePath);
    }

    m_tree->expandAll();
    m_convertButton->setEnabled(m_tree->topLevelItemCount() > 0);
}

void MainWindow::onSearchFinished(const QVector<FileEncodingInfo>& files, int scannedTotal)
{
    populateTree(files);
    setSearchUiState(false);
    m_statusLabel->setText(
        QStringLiteral("Search complete. Scanned %1 files, %2 need conversion.")
            .arg(scannedTotal)
            .arg(files.size()));
}

void MainWindow::onSearchFailed(const QString& message)
{
    setSearchUiState(false);
    m_statusLabel->setText(QStringLiteral("Search failed: %1").arg(message));
    QMessageBox::critical(this, QStringLiteral("Search Failed"), message);
}

QStringList MainWindow::selectedFiles() const
{
    QStringList files;
    for (int i = 0; i < m_tree->topLevelItemCount(); ++i)
    {
        QTreeWidgetItem* folderItem = m_tree->topLevelItem(i);
        for (int j = 0; j < folderItem->childCount(); ++j)
        {
            QTreeWidgetItem* fileItem = folderItem->child(j);
            if (fileItem->checkState(0) == Qt::Checked)
            {
                const QString path = fileItem->data(0, kPathRole).toString();
                if (!path.isEmpty())
                {
                    files.push_back(path);
                }
            }
        }
    }
    return files;
}

void MainWindow::onConvert()
{
    const QStringList files = selectedFiles();
    if (files.isEmpty())
    {
        QMessageBox::information(this, QStringLiteral("No Files"), QStringLiteral("No checked files to convert."));
        return;
    }

    setConvertUiState(true);
    m_convertProgress->setRange(0, files.size());
    m_convertProgress->setValue(0);
    m_statusLabel->setText(QStringLiteral("Converting..."));

    if (m_convertThread)
    {
        m_convertThread->quit();
        m_convertThread->wait();
        m_convertThread->deleteLater();
        m_convertThread = nullptr;
        m_convertWorker = nullptr;
    }

    m_convertThread = new QThread(this);
    m_convertWorker = new ConvertWorker();
    m_convertWorker->moveToThread(m_convertThread);

    connect(m_convertThread, &QThread::finished, m_convertWorker, &QObject::deleteLater);
    connect(m_convertWorker, &ConvertWorker::progress, this, &MainWindow::onConvertProgress);
    connect(m_convertWorker, &ConvertWorker::finished, this, &MainWindow::onConvertFinished);
    connect(m_convertWorker, &ConvertWorker::finished, m_convertThread, &QThread::quit);
    QThread* const currentConvertThread = m_convertThread;
    connect(m_convertThread, &QThread::finished, this, [this, currentConvertThread]() {
        if (m_convertThread == currentConvertThread)
        {
            m_convertThread = nullptr;
            m_convertWorker = nullptr;
        }
        currentConvertThread->deleteLater();
    });

    m_convertThread->start();

    QMetaObject::invokeMethod(
        m_convertWorker,
        "process",
        Qt::QueuedConnection,
        Q_ARG(QStringList, files),
        Q_ARG(TextEncoding, currentTargetEncoding()));
}

void MainWindow::onConvertProgress(int current, int total)
{
    if (total <= 0)
    {
        total = 1;
    }
    m_convertProgress->setRange(0, total);
    m_convertProgress->setValue(current);
}

void MainWindow::onConvertFinished(const QVector<ConvertResult>& results)
{
    int successCount = 0;
    int failCount = 0;
    QStringList failMessages;
    for (const ConvertResult& item : results)
    {
        if (item.success)
        {
            ++successCount;
        }
        else
        {
            ++failCount;
            failMessages.push_back(QStringLiteral("%1 : %2").arg(relativePathForDisplay(item.filePath), item.message));
        }
    }

    setConvertUiState(false);
    m_statusLabel->setText(QStringLiteral("Convert complete. Success: %1, Failed: %2").arg(successCount).arg(failCount));

    if (failCount > 0)
    {
        QMessageBox::warning(
            this,
            QStringLiteral("Convert Finished With Errors"),
            QStringLiteral("Success: %1\nFailed: %2\n\n%3")
                .arg(successCount)
                .arg(failCount)
                .arg(failMessages.join('\n')));
    }
    else
    {
        QMessageBox::information(
            this,
            QStringLiteral("Convert Finished"),
            QStringLiteral("All selected files converted successfully."));
    }

    onSearch();
}

