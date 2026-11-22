#include "MainWindow.h"

// minigit core (the same classes the command-line program uses)
#include "Commands.h"
#include "FileUtil.h"
#include "Repository.h"
#include "Undo.h"

#include <QBrush>
#include <QColor>
#include <QComboBox>
#include <QDateTime>
#include <QDir>
#include <QFileDialog>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QInputDialog>
#include <QLabel>
#include <QLineEdit>
#include <QList>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QSplitter>
#include <QStatusBar>
#include <QTextEdit>
#include <QTimer>
#include <QTreeWidget>
#include <QVBoxLayout>
#include <QWidget>

#include <algorithm>
#include <iostream>
#include <map>
#include <set>
#include <sstream>
#include <string>
#include <vector>

// ============================================================ construction ===
MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent)
{
    buildUi();

    // Look for changes made outside the window (for example in VS Code).
    timer_ = new QTimer(this);
    connect(timer_, &QTimer::timeout, this, &MainWindow::autoRefresh);
    timer_->start(3000);

    reload(true);
}

void MainWindow::buildUi()
{
    setWindowTitle("minigit");
    resize(1250, 820);

    QWidget* central = new QWidget(this);
    setCentralWidget(central);
    QVBoxLayout* root = new QVBoxLayout(central);

    // ---------------------------------------------------------- top bar ----
    QHBoxLayout* top = new QHBoxLayout();
    folderLabel_ = new QLabel();
    folderLabel_->setStyleSheet("color:#555555;");
    branchLabel_ = new QLabel();
    branchLabel_->setStyleSheet(
        "background:#1d4ed8; color:white; border-radius:9px; padding:2px 10px; font-weight:bold;");
    QPushButton* openBtn = new QPushButton("Open folder...");
    initBtn_ = new QPushButton("Initialize repository");
    undoBtn_ = new QPushButton("Undo");
    redoBtn_ = new QPushButton("Redo");
    verifyBtn_ = new QPushButton("Verify");
    statsBtn_ = new QPushButton("Stats");
    top->addWidget(folderLabel_, 1);
    top->addWidget(branchLabel_);
    top->addWidget(openBtn);
    top->addWidget(initBtn_);
    top->addWidget(undoBtn_);
    top->addWidget(redoBtn_);
    top->addWidget(verifyBtn_);
    top->addWidget(statsBtn_);
    root->addLayout(top);

    // ------------------------------------------------------ left column ----
    QWidget* left = new QWidget();
    QVBoxLayout* leftLayout = new QVBoxLayout(left);
    leftLayout->setContentsMargins(0, 0, 0, 0);

    // 1. files
    QGroupBox* filesBox = new QGroupBox("1. Files in the working folder");
    QVBoxLayout* filesLayout = new QVBoxLayout(filesBox);
    files_ = new QTreeWidget();
    files_->setColumnCount(2);
    files_->setHeaderLabels(QStringList() << "File" << "Status");
    files_->setRootIsDecorated(false);
    files_->setColumnWidth(0, 230);
    files_->setMinimumHeight(120);
    filesLayout->addWidget(files_);
    QHBoxLayout* fileButtons = new QHBoxLayout();
    stageBtn_ = new QPushButton("Stage selected");
    stageAllBtn_ = new QPushButton("Stage all");
    changesBtn_ = new QPushButton("Show changes");
    fileButtons->addWidget(stageBtn_);
    fileButtons->addWidget(stageAllBtn_);
    fileButtons->addWidget(changesBtn_);
    filesLayout->addLayout(fileButtons);
    leftLayout->addWidget(filesBox, 2);

    // editor
    QGroupBox* editBox = new QGroupBox("Edit or create a file (double-click a file above to open it)");
    QVBoxLayout* editLayout = new QVBoxLayout(editBox);
    fileName_ = new QLineEdit();
    fileName_->setPlaceholderText("file name, for example notes.txt or src/main.cpp");
    editor_ = new QPlainTextEdit();
    editor_->setPlaceholderText("Type the file content here, then click Save file");
    editor_->setMinimumHeight(100);
    QHBoxLayout* editButtons = new QHBoxLayout();
    saveBtn_ = new QPushButton("Save file");
    QPushButton* newBtn = new QPushButton("New");
    deleteBtn_ = new QPushButton("Delete file");
    editButtons->addWidget(saveBtn_);
    editButtons->addWidget(newBtn);
    editButtons->addWidget(deleteBtn_);
    editLayout->addWidget(fileName_);
    editLayout->addWidget(editor_);
    editLayout->addLayout(editButtons);
    leftLayout->addWidget(editBox, 2);

    // 2. commit
    QGroupBox* commitBox = new QGroupBox("2. Commit the staged files");
    QHBoxLayout* commitLayout = new QHBoxLayout(commitBox);
    message_ = new QLineEdit();
    message_->setPlaceholderText("Commit message");
    commitBtn_ = new QPushButton("Commit");
    commitLayout->addWidget(message_, 1);
    commitLayout->addWidget(commitBtn_);
    leftLayout->addWidget(commitBox);

    // branches
    QGroupBox* branchBox = new QGroupBox("Branches");
    QVBoxLayout* branchLayout = new QVBoxLayout(branchBox);
    QHBoxLayout* newRow = new QHBoxLayout();
    newBranch_ = new QLineEdit();
    newBranch_->setPlaceholderText("new branch name");
    createBranchBtn_ = new QPushButton("Create and switch");
    newRow->addWidget(newBranch_, 1);
    newRow->addWidget(createBranchBtn_);
    QHBoxLayout* otherRow = new QHBoxLayout();
    branchChoice_ = new QComboBox();
    switchBtn_ = new QPushButton("Switch to it");
    mergeBtn_ = new QPushButton("Merge into current");
    otherRow->addWidget(branchChoice_, 1);
    otherRow->addWidget(switchBtn_);
    otherRow->addWidget(mergeBtn_);
    abortBtn_ = new QPushButton("Abort merge (conflicts)");
    abortBtn_->setVisible(false);
    branchLayout->addLayout(newRow);
    branchLayout->addLayout(otherRow);
    branchLayout->addWidget(abortBtn_);
    leftLayout->addWidget(branchBox);

    // ----------------------------------------------------- right column ----
    QWidget* right = new QWidget();
    QVBoxLayout* rightLayout = new QVBoxLayout(right);
    rightLayout->setContentsMargins(0, 0, 0, 0);

    QGroupBox* historyBox = new QGroupBox("3. History (click a commit to see what it changed)");
    QVBoxLayout* historyLayout = new QVBoxLayout(historyBox);
    history_ = new QTreeWidget();
    history_->setColumnCount(6);
    history_->setHeaderLabels(QStringList() << "Commit" << "Message" << "Author" << "Date"
                                            << "Parents" << "Branches");
    history_->setRootIsDecorated(false);
    historyLayout->addWidget(history_);
    rightLayout->addWidget(historyBox, 3);

    diffBox_ = new QGroupBox("Changes");
    QVBoxLayout* diffLayout = new QVBoxLayout(diffBox_);
    diff_ = new QTextEdit();
    diff_->setReadOnly(true);
    diff_->setStyleSheet("background:#0f172a; color:#e2e8f0; font-family:Consolas,monospace;");
    diffLayout->addWidget(diff_);
    rightLayout->addWidget(diffBox_, 2);

    QSplitter* split = new QSplitter(Qt::Horizontal);
    split->addWidget(left);
    split->addWidget(right);
    split->setStretchFactor(0, 0);
    split->setStretchFactor(1, 1);
    split->setSizes(QList<int>() << 440 << 800);
    root->addWidget(split, 1);

    // ------------------------------------------------------ output panel ----
    root->addWidget(new QLabel("Command output (the real minigit command behind every click)"));
    console_ = new QPlainTextEdit();
    console_->setReadOnly(true);
    console_->setMaximumHeight(150);
    console_->setStyleSheet("background:#0b1220; color:#d1d5db; font-family:Consolas,monospace;");
    root->addWidget(console_);

    // ------------------------------------------------------- connections ----
    connect(openBtn, &QPushButton::clicked, this, &MainWindow::chooseFolder);
    connect(initBtn_, &QPushButton::clicked, this, &MainWindow::initRepo);
    connect(undoBtn_, &QPushButton::clicked, this, &MainWindow::undo);
    connect(redoBtn_, &QPushButton::clicked, this, &MainWindow::redo);
    connect(verifyBtn_, &QPushButton::clicked, this, &MainWindow::verify);
    connect(statsBtn_, &QPushButton::clicked, this, &MainWindow::stats);
    connect(stageBtn_, &QPushButton::clicked, this, &MainWindow::stageSelected);
    connect(stageAllBtn_, &QPushButton::clicked, this, &MainWindow::stageAll);
    connect(changesBtn_, &QPushButton::clicked, this, &MainWindow::showChanges);
    connect(files_, &QTreeWidget::itemDoubleClicked, this, &MainWindow::openSelectedFile);
    connect(saveBtn_, &QPushButton::clicked, this, &MainWindow::saveFile);
    connect(newBtn, &QPushButton::clicked, this, &MainWindow::newFile);
    connect(deleteBtn_, &QPushButton::clicked, this, &MainWindow::deleteFile);
    connect(commitBtn_, &QPushButton::clicked, this, &MainWindow::commit);
    connect(message_, &QLineEdit::returnPressed, this, &MainWindow::commit);
    connect(createBranchBtn_, &QPushButton::clicked, this, &MainWindow::createBranch);
    connect(newBranch_, &QLineEdit::returnPressed, this, &MainWindow::createBranch);
    connect(switchBtn_, &QPushButton::clicked, this, &MainWindow::switchBranch);
    connect(mergeBtn_, &QPushButton::clicked, this, &MainWindow::mergeBranch);
    connect(abortBtn_, &QPushButton::clicked, this, &MainWindow::abortMerge);
    connect(history_, &QTreeWidget::itemSelectionChanged, this, &MainWindow::historySelectionChanged);
}

