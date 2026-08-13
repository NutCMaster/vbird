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

namespace {

// What the recent list shows for one entry: filename bold-ish via plain
// text (no rich text needed yet), full path + format as the tooltip.
QString recentListLabel(const RecentProjectEntry &entry) {
    return QStringLiteral("%1  —  %2")
        .arg(QFileInfo(entry.path).fileName(), projectFormatDisplayName(entry.format));
}

} // namespace

StartupDialog::StartupDialog(QWidget *parent) : QDialog(parent) {
    setWindowTitle(QStringLiteral("vBird"));
    resize(560, 360);

    auto *root = new QVBoxLayout(this);

    auto *heading = new QLabel(QStringLiteral("<h2>vBird</h2>"), this);
    root->addWidget(heading);

    auto *body = new QHBoxLayout();
    root->addLayout(body, /*stretch=*/1);

    // --- Left: recent projects ---
    auto *recentColumn = new QVBoxLayout();
    body->addLayout(recentColumn, /*stretch=*/1);

    recentColumn->addWidget(new QLabel(QStringLiteral("Recent"), this));

    m_recentList = new QListWidget(this);
    recentColumn->addWidget(m_recentList, /*stretch=*/1);

    m_removeButton = new QPushButton(QStringLiteral("Remove from list"), this);
    m_removeButton->setEnabled(false);
    recentColumn->addWidget(m_removeButton);

    // --- Right: actions ---
    auto *actionColumn = new QVBoxLayout();
    body->addLayout(actionColumn);

    auto *newButton = new QPushButton(QStringLiteral("New Project"), this);
    actionColumn->addWidget(newButton);

    auto *openButton = new QPushButton(QStringLiteral("Open…"), this);
    actionColumn->addWidget(openButton);

    actionColumn->addSpacing(12);
    actionColumn->addWidget(new QLabel(QStringLiteral("Open as:"), this));

    m_formatCombo = new QComboBox(this);
    m_formatCombo->addItem(projectFormatDisplayName(ProjectFormat::Auto),
                            static_cast<int>(ProjectFormat::Auto));
    m_formatCombo->addItem(projectFormatDisplayName(ProjectFormat::VBirdNative),
                            static_cast<int>(ProjectFormat::VBirdNative));
    m_formatCombo->addItem(projectFormatDisplayName(ProjectFormat::VortexLegacy),
                            static_cast<int>(ProjectFormat::VortexLegacy));
    actionColumn->addWidget(m_formatCombo);

    actionColumn->addStretch(1);

    auto *cancelButton = new QPushButton(QStringLiteral("Quit"), this);
    actionColumn->addWidget(cancelButton);

    connect(newButton, &QPushButton::clicked, this, &StartupDialog::onNewProject);
    connect(openButton, &QPushButton::clicked, this, &StartupDialog::onBrowse);
    connect(m_recentList, &QListWidget::itemDoubleClicked, this, &StartupDialog::onRecentDoubleClicked);
    connect(m_removeButton, &QPushButton::clicked, this, &StartupDialog::onRemoveSelectedRecent);
    connect(cancelButton, &QPushButton::clicked, this, &QDialog::reject);
    connect(m_recentList, &QListWidget::itemSelectionChanged, this, [this] {
        m_removeButton->setEnabled(!m_recentList->selectedItems().isEmpty());
    });

    reloadRecentList();
}

void StartupDialog::reloadRecentList() {
    m_recentList->clear();
    for (const RecentProjectEntry &entry : RecentProjects::load()) {
        auto *item = new QListWidgetItem(recentListLabel(entry), m_recentList);
        item->setData(Qt::UserRole, entry.path);
        item->setData(Qt::UserRole + 1, static_cast<int>(entry.format));
        item->setToolTip(entry.path);
    }
}

void StartupDialog::onNewProject() {
    // "New" doesn't touch the recent list -- there's no file on disk yet to
    // remember. It'll get added the first time the project is saved.
    acceptWith(QString(), ProjectFormat::VBirdNative, /*recordAsRecent=*/false);
}

void StartupDialog::onBrowse() {
    const QString path = QFileDialog::getOpenFileName(
        this, QStringLiteral("Open Project"), QString(),
        QStringLiteral("Project files (*.json)"));
    if (path.isEmpty()) {
        return; // user cancelled the file dialog; stay on the startup dialog
    }
    const auto format = static_cast<ProjectFormat>(m_formatCombo->currentData().toInt());
    acceptWith(path, format, /*recordAsRecent=*/true);
}

void StartupDialog::onRecentDoubleClicked() {
    QListWidgetItem *item = m_recentList->currentItem();
    if (!item) {
        return;
    }
    const QString path = item->data(Qt::UserRole).toString();
    const auto format = static_cast<ProjectFormat>(item->data(Qt::UserRole + 1).toInt());
    acceptWith(path, format, /*recordAsRecent=*/true);
}

void StartupDialog::onRemoveSelectedRecent() {
    QListWidgetItem *item = m_recentList->currentItem();
    if (!item) {
        return;
    }
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
