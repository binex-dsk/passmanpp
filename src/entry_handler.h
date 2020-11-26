#ifndef ENTRY_HANDLER_H
#define ENTRY_HANDLER_H
#include <QInputDialog>

#include "database.h"
#include "db.h"

void displayErr(std::string msg);

class EntryHandler : public QWidget {
    Q_OBJECT
public:
    int entryInteract(Database db);
    bool entryDetails(QString& name, QString& url, QString& email, QString& password, QString& notes);

    bool create(Database db);
    QString randomPass();

    int addEntry(QListWidget *item, Database db);
    template <typename Func>
    QAction *addButton(QIcon icon, QString statusTip, QKeySequence shortcut, Func func);

    int editEntry(QListWidgetItem *item, Database db);
    bool deleteEntry(QListWidgetItem *item, Database db);

    static int _editData(void *, int, char **data, char **);
};

#endif // ENTRY_HANDLER_H