// ============================================================ running minigit ===
// Runs one minigit command in-process, exactly like the command line would,
// and captures what it prints.
bool MainWindow::runCmd(const QStringList& args, QString* output, bool log)
{
    if (args.isEmpty())
        return false;

    std::vector<std::string> rest;
    for (int i = 1; i < args.size(); ++i)
        rest.push_back(args.at(i).toStdString());

    std::ostringstream out;
    std::ostringstream err;
    std::streambuf* old = std::cout.rdbuf(out.rdbuf());   // capture normal output
    int rc = 2;
    try {
        rc = runCommand(args.at(0).toStdString(), rest, false, err);
    } catch (...) {
        err << "unexpected error\n";
    }
    std::cout.rdbuf(old);

    const QString o = QString::fromStdString(out.str());
    const QString e = QString::fromStdString(err.str());
    if (output)
        *output = o;

    if (log) {
        console_->appendPlainText("> minigit " + args.join(" "));
        if (!o.trimmed().isEmpty())
            console_->appendPlainText(o.trimmed());
        if (!e.trimmed().isEmpty())
            console_->appendPlainText(e.trimmed());
    }
    if (rc != 0 && !e.trimmed().isEmpty())
        statusBar()->showMessage(e.trimmed(), 8000);
    return rc == 0;
}

