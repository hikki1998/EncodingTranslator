#include "MainWindow.h"

#include "ConvertWorker.h"
#include "SearchWorker.h"

#include <QColor>
#include <QComboBox>
#include <QDir>
#include <QFileDialog>
#include <QFont>
#include <QFrame>
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

QLabel* createFieldLabel(const QString& text, QWidget* parent)
{
    auto* label = new QLabel(text, parent);
    label->setObjectName(QStringLiteral("fieldLabel"));
    return label;
}

QColor encodingColor(TextEncoding encoding)
{
    switch (encoding)
    {
    case TextEncoding::Utf8:
        return QColor(QStringLiteral("#16794c"));
    case TextEncoding::Gbk:
        return QColor(QStringLiteral("#b35c00"));
    case TextEncoding::Ascii:
        return QColor(QStringLiteral("#5c6670"));
    default:
        return QColor(QStringLiteral("#b42318"));
    }
}
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
    resize(1120, 720);
    setMinimumSize(920, 620);

    auto* central = new QWidget(this);
    central->setObjectName(QStringLiteral("appRoot"));
    auto* rootLayout = new QVBoxLayout(central);
    rootLayout->setContentsMargins(16, 16, 16, 14);
    rootLayout->setSpacing(12);

    auto* header = new QFrame(central);
    header->setObjectName(QStringLiteral("headerBar"));
    auto* headerLayout = new QHBoxLayout(header);
    headerLayout->setContentsMargins(18, 14, 18, 14);
    headerLayout->setSpacing(14);

    auto* titleLayout = new QVBoxLayout();
    titleLayout->setSpacing(2);
    auto* titleLabel = new QLabel(QStringLiteral("Source Encoding Translator"), header);
    titleLabel->setObjectName(QStringLiteral("titleLabel"));
    auto* subtitleLabel = new QLabel(QStringLiteral("Batch scan and convert source file encodings"), header);
    subtitleLabel->setObjectName(QStringLiteral("subtitleLabel"));
    titleLayout->addWidget(titleLabel);
    titleLayout->addWidget(subtitleLabel);
    headerLayout->addLayout(titleLayout, 1);

    auto* targetLabel = createFieldLabel(QStringLiteral("Target"), header);
    targetLabel->setObjectName(QStringLiteral("headerFieldLabel"));
    m_targetCombo = new QComboBox(header);
    m_targetCombo->addItem(QStringLiteral("GBK"), static_cast<int>(TextEncoding::Gbk));
    m_targetCombo->addItem(QStringLiteral("UTF-8"), static_cast<int>(TextEncoding::Utf8));
    m_targetCombo->setMinimumWidth(104);

    auto* threadLabel = createFieldLabel(QStringLiteral("Threads"), header);
    threadLabel->setObjectName(QStringLiteral("headerFieldLabel"));
    m_threadCountSpin = new QSpinBox(header);
    m_threadCountSpin->setRange(1, 64);
    m_threadCountSpin->setValue(QThread::idealThreadCount());
    m_threadCountSpin->setToolTip(QStringLiteral("Number of threads for search"));
    m_threadCountSpin->setMinimumWidth(80);

    m_searchButton = new QPushButton(QStringLiteral("Search"), header);
    m_searchButton->setObjectName(QStringLiteral("primaryButton"));
    m_searchButton->setMinimumWidth(108);
    m_convertButton = new QPushButton(QStringLiteral("Convert Selected"), header);
    m_convertButton->setObjectName(QStringLiteral("accentButton"));
    m_convertButton->setMinimumWidth(148);
    m_convertButton->setEnabled(false);

    headerLayout->addWidget(targetLabel);
    headerLayout->addWidget(m_targetCombo);
    headerLayout->addWidget(threadLabel);
    headerLayout->addWidget(m_threadCountSpin);
    headerLayout->addWidget(m_searchButton);
    headerLayout->addWidget(m_convertButton);
    rootLayout->addWidget(header);

    auto* scanPanel = new QFrame(central);
    scanPanel->setObjectName(QStringLiteral("panel"));
    auto* scanLayout = new QVBoxLayout(scanPanel);
    scanLayout->setContentsMargins(14, 14, 14, 14);
    scanLayout->setSpacing(10);

    auto* pathLayout = new QHBoxLayout();
    pathLayout->setSpacing(10);
    auto* pathLabel = createFieldLabel(QStringLiteral("Directory"), scanPanel);
    m_directoryEdit = new QLineEdit(central);
    m_directoryEdit->setPlaceholderText(QStringLiteral("Choose a project directory to scan"));
    m_browseButton = new QPushButton(QStringLiteral("Browse"), central);
    m_browseButton->setObjectName(QStringLiteral("secondaryButton"));
    m_browseButton->setMinimumWidth(96);
    pathLayout->addWidget(pathLabel);
    pathLayout->addWidget(m_directoryEdit, 1);
    pathLayout->addWidget(m_browseButton);
    scanLayout->addLayout(pathLayout);

    auto* optionLayout = new QHBoxLayout();
    optionLayout->setSpacing(10);
    auto* extLabel = createFieldLabel(QStringLiteral("Extensions"), scanPanel);
    m_extEdit = new QLineEdit(central);
    m_extEdit->setPlaceholderText(QStringLiteral("e.g. .cpp,.h,.hpp,.c,.cc,.cxx"));
    m_extEdit->setText(defaultExtensions().join(QStringLiteral(",")));
    auto* excludeLabel = createFieldLabel(QStringLiteral("Exclude Dirs"), scanPanel);
    m_excludeEdit = new QLineEdit(central);
    m_excludeEdit->setPlaceholderText(QStringLiteral("e.g. build,Debug,.git,node_modules"));
    m_excludeEdit->setText(defaultExcludes().join(QStringLiteral(",")));
    optionLayout->addWidget(extLabel);
    optionLayout->addWidget(m_extEdit, 1);
    optionLayout->addWidget(excludeLabel);
    optionLayout->addWidget(m_excludeEdit, 1);
    scanLayout->addLayout(optionLayout);
    rootLayout->addWidget(scanPanel);

    auto* progressPanel = new QFrame(central);
    progressPanel->setObjectName(QStringLiteral("panel"));
    auto* progressLayout = new QHBoxLayout(progressPanel);
    progressLayout->setContentsMargins(14, 12, 14, 12);
    progressLayout->setSpacing(18);

    auto* searchProgressLayout = new QVBoxLayout();
    searchProgressLayout->setSpacing(6);
    auto* searchProgressLabel = createFieldLabel(QStringLiteral("Search Progress"), progressPanel);
    m_searchProgress = new QProgressBar(progressPanel);
    m_searchProgress->setRange(0, 100);
    m_searchProgress->setValue(0);
    searchProgressLayout->addWidget(searchProgressLabel);
    searchProgressLayout->addWidget(m_searchProgress);

    auto* convertProgressLayout = new QVBoxLayout();
    convertProgressLayout->setSpacing(6);
    auto* convertProgressLabel = createFieldLabel(QStringLiteral("Convert Progress"), progressPanel);
    m_convertProgress = new QProgressBar(progressPanel);
    m_convertProgress->setRange(0, 100);
    m_convertProgress->setValue(0);
    convertProgressLayout->addWidget(convertProgressLabel);
    convertProgressLayout->addWidget(m_convertProgress);

    progressLayout->addLayout(searchProgressLayout, 1);
    progressLayout->addLayout(convertProgressLayout, 1);
    rootLayout->addWidget(progressPanel);

    m_tree = new QTreeWidget(central);
    m_tree->setObjectName(QStringLiteral("resultTree"));
    m_tree->setColumnCount(2);
    m_tree->setHeaderLabels({QStringLiteral("File"), QStringLiteral("Detected Encoding")});
    m_tree->setAlternatingRowColors(true);
    m_tree->setUniformRowHeights(true);
    m_tree->setIndentation(22);
    m_tree->header()->setStretchLastSection(false);
    m_tree->header()->setSectionResizeMode(0, QHeaderView::Stretch);
    m_tree->header()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    rootLayout->addWidget(m_tree, 1);

    m_summaryLabel = new QLabel(central);
    m_summaryLabel->setObjectName(QStringLiteral("summaryLabel"));
    updateSummary(0, 0);
    rootLayout->addWidget(m_summaryLabel);

    m_statusLabel = new QLabel(QStringLiteral("Ready"), central);
    m_statusLabel->setObjectName(QStringLiteral("statusLabel"));
    rootLayout->addWidget(m_statusLabel);

    setCentralWidget(central);
    applyStyle();

    connect(m_browseButton, &QPushButton::clicked, this, &MainWindow::onBrowse);
    connect(m_searchButton, &QPushButton::clicked, this, &MainWindow::onSearch);
    connect(m_convertButton, &QPushButton::clicked, this, &MainWindow::onConvert);
}

