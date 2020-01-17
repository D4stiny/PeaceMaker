#include "mainwindow.h"
#include "ui_mainwindow.h"

VOID
pmlog (
    const char* format,
    ...
    )
{
    va_list vargs;
    va_start(vargs, format);
    printf("[PeaceMaker] ");
    vprintf(format, vargs);
    printf("\n");
    va_end(vargs);
}

/**
 * @brief MainWindow::InitializeCommonTable - Common initialization across all tables.
 * @param table
 */
void MainWindow::InitializeCommonTable(QTableWidget *table)
{
    //
    // Set properties that are common across tables.
    //
    table->horizontalHeader()->setStretchLastSection(true);
    table->horizontalHeader()->setHighlightSections(false);
    table->verticalHeader()->setVisible(false);
    table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    table->setSelectionBehavior(QAbstractItemView::SelectRows);
    table->setSelectionMode(QAbstractItemView::SingleSelection);
    table->verticalScrollBar()->setStyleSheet("color: white;");
    table->setSortingEnabled(false);
    table->setWordWrap(true);
}

/**
 * @brief MainWindow::InitializeAlertsTable - Initialize the table used for alerts.
 */
void MainWindow::InitializeAlertsTable()
{
    QStringList headers;

    this->AlertsTableSize = 0;

    this->ui->AlertsTable->setColumnCount(2);
    this->ui->AlertsTable->setRowCount(100);

    headers << "Date" << "Alert Name";
    this->ui->AlertsTable->setHorizontalHeaderLabels(headers);
    this->ui->AlertsTable->setColumnWidth(1, 100);

    InitializeCommonTable(this->ui->AlertsTable);

    //
    // Add the table as an "associated element".
    //
    this->ui->AlertsLabel->AddAssociatedElement(RCAST<QWidget*>(this->ui->AlertsTable));
}

/**
 * @brief MainWindow::InitializeProcessesTable - Initialize the processes table.
 */
void MainWindow::InitializeProcessesTable()
{
    QStringList headers;

    this->ProcessesTableSize = 0;

    this->ui->ProcessesTable->setColumnCount(3);
    this->ui->ProcessesTable->setRowCount(100);

    headers << "Process Id" << "Execution Date" << "Path";
    this->ui->ProcessesTable->setHorizontalHeaderLabels(headers);

    InitializeCommonTable(this->ui->ProcessesTable);

    this->ui->ProcessesTable->setColumnWidth(1, 200);
    this->ui->ProcessesTable->setColumnWidth(2, 400);

    //
    // Add the table as an "associated element".
    //
    this->ui->ProcessesLabel->AddAssociatedElement(RCAST<QWidget*>(this->ui->ProcessesTable));
    this->ui->ProcessesLabel->AddAssociatedElement(RCAST<QWidget*>(this->ui->InvestigateProcessButton));
    this->ui->ProcessesLabel->AddAssociatedElement(RCAST<QWidget*>(this->ui->ProcessSearch));
}

/**
 * @brief MainWindow::InitializeFiltersTable - Initialize the filters table.
 */
void MainWindow::InitializeFiltersTable()
{
    QStringList headers;

    this->FiltersTableSize = 0;
    this->FilesystemFiltersCount = 0;
    this->RegistryFiltersCount = 0;

    this->ui->FiltersTable->setColumnCount(2);
    this->ui->FiltersTable->setRowCount(100);

    headers << "Filter Type" << "Filter Flags" << "Filter Content";
    this->ui->FiltersTable->setHorizontalHeaderLabels(headers);
    this->ui->FiltersTable->setColumnWidth(2, 100);

    InitializeCommonTable(this->ui->FiltersTable);

    //
    // Add the table as an "associated element".
    //
    this->ui->FiltersLabel->AddAssociatedElement(RCAST<QWidget*>(this->ui->FiltersTable));
}

/**
 * @brief MainWindow::ThreadUpdateTables - Thread callback that updates all tables.
 * @param This - This pointer for the MainWindow instance.
 */
