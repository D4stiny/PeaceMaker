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
    this->ui->AlertsTable->setColumnWidth(0, 200);

    InitializeCommonTable(this->ui->AlertsTable);

    //
    // Add the table as an "associated element".
    //
    this->ui->AlertsLabel->AddAssociatedElement(RCAST<QWidget*>(this->ui->AlertsTable));
    this->ui->AlertsLabel->AddAssociatedElement(RCAST<QWidget*>(this->ui->OpenAlertButton));
    this->ui->AlertsLabel->AddAssociatedElement(RCAST<QWidget*>(this->ui->DeleteAlertButton));
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
    this->ui->ProcessesLabel->AddAssociatedElement(RCAST<QWidget*>(this->ui->ProcessSearchLabel));
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

    this->ui->FiltersTable->setColumnCount(3);
    this->ui->FiltersTable->setRowCount(0);

    headers << "Filter Type" << "Filter Flags" << "Filter Content";
    this->ui->FiltersTable->setHorizontalHeaderLabels(headers);
    this->ui->FiltersTable->setColumnWidth(2, 100);

    InitializeCommonTable(this->ui->FiltersTable);

    //
    // Add the table as an "associated element".
    //
    this->ui->FiltersLabel->AddAssociatedElement(RCAST<QWidget*>(this->ui->FiltersTable));
    this->ui->FiltersLabel->AddAssociatedElement(RCAST<QWidget*>(this->ui->AddFilterButton));
    this->ui->FiltersLabel->AddAssociatedElement(RCAST<QWidget*>(this->ui->DeleteFilterButton));
}

/**
 * @brief MainWindow::ImportConfigFilters - One-time config filter import.
 */
void MainWindow::ImportConfigFilters()
{
    std::vector<FILTER_INFO> configFilters;
    BOOLEAN filterExists;

    configFilters = config.GetConfigFilters();
    for(FILTER_INFO configFilter : configFilters)
    {
        filterExists = FALSE;
        //
        // Make sure we don't add an existing filter.
        //
        for(FILTER_INFO existingFilter : filters)
        {
            if(existingFilter.Type == configFilter.Type &&
               wcscmp(existingFilter.MatchString, configFilter.MatchString) == 0)
            {
                filterExists = TRUE;
                break;
            }
        }
        if(filterExists)
        {
            printf("MainWindow!ImportConfigFilters: Ignoring filter with content %ls. Already exists.\n", configFilter.MatchString);
            continue;
        }
        communicator.AddFilter(configFilter.Type, configFilter.Flags, configFilter.MatchString, configFilter.MatchStringSize);
        printf("MainWindow!ImportConfigFilters: Added config filter with content %ls.\n", configFilter.MatchString);
    }
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

            This->AddAlertSummary(currentAlert);
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
                    This->ui->ProcessesTable->resizeRowsToContents();
                } else
                {
                    pmlog("processSummaries is NULL.");
                }
            }
        }

        totalFilterSize = globalSizes.FilesystemFilterSize + globalSizes.RegistryFilterSize;
        printf("FS Size: %i, RY Size: %i, This Size: %i\n", globalSizes.FilesystemFilterSize, globalSizes.RegistryFilterSize, This->FiltersTableSize);
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
                    This->AddFilterSummary(filterRequest.Filters[c], FilesystemFilter);
                }
            }

            for(i = 0; i < registryFilterIterations; i++)
            {
                filterRequest = This->communicator.RequestFilters(RegistryFilter, This->RegistryFiltersCount);
                for(c = 0; c < filterRequest.CopiedFilters; c++)
                {
                    This->AddFilterSummary(filterRequest.Filters[c], RegistryFilter);
                }
            }
        }
        //
        // Import filters defined in our config if not done before.
        //
        if(This->importedConfigFilters == FALSE)
        {
            This->ImportConfigFilters();
            This->importedConfigFilters = TRUE;
        }
        Sleep(3000);
    }
}

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
    , config(CONFIG_FILE_NAME)
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
    this->alertWindow = new DetailedAlertWindow();
    this->addFilterWindow = new AddFilterWindow();
    this->addFilterWindow->communicator = &this->communicator;
    this->alertFalsePositives = config.GetConfigFalsePositives();

    CreateThread(NULL, 0, RCAST<LPTHREAD_START_ROUTINE>(this->ThreadUpdateTables), this, 0, NULL);

    SymInitialize(GetCurrentProcess(), NULL, TRUE);
}

MainWindow::~MainWindow()
{
    delete ui;
    delete investigatorWindow;
    delete alertWindow;
}

/**
 * @brief MainWindow::NotifyTabClick - Swap the active tab when a click is detected.
 * @param tab - The tab to swap to.
 */
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

/**
 * @brief MainWindow::AddAlertSummary - Add an alert to the table of alerts.
 * @param Alert - The alert to add.
 */
