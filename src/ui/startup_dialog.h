#pragma once

#include <QDialog>

#include "../recent_projects.h"

class QListWidget;
class QComboBox;
class QPushButton;

// Shown before the main window. Lets the user start a new project, browse
// for one, or pick from the recent list -- and, since vBird may need to
// read project files written by the original Vortex Studio as well as its
// own, choose which format to interpret the file as.
//
// Deliberately basic: one dialog, no wizard pages, no project templates yet.
class StartupDialog : public QDialog {
    Q_OBJECT

public:
    explicit StartupDialog(QWidget *parent = nullptr);

    // Valid only after exec() returns QDialog::Accepted. Empty path means
    // "New Project" was chosen rather than an existing file.
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

    QListWidget *m_recentList;
    QComboBox *m_formatCombo;
    QPushButton *m_removeButton;

    QString m_selectedPath;
    ProjectFormat m_selectedFormat = ProjectFormat::Auto;
};