void MainWindow::ThreadUpdateTables(MainWindow *This)
{
    BOOLEAN connected;
    ULONG i;
    ULONG c;

    PBASE_ALERT_INFO currentAlert;
    GLOBAL_SIZES globalSizes;
    ULONG newProcessCount;
    PPROCESS_SUMMARY_REQUEST processSummaries;
    ULONG totalFilterSize;
    ULONG filesystemFilterIterations;
    ULONG registryFilterIterations;
    LIST_FILTERS_REQUEST filterRequest;

    connected = FALSE;

    while(connected == FALSE)
    {
        if(This->communicator.ConnectDevice() == FALSE)
        {
            pmlog("Failed to connect to device. Retrying in 2 seconds.");
            Sleep(2000);
            continue;
        }
        connected = TRUE;
    }

    pmlog("Established connection.");

    //
    // Loop update tables.
    //
    while(true)
    {
        //
        // Check for alerts.
        //
        while(This->communicator.QueuedAlerts())
        {
            currentAlert = This->communicator.PopAlert();
            if(currentAlert == NULL)
            {
                break;
            }

            This->AddAlertSummary(*currentAlert);
            free(currentAlert);
        }

        //
        // Get global sizes.
        //
        globalSizes = This->communicator.GetGlobalSizes();

        //
        // Check for processes.
        //
        if(globalSizes.ProcessHistorySize != This->ProcessesTableSize)
        {
            newProcessCount = globalSizes.ProcessHistorySize - This->ProcessesTableSize;
            if(newProcessCount > 0)
            {
                processSummaries = This->communicator.RequestProcessSummary(This->ProcessesTableSize, newProcessCount);
                if(processSummaries)
                {
                    for(i = 0; i < processSummaries->ProcessHistorySize; i++)
                    {
                        This->AddProcessSummary(processSummaries->ProcessHistory[i]);
                    }
                    free(processSummaries);
                } else
                {
                    pmlog("processSummaries is NULL.");
                }
            }
        }

        totalFilterSize = globalSizes.FilesystemFilterSize + globalSizes.RegistryFilterSize;
        //
        // Look for new filters.
        //
        if(totalFilterSize != This->FiltersTableSize)
        {
            filesystemFilterIterations = (globalSizes.FilesystemFilterSize - This->FilesystemFiltersCount) % 10;
            registryFilterIterations = (globalSizes.RegistryFilterSize - This->RegistryFiltersCount) % 10;

            for(i = 0; i < filesystemFilterIterations; i++)
            {
                filterRequest = This->communicator.RequestFilters(FilesystemFilter, This->FilesystemFiltersCount);
                for(c = 0; c < filterRequest.CopiedFilters; c++)
                {
                    This->AddFilterSummary(filterRequest.Filters[c]);
                }
            }

            for(i = 0; i < registryFilterIterations; i++)
            {
                filterRequest = This->communicator.RequestFilters(RegistryFilter, This->RegistryFiltersCount);
                for(c = 0; c < filterRequest.CopiedFilters; c++)
                {
                    This->AddFilterSummary(filterRequest.Filters[c]);
                }
            }
        }
        Sleep(3000);
    }
}

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    ui->setupUi(this);
    this->setFixedSize(QSize(990, 610));

    InitializeAlertsTable();
    InitializeProcessesTable();
    InitializeFiltersTable();

    //
    // By default set the "activeTab" to the alerts tab.
    //
    ui->AlertsLabel->SwapActiveState();
    this->activeTab = ui->AlertsLabel;

    this->investigatorWindow = new InvestigateProcessWindow();
    this->investigatorWindow->communicator = &this->communicator;

    CreateThread(NULL, 0, RCAST<LPTHREAD_START_ROUTINE>(this->ThreadUpdateTables), this, 0, NULL);
}

MainWindow::~MainWindow()
{
    delete ui;
    delete investigatorWindow;
}

void MainWindow::NotifyTabClick(ClickableTab *tab)
{
    std::string tabName;

    //
    // The tab name is just its object name.
    //
    tabName = tab->objectName().toStdString();

    //
    // Swap the active/inactive state because of the click.
    //
    activeTab->SwapActiveState();
    activeTab = tab;
    activeTab->SwapActiveState();
}

void MainWindow::AddAlertSummary(BASE_ALERT_INFO Alert)
{
    std::time_t currentTime;
    std::string date;
    std::string alertName;

    //
    // Get the current time.
    //
    currentTime = std::time(nullptr);
    date = std::ctime(&currentTime);

    //
    // Determine the alert type.
    //
    switch(Alert.AlertType)
    {
    case StackViolation:
        alertName = "Stack Violation";
        break;
    case FilterViolation:
        alertName = "Filter Violation";
        break;
    case ParentProcessIdSpoofing:
        alertName = "Parent Process ID Spoofing";
        break;
    }

    this->ui->AlertsTable->setRowCount(this->AlertsTableSize + 1);
    this->ui->AlertsTable->setItem(this->AlertsTableSize, 0, new QTableWidgetItem(QString::fromStdString(date)));
    this->ui->AlertsTable->setItem(this->AlertsTableSize, 1, new QTableWidgetItem(QString::fromStdString(alertName)));
    this->AlertsTableSize++;

    alerts.push_back(Alert);
}