bool MainWindow::runAndRefresh(const QStringList& args)
{
    const bool ok = runCmd(args, 0, true);
    reload(true);
    return ok;
}

// ================================================================= redraw ===
void MainWindow::setRepoControls(bool isRepo)
{
    initBtn_->setVisible(!isRepo);
    stageBtn_->setEnabled(isRepo);
    stageAllBtn_->setEnabled(isRepo);
    changesBtn_->setEnabled(isRepo);
    commitBtn_->setEnabled(isRepo);
    createBranchBtn_->setEnabled(isRepo);
    switchBtn_->setEnabled(isRepo);
    mergeBtn_->setEnabled(isRepo);
    undoBtn_->setEnabled(isRepo);
    redoBtn_->setEnabled(isRepo);
    verifyBtn_->setEnabled(isRepo);
    statsBtn_->setEnabled(isRepo);
    if (!isRepo)
        abortBtn_->setVisible(false);
}

void MainWindow::autoRefresh()
{
    reload(false);
}

// Re-reads the repository and rebuilds the lists. With force == false nothing
// is touched unless something actually changed (so selections are kept).
void MainWindow::reload(bool force)
{
    const bool isRepo = fs::isDir(Repository::DIR);
    const QString folder = QString::fromStdString(fs::cwd());
    folderLabel_->setText(folder);
    setWindowTitle("minigit - " + folder);
    setRepoControls(isRepo);

    if (!isRepo) {
        branchLabel_->setText("not a repository yet");
        if (force || lastSig_ != "none") {
            files_->clear();
            history_->clear();
            branchChoice_->clear();
            lastSig_ = "none";
        }
        return;
    }

    try {
        Repository repo("");
        const StatusResult st = repo.status();
        const std::vector<std::string> work = repo.workingFiles();
        const std::vector<CommitInfo> commits = repo.allHistory();
        const std::vector<std::string> branches = repo.refs().branches();
        const std::string head = repo.refs().headCommit();
        const std::string cur = repo.refs().currentBranch();
        const bool canUndo = !UndoLog::load(Repository::DIR + "/undo").empty();
        const bool canRedo = !UndoLog::load(Repository::DIR + "/redo").empty();

        // A "signature" of the whole state: unchanged signature = nothing to redraw.
        std::string sig = head + "|" + cur + (st.merging ? "|M" : "|-");
        for (size_t i = 0; i < st.staged.size(); ++i)
            sig += "|s" + std::string(1, st.staged[i].first) + st.staged[i].second;
        for (size_t i = 0; i < st.unstaged.size(); ++i)
            sig += "|u" + std::string(1, st.unstaged[i].first) + st.unstaged[i].second;
        for (size_t i = 0; i < st.untracked.size(); ++i)
            sig += "|t" + st.untracked[i];
        for (size_t i = 0; i < work.size(); ++i)
            sig += "|w" + work[i];
        for (size_t i = 0; i < branches.size(); ++i)
            sig += "|b" + branches[i] + "=" + repo.refs().branchTip(branches[i]);
        for (size_t i = 0; i < commits.size(); ++i)
            sig += "|c" + commits[i].hash;
        sig += canUndo ? "|U" : "|-";
        sig += canRedo ? "|R" : "|-";
        const QString qsig = QString::fromStdString(sig);
        if (!force && qsig == lastSig_)
            return;
        lastSig_ = qsig;

        // remember what is selected so it can be selected again
        QString selFile;
        if (files_->currentItem())
            selFile = files_->currentItem()->text(0);
        QString selCommit;
        const QList<QTreeWidgetItem*> selected = history_->selectedItems();
        if (!selected.isEmpty())
            selCommit = selected.first()->data(0, Qt::UserRole).toString();

        // ---- top bar ----
        if (cur.empty())
            branchLabel_->setText(QString("detached HEAD"));
        else
            branchLabel_->setText(QString::fromStdString("on branch " + cur));
        undoBtn_->setEnabled(canUndo);
        redoBtn_->setEnabled(canRedo);
        commitBtn_->setEnabled(!st.staged.empty() || st.merging);
        abortBtn_->setVisible(st.merging);

        // ---- files: one row per file with a coloured status ----
        std::set<std::string> names(work.begin(), work.end());
        std::map<std::string, std::string> label;   // path -> status text
        std::map<std::string, int> kind;            // 0 committed, 1 staged, 2 needs staging
        for (size_t i = 0; i < st.staged.size(); ++i) {
            const std::string& p = st.staged[i].second;
            const char c = st.staged[i].first;
            names.insert(p);
            std::string& l = label[p];
            if (!l.empty()) l += ", ";
            l += (c == 'A') ? "staged: new" : (c == 'D') ? "staged: deleted" : "staged: modified";
            kind[p] = std::max(kind[p], 1);
        }
        for (size_t i = 0; i < st.unstaged.size(); ++i) {
            const std::string& p = st.unstaged[i].second;
            names.insert(p);
            std::string& l = label[p];
            if (!l.empty()) l += ", ";
            l += (st.unstaged[i].first == 'D') ? "deleted" : "modified";
            kind[p] = std::max(kind[p], 2);
        }
        for (size_t i = 0; i < st.untracked.size(); ++i) {
            const std::string& p = st.untracked[i];
            names.insert(p);
            std::string& l = label[p];
            if (!l.empty()) l += ", ";
            l += "untracked";
            kind[p] = std::max(kind[p], 2);
        }

        files_->clear();
        for (std::set<std::string>::const_iterator it = names.begin(); it != names.end(); ++it) {
            QTreeWidgetItem* item = new QTreeWidgetItem(files_);
            item->setText(0, QString::fromStdString(*it));
            const std::string text = label.count(*it) ? label[*it] : std::string("committed");
            item->setText(1, QString::fromStdString(text));
            const int k = kind.count(*it) ? kind[*it] : 0;
            QColor color("#888888");
            if (k >= 2) color = QColor("#b45309");
            else if (k == 1) color = QColor("#15803d");
            item->setForeground(1, QBrush(color));
            if (item->text(0) == selFile)
                files_->setCurrentItem(item);
        }

        // ---- branches: the drop-down lists the other branches ----
        branchChoice_->clear();
        std::map<std::string, std::string> tipNames;   // commit hash -> branch names
        for (size_t i = 0; i < branches.size(); ++i) {
            if (branches[i] == cur)
                continue;
            branchChoice_->addItem(QString::fromStdString(branches[i]));
            std::string& s = tipNames[repo.refs().branchTip(branches[i])];
            if (!s.empty()) s += ", ";
            s += branches[i];
        }
        const bool others = branchChoice_->count() > 0;
        switchBtn_->setEnabled(others);
        mergeBtn_->setEnabled(others && !st.merging);

        // ---- history: newest first, with parents so merges are visible ----
        history_->blockSignals(true);
        history_->clear();
        QTreeWidgetItem* reselect = 0;
        for (size_t i = 0; i < commits.size(); ++i) {
            const Commit& c = commits[i].commit;
            const std::string& h = commits[i].hash;
            QTreeWidgetItem* item = new QTreeWidgetItem(history_);
            item->setText(0, QString::fromStdString(util::shortHash(h)));
            item->setData(0, Qt::UserRole, QString::fromStdString(h));
            item->setText(1, QString::fromStdString(c.summary()));
            item->setText(2, QString::fromStdString(c.authorName()));
            item->setText(3, QDateTime::fromSecsSinceEpoch((qint64)c.timestamp()).toString("yyyy-MM-dd HH:mm"));

            std::string parents;
            for (size_t k = 0; k < c.parents.size(); ++k) {
                if (k) parents += " ";
                parents += util::shortHash(c.parents[k]);
            }
            if (c.parents.size() > 1) parents += "  (merge)";
            item->setText(4, QString::fromStdString(parents));

            std::string refs;
            if (h == head)
                refs = cur.empty() ? std::string("HEAD") : "HEAD -> " + cur;
            const std::map<std::string, std::string>::const_iterator tn = tipNames.find(h);
            if (tn != tipNames.end()) {
                if (!refs.empty()) refs += ", ";
                refs += tn->second;
            }
            item->setText(5, QString::fromStdString(refs));

            if (QString::fromStdString(h) == selCommit)
                reselect = item;
        }
        if (reselect)
            history_->setCurrentItem(reselect);
        history_->blockSignals(false);
        for (int col = 0; col < 6; ++col)
            history_->resizeColumnToContents(col);
    } catch (const std::exception& e) {
        statusBar()->showMessage(QString::fromStdString(e.what()), 6000);
    }
}

