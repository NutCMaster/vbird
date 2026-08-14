#include "main_window.h"

#include <QAction>
#include <QActionGroup>
#include <QColorDialog>
#include <QDockWidget>
#include <QDoubleSpinBox>
#include <QFileInfo>
#include <QFont>
#include <QFormLayout>
#include <QIcon>
#include <QKeySequence>
#include <QLabel>
#include <QPushButton>
#include <QSignalBlocker>
#include <QSize>
#include <QStackedWidget>
#include <QToolBar>
#include <QTreeWidget>
#include <QVBoxLayout>
#include <QVulkanInstance>
#include <QWidget>

#include "../engine/scene.h"
#include "../engine/vulkan_viewport.h"

namespace {
// Index stashed on each Explorer row via Qt::UserRole; the "Workspace" root
// row has none, so data(0, Qt::UserRole) comes back invalid for it.
constexpr int kPartIndexRole = Qt::UserRole;
}

MainWindow::MainWindow(const QString &projectPath, QVulkanInstance *vulkanInstance, QWidget *parent)
    : QMainWindow(parent) {
    setWindowTitle(
        projectPath.isEmpty()
            ? QStringLiteral("vBird — New Project")
            : QStringLiteral("vBird — %1").arg(QFileInfo(projectPath).fileName())
    );

    m_viewport = new VulkanViewport(vulkanInstance);
    setCentralWidget(QWidget::createWindowContainer(m_viewport, this));

    setupToolBar();

    // Explorer and Properties both live on the right, stacked -- kept
    // visually close to familiar genre convention (a docked outliner above a
    // docked property grid) without copying any specific application's exact
    // ribbon/panel design.
    QDockWidget *explorerDock = setupExplorerDock();
    QDockWidget *propertiesDock = setupPropertiesDock();
    addDockWidget(Qt::RightDockWidgetArea, explorerDock);
    splitDockWidget(explorerDock, propertiesDock, Qt::Vertical);

    Scene &scene = m_viewport->scene();
    connect(&scene, &Scene::partAdded, this, &MainWindow::onPartAdded);
    // Selection lives on Scene rather than here because two independent
    // input paths -- the Explorer tree and viewport clicks -- both need to
    // drive and observe it; this one slot handles both directions.
    connect(&scene, &Scene::selectionChanged, this, &MainWindow::onSceneSelectionChanged);
}

void MainWindow::setupToolBar() {
    QToolBar *toolBar = addToolBar(QStringLiteral("Tools"));
    toolBar->setMovable(false);
    toolBar->setIconSize(QSize(24, 24));

    // Select/Move/Rotate/Scale: a mutually-exclusive tool-mode group. Nothing
    // in the viewport reads "which tool is active" yet (that's gizmo
    // manipulation, a later milestone) -- this just gives that future work
    // somewhere to plug in rather than leaving the buttons inert individually.
    auto *toolGroup = new QActionGroup(this);
    toolGroup->setExclusive(true);

    auto *selectAction = toolBar->addAction(QIcon(QStringLiteral(":/icons/select.webp")), QStringLiteral("Select"));
    selectAction->setCheckable(true);
    selectAction->setChecked(true);
    toolGroup->addAction(selectAction);

    auto *moveAction = toolBar->addAction(QIcon(QStringLiteral(":/icons/move.webp")), QStringLiteral("Move"));
    moveAction->setCheckable(true);
    toolGroup->addAction(moveAction);

    auto *rotateAction = toolBar->addAction(QIcon(QStringLiteral(":/icons/rotate.webp")), QStringLiteral("Rotate"));
    rotateAction->setCheckable(true);
    toolGroup->addAction(rotateAction);

    auto *scaleAction = toolBar->addAction(QIcon(QStringLiteral(":/icons/scale.webp")), QStringLiteral("Scale"));
    scaleAction->setCheckable(true);
    toolGroup->addAction(scaleAction);

    toolBar->addSeparator();

    auto *partAction = toolBar->addAction(QIcon(QStringLiteral(":/icons/part.webp")), QStringLiteral("Add Part"));
    connect(partAction, &QAction::triggered, this, &MainWindow::onAddPart);

    // SpawnLocation-style marker, distinct from a generic Part -- no such
    // object type exists yet.
    toolBar->addAction(QIcon(QStringLiteral(":/icons/spawn.webp")), QStringLiteral("Spawn"))->setEnabled(false);

    toolBar->addSeparator();

    m_copyAction = toolBar->addAction(QIcon(QStringLiteral(":/icons/copy.webp")), QStringLiteral("Copy"));
    m_copyAction->setShortcut(QKeySequence::Copy);
    m_copyAction->setEnabled(false); // enabled once something is selected
    connect(m_copyAction, &QAction::triggered, this, &MainWindow::onCopyPart);

    m_pasteAction = toolBar->addAction(QIcon(QStringLiteral(":/icons/paste.webp")), QStringLiteral("Paste"));
    m_pasteAction->setShortcut(QKeySequence::Paste);
    m_pasteAction->setEnabled(false); // enabled once something has been copied
    connect(m_pasteAction, &QAction::triggered, this, &MainWindow::onPastePart);

    m_duplicateAction = toolBar->addAction(QIcon(QStringLiteral(":/icons/duplicate.webp")), QStringLiteral("Duplicate"));
    m_duplicateAction->setShortcut(QKeySequence(QStringLiteral("Ctrl+D"))); // generic convention, not engine-specific
    m_duplicateAction->setEnabled(false);
    connect(m_duplicateAction, &QAction::triggered, this, &MainWindow::onDuplicatePart);

    toolBar->addSeparator();

    // Each of these implies a system that doesn't exist yet (workspace
    // settings, texture assets, lighting, play-mode simulation) -- shown for
    // a complete, genre-correct toolbar rather than wired to something that
    // would silently do nothing.
    toolBar->addAction(QIcon(QStringLiteral(":/icons/workspace.webp")), QStringLiteral("Workspace"))->setEnabled(false);
    toolBar->addAction(QIcon(QStringLiteral(":/icons/texture.webp")), QStringLiteral("Texture"))->setEnabled(false);
    toolBar->addAction(QIcon(QStringLiteral(":/icons/light.webp")), QStringLiteral("Light"))->setEnabled(false);

    toolBar->addSeparator();

    toolBar->addAction(QIcon(QStringLiteral(":/icons/play.webp")), QStringLiteral("Play"))->setEnabled(false);
    toolBar->addAction(QIcon(QStringLiteral(":/icons/stop.webp")), QStringLiteral("Stop"))->setEnabled(false);
}

