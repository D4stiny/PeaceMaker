#ifndef DETAILEDALERTWINDOW_H
#define DETAILEDALERTWINDOW_H

#include <QWidget>
#include <QTableWidget>
#include <QScrollBar>

#include <Windows.h>
#include <DbgHelp.h>
#include "shared.h"

namespace Ui {
class DetailedAlertWindow;
}

class DetailedAlertWindow : public QWidget
{
    Q_OBJECT

    void InitializeCommonTable(QTableWidget* table);

    void InitializeStackViolationAlertTable();
public:
    explicit DetailedAlertWindow(QWidget *parent = nullptr);
    ~DetailedAlertWindow();

    void UpdateDisplayAlert(PBASE_ALERT_INFO AlertInfo);

private:
    Ui::DetailedAlertWindow *ui;
};

#endif // DETAILEDALERTWINDOW_H