void MainWindow::showDiffText(const QString& text, const QString& title)
{
    diffBox_->setTitle(title);
    QString html = "<pre style=\"font-family:Consolas,monospace; font-size:12px;\">";
    const QStringList lines = text.split(QChar('\n'));
    for (int i = 0; i < lines.size(); ++i) {
        const QString& l = lines.at(i);
        QString color;
        if (l.startsWith("###")) color = "#fbbf24";
        else if (l.startsWith("+++") || l.startsWith("---") || l.startsWith("diff ") || l.startsWith("index "))
            color = "#ffffff";
        else if (l.startsWith("@@")) color = "#60a5fa";
        else if (l.startsWith("+")) color = "#4ade80";
        else if (l.startsWith("-")) color = "#f87171";

        if (color.isEmpty())
            html += l.toHtmlEscaped() + "\n";
        else
            html += "<span style=\"color:" + color + "\">" + l.toHtmlEscaped() + "</span>\n";
    }
    html += "</pre>";
    diff_->setHtml(html);
}

// ================================================================== actions ===
void MainWindow::chooseFolder()
{
    const QString dir = QFileDialog::getExistingDirectory(this, "Choose your project folder", QDir::currentPath());
    if (dir.isEmpty())
        return;
    if (!QDir::setCurrent(dir)) {
        QMessageBox::warning(this, "minigit", "Cannot open that folder.");
        return;
    }
    fileName_->clear();
    editor_->clear();
    diff_->clear();
    lastSig_.clear();
    reload(true);
}