void MainWindow::applyStyle()
{
    setStyleSheet(QStringLiteral(R"(
        QWidget#appRoot {
            background: #eef2f6;
            color: #18212f;
            font-family: "Segoe UI", "Microsoft YaHei UI", sans-serif;
            font-size: 10pt;
        }
        QFrame#headerBar {
            background: #18212f;
            border-radius: 8px;
        }
        QLabel#titleLabel {
            color: #ffffff;
            font-size: 18pt;
            font-weight: 700;
        }
        QLabel#subtitleLabel {
            color: #b8c2d0;
            font-size: 9pt;
        }
        QFrame#panel {
            background: #ffffff;
            border: 1px solid #d8dee8;
            border-radius: 8px;
        }
        QLabel#fieldLabel {
            color: #5d6878;
            font-size: 9pt;
            font-weight: 600;
        }
        QLabel#headerFieldLabel {
            color: #d7deea;
            font-size: 9pt;
            font-weight: 600;
        }
        QLineEdit, QComboBox, QSpinBox {
            min-height: 30px;
            padding: 4px 9px;
            background: #ffffff;
            border: 1px solid #c9d2df;
            border-radius: 6px;
            selection-background-color: #1f6feb;
        }
        QLineEdit:focus, QComboBox:focus, QSpinBox:focus {
            border-color: #1f6feb;
        }
        QPushButton {
            min-height: 32px;
            padding: 5px 14px;
            border-radius: 6px;
            border: 1px solid #b8c2d0;
            background: #ffffff;
            color: #18212f;
            font-weight: 600;
        }
        QPushButton:hover {
            background: #f4f7fb;
            border-color: #8fa2b8;
        }
        QPushButton:disabled {
            background: #edf1f5;
            border-color: #d8dee8;
            color: #9aa6b5;
        }
        QPushButton#primaryButton {
            background: #1f6feb;
            border-color: #1f6feb;
            color: #ffffff;
        }
        QPushButton#primaryButton:hover {
            background: #185fc9;
        }
        QPushButton#accentButton {
            background: #138a5b;
            border-color: #138a5b;
            color: #ffffff;
        }
        QPushButton#accentButton:hover {
            background: #0f734b;
        }
        QPushButton#secondaryButton {
            background: #f8fafc;
        }
        QProgressBar {
            min-height: 12px;
            max-height: 12px;
            border: 0;
            border-radius: 6px;
            background: #e5ebf2;
            text-align: center;
            color: transparent;
        }
        QProgressBar::chunk {
            border-radius: 6px;
            background: #1f6feb;
        }
        QTreeWidget#resultTree {
            background: #ffffff;
            alternate-background-color: #f8fafc;
            border: 1px solid #d8dee8;
            border-radius: 8px;
            outline: 0;
        }
        QTreeWidget#resultTree::item {
            min-height: 28px;
            padding: 3px 6px;
            border-bottom: 1px solid #edf1f5;
        }
        QTreeWidget#resultTree::item:hover {
            background: #edf6ff;
        }
        QTreeWidget#resultTree::item:selected {
            background: #dcecff;
            color: #18212f;
        }
        QHeaderView::section {
            min-height: 32px;
            padding: 5px 8px;
            background: #f3f6fa;
            border: 0;
            border-right: 1px solid #d8dee8;
            border-bottom: 1px solid #d8dee8;
            color: #4b5565;
            font-weight: 700;
        }
        QLabel#summaryLabel {
            color: #344054;
            font-weight: 600;
        }
        QLabel#statusLabel {
            min-height: 28px;
            padding: 6px 10px;
            background: #ffffff;
            border: 1px solid #d8dee8;
            border-radius: 6px;
            color: #344054;
        }
    )"));
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
    updateSummary(0, 0);
}

