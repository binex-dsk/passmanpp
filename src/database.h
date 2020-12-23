#ifndef DATABASE_H
#define DATABASE_H

#include <botan/compression.h>
#include <botan/pwdhash.h>
#include <botan/hash.h>
#include <botan/cipher_mode.h>
#include <botan/auto_rng.h>
#include <botan/hex.h>
#include <experimental/filesystem>
#include <QTableWidgetItem>

#include "constants.h"
class Entry;

void showMessage(const QString &msg);
std::string getCS(uint8_t cs, uint8_t encr);

class Database
{
public:
    Database();

    secvec getPw(QString password);
    bool showErr(QString msg);
    bool parse();

    void encrypt(const QString &password);
    QString decrypt(const QString &txt = "", const QString &password = "");
    bool config(bool create = true);

    bool open();
    int backup();

    bool save(const QString &password = "");
    bool convert();

    int verify(const QString &mpass);
    void get();

    int add(QTableWidget *table);
    int edit();

    bool saveSt(bool exec = true);

    Entry *entryNamed(const QString &name);

    template <typename Func>
    QAction *addButton(QIcon icon, const char *whatsThis, QKeySequence shortcut, Func func);

    bool keyFile;
    bool modified = false;

    uint8_t checksum = 0;
    uint8_t hash = 0;
    uint8_t hashIters = 0;
    uint8_t encryption = 0;
    uint8_t version = MAX_SUPPORTED_VERSION_NUMBER;

    uint8_t ivLen;

    secvec iv;
    secvec data;

    QString name;
    QString desc;

    QString stList;
    QString path;
    QString keyFilePath;

    QList<Entry *> entries;
};

#endif // DATABASE_H
