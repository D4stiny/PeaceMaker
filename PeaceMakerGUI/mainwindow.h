#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QTableWidgetItem>
#include <QScrollBar>
#include <QString>
#include <QMessageBox>
#include <QDir>

#include <ctime>
#include <string>
#include <vector>
#include "shared.h"
#include "InvestigateProcessWindow.h"
#include "detailedalertwindow.h"
#include "addfilterwindow.h"
#include "ClickableTab.h"
#include "IOCTLCommunicationUser.h"

QT_BEGIN_NAMESPACE
namespace Ui { class MainWindow; }
QT_END_NAMESPACE

VOID
pmlog (
    const char* format,
    ...
    );

class MainWindow : public QMainWindow
{
    Q_OBJECT
    ClickableTab* activeTab;
    int AlertsTableSize;
    ULONG64 ProcessesTableSize;
    ULONG FiltersTableSize;

    int FilesystemFiltersCount;
    int RegistryFiltersCount;

    std::vector<PBASE_ALERT_INFO> alerts;
    std::vector<PROCESS_SUMMARY_ENTRY> processes;
    std::vector<FILTER_INFO> filters;

    IOCTLCommunication communicator;

    void InitializeCommonTable(QTableWidget* table);

    void InitializeAlertsTable();
    void InitializeProcessesTable();
    void InitializeFiltersTable();

    static void ThreadUpdateTables(MainWindow* This);

    InvestigateProcessWindow* investigatorWindow;
    DetailedAlertWindow* alertWindow;
    AddFilterWindow* addFilterWindow;
public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

    void NotifyTabClick(ClickableTab* tab);

    void AddAlertSummary(PBASE_ALERT_INFO Alert);
    void AddProcessSummary(PROCESS_SUMMARY_ENTRY ProcessSummary);
    void AddFilterSummary(FILTER_INFO FilterInfo, STRING_FILTER_TYPE FilterType);

    void ActivateAlertsWindow();
    void ActivateProcessesWindow();
    void ActivateFiltersWindow();
    void ActivateConfigWindow();
private slots:

    void on_InvestigateProcessButton_clicked();

    void on_ProcessSearch_editingFinished();

    void on_OpenAlertButton_clicked();

    void on_DeleteAlertButton_clicked();

    void on_AddFilterButton_clicked();

    void on_DeleteFilterButton_clicked();

private:
    Ui::MainWindow *ui;
};
#endif // MAINWINDOW_H
