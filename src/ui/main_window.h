#pragma once

#include <QColor>
#include <QMainWindow>
#include <optional>

#include "../engine/scene.h"

class QVulkanInstance;
class QTreeWidget;
class QTreeWidgetItem;
class QDoubleSpinBox;
class QLabel;
class QPushButton;
class QStackedWidget;
class QDockWidget;
class QAction;
class VulkanViewport;

class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    MainWindow(const QString &projectPath, QVulkanInstance *vulkanInstance, QWidget *parent = nullptr);

private slots:
    void onAddPart();
    void onPartAdded(int index);
    void onExplorerItemClicked();
    void onSceneSelectionChanged(int index);
    void onPropertyPositionChanged();
    void onPropertyRotationChanged();
    void onPropertyScaleChanged();
    void onPropertyColorClicked();
    void onCopyPart();
    void onPastePart();
    void onDuplicatePart();

private:
    void setupToolBar();
    QDockWidget *setupExplorerDock();
    QDockWidget *setupPropertiesDock();
    void updateColorSwatch(const QColor &color);

    VulkanViewport *m_viewport = nullptr;

    QTreeWidget *m_explorerTree = nullptr;
    QTreeWidgetItem *m_workspaceItem = nullptr;

    // Page 0: emoji empty-state shown with nothing selected. Page 1: the
    // actual Name/Color/Position/Rotation/Scale form, shown once a Part is
    // selected.
    QStackedWidget *m_propertiesStack = nullptr;
    QLabel *m_nameLabel = nullptr;
    QPushButton *m_colorSwatch = nullptr;
    QDoubleSpinBox *m_positionX = nullptr;
    QDoubleSpinBox *m_positionY = nullptr;
    QDoubleSpinBox *m_positionZ = nullptr;
    QDoubleSpinBox *m_rotationX = nullptr;
    QDoubleSpinBox *m_rotationY = nullptr;
    QDoubleSpinBox *m_rotationZ = nullptr;
    QDoubleSpinBox *m_scaleX = nullptr;
    QDoubleSpinBox *m_scaleY = nullptr;
    QDoubleSpinBox *m_scaleZ = nullptr;

    QAction *m_copyAction = nullptr;
    QAction *m_pasteAction = nullptr;
    QAction *m_duplicateAction = nullptr;
    std::optional<Part> m_clipboard;
};
