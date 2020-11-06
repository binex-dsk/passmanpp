#include "file_handler.h"
#include "security.h"

int FileHandler::backup(std::string path) {
    QString fileName = QFileDialog::getSaveFileName(this, tr("Backup Location"), "", tr("passman++ Database Files (*.pdpp);;All Files (*)"));
    if (fileName.isEmpty()) return 3;
    QFile file(fileName);
    if (!file.open(QIODevice::WriteOnly)) return 17;
    std::vector<std::string> mp;
    try {
        mp = getmpass(" to backup", path);
    } catch (std::exception& e) {
        displayErr(e.what());
    }
    encrypt(mp[1], fileName.toStdString());
    return 0;
}
std::string FileHandler::newLoc() {
    QString fileName = QFileDialog::getSaveFileName(this, tr("New Database Location"), "", tr("passman++ Database Files (*.pdpp);;All Files (*)"));
    return fileName.toStdString();
}
std::string FileHandler::getDb() {
    QString fileName = QFileDialog::getOpenFileName(this, tr("Open Database"), "", tr("passman++ Database Files (*.pdpp);;All Files (*)"));
    return fileName.toStdString();
}
