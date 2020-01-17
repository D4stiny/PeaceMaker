#ifndef CLICKABLETAB_H
#define CLICKABLETAB_H

#include <QLabel>
#include <QWidget>
#include <Qt>
#include <vector>

//
// These tab types allow the MainWindow to know what was clicked.
//
enum TabType
{
    AlertsTab,
    ProcessesTab,
    FiltersTab,
    ConfigTab
};

class ClickableTab : public QLabel {
    Q_OBJECT
    QWidget* mainWindow;
    bool tabActive;
    std::vector<QWidget*> associatedElements;

public:
    explicit ClickableTab(QWidget* parent = Q_NULLPTR, Qt::WindowFlags f = Qt::WindowFlags());
    ~ClickableTab();

    void SwapActiveState();
    void AddAssociatedElement(QWidget* widget);
signals:
    void clicked();

protected:
    void mousePressEvent(QMouseEvent* event);

};

#endif // CLICKABLETAB_H
