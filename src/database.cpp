#include "database.h"
#include "constants.h"
#include "entry_handler.h"

#include <QInputDialog>
#include <QLabel>
#include <QGridLayout>
#include <QDialogButtonBox>
#include <QPushButton>
#include <QGraphicsOpacityEffect>
#include <QPropertyAnimation>
#include <QTimer>

Database::Database() {}

void showMessage(std::string msg) {
    QMessageBox box;
    box.setText(QString::fromStdString(msg));
    box.setStandardButtons(QMessageBox::Ok);
    box.exec();
}

std::string Database::getPw(std::string password) {
    std::string checksumChoice = checksumMatch[checksum - 1];
    if (checksumChoice == "Skein-512") {
        checksumChoice = "Skein-512(256, " + Botan::hex_encode(iv) + ")";
    }

    std::string hashChoice = hashMatch[hash - 1];

    if (hashChoice != "No hashing, only derivation") {
        std::unique_ptr<Botan::PasswordHashFamily> pfHash = Botan::PasswordHashFamily::create(hashChoice);
        std::unique_ptr<Botan::PasswordHash> pHash;
        if (hashChoice == "Argon2id") {
            pHash = pfHash->from_params(80000, hashIters, 1);
        } else {
            pHash = pfHash->from_params(hashIters);
        }

        Botan::secure_vector<uint8_t> ptr(1024);
        pHash->derive_key(ptr.data(), ptr.size(), password.c_str(), password.size(), uuid.data(), uuidLen);
        password = toStr(ptr);
    }

    Botan::secure_vector<uint8_t> ptr(32);
    std::unique_ptr<Botan::PasswordHash> ph = Botan::PasswordHashFamily::create("PBKDF2(" + checksumChoice + ")")->default_params();

    ph->derive_key(ptr.data(), ptr.size(), password.c_str(), password.size(), uuid.data(), uuidLen);
    return toStr(ptr);
}

void Database::encrypt(std::string password) {
    std::ofstream pd(path, std::ios_base::binary | std::ios_base::trunc | std::ios_base::out | std::ios_base::in);
    pd.seekp(0);
    pd << "PD++";
    std::string magic, gs, version;

    magic = atos(0x11U);
    pd << magic << magic;

    gs = atos(0x1DU);
    pd << gs;

    version = atos(MAX_SUPPORTED_VERSION_NUMBER);
    pd << version << atos(checksum) << atos(deriv) << atos(hash) << atos(hashIters) << atos(keyFile + 1) << atos(encryption) << gs;

    std::unique_ptr<Botan::Cipher_Mode> enc = Botan::Cipher_Mode::create(encryptionMatch.at(encryption - 1), Botan::ENCRYPTION);

    if (toStr(iv) == "") {
        Botan::AutoSeeded_RNG rng;
        ivLen = enc->default_nonce_length();
        iv = rng.random_vec(ivLen);
    }

    pd << atos(ivLen) << toChar(iv);
    pd << atos(uuidLen) << toChar(uuid);

    pd << atos(nameLen) << name.data();
    pd << atos(descLen) << desc.data();

    std::string ptr = getPw(password);
    Botan::secure_vector<uint8_t> vPassword = toVec(ptr);

    enc->set_key(vPassword);

    Botan::secure_vector<uint8_t> pt(stList.data(), stList.data() + stList.length());

    std::unique_ptr<Botan::Compression_Algorithm> ptComp = Botan::Compression_Algorithm::create("gzip");

    ptComp->start();
    ptComp->finish(pt);

    enc->start(iv);
    enc->finish(pt);

    std::string pts = toStr(pt);
    data = pt;
    pd << pts;

    pd.flush();
    pd.close();
}

bool Database::verify(std::string mpass) {
    std::string ptr = getPw(mpass);

    Botan::secure_vector<uint8_t> vPtr = toVec(ptr), pData = data;

    std::unique_ptr<Botan::Cipher_Mode> decr = Botan::Cipher_Mode::create(encryptionMatch.at(encryption - 1), Botan::DECRYPTION);

    decr->set_key(vPtr);
    decr->start(iv);

    try {
        decr->finish(pData);

        std::unique_ptr<Botan::Decompression_Algorithm> dataDe = Botan::Decompression_Algorithm::create("gzip");
        dataDe->start();
        dataDe->finish(pData);

        this->stList = toStr(pData);
    } catch (std::exception& e) {
        std::cerr << e.what() << std::endl;
        return false;
    }
    return true;
}