QDockWidget *MainWindow::setupExplorerDock() {
    auto *dock = new QDockWidget(QStringLiteral("Explorer"), this);
    dock->setFeatures(QDockWidget::DockWidgetMovable | QDockWidget::DockWidgetFloatable);

    m_explorerTree = new QTreeWidget(dock);
    m_explorerTree->setHeaderHidden(true);

    m_workspaceItem = new QTreeWidgetItem(m_explorerTree, QStringList{ QStringLiteral("Workspace") });
    m_explorerTree->expandItem(m_workspaceItem);

    connect(m_explorerTree, &QTreeWidget::itemSelectionChanged, this, &MainWindow::onExplorerItemClicked);

    dock->setWidget(m_explorerTree);
    return dock;
}

QDockWidget *MainWindow::setupPropertiesDock() {
    auto *dock = new QDockWidget(QStringLiteral("Properties"), this);
    dock->setFeatures(QDockWidget::DockWidgetMovable | QDockWidget::DockWidgetFloatable);

    m_propertiesStack = new QStackedWidget(dock);

    // Page 0: empty state, shown while nothing is selected.
    auto *emptyState = new QWidget(m_propertiesStack);
    auto *emptyLayout = new QVBoxLayout(emptyState);
    emptyLayout->addStretch();
    auto *emptyEmoji = new QLabel(QStringLiteral("\U0001F4E6"), emptyState); // 📦
    QFont emojiFont = emptyEmoji->font();
    emojiFont.setPointSize(36);
    emptyEmoji->setFont(emojiFont);
    emptyEmoji->setAlignment(Qt::AlignCenter);
    auto *emptyText = new QLabel(QStringLiteral("No Part selected"), emptyState);
    emptyText->setAlignment(Qt::AlignCenter);
    emptyLayout->addWidget(emptyEmoji);
    emptyLayout->addWidget(emptyText);
    emptyLayout->addStretch();
    m_propertiesStack->addWidget(emptyState);

    // Page 1: Name/Color/Position/Rotation/Scale form, shown once a Part is
    // selected.
    auto *panel = new QWidget(m_propertiesStack);
    auto *form = new QFormLayout(panel);

    m_nameLabel = new QLabel(panel);
    form->addRow(QStringLiteral("Name"), m_nameLabel);

    m_colorSwatch = new QPushButton(panel);
    m_colorSwatch->setFixedWidth(60);
    connect(m_colorSwatch, &QPushButton::clicked, this, &MainWindow::onPropertyColorClicked);
    form->addRow(QStringLiteral("Color"), m_colorSwatch);

    auto makeSpinBox = [panel](double minValue, double maxValue, double step) {
        auto *box = new QDoubleSpinBox(panel);
        box->setRange(minValue, maxValue);
        box->setDecimals(2);
        box->setSingleStep(step);
        return box;
    };

    m_positionX = makeSpinBox(-1000.0, 1000.0, 0.5);
    m_positionY = makeSpinBox(-1000.0, 1000.0, 0.5);
    m_positionZ = makeSpinBox(-1000.0, 1000.0, 0.5);
    form->addRow(QStringLiteral("Position X"), m_positionX);
    form->addRow(QStringLiteral("Position Y"), m_positionY);
    form->addRow(QStringLiteral("Position Z"), m_positionZ);
    connect(m_positionX, &QDoubleSpinBox::valueChanged, this, &MainWindow::onPropertyPositionChanged);
    connect(m_positionY, &QDoubleSpinBox::valueChanged, this, &MainWindow::onPropertyPositionChanged);
    connect(m_positionZ, &QDoubleSpinBox::valueChanged, this, &MainWindow::onPropertyPositionChanged);

    m_rotationX = makeSpinBox(-360.0, 360.0, 5.0);
    m_rotationY = makeSpinBox(-360.0, 360.0, 5.0);
    m_rotationZ = makeSpinBox(-360.0, 360.0, 5.0);
    form->addRow(QStringLiteral("Rotation X"), m_rotationX);
    form->addRow(QStringLiteral("Rotation Y"), m_rotationY);
    form->addRow(QStringLiteral("Rotation Z"), m_rotationZ);
    connect(m_rotationX, &QDoubleSpinBox::valueChanged, this, &MainWindow::onPropertyRotationChanged);
    connect(m_rotationY, &QDoubleSpinBox::valueChanged, this, &MainWindow::onPropertyRotationChanged);
    connect(m_rotationZ, &QDoubleSpinBox::valueChanged, this, &MainWindow::onPropertyRotationChanged);

    m_scaleX = makeSpinBox(0.1, 100.0, 0.1);
    m_scaleY = makeSpinBox(0.1, 100.0, 0.1);
    m_scaleZ = makeSpinBox(0.1, 100.0, 0.1);
    form->addRow(QStringLiteral("Scale X"), m_scaleX);
    form->addRow(QStringLiteral("Scale Y"), m_scaleY);
    form->addRow(QStringLiteral("Scale Z"), m_scaleZ);
    connect(m_scaleX, &QDoubleSpinBox::valueChanged, this, &MainWindow::onPropertyScaleChanged);
    connect(m_scaleY, &QDoubleSpinBox::valueChanged, this, &MainWindow::onPropertyScaleChanged);
    connect(m_scaleZ, &QDoubleSpinBox::valueChanged, this, &MainWindow::onPropertyScaleChanged);

    m_propertiesStack->addWidget(panel);
    m_propertiesStack->setCurrentIndex(0);

    dock->setWidget(m_propertiesStack);
    return dock;
}

