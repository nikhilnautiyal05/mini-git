// Entry point of the Qt interface: pick a project folder, then open the window.
#include <QApplication>
#include <QDir>
#include <QFileDialog>
#include <QMessageBox>
#include <QString>
#include <QStringList>

#include "MainWindow.h"

int main(int argc, char* argv[])
{
    QApplication app(argc, argv);
    QApplication::setStyle("Fusion");

    // minigit-qt.exe [folder]   - without an argument a folder chooser opens.
    const QStringList args = app.arguments();
    QString folder;
    if (args.size() > 1)
        folder = args.at(1);
    else
        folder = QFileDialog::getExistingDirectory(0, "Choose your project folder");
    if (folder.isEmpty())
        return 0;

    // All minigit code works on the current folder, so move there first.
    if (!QDir::setCurrent(folder)) {
        QMessageBox::warning(0, "minigit", "Cannot open the folder:\n" + folder);
        return 1;
    }

    MainWindow window;
    window.show();
    return app.exec();
}
