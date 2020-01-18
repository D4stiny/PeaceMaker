#include "detailedalertwindow.h"
#include "ui_detailedalertwindow.h"

/**
 * @brief DetailedAlertWindow::InitializeCommonTable - Common initialization across all tables.
 * @param table
 */
void DetailedAlertWindow::InitializeCommonTable(QTableWidget *table)
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
    table->horizontalHeader()->setVisible(false);
}

void DetailedAlertWindow::InitializeStackViolationAlertTable()
{
    this->ui->AlertDetailsTable->setColumnCount(2);
    InitializeCommonTable(this->ui->AlertDetailsTable);
}

DetailedAlertWindow::DetailedAlertWindow(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::DetailedAlertWindow)
{
    ui->setupUi(this);
    this->setFixedSize(QSize(400, 550));
    InitializeStackViolationAlertTable();

    SymSetOptions(SYMOPT_UNDNAME);
    SymInitialize(GetCurrentProcess(), nullptr, TRUE);
}

DetailedAlertWindow::~DetailedAlertWindow()
{
    delete ui;
}

void DetailedAlertWindow::UpdateDisplayAlert(PBASE_ALERT_INFO AlertInfo)
{
    QString alertName;
    QString alertSource;
    PSTACK_VIOLATION_ALERT stackViolationAlert;
    PFILTER_VIOLATION_ALERT filterViolationAlert;
    ULONG i;
    QString violatingAddress;

    STACK_RETURN_INFO* stackHistory;
    ULONG stackHistorySize;
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

    this->ui->AlertDetailsTable->setColumnCount(2);
    this->ui->AlertDetailsTable->setRowCount(6);

    //
    // Fill out basic fields.
    //
    this->ui->AlertDetailsTable->setItem(0, 0, new QTableWidgetItem("Name"));

    this->ui->AlertDetailsTable->setItem(1, 0, new QTableWidgetItem("Source"));

    this->ui->AlertDetailsTable->setItem(2, 0, new QTableWidgetItem("Source Id"));
    this->ui->AlertDetailsTable->setItem(2, 1, new QTableWidgetItem(QString::number(RCAST<ULONG64>(AlertInfo->SourceId))));

    this->ui->AlertDetailsTable->setItem(3, 0, new QTableWidgetItem("Source Path"));
    newWidget = new QTableWidgetItem(QString::fromWCharArray(AlertInfo->SourcePath));
    newWidget->setToolTip(QString::fromWCharArray(AlertInfo->SourcePath));
    this->ui->AlertDetailsTable->setItem(3, 1, newWidget);

    this->ui->AlertDetailsTable->setItem(4, 0, new QTableWidgetItem("Target Path"));
    newWidget = new QTableWidgetItem(QString::fromWCharArray(AlertInfo->TargetPath));
    newWidget->setToolTip(QString::fromWCharArray(AlertInfo->TargetPath));
    this->ui->AlertDetailsTable->setItem(4, 1, newWidget);

    //
    // Get the name of the source.
    //
    switch(AlertInfo->AlertSource)
    {
    case ProcessCreate:
        alertSource = "Process Creation";
        break;
    case ImageLoad:
        alertSource = "Image Load";
        break;
    case RegistryFilterMatch:
        alertSource = "Registry Filter Match";
        break;
    case FileFilterMatch:
        alertSource = "Filter Filter Match";
        break;
    case ThreadCreate:
        alertSource = "Thread Create";
        break;
    }

    //
    // Grab alert-type specific stuff.
    //
    switch(AlertInfo->AlertType)
    {
    case StackViolation:
        stackViolationAlert = RCAST<PSTACK_VIOLATION_ALERT>(AlertInfo);
        this->ui->AlertDetailsTable->setRowCount(6);
        this->ui->AlertDetailsTable->setItem(5, 0, new QTableWidgetItem("Violating Address"));
        violatingAddress = violatingAddress.sprintf("0x%llx", stackViolationAlert->ViolatingAddress);
        this->ui->AlertDetailsTable->setItem(5, 1, new QTableWidgetItem(violatingAddress));

        //
        // Set the stack history info for this alert.
        //
        stackHistory = stackViolationAlert->StackHistory;
        stackHistorySize = stackViolationAlert->StackHistorySize;
        alertName = "Stack Violation Alert";
        break;
    case FilterViolation:
        filterViolationAlert = RCAST<PFILTER_VIOLATION_ALERT>(AlertInfo);
        this->ui->AlertDetailsTable->setRowCount(7);
        this->ui->AlertDetailsTable->setItem(5, 0, new QTableWidgetItem("Filter Type"));
        this->ui->AlertDetailsTable->setItem(6, 0, new QTableWidgetItem("Filter Content"));

        //
        // Set the stack history info for this alert.
        //
        stackHistory = filterViolationAlert->StackHistory;
        stackHistorySize = filterViolationAlert->StackHistorySize;
        alertName = "Filter Violation Alert";
        break;
    }

    this->ui->AlertDetailsTable->setItem(0, 1, new QTableWidgetItem(alertName));
    this->ui->AlertDetailsTable->setItem(1, 1, new QTableWidgetItem(alertSource));

    this->ui->AlertDetailsTable->resizeRowsToContents();

    //
    // Copy the stack history.
    //
    InitializeCommonTable(this->ui->AlertStackHistoryTable);
    this->ui->AlertStackHistoryTable->setRowCount(stackHistorySize);
    this->ui->AlertStackHistoryTable->setColumnCount(1);
    for(i = 0; i < stackHistorySize; i++)
    {
        stackHistoryString = "";
        stackHistoryViolation = false;

        //
        // First, try a symbol lookup.
        //
        if(SymFromAddr(GetCurrentProcess(), RCAST<DWORD64>(stackHistory[i].RawAddress), &offset, currentSymbolInformation))
        {
            stackHistoryString = stackHistoryString.sprintf("%s+0x%llx", currentSymbolInformation->Name, offset);
            tooltip = stackHistoryString.sprintf("%ls+0x%llx", stackHistory[i].BinaryPath, stackHistory[i].BinaryOffset);
        }
        else
        {
            if(wcslen(stackHistory[i].BinaryPath) == 0)
            {
                stackHistoryString = stackHistoryString.sprintf("0x%llx", stackHistory[i].RawAddress);
                stackHistoryViolation = true;
            }
            else
            {
                stackHistoryString = stackHistoryString.sprintf("%ls+0x%llx", stackHistory[i].BinaryPath, stackHistory[i].BinaryOffset);
            }
            tooltip = stackHistoryString;
        }

        this->ui->AlertStackHistoryTable->setRowCount(i + 1);
        newWidget = new QTableWidgetItem(stackHistoryString);
        newWidget->setToolTip(tooltip);
        if(stackHistoryViolation)
        {
            newWidget->setBackground(Qt::red);
        }
        this->ui->AlertStackHistoryTable->setItem(i, 0, newWidget);
        this->ui->AlertStackHistoryTable->resizeRowsToContents();
    }
}
