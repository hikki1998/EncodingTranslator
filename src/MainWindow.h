#pragma once

#include "EncodingUtils.h"

#include <QMainWindow>

class QTreeWidget;
class QLineEdit;
class QPushButton;
class QComboBox;
class QSpinBox;
class QProgressBar;
class QLabel;
class QThread;
class SearchWorker;
class ConvertWorker;
class QTreeWidgetItem;

class MainWindow : public QMainWindow
{
    Q_OBJECT
public:
    explicit MainWindow(QWidget* parent = nullptr);
    ~MainWindow() override;

private slots:
    void onBrowse();
    void onSearch();
    void onConvert();

    void onSearchProgress(int current, int total);
    void onSearchFinished(const QVector<FileEncodingInfo>& files, int scannedTotal);
    void onSearchFailed(const QString& message);

    void onConvertProgress(int current, int total);
    void onConvertFinished(const QVector<ConvertResult>& results);

private:
    void setupUi();
    void applyStyle();
    void populateTree(const QVector<FileEncodingInfo>& files);
    QStringList defaultExtensions() const;
    QStringList defaultExcludes() const;
    TextEncoding currentTargetEncoding() const;
    QStringList selectedFiles() const;
    void setSearchUiState(bool searching);
    void setConvertUiState(bool converting);
    void clearTree();
    void updateSummary(int scannedTotal, int needConversion) const;
    QString relativePathForDisplay(const QString& absolutePath) const;

    QLineEdit* m_directoryEdit = nullptr;
    QPushButton* m_browseButton = nullptr;
    QLineEdit* m_extEdit = nullptr;
    QLineEdit* m_excludeEdit = nullptr;
    QSpinBox* m_threadCountSpin = nullptr;
    QComboBox* m_targetCombo = nullptr;
    QPushButton* m_searchButton = nullptr;
    QPushButton* m_convertButton = nullptr;
    QTreeWidget* m_tree = nullptr;
    QProgressBar* m_searchProgress = nullptr;
    QProgressBar* m_convertProgress = nullptr;
    QLabel* m_summaryLabel = nullptr;
    QLabel* m_statusLabel = nullptr;

    QThread* m_searchThread = nullptr;
    SearchWorker* m_searchWorker = nullptr;
    QThread* m_convertThread = nullptr;
    ConvertWorker* m_convertWorker = nullptr;

    QString m_rootDir;
};

