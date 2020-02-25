#include "InvestigateProcessWindow.h"
#include "ui_InvestigateProcessWindow.h"

/**
 * @brief InvestigateProcessWindow::InitializeCommonTable - Common initialization across all tables.
 * @param table
 */
void InvestigateProcessWindow::InitializeCommonTable(QTableWidget *table)
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
    table->setStyleSheet("background: white;");
    table->verticalHeader()->setSectionResizeMode(QHeaderView::Fixed);
    table->verticalHeader()->setDefaultSectionSize(10);
}

/**
 * @brief InvestigateProcessWindow::InitializeProcessInfoTable - Initialize properties for the process info table.
 */
void InvestigateProcessWindow::InitializeProcessInfoTable()
{
    this->ui->ProcessInformationTable->setColumnCount(2);
    this->ProcessInfoTableSize = 0;

    AddProcessInfoItem("Process Id");
    AddProcessInfoItem("Path");
    AddProcessInfoItem("Command Line");
    AddProcessInfoItem("Execution Time");
    AddProcessInfoItem("Caller Process Id");
    //AddProcessInfoItem("Caller Path");
    AddProcessInfoItem("Parent Process Id");
    AddProcessInfoItem("Parent Path");

    this->ui->ProcessInformationTable->horizontalHeader()->hide();
    this->ui->ProcessInformationTable->setVerticalScrollBarPolicy(Qt::ScrollBarPolicy::ScrollBarAlwaysOff);
    InitializeCommonTable(this->ui->ProcessInformationTable);
}

/**
 * @brief InvestigateProcessWindow::InitializeStackHistoryTable - Initialize properties for the stack history table.
 * @param historyTable - The stack history table to modify.
 */
void InvestigateProcessWindow::InitializeStackHistoryTable(QTableWidget* historyTable)
{
    QStringList headers;

    historyTable->setColumnCount(1);
    InitializeCommonTable(historyTable);

    headers << "Stack Return Address";
    historyTable->setHorizontalHeaderLabels(headers);
    historyTable->setColumnWidth(0, 300);
    historyTable->setWordWrap(true);
}

/**
 * @brief InvestigateProcessWindow::InitializeProcessImagesTable - Initialize properties for the process images table.
 */
void InvestigateProcessWindow::InitializeProcessImagesTable()
{
    InitializeCommonTable(this->ui->ImagesTable);

    this->ui->ImagesTable->setColumnCount(1);
    this->ui->ImagesTable->setColumnWidth(0, 400);

    this->ui->ImagesTable->horizontalHeader()->hide();
    this->ui->ImagesTable->setWordWrap(true);
}

/**
 * @brief InvestigateProcessWindow::AddProcessInfoItem - Add a header item to the process info table.
 * @param HeaderName - Header name.
 */
void InvestigateProcessWindow::AddProcessInfoItem(std::string HeaderName)
{
    this->ui->ProcessInformationTable->setRowCount(this->ProcessInfoTableSize + 1);
    this->ui->ProcessInformationTable->setItem(this->ProcessInfoTableSize, 0, new QTableWidgetItem(QString::fromStdString(HeaderName)));
    this->ProcessInfoTableSize++;
}

/**
 * @brief InvestigateProcessWindow::UpdateWidget - Refresh stylesheet (Qt bug).
 * @param widget - Widget to update.
 */
void InvestigateProcessWindow::UpdateWidget(QWidget *widget)
{
    widget->style()->unpolish(widget);
    widget->style()->polish(widget);
    QEvent event(QEvent::StyleChange);
    QApplication::sendEvent(widget, &event);
    widget->update();
    widget->updateGeometry();
}

InvestigateProcessWindow::InvestigateProcessWindow(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::InvestigateProcessWindow)
{
    ui->setupUi(this);

    this->setFixedSize(QSize(930, 400));

    InitializeProcessInfoTable();
    InitializeStackHistoryTable(this->ui->ProcessStackHistoryTable);
    InitializeProcessImagesTable();
    InitializeStackHistoryTable(this->ui->ImageStackHistoryTable);
}

InvestigateProcessWindow::~InvestigateProcessWindow()
{
    delete ui;
}

/**
 * @brief InvestigateProcessWindow::UpdateNewProcess - Set the investigator to display another process.
 * @param detailedProcessInformation - The new process to display.
 */
