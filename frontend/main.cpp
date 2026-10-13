#include <QApplication>
#include <QWidget>
#include <QPushButton>
#include <QLabel>
#include <QLineEdit>
#include <QVBoxLayout>
#include <QHBoxLayout>

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);

    QWidget window;
    window.setWindowTitle("Mini Git");
    window.resize(450, 400);

    QVBoxLayout *layout = new QVBoxLayout(&window);

    QLabel *title = new QLabel("Mini Git Version Control System");
    layout->addWidget(title);

    QPushButton *initButton =
        new QPushButton("Initialize Repository");

    QPushButton *addButton =
        new QPushButton("Add File");

    QLabel *commitLabel =
        new QLabel("Commit Message:");

    QLineEdit *commitMessage =
        new QLineEdit();

    QPushButton *commitButton =
        new QPushButton("Commit");

    QPushButton *statusButton =
        new QPushButton("Status");

    QPushButton *historyButton =
        new QPushButton("History");

    QLabel *branchLabel =
        new QLabel("Branch:");

    QHBoxLayout *branchLayout = new QHBoxLayout();

    QPushButton *createBranchButton =
        new QPushButton("Create Branch");

    QPushButton *switchBranchButton =
        new QPushButton("Switch Branch");

    branchLayout->addWidget(createBranchButton);
    branchLayout->addWidget(switchBranchButton);

    QLabel *status =
        new QLabel("Status: Ready");

    layout->addWidget(initButton);
    layout->addWidget(addButton);
    layout->addWidget(commitLabel);
    layout->addWidget(commitMessage);
    layout->addWidget(commitButton);
    layout->addWidget(statusButton);
    layout->addWidget(historyButton);
    layout->addWidget(branchLabel);
    layout->addLayout(branchLayout);
    layout->addWidget(status);

    window.show();

    return app.exec();
}
