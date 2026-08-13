#pragma once

#include <QDialog>
#include "../recent_projects.h"

class QListWidget;
class QComboBox;
class QPushButton;

class StartupDialog : public QDialog {
    Q_OBJECT
public:
    explicit StartupDialog(QWidget *parent = nullptr);
    QString selectedPath() const { return m_selectedPath; }
    ProjectFormat selectedFormat() const { return m_selectedFormat; }

private slots:
    void onNewProject();
    void onBrowse();
    void onRecentDoubleClicked();
    void onRemoveSelectedRecent();

private:
    void reloadRecentList();
    void acceptWith(const QString &path, ProjectFormat format, bool recordAsRecent);

    QListWidget *m_recentList = nullptr;
    QComboBox *m_formatCombo = nullptr;
    QPushButton *m_removeButton = nullptr;
    QString m_selectedPath;
    ProjectFormat m_selectedFormat = ProjectFormat::Auto;
};