void InvestigateProcessWindow::UpdateNewProcess(PROCESS_DETAILED_REQUEST detailedProcessInformation)
{
    std::time_t processExecutionDate;
    std::string dateString;
    ULONG i;
    CHAR tempBuffer[sizeof(SYMBOL_INFO) + MAX_SYM_NAME];
    PSYMBOL_INFO currentSymbolInformation;
    ULONG64 offset;
    QString stackHistoryString;
    QString tooltip;
    QTableWidgetItem* newWidget;
    bool stackHistoryViolation;

    memset(tempBuffer, 0, sizeof(tempBuffer));
    currentSymbolInformation = RCAST<PSYMBOL_INFO>(tempBuffer);
    currentSymbolInformation->SizeOfStruct = sizeof(SYMBOL_INFO);
    currentSymbolInformation->MaxNameLen = MAX_SYM_NAME;

    //
    // First, we need to convert the epoch time to a date.
    //
    processExecutionDate = detailedProcessInformation.EpochExecutionTime;
    dateString = std::ctime(&processExecutionDate);
    dateString[dateString.length()-1] = '\0'; // Remove the newline.

    this->ProcessId = detailedProcessInformation.ProcessId;
    this->EpochExecutionTime = detailedProcessInformation.EpochExecutionTime;

    //
    // Copy basic process information.
    //
    this->ui->ProcessInformationTable->setItem(0, 1, new QTableWidgetItem(QString::number(RCAST<ULONG64>(detailedProcessInformation.ProcessId))));

    newWidget = new QTableWidgetItem(QString::fromWCharArray(detailedProcessInformation.ProcessPath));
    newWidget->setToolTip(QString::fromWCharArray(detailedProcessInformation.ProcessPath));
    this->ui->ProcessInformationTable->setItem(1, 1, newWidget);

    newWidget = new QTableWidgetItem(QString::fromWCharArray(detailedProcessInformation.ProcessCommandLine));
    newWidget->setToolTip(QString::fromWCharArray(detailedProcessInformation.ProcessCommandLine));
    this->ui->ProcessInformationTable->setItem(2, 1, newWidget);

    this->ui->ProcessInformationTable->setItem(3, 1, new QTableWidgetItem(QString::fromStdString(dateString)));
    this->ui->ProcessInformationTable->resizeRowsToContents();

    this->ui->ProcessInformationTable->setItem(4, 1, new QTableWidgetItem(QString::number(RCAST<ULONG64>(detailedProcessInformation.CallerProcessId))));

    //newWidget = new QTableWidgetItem(QString::fromWCharArray(detailedProcessInformation.CallerProcessPath));
    //newWidget->setToolTip(QString::fromWCharArray(detailedProcessInformation.CallerProcessPath));
    //this->ui->ProcessInformationTable->setItem(5, 1, new QTableWidgetItem(QString::fromWCharArray(detailedProcessInformation.CallerProcessPath)));

    this->ui->ProcessInformationTable->setItem(5, 1, new QTableWidgetItem(QString::number(RCAST<ULONG64>(detailedProcessInformation.ParentProcessId))));

    newWidget = new QTableWidgetItem(QString::fromWCharArray(detailedProcessInformation.ParentProcessPath));
    newWidget->setToolTip(QString::fromWCharArray(detailedProcessInformation.ParentProcessPath));
    this->ui->ProcessInformationTable->setItem(6, 1, new QTableWidgetItem(QString::fromWCharArray(detailedProcessInformation.ParentProcessPath)));

    this->ui->ProcessInformationTable->resizeRowsToContents();

    //
    // Fix sizing.
    //
    this->ui->ProcessInformationTable->verticalHeader()->setSectionResizeMode(QHeaderView::Fixed);
    this->ui->ProcessInformationTable->verticalHeader()->setDefaultSectionSize(10);

    //
    // Copy the stack history.
    //
    this->ui->ProcessStackHistoryTable->setRowCount(detailedProcessInformation.StackHistorySize);
    for(i = 0; i < detailedProcessInformation.StackHistorySize; i++)
    {
        stackHistoryString = "";
        stackHistoryViolation = false;
        //
        // First, try a symbol lookup.
        //
        if(SymFromAddr(GetCurrentProcess(), RCAST<DWORD64>(detailedProcessInformation.StackHistory[i].RawAddress), &offset, currentSymbolInformation))
        {
            stackHistoryString = stackHistoryString.sprintf("%s+0x%llx", currentSymbolInformation->Name, offset);
            tooltip = tooltip.sprintf("%ls+0x%llx", detailedProcessInformation.StackHistory[i].BinaryPath, detailedProcessInformation.StackHistory[i].BinaryOffset);
        }
        else
        {
            printf("Failed SymFromAddr %i.\n", GetLastError());

            if(wcslen(detailedProcessInformation.StackHistory[i].BinaryPath) == 0)
            {
                stackHistoryString = stackHistoryString.sprintf("0x%llx", detailedProcessInformation.StackHistory[i].RawAddress);
                stackHistoryViolation = true;
            }
            else
            {
                stackHistoryString = stackHistoryString.sprintf("%ls+0x%llx", detailedProcessInformation.StackHistory[i].BinaryPath, detailedProcessInformation.StackHistory[i].BinaryOffset);
            }
            tooltip = stackHistoryString;
        }

        this->ui->ProcessStackHistoryTable->setRowCount(i + 1);
        newWidget = new QTableWidgetItem(stackHistoryString);
        newWidget->setToolTip(tooltip);
        if(stackHistoryViolation)
        {
            newWidget->setBackground(Qt::red);
        }
        this->ui->ProcessStackHistoryTable->setItem(i, 0, newWidget);
        this->ui->ProcessStackHistoryTable->resizeRowsToContents();
    }

    //
    // Copy the images.
    //
    this->images.clear();
    for(i = 0; i < detailedProcessInformation.ImageSummarySize; i++)
    {
        this->images.push_back(detailedProcessInformation.ImageSummary[i]);
        this->ui->ImagesTable->setRowCount(i + 1);
        newWidget = new QTableWidgetItem(QString::fromWCharArray(detailedProcessInformation.ImageSummary[i].ImagePath));
        newWidget->setToolTip(QString::fromWCharArray(detailedProcessInformation.ImageSummary[i].ImagePath));
        this->ui->ImagesTable->setItem(i, 0, newWidget);
        this->ui->ImagesTable->resizeRowsToContents();
    }

    this->ui->ImagesTable->selectRow(0);

    //
    // By default, first image is picked.
    //
    this->on_ImagesTable_cellClicked(0, 0);
}

