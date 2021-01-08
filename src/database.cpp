#include <QComboBox>
#include <QSpinBox>
#include <QUrl>
#include <QDesktopServices>
#include <QInputDialog>
#include <QDialogButtonBox>
#include <QLabel>
#include <QFormLayout>
#include <QPushButton>
#include <QMenuBar>
#include <QHeaderView>
#include <QToolButton>
#include <QApplication>
#include <QClipboard>
#include <QTimer>
#include <QCheckBox>
#include <QSizePolicy>

#include "entry.h"
#include "gui/database_edit_dialog.h"
#include "gui/config_dialog.h"
#include "gui/password_dialog.h"

Database::Database() {}

void showMessage(const QString &msg) {
    QMessageBox box;

    box.setText(msg);
    box.setStandardButtons(QMessageBox::Ok);
    box.setTextInteractionFlags(Qt::LinksAccessibleByMouse | Qt::TextSelectableByMouse);

    box.exec();
}

std::string getCS(uint8_t cs, uint8_t encr) {
    std::unique_ptr<Botan::Cipher_Mode> enc = Botan::Cipher_Mode::create(encryptionMatch.at(encr), Botan::ENCRYPTION);
    std::string checksumChoice(checksumMatch[cs]);

    if (checksumChoice != "SHA-512") {
        checksumChoice += "(" + std::to_string(enc->maximum_keylength() * 8) + ")";
    }
    return checksumChoice;
}

void Database::get() {
    entries = {};
    for (QSqlQuery q : selectAll()) {
        while (q.next()) {
            QList<Field *> fields;

            for (int i = 0; i < q.record().count(); ++i) {
                QString val = q.record().value(i).toString().replace(" || char(10) || ", "\n");
                QString vName = q.record().fieldName(i);
                QMetaType::Type id = (QMetaType::Type)q.record().field(i).metaType().id();

                Field *f = new Field(vName, val, id);
                fields.push_back(f);
            }

            Entry *entry = new Entry(fields, this);
            entries.push_back(entry);
        }
    }
}

int Database::add(QTableWidget *table) {
    Entry *entry = new Entry({}, this);
    entry->setDefaults();
    entries.push_back(entry);

    int ret = entry->edit(nullptr, table);
    return ret;
}

int Database::edit() {
    DatabaseEditDialog *di = new DatabaseEditDialog(this);
    di->init();
    di->setup();
    return di->show();
}

bool Database::saveSt(bool exec) {
    QString execSt = "";

    for (Entry *entry : entries) {
        execSt += entry->getCreate();
    }

    if (exec) {
        QSqlQuery delQuery(db);
        delQuery.exec("SELECT 'DROP TABLE \"' || name || '\"' FROM sqlite_master WHERE type = 'table'");
        QString delSt;

        while (delQuery.next()) {
            delSt += delQuery.value(0).toString() + "\n";
        }

        delQuery.finish();
        execAll(delSt);

        execAll(execSt);
    }
    this->stList = toVec(execSt);
    return true;
}

secvec Database::getPw(QString password) {
    std::string checksumChoice = getCS(checksum, encryption);
    std::string hashChoice = hashMatch[hash];

    if (hash < 3) {
        std::unique_ptr<Botan::PasswordHashFamily> pfHash = Botan::PasswordHashFamily::create(hashChoice);
        std::unique_ptr<Botan::PasswordHash> pHash;

        if (hash == 0) {
            pHash = pfHash->from_params(memoryUsage * 1000, hashIters, 1);
        } else if (hash == 2) {
            pHash = pfHash->from_params(32768, hashIters, 1);
        } else {
            pHash = pfHash->from_params(hashIters);
        }

        if (verbose) {
            qDebug() << hashIters << hashChoice.data() << checksumChoice.data() << iv << ivLen << password << Qt::endl;
        }

        secvec ptr(512);
        pHash->derive_key(ptr.data(), ptr.size(), password.toStdString().data(), password.size(), iv.data(), ivLen);
        password = toStr(ptr);
        if(verbose) {
            qDebug() << "After Hashing:" << Botan::hex_encode(ptr).data() << Qt::endl;
        }
    }

    std::unique_ptr<Botan::Cipher_Mode> enc = Botan::Cipher_Mode::create(encryptionMatch.at(encryption), Botan::ENCRYPTION);
    secvec ptr(enc->maximum_keylength());
    std::unique_ptr<Botan::PasswordHash> ph = Botan::PasswordHashFamily::create("PBKDF2(" + checksumChoice + ")")->default_params();

    ph->derive_key(ptr.data(), ptr.size(), password.toStdString().data(), password.size(), iv.data(), ivLen);
    if (verbose) {
        qDebug() << iv;
        qDebug() << "After Derivation:" << Botan::hex_encode(ptr).data() << Qt::endl;
    }

    return ptr;
}