std::string Database::decrypt(std::string txt, std::string password) {
    std::string mpass;

    if (password == "") {
        QDialog *passDi = new QDialog;
        passDi->setWindowTitle(QWidget::tr("Enter your password"));

        QGridLayout *layout = new QGridLayout;

        QLabel *passLabel = new QLabel(QWidget::tr(std::string("Please enter your master password" + txt + ".").c_str()));

        QLineEdit *passEdit = new QLineEdit;
        passEdit->setEchoMode(QLineEdit::Password);
        passEdit->setCursorPosition(0);

        QDialogButtonBox *passButtons = new QDialogButtonBox(QDialogButtonBox::Ok);
        QLabel *errLabel = new QLabel;
        errLabel->setFrameStyle(QFrame::Panel | QFrame::Raised);
        errLabel->setLineWidth(2);

        errLabel->setText("Password is incorrect.\nIf this problem continues, the database may be corrupt.");
        errLabel->setMargin(5);

        QPalette palette;

        QColor lColor;
        lColor.setNamedColor("#DA4453");
        palette.setColor(QPalette::Light, lColor);

        QColor dColor;
        dColor.setNamedColor("#DA4453");
        palette.setColor(QPalette::Dark, dColor);

        QColor tColor;
        tColor.setNamedColor("#C4DA4453");
        palette.setColor(QPalette::Window, tColor);

        palette.setColor(QPalette::Text, Qt::white);

        QWidget::connect(passButtons->button(QDialogButtonBox::Ok), &QPushButton::clicked, [passEdit, passLabel, passDi, passButtons, errLabel, layout, palette, this]() mutable {
            std::string pw = passEdit->text().toStdString();
            passDi->setCursor(QCursor(Qt::WaitCursor));

            QPalette textPal;
            textPal.setColor(QPalette::WindowText, Qt::darkGray);
            textPal.setColor(QPalette::ButtonText, Qt::darkGray);

            passLabel->setPalette(textPal);
            passButtons->setPalette(textPal);

            errLabel->setPalette(textPal);

            passDi->repaint();

            if (pw == "" || verify(pw)) {
                passDi->accept();
            } else {
                layout->addWidget(errLabel, 2, 0);

                errLabel->setPalette(palette);

                passLabel->setPalette(QPalette());
                passButtons->setPalette(QPalette());

                passDi->unsetCursor();
            }
        });

        layout->addWidget(passLabel, 0, 0);
        layout->addWidget(passEdit, 1, 0);
        layout->addWidget(passButtons, 3, 0);

        passDi->setLayout(layout);

        passDi->exec();
        return passEdit->text().toStdString();
    } else {
        verify(password);
        return password;
    }

}

bool Database::save(std::string password) {
    std::string pw = decrypt(" to save");
    if (pw == "") {
        return false;
    }

    std::string mpass;
    if (pw != "") {
        mpass = pw;
    } else {
        mpass = password;
    }
    saveSt(*this);
    this->stList = glob_stList;
    encrypt(mpass);

    modified = false;
    return true;
}