/**
 * @brief InvestigateProcessWindow::RefreshWidgets - Update the standard tables.
 */
void InvestigateProcessWindow::RefreshWidgets()
{
    UpdateWidget(this->ui->ProcessInformationTable);
    UpdateWidget(this->ui->ProcessStackHistoryTable);
    UpdateWidget(this->ui->ImagesTable);
    UpdateWidget(this->ui->ImageStackHistoryTable);
    UpdateWidget(this);
    this->setStyleSheet("background-color: #404040;");
}

/**
 * @brief InvestigateProcessWindow::on_ImagesTable_cellClicked - Grab the stack for the new image.
 * @param row - Row of the image selected.
 * @param column - Column of the image selected.
 */
void InvestigateProcessWindow::on_ImagesTable_cellClicked(int row, int column)
{
    IMAGE_SUMMARY image;
    PIMAGE_DETAILED_REQUEST imageDetailed;
    ULONG i;
    QString stackHistoryString;
    QString tooltip;
    CHAR tempBuffer[sizeof(IMAGEHLP_SYMBOL64) + MAX_SYM_NAME];
    PIMAGEHLP_SYMBOL64 currentSymbolInformation;
    ULONG64 offset;
    QTableWidgetItem* newWidget;
    bool stackHistoryViolation;

    memset(tempBuffer, 0, sizeof(tempBuffer));
    currentSymbolInformation = RCAST<PIMAGEHLP_SYMBOL64>(tempBuffer);
    currentSymbolInformation->SizeOfStruct = sizeof(IMAGEHLP_SYMBOL64);
    currentSymbolInformation->MaxNameLength = MAX_SYM_NAME;

    image = images[row];
    imageDetailed = communicator->RequestDetailedImage(this->ProcessId, this->EpochExecutionTime, row, image.StackSize);
    if (imageDetailed == NULL || imageDetailed->Populated == FALSE)
    {
        return;
    }

    //
    // Copy the stack history.
    //
    this->ui->ImageStackHistoryTable->setRowCount(imageDetailed->StackHistorySize);
    for(i = 0; i < imageDetailed->StackHistorySize; i++)
    {
        stackHistoryViolation = false;
        //
        // First, try a symbol lookup.
        //
        if(SymGetSymFromAddr64(GetCurrentProcess(), RCAST<DWORD64>(imageDetailed->StackHistory[i].RawAddress), &offset, currentSymbolInformation))
        {
            stackHistoryString = stackHistoryString.sprintf("%s+0x%llx", currentSymbolInformation->Name, offset);
            tooltip = tooltip.sprintf("%ls+0x%llx", imageDetailed->StackHistory[i].BinaryPath, imageDetailed->StackHistory[i].BinaryOffset);
        }
        else
        {
            if(wcslen(imageDetailed->StackHistory[i].BinaryPath) == 0)
            {
                stackHistoryString = stackHistoryString.sprintf("0x%llx", imageDetailed->StackHistory[i].RawAddress);
                stackHistoryViolation = true;
            }
            else
            {
                stackHistoryString = stackHistoryString.sprintf("%ls+0x%llx", imageDetailed->StackHistory[i].BinaryPath, imageDetailed->StackHistory[i].BinaryOffset);
            }

            tooltip = stackHistoryString;
        }

        this->ui->ImageStackHistoryTable->setRowCount(i + 1);
        newWidget = new QTableWidgetItem(stackHistoryString);

        //
        // Always set the tooltip to the expanded version.
        //
        newWidget->setToolTip(tooltip);
        if(stackHistoryViolation)
        {
            newWidget->setBackground(Qt::red);
        }
        this->ui->ImageStackHistoryTable->setItem(i, 0, newWidget);
        this->ui->ImageStackHistoryTable->resizeRowsToContents();
    }

    free(imageDetailed);
}