void MainWindow::initRepo()
{
    if (!runAndRefresh(QStringList() << "init"))
        return;

    bool ok = false;
    const QString name = QInputDialog::getText(this, "Your name", "Name to put on your commits:",
                                               QLineEdit::Normal, QString(), &ok);
    if (ok && !name.trimmed().isEmpty())
        runCmd(QStringList() << "config" << "user.name" << name.trimmed(), 0, true);

    ok = false;
    const QString mail = QInputDialog::getText(this, "Your email", "Email to put on your commits:",
                                               QLineEdit::Normal, QString(), &ok);
    if (ok && !mail.trimmed().isEmpty())
        runCmd(QStringList() << "config" << "user.email" << mail.trimmed(), 0, true);
    reload(true);
}

void MainWindow::stageSelected()
{
    QTreeWidgetItem* item = files_->currentItem();
    if (!item) {
        statusBar()->showMessage("Select a file in the list first.", 4000);
        return;
    }
    runAndRefresh(QStringList() << "add" << item->text(0));
}

void MainWindow::stageAll()
{
    runAndRefresh(QStringList() << "add" << ".");
}

void MainWindow::showChanges()
{
    QString notStaged;
    QString staged;
    runCmd(QStringList() << "diff", &notStaged, false);
    runCmd(QStringList() << "diff" << "--staged", &staged, false);

    QString text;
    if (!notStaged.isEmpty())
        text += "### Not staged yet\n" + notStaged;
    if (!staged.isEmpty()) {
        if (!text.isEmpty())
            text += "\n";
        text += "### Staged (will go into the next commit)\n" + staged;
    }
    if (text.isEmpty())
        text = "No changes. Edit a file and save it.";
    showDiffText(text, "Working changes");
}

