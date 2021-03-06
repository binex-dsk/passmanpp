#ifndef PasswordGeneratorDialog_H
#define PasswordGeneratorDialog_H
#include <QString>
#include <QDialog>

class QGridLayout;
class QLabel;
class QLineEdit;
class QDialogButtonBox;
class QSpinBox;
class QSlider;
class QFrame;
class QProgressBar;

typedef QList<QChar> Group;

class PasswordGeneratorDialog : public QDialog
{
public:
    PasswordGeneratorDialog();

    enum Option {
        Lowers = (1 << 0),
        Uppers = (1 << 1),
        Numbers = (1 << 2),
        Braces = (1 << 3),
        Punctuation = (1 << 4),
        Dashes = (1 << 5),
        Math = (1 << 6),
        Logograms = (1 << 7),
        Symbols = Braces | Punctuation | Dashes | Math | Logograms,
        EASCII = (1 << 8),
        Default = Lowers | Uppers | Numbers
    };
    Q_DECLARE_FLAGS(Options, Option)

    inline Group fromString(QString from) {
        return Group(from.begin(), from.end());
    }

    const QString generate();

    void setup();
    const QString show();

    Group getGroup();
    Options getOptions();

    double calcEntropy(const int t_passwordLength, const int t_setLength);
    const QString getQuality(double t_entropy);

    Options options;

    int length;

    Group includes;
    Group excludes;

    QGridLayout *layout;

    QLineEdit *display;
    QAction *visible;
    QPushButton *regen;

    QLabel *entropyLabel;
    QProgressBar *entropyBar;

    QLabel *lengthLabel;
    QSlider *lengthSlider;
    QSpinBox *lengthBox;

    QLabel *buttonLabel;

    QGridLayout *buttonLayout;
    QFrame *buttonWidget;

    QPushButton *lowersBox;
    QPushButton *uppersBox;
    QPushButton *numbersBox;
    QPushButton *bracesBox;
    QPushButton *punctsBox;
    QPushButton *dashesBox;
    QPushButton *mathBox;
    QPushButton *logogramsBox;
    QPushButton *easciiBox;

    QLineEdit *extraInclude;
    QLabel *includeLabel;

    QLineEdit *extraExclude;
    QLabel *excludeLabel;

    QDialogButtonBox *buttonBox;
};

#endif // PasswordGeneratorDialog_H