void MainWindow::AddAlertSummary(PBASE_ALERT_INFO Alert)
{
    std::time_t currentTime;
    std::string date;
    std::string alertName;

    PSTACK_VIOLATION_ALERT stackViolationAlert;
    PFILTER_VIOLATION_ALERT filterViolationAlert;
    PREMOTE_OPERATION_ALERT remoteOperationAlert;
    STACK_RETURN_INFO* stackHistory;
    ULONG stackHistorySize;
    ULONG i;

    //
    // Filter source path.
    //
    for(std::wstring filteredSourcePath : alertFalsePositives.SourcePathFilter)
    {
        if(wcsstr(Alert->SourcePath, filteredSourcePath.c_str()) != NULL)
        {
            printf("MainWindow!AddAlertSummary: Ignoring alert, matched source path filter %ls.\n", filteredSourcePath.c_str());
            return;
        }
    }
    //
    // Filter target path.
    //
    for(std::wstring filteredTargetPath : alertFalsePositives.TargetPathFilter)
    {
        if(wcsstr(Alert->TargetPath, filteredTargetPath.c_str()) != NULL)
        {
            printf("MainWindow!AddAlertSummary: Ignoring alert, matched target path filter %ls.\n", filteredTargetPath.c_str());
            return;
        }
    }

    //
    // Get the current time.
    //
    currentTime = std::time(nullptr);
    date = std::ctime(&currentTime);
    date[date.length()-1] = '\0'; // Remove the newline.

    //
    // Determine the alert type.
    //
    switch(Alert->AlertType)
    {
    case StackViolation:
        alertName = "Stack Violation";
        stackViolationAlert = RCAST<PSTACK_VIOLATION_ALERT>(Alert);
        //
        // Set the stack history info for this alert.
        //
        stackHistory = stackViolationAlert->StackHistory;
        stackHistorySize = stackViolationAlert->StackHistorySize;
        break;
    case FilterViolation:
        alertName = "Filter Violation";
        filterViolationAlert = RCAST<PFILTER_VIOLATION_ALERT>(Alert);
        //
        // Set the stack history info for this alert.
        //
        stackHistory = filterViolationAlert->StackHistory;
        stackHistorySize = filterViolationAlert->StackHistorySize;
        break;
    case ParentProcessIdSpoofing:
        alertName = "Parent Process ID Spoofing";
        remoteOperationAlert = RCAST<PREMOTE_OPERATION_ALERT>(Alert);
        //
        // Set the stack history info for this alert.
        //
        stackHistory = remoteOperationAlert->StackHistory;
        stackHistorySize = remoteOperationAlert->StackHistorySize;
        break;
    case RemoteThreadCreation:
        alertName = "Remote Thread Creation";
        remoteOperationAlert = RCAST<PREMOTE_OPERATION_ALERT>(Alert);
        //
        // Set the stack history info for this alert.
        //
        stackHistory = remoteOperationAlert->StackHistory;
        stackHistorySize = remoteOperationAlert->StackHistorySize;
        break;
    }

    //
    // Filter stack history.
    //
    for(i = 0; i < stackHistorySize; i++)
    {
        for(std::wstring filteredStackHistory : alertFalsePositives.StackHistoryFilter)
        {
            if(wcsstr(stackHistory[i].BinaryPath, filteredStackHistory.c_str()) != NULL)
            {
                printf("MainWindow!AddAlertSummary: Ignoring alert, matched stack history filter %ls.\n", filteredStackHistory.c_str());
                return;
            }
        }
    }


    this->ui->AlertsTable->setRowCount(this->AlertsTableSize + 1);
    this->ui->AlertsTable->insertRow(0);
    this->ui->AlertsTable->setItem(0, 0, new QTableWidgetItem(QString::fromStdString(date)));
    this->ui->AlertsTable->setItem(0, 1, new QTableWidgetItem(QString::fromStdString(alertName)));
    this->AlertsTableSize++;

    alerts.push_back(Alert);
    this->ui->AlertsLabel->SetCustomText("<img src=\":/assets/PendingAlertsTab.png\"/>");
}

/**
 * @brief MainWindow::AddProcessSummary - Add a process to the processes table.
 * @param ProcessSummary - The process to add.
 */
void MainWindow::AddProcessSummary(PROCESS_SUMMARY_ENTRY ProcessSummary)
{
    std::time_t processExecutionDate;
    std::string dateString;
    QTableWidgetItem* pathItem;

    //
    // First, we need to convert the epoch time to a date.
    //
    processExecutionDate = ProcessSummary.EpochExecutionTime;
    dateString = std::ctime(&processExecutionDate);
    dateString[dateString.length()-1] = '\0'; // Remove the newline.

    //
    // Next, add the appropriate information to the table.
    //
    this->ui->ProcessesTable->setRowCount(this->ProcessesTableSize + 1);
    this->ui->ProcessesTable->insertRow(0);
    this->ui->ProcessesTable->setItem(0, 0, new QTableWidgetItem(QString::number(RCAST<ULONG64>(ProcessSummary.ProcessId))));
    this->ui->ProcessesTable->setItem(0, 1, new QTableWidgetItem(QString::fromStdString(dateString)));

    pathItem = new QTableWidgetItem(QString::fromWCharArray(ProcessSummary.ImageFileName));
    pathItem->setToolTip(QString::fromWCharArray(ProcessSummary.ImageFileName));
    this->ui->ProcessesTable->setItem(0, 2, pathItem);
    //this->ui->ProcessesTable->resizeRowsToContents();
    this->ProcessesTableSize++;

    processes.push_back(ProcessSummary);
}

