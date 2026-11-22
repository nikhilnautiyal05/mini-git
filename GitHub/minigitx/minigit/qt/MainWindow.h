#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QString>
#include <QStringList>

class QComboBox;
class QGroupBox;
class QLabel;
class QLineEdit;
class QPlainTextEdit;
class QPushButton;
class QTextEdit;
class QTimer;
class QTreeWidget;

// The main window. It does not run minigit.exe: it calls the same C++ classes
// (Repository, runCommand, ...) that the command-line program uses.
class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget* parent = 0);

private slots:
    void chooseFolder();
    void initRepo();
    void autoRefresh();
    void stageSelected();
    void stageAll();
    void showChanges();
    void openSelectedFile();
    void saveFile();
    void newFile();
    void deleteFile();
    void commit();
    void switchBranch();
    void createBranch();
    void mergeBranch();
    void abortMerge();
    void undo();
    void redo();
    void verify();
    void stats();
    void historySelectionChanged();

private:
    void buildUi();
    void reload(bool force);
    void setRepoControls(bool isRepo);
    bool runCmd(const QStringList& args, QString* output, bool log);
    bool runAndRefresh(const QStringList& args);
    void showDiffText(const QString& text, const QString& title);

    // top bar
    QLabel* folderLabel_;
    QLabel* branchLabel_;
    QPushButton* initBtn_;
    QPushButton* undoBtn_;
    QPushButton* redoBtn_;
    QPushButton* verifyBtn_;
    QPushButton* statsBtn_;
    // files
    QTreeWidget* files_;
    QPushButton* stageBtn_;
    QPushButton* stageAllBtn_;
    QPushButton* changesBtn_;
    // editor
    QLineEdit* fileName_;
    QPlainTextEdit* editor_;
    QPushButton* saveBtn_;
    QPushButton* deleteBtn_;
    // commit
    QLineEdit* message_;
    QPushButton* commitBtn_;
    // branches
    QLineEdit* newBranch_;
    QPushButton* createBranchBtn_;
    QComboBox* branchChoice_;
    QPushButton* switchBtn_;
    QPushButton* mergeBtn_;
    QPushButton* abortBtn_;
    // history + output
    QTreeWidget* history_;
    QGroupBox* diffBox_;
    QTextEdit* diff_;
    QPlainTextEdit* console_;

    QTimer* timer_;
    QString lastSig_;
};

#endif
