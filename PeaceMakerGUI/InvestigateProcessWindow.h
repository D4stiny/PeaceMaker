#ifndef INVESTIGATEPROCESSWINDOW_H
#define INVESTIGATEPROCESSWINDOW_H

#include <QWidget>
#include <QTableWidget>
#include <QScrollBar>

#include <Windows.h>
#include <DbgHelp.h>
#include <string>
#include <sstream>
#include <vector>
#include "shared.h"
#include "IOCTLCommunicationUser.h"

namespace Ui {
class InvestigateProcessWindow;
}

class InvestigateProcessWindow : public QWidget
{
    Q_OBJECT

    void InitializeCommonTable(QTableWidget* table);

    void InitializeProcessInfoTable();
    void InitializeStackHistoryTable(QTableWidget* historyTable);
    void InitializeProcessImagesTable();

    void AddProcessInfoItem(std::string HeaderName);

    HANDLE ProcessId;
    ULONG64 EpochExecutionTime;

    int ProcessInfoTableSize;

    std::vector<IMAGE_SUMMARY> images;

    void UpdateWidget(QWidget* widget);

public:
    explicit InvestigateProcessWindow(QWidget *parent = nullptr);
    ~InvestigateProcessWindow();

    void UpdateNewProcess(PROCESS_DETAILED_REQUEST detailedProcessInformation);

    void RefreshWidgets();

    IOCTLCommunication* communicator;
private slots:
    void on_ImagesTable_cellClicked(int row, int column);

private:
    Ui::InvestigateProcessWindow *ui;
};

#endif // INVESTIGATEPROCESSWINDOW_H
