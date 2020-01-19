#ifndef ADDFILTERWINDOW_H
#define ADDFILTERWINDOW_H

#include <QWidget>

#include <string>
#include "shared.h"
#include "IOCTLCommunicationUser.h"

namespace Ui {
class AddFilterWindow;
}

class AddFilterWindow : public QWidget
{
    Q_OBJECT

public:
    explicit AddFilterWindow(QWidget *parent = nullptr);
    ~AddFilterWindow();

    void ClearStates();

    IOCTLCommunication* communicator;
private slots:
    void on_pushButton_clicked();

private:
    Ui::AddFilterWindow *ui;
};

#endif // ADDFILTERWINDOW_H
