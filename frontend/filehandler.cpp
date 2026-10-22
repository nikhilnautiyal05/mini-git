#include "filehandler.h"
#include <QFileDialog>

QString FileHandler::selectFile()
{
    return QFileDialog::getOpenFileName(
        nullptr,
        "Select File"
    );
}