bool Database::convert() {
    std::ifstream pd(path, std::ios_base::binary | std::ios_base::out);
    std::string iv;
    std::getline(pd, iv);

    std::vector<uint8_t> ivd;
    try {
        ivd = Botan::hex_decode(iv);
    } catch (...) {
        return false;
    }
    std::string r(std::istreambuf_iterator<char>{pd}, {});

    if (ivd.size() != 12) {
        return false;
    }

    showMessage("This database may be for an older version of passman++. I will try to convert it now.");

    Botan::secure_vector<uint8_t> vData;
    std::string password;

    while (true) {
        vData = toVec(r);
        QString pass = QInputDialog::getText(nullptr, QWidget::tr("Enter your password"), QWidget::tr(std::string("Please enter your master password to convert the database.").c_str()), QLineEdit::Password);
        password = pass.toStdString();

        Botan::secure_vector<uint8_t> mptr(32);
        std::unique_ptr<Botan::PasswordHash> ph = Botan::PasswordHashFamily::create("PBKDF2(SHA-256)")->default_params();

        ph->derive_key(mptr.data(), mptr.size(), password.c_str(), password.size(), ivd.data(), ivd.size());

        std::unique_ptr<Botan::Cipher_Mode> decr = Botan::Cipher_Mode::create("AES-256/GCM", Botan::DECRYPTION);

        decr->set_key(mptr);
        decr->start(ivd);

        try {
            decr->finish(vData);
            break;
        }  catch (...) {
            displayErr("Wrong password, try again.");
        }
    }

    int uuidLen = 40 + randombytes_uniform(40);
    Botan::AutoSeeded_RNG rng;
    Botan::secure_vector<uint8_t> uuid = rng.random_vec(uuidLen);

    std::string name = split(std::string(basename(path.c_str())), '.')[0];
    std::string rdata = toStr(vData);

    this->checksum = 1;
    this->deriv = 1;
    this->hash = 1;
    this->hashIters = 8;
    this->keyFile = false;
    this->encryption = 1;
    this->iv = Botan::secure_vector<uint8_t>(ivd.begin(), ivd.end());
    this->ivLen = ivd.size();
    this->uuid = uuid;
    this->uuidLen = uuidLen;
    this->name = name;
    this->nameLen = name.length();
    this->desc = "Converted from old database format.";
    this->descLen = desc.length();
    this->stList = rdata;

    encrypt(password);
    pd.close();
    return true;
}

bool Database::showErr(std::string msg) {
    displayErr("Error: database file is corrupt or invalid.\nDetails: " + msg);
    return {};
}

bool Database::parse() {
    std::ifstream pd(path, std::ios_base::binary | std::ios_base::out);
    char readData[65536], len[2];
    pd.read(readData, 6);
    if (readData != "PD++" + atos(0x11U) + atos(0x11U)) {
        bool conv = convert();
        if (!conv) {
            return showErr("Invalid magic number \"" + std::string(readData) + "\".");
        }
        return true;
    }

    pd.read(readData, 1);
    if (int(readData[0]) != 0x1DU) {
        return showErr("No group separator byte after valid magic number.");
    }

    pd.read(readData, 1);
    if (int(readData[0]) > MAX_SUPPORTED_VERSION_NUMBER) {
        return showErr("Invalid version number.");
    }
    version = readData[0];

    pd.read(readData, 1);
    try {
        checksumMatch.at(int(readData[0]) - 1);
    }  catch (...) {
        return showErr("Invalid checksum option.");
    }
    checksum = readData[0];

    pd.read(readData, 1);
    try {
        derivMatch.at(int(readData[0]) - 1);
    }  catch (...) {
        return showErr("Invalid derivation option.");
    }
    deriv = readData[0];

    pd.read(readData, 1);
    try {
        hashMatch.at(int(readData[0]) - 1);
    }  catch (...) {
        return showErr("Invalid hash option.");
    }
    hash = readData[0];

    pd.read(readData, 1);
    hashIters = int(readData[0]);

    pd.read(readData, 1);
    keyFile = readData[0] - 1;

    pd.read(readData, 1);
    try {
        encryptionMatch.at(int(readData[0]));
    }  catch (...) {
        return showErr("Invalid encryption option.");
    }
    encryption = readData[0];

    pd.read(readData, 1);
    if (int(readData[0]) != 0x1DU) {
        return showErr("No group separator byte after valid database configuration.");
    }

    pd.read(len, 1);
    ivLen = int(len[0]);

    pd.read(readData, ivLen);
    iv = toVec(readData, ivLen);

    pd.read(len, 1);
    uuidLen = int(len[0]);

    pd.read(readData, uuidLen);

    uuid = toVec(readData, uuidLen);

    pd.read(len, 1);
    nameLen = int(len[0]);

    pd.read(readData, nameLen);
    name = std::string(readData, nameLen);

    pd.read(len, 1);
    descLen = int(len[0]);

    pd.read(readData, descLen);
    desc = std::string(readData, descLen);

    data = Botan::secure_vector<uint8_t>(std::istreambuf_iterator<char>{pd}, {});

    pd.close();
    return true;
}