void MainWindow::onAddPart() {
    Scene &scene = m_viewport->scene();
    // Offsets each new Part along X so repeated clicks don't perfectly
    // overlap -- there's no gizmo/mouse-picking yet to reposition them by hand.
    scene.addPart(QVector3D(static_cast<float>(scene.count()) * 2.0f, 0.0f, 0.0f));
}

void MainWindow::onPartAdded(int index) {
    const Part &part = m_viewport->scene().part(index);
    auto *item = new QTreeWidgetItem(m_workspaceItem, QStringList{ part.name });
    item->setData(0, kPartIndexRole, index);
    m_explorerTree->setCurrentItem(item);
}

void MainWindow::onExplorerItemClicked() {
    const QList<QTreeWidgetItem *> selected = m_explorerTree->selectedItems();
    const QVariant indexData = selected.isEmpty() ? QVariant() : selected.first()->data(0, kPartIndexRole);
    m_viewport->scene().setSelectedIndex(indexData.isValid() ? indexData.toInt() : -1);
}

void MainWindow::onSceneSelectionChanged(int index) {
    m_copyAction->setEnabled(index >= 0);
    m_duplicateAction->setEnabled(index >= 0);

    // Keep the Explorer tree's highlighted row in sync -- this fires for
    // both a tree click (already matches, no-op below) and a viewport click
    // (tree needs to catch up). Blocked so this doesn't loop back into
    // onExplorerItemClicked() and redundantly re-set the same selection.
    {
        const QSignalBlocker blockTree(m_explorerTree);
        if (index < 0) {
            m_explorerTree->setCurrentItem(nullptr);
        } else {
            for (int i = 0; i < m_workspaceItem->childCount(); ++i) {
                QTreeWidgetItem *child = m_workspaceItem->child(i);
                if (child->data(0, kPartIndexRole).toInt() == index) {
                    m_explorerTree->setCurrentItem(child);
                    break;
                }
            }
        }
    }

    if (index < 0) {
        m_propertiesStack->setCurrentIndex(0); // emoji empty state
        return;
    }

    const Part &part = m_viewport->scene().part(index);

    m_propertiesStack->setCurrentIndex(1); // full form
    m_nameLabel->setText(part.name);
    updateColorSwatch(part.color);

    // setValue() emits valueChanged() when the value actually differs from a
    // spin box's previous contents (near-certain when switching between
    // Parts) -- block signals while populating so that doesn't loop back and
    // immediately overwrite the Part we just read from.
    {
        const QSignalBlocker blockX(m_positionX);
        const QSignalBlocker blockY(m_positionY);
        const QSignalBlocker blockZ(m_positionZ);
        m_positionX->setValue(part.position.x());
        m_positionY->setValue(part.position.y());
        m_positionZ->setValue(part.position.z());
    }
    {
        const QSignalBlocker blockX(m_rotationX);
        const QSignalBlocker blockY(m_rotationY);
        const QSignalBlocker blockZ(m_rotationZ);
        m_rotationX->setValue(part.rotation.x());
        m_rotationY->setValue(part.rotation.y());
        m_rotationZ->setValue(part.rotation.z());
    }
    {
        const QSignalBlocker blockX(m_scaleX);
        const QSignalBlocker blockY(m_scaleY);
        const QSignalBlocker blockZ(m_scaleZ);
        m_scaleX->setValue(part.scale.x());
        m_scaleY->setValue(part.scale.y());
        m_scaleZ->setValue(part.scale.z());
    }
}