void MainWindow::updateSummary(int scannedTotal, int needConversion) const
{
    if (!m_summaryLabel)
    {
        return;
    }
    m_summaryLabel->setText(
        QStringLiteral("Scanned %1 files  |  Need conversion %2  |  Target %3")
            .arg(scannedTotal)
            .arg(needConversion)
            .arg(encodingToString(currentTargetEncoding())));
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
    updateSummary(0, 0);

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
        updateSummary(current, 0);
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
            QFont folderFont = folderItem->font(0);
            folderFont.setBold(true);
            folderItem->setFont(0, folderFont);
            folderItem->setForeground(0, QColor(QStringLiteral("#344054")));
            folderItem->setBackground(0, QColor(QStringLiteral("#f3f6fa")));
            folderMap.insert(folder, folderItem);
        }

        auto* fileItem = new QTreeWidgetItem(folderItem);
        fileItem->setText(0, fi.fileName());
        fileItem->setToolTip(0, relativePathForDisplay(item.filePath));
        fileItem->setText(1, encodingToString(item.encoding));
        fileItem->setForeground(1, encodingColor(item.encoding));
        QFont encodingFont = fileItem->font(1);
        encodingFont.setBold(true);
        fileItem->setFont(1, encodingFont);
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
    updateSummary(scannedTotal, files.size());
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
    m_summaryLabel->setText(QStringLiteral("Selected %1 files for conversion  |  Target %2")
                                .arg(files.size())
                                .arg(encodingToString(currentTargetEncoding())));

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
    m_summaryLabel->setText(QStringLiteral("Converted %1 files  |  Failed %2  |  Target %3")
                                .arg(successCount)
                                .arg(failCount)
                                .arg(encodingToString(currentTargetEncoding())));

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