/**
 * @brief MainWindow::AddFilterSummary - Add a filter to the filters table.
 * @param FilterInfo - The filter to add.
 * @param FilterType - The type of filter to add.
 */
void MainWindow::AddFilterSummary(FILTER_INFO FilterInfo, STRING_FILTER_TYPE FilterType)
{
    std::string filterType;
    std::string filterFlags;

    //
    // Depending on the filter type, make the enum value a string.
    //
    switch(FilterType)
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

/**
 * @brief MainWindow::on_InvestigateProcessButton_clicked - Open the process investigation window for the selected process.
 */
void MainWindow::on_InvestigateProcessButton_clicked()
{
    PPROCESS_DETAILED_REQUEST processDetailed;
    PROCESS_SIZES_REQUEST processSizes;
    int selectedRow;

    if(this->ui->ProcessesTable->selectedItems().size() == 0)
    {
        return;
    }

    //
    // Since rows start from 0, we need to subtract
    // the row count from the table size to get the
    // right index.
    //
    selectedRow = processes.size() - 1 - this->ui->ProcessesTable->selectedItems()[0]->row();

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

/**
 * @brief MainWindow::on_ProcessSearch_editingFinished - Search for the specified process.
 */
void MainWindow::on_ProcessSearch_editingFinished()
{
    QList<QTableWidgetItem*> searchResults;
    QModelIndexList indexList;
    int firstSelectedRow;
    QTableWidgetItem* nextWidgetItem;

    nextWidgetItem = NULL;

    indexList = this->ui->ProcessesTable->selectionModel()->selectedIndexes();
    if(indexList.count() >= 1)
    {
        firstSelectedRow = indexList[0].row();
    }
    else
    {
        firstSelectedRow = 0;
    }


    //
    // Search for the first widget after whatever row I have selected.
    // Allows for searching of multiple items, not just one.
    //
    searchResults = this->ui->ProcessesTable->findItems(this->ui->ProcessSearch->text(), Qt::MatchContains);
    for(QTableWidgetItem* result : searchResults)
    {
        if(result->row() > firstSelectedRow)
        {
            nextWidgetItem = result;
            break;
        }
    }
    if(nextWidgetItem)
    {
        this->ui->ProcessesTable->selectRow(nextWidgetItem->row());
    }
}

/**
 * @brief MainWindow::on_OpenAlertButton_clicked - Open the selected alert.
 */
void MainWindow::on_OpenAlertButton_clicked()
{
    int selectedRow;

    if(this->ui->AlertsTable->selectedItems().size() == 0)
    {
        return;
    }

    //
    // Since rows start from 0, we need to subtract
    // the row count from the table size to get the
    // right index.
    //
    selectedRow = alerts.size() - 1 - this->ui->AlertsTable->selectedItems()[0]->row();

    alertWindow->UpdateDisplayAlert(alerts[selectedRow]);
    alertWindow->show();
}

/**
 * @brief MainWindow::on_DeleteAlertButton_clicked - Delete the selected alert.
 */
void MainWindow::on_DeleteAlertButton_clicked()
{
    int selectedRow;

    if(this->ui->AlertsTable->selectedItems().size() == 0)
    {
        return;
    }

    selectedRow = this->ui->AlertsTable->selectedItems()[0]->row();
    this->ui->AlertsTable->removeRow(selectedRow);
}

/**
 * @brief MainWindow::on_AddFilterButton_clicked - Add a filter.
 */
void MainWindow::on_AddFilterButton_clicked()
{
    this->addFilterWindow->ClearStates();
    this->addFilterWindow->show();
}

/**
 * @brief MainWindow::on_DeleteFilterButton_clicked - Make a request to delete the selected filter.
 */
void MainWindow::on_DeleteFilterButton_clicked()
{
    int selectedRow;

    if(this->ui->FiltersTable->selectedItems().size() == 0)
    {
        return;
    }

    //
    // Since rows start from 0, we need to subtract
    // the row count from the table size to get the
    // right index.
    //
    selectedRow = this->ui->FiltersTable->selectedItems()[0]->row();
    //
    // If we successfully deleted the filter, remove the row.
    //
    if(communicator.DeleteFilter(filters[selectedRow]))
    {
        this->ui->FiltersTable->removeRow(selectedRow);

        //
        // Delete the filter from our records.
        //
        switch(filters[selectedRow].Type)
        {
        case FilesystemFilter:
            this->FilesystemFiltersCount--;
            break;
        case RegistryFilter:
            this->RegistryFiltersCount--;
            break;
        }

        filters.erase(filters.begin() + selectedRow);
        this->FiltersTableSize--;
    }
}