void Database::encrypt() {
    std::ofstream pd(path.toStdString(), std::fstream::binary | std::fstream::trunc);
    pd.seekp(0);

    pd << "PD++";

    pd.put(MAX_SUPPORTED_VERSION_NUMBER);
    pd.put(checksum);
    pd.put(hash);

    if (hash != 3) {
        pd.put(hashIters);
    }

    pd.put(keyFile);
    pd.put(encryption);

    if (hash == 0) {
        pd.put(memoryUsage >> 8);
        pd.put(memoryUsage & 0xFF);
    }

    pd.put(clearSecs);
    pd.put(compress);

    std::unique_ptr<Botan::Cipher_Mode> enc = Botan::Cipher_Mode::create(encryptionMatch.at(encryption), Botan::ENCRYPTION);

    pd << iv.data();

    pd << name.toStdString();
    pd.put(10);

    pd << desc.toStdString();
    pd.put(10);

    enc->set_key(passw);
    if (verbose) {
        qDebug() << "STList before saveSt:" << toStr(stList);
    }

    saveSt();

    if (verbose) {
        qDebug() << "STList after saveSt:" << toStr(stList);
    }

    secvec pt = stList;

    if (compress) {
        std::unique_ptr<Botan::Compression_Algorithm> ptComp = Botan::Compression_Algorithm::create("gzip");

        ptComp->start();
        ptComp->finish(pt);
    }

    enc->start(iv);
    enc->finish(pt);

    if (keyFile) {
        QFile kf(keyFilePath);
        kf.open(QIODevice::ReadOnly);
        QTextStream key(&kf);
        QString keyPw = key.readAll();

        std::unique_ptr<Botan::Cipher_Mode> keyEnc = Botan::Cipher_Mode::create(encryptionMatch.at(encryption), Botan::ENCRYPTION);

        keyEnc->set_key(getPw(keyPw));
        keyEnc->start(iv);
        keyEnc->finish(pt);
    }

    data = pt;
    if (verbose) {
        qDebug() << "Data (Encryption):" << Botan::hex_encode(pt).data();
    }

    for (unsigned long i = 0; i < pt.size(); ++i) {
        pd.put(pt[i]);
    }

    pd.flush();
    pd.close();
}

int Database::verify(const QString &mpass, bool convert) {
    if (convert) {
        QFile f(path);
        f.open(QIODevice::ReadOnly);
        QTextStream pd(&f);
        QString iv = pd.readLine();

        std::vector<uint8_t> ivd;
        try {
            ivd = Botan::hex_decode(iv.toStdString());
        } catch (...) {
            return false;
        }

        QString r = pd.readAll();

        secvec vData = toVec(r);

        secvec mptr(32);
        std::unique_ptr<Botan::PasswordHash> ph = Botan::PasswordHashFamily::create("PBKDF2(SHA-256)")->default_params();

        ph->derive_key(mptr.data(), mptr.size(), mpass.toStdString().data(), mpass.size(), ivd.data(), ivd.size());

        std::unique_ptr<Botan::Cipher_Mode> decr = Botan::Cipher_Mode::create("AES-256/GCM", Botan::DECRYPTION);

        decr->set_key(mptr);
        decr->start(ivd);

        try {
            decr->finish(vData);
            this->passw = getPw(mpass);
        }  catch (...) {
            return false;
        }

        QString rdata = toStr(vData);

        this->iv = secvec(ivd.begin(), ivd.end());
        this->name = QString(basename(path.toStdString().data())).split(".")[0];;
        this->desc = "Converted from old database format.";
        this->stList = vData;
        execAll(rdata);

        QSqlQuery q(db);
        q.exec("SELECT tbl_name FROM sqlite_master WHERE type='table'");

        while (q.next()) {
            QString tblName = q.value(0).toString();
            db.exec("CREATE TABLE '" + tblName + "_COPY' (Name text, Email text, URL text, Notes blob, Password text)");
            db.exec("INSERT INTO '" + tblName + "_COPY' SELECT name, email, url, notes, password FROM '" + tblName + "'");
            db.exec("DROP TABLE '" + tblName + "'");
            db.exec("ALTER TABLE '" + tblName + "_COPY' RENAME TO '" + tblName + "'");
        }
        get();
        saveSt();
        db.exec("DROP TABLE data");

        encrypt();
        f.close();
        return true;
    }

    secvec vPtr = getPw(mpass), pData = data;

    if (keyFile) {
        QFile kf(keyFilePath);
        kf.open(QIODevice::ReadOnly);
        QTextStream key(&kf);
        QString keyPw = key.readAll();

        std::unique_ptr<Botan::Cipher_Mode> keyDec = Botan::Cipher_Mode::create(encryptionMatch.at(encryption), Botan::DECRYPTION);

        keyDec->set_key(getPw(keyPw));

        keyDec->start(iv);

        try {
            keyDec->finish(pData);
        } catch (std::exception& e) {
            std::cerr << e.what() << std::endl;
            return 3;
        }
    }

    std::unique_ptr<Botan::Cipher_Mode> decr = Botan::Cipher_Mode::create(encryptionMatch.at(encryption), Botan::DECRYPTION);

    decr->set_key(vPtr);
    decr->start(iv);

    try {
        if (verbose) {
            qDebug() << "Data (Verification):" << Botan::hex_encode(pData).data();
        }
        decr->finish(pData);

        if (compress) {
            std::unique_ptr<Botan::Decompression_Algorithm> dataDe = Botan::Decompression_Algorithm::create("gzip");
            dataDe->start();
            dataDe->finish(pData);
        }

        this->stList = pData;
        this->passw = vPtr;
        if (verbose) {
            qDebug() << "STList (verification):" << toStr(stList);
        }
    } catch (std::exception& e) {
        std::cerr << e.what() << std::endl;
        return 2;
    }
    return true;
}