void MainWindow::onPropertyPositionChanged() {
    Scene &scene = m_viewport->scene();
    const int index = scene.selectedIndex();
    if (index < 0) {
        return;
    }
    scene.part(index).position = QVector3D(
        static_cast<float>(m_positionX->value()),
        static_cast<float>(m_positionY->value()),
        static_cast<float>(m_positionZ->value())
    );
    // No explicit invalidation needed: SceneRenderer::startNextFrame() rereads
    // every Part fresh each frame (continuous-present loop from M1/M2), so
    // the edit shows up on the next frame on its own.
}

void MainWindow::onPropertyRotationChanged() {
    Scene &scene = m_viewport->scene();
    const int index = scene.selectedIndex();
    if (index < 0) {
        return;
    }
    scene.part(index).rotation = QVector3D(
        static_cast<float>(m_rotationX->value()),
        static_cast<float>(m_rotationY->value()),
        static_cast<float>(m_rotationZ->value())
    );
}

void MainWindow::onPropertyScaleChanged() {
    Scene &scene = m_viewport->scene();
    const int index = scene.selectedIndex();
    if (index < 0) {
        return;
    }
    scene.part(index).scale = QVector3D(
        static_cast<float>(m_scaleX->value()),
        static_cast<float>(m_scaleY->value()),
        static_cast<float>(m_scaleZ->value())
    );
}

void MainWindow::onPropertyColorClicked() {
    Scene &scene = m_viewport->scene();
    const int index = scene.selectedIndex();
    if (index < 0) {
        return;
    }
    Part &part = scene.part(index);

    const QColor chosen = QColorDialog::getColor(part.color, this, QStringLiteral("Part Color"));
    if (!chosen.isValid()) {
        return; // user cancelled
    }

    part.color = chosen;
    updateColorSwatch(chosen);
}

void MainWindow::onCopyPart() {
    Scene &scene = m_viewport->scene();
    const int index = scene.selectedIndex();
    if (index < 0) {
        return;
    }
    m_clipboard = scene.part(index);
    m_pasteAction->setEnabled(true);
}

void MainWindow::onPastePart() {
    if (!m_clipboard) {
        return;
    }
    Scene &scene = m_viewport->scene();
    // Offset from the copied position so the paste doesn't perfectly overlap
    // the original -- same reasoning as onAddPart()'s X offset.
    const int index = scene.addPart(m_clipboard->position + QVector3D(1.0f, 0.0f, 0.0f));
    Part &pasted = scene.part(index);
    pasted.rotation = m_clipboard->rotation;
    pasted.scale = m_clipboard->scale;
    pasted.color = m_clipboard->color;
    scene.setSelectedIndex(index);
}

void MainWindow::onDuplicatePart() {
    onCopyPart();
    onPastePart();
}

void MainWindow::updateColorSwatch(const QColor &color) {
    m_colorSwatch->setStyleSheet(
        QStringLiteral("background-color: %1; border: 1px solid #555;").arg(color.name())
    );
}