void MainWindow::openSelectedFile()
{
    QTreeWidgetItem* item = files_->currentItem();
    if (!item)
        return;
    const std::string path = item->text(0).toStdString();
    if (!fs::isFile(path)) {
        statusBar()->showMessage("That file no longer exists in the folder.", 4000);
        return;
    }
    try {
        fileName_->setText(item->text(0));
        editor_->setPlainText(QString::fromStdString(fs::readFile(path)));
    } catch (const std::exception& e) {
        statusBar()->showMessage(QString::fromStdString(e.what()), 5000);
    }
}

void MainWindow::saveFile()
{
    const QString name = fileName_->text().trimmed();
    if (name.isEmpty()) {
        statusBar()->showMessage("Type a file name first.", 4000);
        return;
    }
    try {
        const std::string path = fs::safeRelativePath(name.toStdString());
        fs::writeFile(path, editor_->toPlainText().toStdString());
        console_->appendPlainText("> saved " + QString::fromStdString(path));
    } catch (const std::exception& e) {
        console_->appendPlainText(QString("Save failed: ") + e.what());
    }
    reload(true);
}

void MainWindow::newFile()
{
    fileName_->clear();
    editor_->clear();
    fileName_->setFocus();
}

void MainWindow::deleteFile()
{
    const QString name = fileName_->text().trimmed();
    if (name.isEmpty())
        return;
    if (QMessageBox::question(this, "Delete file", "Delete " + name + " from the folder?") != QMessageBox::Yes)
        return;
    try {
        const std::string path = fs::safeRelativePath(name.toStdString());
        if (!fs::isFile(path))
            throw MiniGitError("file '" + path + "' does not exist");
        fs::removeFile(path);
        fs::removeEmptyParents(path);
        console_->appendPlainText("> deleted " + QString::fromStdString(path));
        fileName_->clear();
        editor_->clear();
    } catch (const std::exception& e) {
        console_->appendPlainText(QString("Delete failed: ") + e.what());
    }
    reload(true);
}

