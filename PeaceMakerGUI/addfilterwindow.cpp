#include "addfilterwindow.h"
#include "ui_addfilterwindow.h"

AddFilterWindow::AddFilterWindow(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::AddFilterWindow)
{
    ui->setupUi(this);
    this->setFixedSize(QSize(300, 240));

    //
    // Add the different types of filter.
    //
    this->ui->FilterTypeBox->addItem("Filesystem Filter");
    this->ui->FilterTypeBox->addItem("Registry Filter");
}

AddFilterWindow::~AddFilterWindow()
{
    delete ui;
}

/**
 * @brief AddFilterWindow::ClearStates - Clear previously entered input.
 */
void AddFilterWindow::ClearStates()
{
    this->ui->FilterTypeBox->setCurrentIndex(0);
    this->ui->FilterContent->setText("");
    this->ui->DeleteFlag->setChecked(false);
    this->ui->WriteFlag->setChecked(false);
    this->ui->ExecuteFlag->setChecked(false);
}

/**
 * @brief AddFilterWindow::on_pushButton_clicked - Handle the "Add Filter" button and actually add the filter.
 */
void AddFilterWindow::on_pushButton_clicked()
{
    STRING_FILTER_TYPE filterType;
    std::wstring filterContent;
    ULONG filterFlags;

    filterFlags = 0;

    switch(this->ui->FilterTypeBox->currentIndex())
    {
    case 0:
        filterType = FilesystemFilter;
        break;
    case 1:
        filterType = RegistryFilter;
        break;
    }

    filterContent = this->ui->FilterContent->text().toStdWString();

    if(this->ui->DeleteFlag->isChecked())
    {
        filterFlags |= FILTER_FLAG_DELETE;
    }
    if(this->ui->WriteFlag->isChecked())
    {
        filterFlags |= FILTER_FLAG_WRITE;
    }
    if(this->ui->ExecuteFlag->isChecked())
    {
        filterFlags |= FILTER_FLAG_EXECUTE;
    }

    communicator->AddFilter(filterType, filterFlags, CCAST<PWCHAR>(filterContent.c_str()), filterContent.length() + 1);
    this->hide();
}