void MainWindow::AddProcessSummary(PROCESS_SUMMARY_ENTRY ProcessSummary)
{
    std::time_t processExecutionDate;
    std::string dateString;

    //
    // First, we need to convert the epoch time to a date.
    //
    processExecutionDate = ProcessSummary.EpochExecutionTime;
    dateString = std::ctime(&processExecutionDate);
    dateString.erase(std::remove(dateString.begin(), dateString.end(), '\n'), dateString.end());

    //
    // Next, add the appropriate information to the table.
    //
    this->ui->ProcessesTable->setRowCount(this->ProcessesTableSize + 1);
    this->ui->ProcessesTable->insertRow(0);
    this->ui->ProcessesTable->setItem(0, 0, new QTableWidgetItem(QString::number(RCAST<ULONG64>(ProcessSummary.ProcessId))));
    this->ui->ProcessesTable->setItem(0, 1, new QTableWidgetItem(QString::fromStdString(dateString)));
    this->ui->ProcessesTable->setItem(0, 2, new QTableWidgetItem(QString::fromWCharArray(ProcessSummary.ImageFileName)));
    this->ui->ProcessesTable->resizeRowsToContents();
    this->ProcessesTableSize++;

    processes.push_back(ProcessSummary);
}

void MainWindow::AddFilterSummary(FILTER_INFO FilterInfo)
{
    std::string filterType;
    std::string filterFlags;

    //
    // Depending on the filter type, make the enum value a string.
    //
    switch(FilterInfo.Type)
    {
    case FilesystemFilter:
        filterType = "Filesystem Filter";
        this->FilesystemFiltersCount++;
        break;
    case RegistryFilter:
        filterType = "Registry Filter";
        this->RegistryFiltersCount++;
        break;
    }

    //
    // Now convert the flags.
    //
    if(FlagOn(FilterInfo.Flags, FILTER_FLAG_DELETE))
    {
        filterFlags = "Delete";
    }
    if(FlagOn(FilterInfo.Flags, FILTER_FLAG_WRITE))
    {
        //
        // Add a comma if this is the second flag.
        //
        if(FlagOn(FilterInfo.Flags, FILTER_FLAG_DELETE))
        {
            filterFlags += ", ";
        }
        filterFlags += "Write";
    }
    if(FlagOn(FilterInfo.Flags, FILTER_FLAG_EXECUTE))
    {
        //
        // Add a comma if this is the third flag.
        //
        if(FlagOn(FilterInfo.Flags, FILTER_FLAG_DELETE) || FlagOn(FilterInfo.Flags, FILTER_FLAG_WRITE))
        {
            filterFlags += ", ";
        }
        filterFlags += "Execute";
    }

    this->ui->FiltersTable->setRowCount(this->FiltersTableSize + 1);
    this->ui->FiltersTable->setItem(this->FiltersTableSize, 0, new QTableWidgetItem(QString::fromStdString(filterType)));
    this->ui->FiltersTable->setItem(this->FiltersTableSize, 1, new QTableWidgetItem(QString::fromStdString(filterFlags)));
    this->ui->FiltersTable->setItem(this->FiltersTableSize, 2, new QTableWidgetItem(QString::fromWCharArray(FilterInfo.MatchString)));
    this->FiltersTableSize++;

    filters.push_back(FilterInfo);
}

void MainWindow::on_InvestigateProcessButton_clicked()
{
    PPROCESS_DETAILED_REQUEST processDetailed;
    PROCESS_SIZES_REQUEST processSizes;
    int selectedRow;

    if(this->ui->ProcessesTable->selectedItems().size() == 0)
    {
        return;
    }

    selectedRow = this->ui->ProcessesTable->selectedItems()[0]->row();

    processSizes = communicator.GetProcessSizes(processes[selectedRow].ProcessId, processes[selectedRow].EpochExecutionTime);
    processDetailed = communicator.RequestDetailedProcess(processes[selectedRow].ProcessId, processes[selectedRow].EpochExecutionTime, processSizes.ImageSize, processSizes.StackSize);
    if (processDetailed == NULL || processDetailed->Populated == FALSE)
    {
        pmlog("main: Failed to retrieve a detailed process report.");
        return;
    }

    this->investigatorWindow->UpdateNewProcess(*processDetailed);
    free(processDetailed);
    this->investigatorWindow->show();
    this->investigatorWindow->RefreshWidgets();
}