QString Database::decrypt(const QString &txt, bool convert) {
    PasswordDialog *di = new PasswordDialog(this, convert, txt);
    di->init();
    bool set = di->setup();
    if (!set) {
        return "";
    }
    return di->show();
}

bool Database::save() {
    encrypt();

    modified = false;
    return true;
}

bool Database::showErr(QString msg) {
    displayErr("Error: database file is corrupt or invalid.\nDetails: " + msg);
    return {};
}

bool Database::parse() {
    char readData[4];

    QFile f(path);
    f.open(QIODevice::ReadOnly);
    QDataStream q(&f);

    q.readRawData(readData, 4);

    if (std::string(readData, 4) != "PD++") {
        PasswordDialog *di = new PasswordDialog(this, true, " to convert your database to the new format");
        di->init();
        bool conv = di->setup();
        if (!conv) {
            return showErr("Invalid magic number \"" + QString(readData) + "\".");
        }

        di->show();

        return true;
    }

    q >> version;
    if (version > MAX_SUPPORTED_VERSION_NUMBER) {
        return showErr("Invalid version number.");
    }

    q >> checksum;
    if (checksum >= checksumMatch.size()){
        return showErr("Invalid checksum option.");
    }

    if (version < 6) {
        q.skipRawData(1);
    }

    q >> hash;
    if (hash >= hashMatch.size()){
        return showErr("Invalid hash option.");
    }

    if (hash != 3) {
        q >> hashIters;
    }

    q >> keyFile;

    q >> encryption;
    if (encryption >= encryptionMatch.size()){
        return showErr("Invalid encryption option.");
    }

    if (version >= 7) {
        if (hash == 0) {
            q >> memoryUsage;
        }
        q >> clearSecs;
        q >> compress;
    }

    std::unique_ptr<Botan::Cipher_Mode> enc = Botan::Cipher_Mode::create(encryptionMatch.at(encryption), Botan::ENCRYPTION);
    ivLen = enc->default_nonce_length();

    char ivc[ivLen];
    q.readRawData(ivc, ivLen);
    iv = toVec(ivc, ivLen);

    QByteArray namec = f.readLine(), descc = f.readLine();

    desc = QString(namec).trimmed();
    name = QString(descc).trimmed();

    int available = f.bytesAvailable();
    char datac[available];
    q.readRawData(datac, available);

    data = toVec(datac, available);

    return true;
}

bool Database::config(bool create) {
    ConfigDialog *di = new ConfigDialog(this, create);
    di->init();
    di->setup();
    return di->show();
}

bool Database::open() {
    if (QFile::exists(path)) {
        if (!parse()) {
            return false;
        }

        if (stList.empty()) {
            try {
                QString p = decrypt(" to login");
                if (p.isEmpty()) {
                    return false;
                }
            } catch (std::exception& e) {
                displayErr(e.what());
                return false;
            }
        }

        for (const QString &line : toStr(stList).split('\n')) {
            if (line.isEmpty()) {
                continue;
            }
            QSqlQuery q(db);
            bool ok = q.exec(line);
            if (!ok) {
               displayErr("Warning: Error during database initialization: " + q.lastError().text());
            }
        }
        get();
        return true;
    }
    displayErr("Please enter a valid path!");
    return false;
}

int Database::saveAs() {
    QString fileName = QFileDialog::getSaveFileName(nullptr, tr("Save Location"), "", fileExt);
    if (fileName.isEmpty()) {
        return 3;
    }
    QFile file(fileName);
    if (!file.open(QIODevice::WriteOnly)) {
        return 17;
    }
    try {
        path = fileName;
        bool ok = save();
        if (!ok) {
            return false;
        }
    } catch (std::exception& e) {
        displayErr(e.what());
    }
    return true;
}

Entry *Database::entryNamed(QString &name) {
    for (Entry *e : entries) {
        if (e->getName() == name) {
            return e;
        }
    }

    return nullptr;
}

void Database::addEntry(Entry *entry) {
    entries.push_back(entry);
}

bool Database::removeEntry(Entry *entry) {
    return entries.removeOne(entry);
}

int Database::entryLength() {
    return entries.length();
}

QList<Entry *> &Database::getEntries() {
    return entries;
}

void Database::setEntries(QList<Entry *> entries) {
    this->entries = entries;
}