void MainWindow::commit()
{
    const QString msg = message_->text().simplified();
    const bool merging = fs::exists(Repository::DIR + "/MERGE_HEAD");
    if (msg.isEmpty() && !merging) {
        statusBar()->showMessage("Write a commit message first.", 4000);
        return;
    }
    if (!commitBtn_->isEnabled())
        return;
    QStringList args;
    args << "commit";
    if (!msg.isEmpty())
        args << "-m" << msg;
    if (runAndRefresh(args))
        message_->clear();
}

void MainWindow::switchBranch()
{
    const QString name = branchChoice_->currentText();
    if (!name.isEmpty())
        runAndRefresh(QStringList() << "checkout" << name);
}

void MainWindow::createBranch()
{
    const QString name = newBranch_->text().trimmed();
    if (name.isEmpty()) {
        statusBar()->showMessage("Type a branch name first.", 4000);
        return;
    }
    if (runAndRefresh(QStringList() << "checkout" << "-b" << name))
        newBranch_->clear();
}

void MainWindow::mergeBranch()
{
    const QString name = branchChoice_->currentText();
    if (!name.isEmpty())
        runAndRefresh(QStringList() << "merge" << name);
}

void MainWindow::abortMerge()
{
    runAndRefresh(QStringList() << "merge" << "--abort");
}

void MainWindow::undo()
{
    runAndRefresh(QStringList() << "undo");
}

void MainWindow::redo()
{
    runAndRefresh(QStringList() << "redo");
}

void MainWindow::verify()
{
    QString out;
    runCmd(QStringList() << "verify", &out, true);
    QMessageBox::information(this, "Verify", out.trimmed().isEmpty() ? QString("Done.") : out.trimmed());
}

void MainWindow::stats()
{
    QString out;
    runCmd(QStringList() << "stats", &out, true);
    QMessageBox::information(this, "Statistics", out.trimmed().isEmpty() ? QString("No data yet.") : out.trimmed());
}

void MainWindow::historySelectionChanged()
{
    const QList<QTreeWidgetItem*> selected = history_->selectedItems();
    if (selected.isEmpty())
        return;
    const QString hash = selected.first()->data(0, Qt::UserRole).toString();
    QString out;
    runCmd(QStringList() << "diff" << hash, &out, false);
    showDiffText(out.isEmpty() ? QString("No file changes in this commit.") : out,
                 "Commit " + hash.left(7));
}
