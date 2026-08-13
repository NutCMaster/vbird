#include "startup_dialog.h"
#include <QComboBox>
#include <QDateTime>
#include <QFileDialog>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QLabel>
#include <QListWidget>
#include <QPushButton>
#include <QVBoxLayout>

StartupDialog::StartupDialog(QWidget *parent) : QDialog(parent) {
    setWindowTitle(QStringLiteral("vBird"));
    resize(560, 360);

    auto *root = new QVBoxLayout(this);
    root->addWidget(new QLabel(QStringLiteral("<h2>vBird</h2>"), this));

    auto *body = new QHBoxLayout;
    root->addLayout(body, 1);

    auto *recentColumn = new QVBoxLayout;
    body->addLayout(recentColumn, 1);
    recentColumn->addWidget(new QLabel(QStringLiteral("Recent"), this));

    m_recentList = new QListWidget(this);
    recentColumn->addWidget(m_recentList, 1);

    m_removeButton = new QPushButton(QStringLiteral("Remove from list"), this);
    m_removeButton->setEnabled(false);
    recentColumn->addWidget(m_removeButton);

    auto *actions = new QVBoxLayout;
    body->addLayout(actions);

    auto *newButton = new QPushButton(QStringLiteral("New Project"), this);
    auto *openButton = new QPushButton(QStringLiteral("Open…"), this);
    actions->addWidget(newButton);
    actions->addWidget(openButton);
    actions->addSpacing(12);
    actions->addWidget(new QLabel(QStringLiteral("Open as:"), this));

    m_formatCombo = new QComboBox(this);
    m_formatCombo->addItem(projectFormatDisplayName(ProjectFormat::Auto), static_cast<int>(ProjectFormat::Auto));
    m_formatCombo->addItem(projectFormatDisplayName(ProjectFormat::VBirdNative), static_cast<int>(ProjectFormat::VBirdNative));
    m_formatCombo->addItem(projectFormatDisplayName(ProjectFormat::VortexLegacy), static_cast<int>(ProjectFormat::VortexLegacy));
    actions->addWidget(m_formatCombo);
    actions->addStretch(1);

    auto *quitButton = new QPushButton(QStringLiteral("Quit"), this);
    actions->addWidget(quitButton);

    connect(newButton, &QPushButton::clicked, this, &StartupDialog::onNewProject);
    connect(openButton, &QPushButton::clicked, this, &StartupDialog::onBrowse);
    connect(m_recentList, &QListWidget::itemDoubleClicked, this, &StartupDialog::onRecentDoubleClicked);
    connect(m_removeButton, &QPushButton::clicked, this, &StartupDialog::onRemoveSelectedRecent);
    connect(quitButton, &QPushButton::clicked, this, &QDialog::reject);
    connect(m_recentList, &QListWidget::itemSelectionChanged, this, [this] {
        m_removeButton->setEnabled(!m_recentList->selectedItems().isEmpty());
    });

    reloadRecentList();
}

void StartupDialog::reloadRecentList() {
    m_recentList->clear();
    for (const auto &entry : RecentProjects::load()) {
        auto *item = new QListWidgetItem(
            QStringLiteral("%1  —  %2").arg(QFileInfo(entry.path).fileName(), projectFormatDisplayName(entry.format)),
            m_recentList);
        item->setData(Qt::UserRole, entry.path);
        item->setData(Qt::UserRole + 1, static_cast<int>(entry.format));
        item->setToolTip(entry.path);
    }
}

void StartupDialog::onNewProject() {
    acceptWith(QString(), ProjectFormat::VBirdNative, false);
}

void StartupDialog::onBrowse() {
    const QString path = QFileDialog::getOpenFileName(
        this, QStringLiteral("Open Project"), QString(), QStringLiteral("Project files (*.json)"));
    if (path.isEmpty()) return;
    acceptWith(path, static_cast<ProjectFormat>(m_formatCombo->currentData().toInt()), true);
}

void StartupDialog::onRecentDoubleClicked() {
    auto *item = m_recentList->currentItem();
    if (!item) return;
    acceptWith(item->data(Qt::UserRole).toString(),
               static_cast<ProjectFormat>(item->data(Qt::UserRole + 1).toInt()), true);
}

void StartupDialog::onRemoveSelectedRecent() {
    auto *item = m_recentList->currentItem();
    if (!item) return;
    RecentProjects::removeEntry(item->data(Qt::UserRole).toString());
    reloadRecentList();
}

void StartupDialog::acceptWith(const QString &path, ProjectFormat format, bool recordAsRecent) {
    m_selectedPath = path;
    m_selectedFormat = format;
    if (recordAsRecent && !path.isEmpty()) {
        RecentProjectEntry entry;
        entry.path = path;
        entry.format = format;
        entry.lastOpened = QDateTime::currentDateTime();
        RecentProjects::addEntry(entry);
    }
    accept();
}
