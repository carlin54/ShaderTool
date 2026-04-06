#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QString>

#include "shaderproject.h"

class QCloseEvent;
class QTextEdit;
class QVulkanInstance;
class PreviewVulkanWindow;
class RtPreviewWindow;
class QStackedWidget;
class QAction;
class QMenu;
class QTabWidget;
class QComboBox;
class QLineEdit;
class StagePage;

class MainWindow : public QMainWindow
{
    Q_OBJECT
public:
    explicit MainWindow(QVulkanInstance *vulkanInstance, QWidget *parent = nullptr);
    ~MainWindow() override;

protected:
    void closeEvent(QCloseEvent *e) override;

private slots:
    void compile();
    void newProject();
    void openProject();
    void saveProject();
    void saveProjectAs();
    void onEditorChanged();
    void onPipelineKindChanged(int index);
    void loadExampleHello();
    void loadExampleWarm();
    void loadExampleMeshObj();
    void loadExampleRtMinimal();
    void showExportDialog();
    void addShaderStage();
    void removeCurrentStage();
    void findInEditor();
    void findNext();

private:
    void setupDefaultUi();
    void rebuildStageEditors(const ShaderProject &p);
    StagePage *findStagePage(const QString &kind) const;
    void applyShaderProject(const ShaderProject &p);
    ShaderProject shaderProjectFromEditors() const;
    QString snapshotKey() const;
    void tryCompileAndUpload(bool showErrors);
    void compileRaytraceProject(bool showErrors);
    void updateCompileUi();
    void updatePreviewStackForPipelineKind();
    void addRecentFile(const QString &path);
    void rebuildRecentMenu();
    bool openProjectFromPath(const QString &path, QString *errorOut);
    void loadExampleFromJson(const QByteArray &json, const QString &titleForDirty);

    QVulkanInstance *m_vulkanInstance = nullptr;
    PreviewVulkanWindow *m_previewWindow = nullptr;
    RtPreviewWindow *m_rtPreviewWindow = nullptr;
    QStackedWidget *m_previewStack = nullptr;

    QTabWidget *m_stageTabs = nullptr;
    QComboBox *m_pipelineKindCombo = nullptr;
    QLineEdit *m_meshPathEdit = nullptr;
    QTextEdit *m_compileLog = nullptr;

    QString m_currentPath;
    bool m_dirty = false;
    QString m_lastGoodSnapshot;

    ShaderProject m_loadedProject;

    QAction *m_compileAction = nullptr;
    QMenu *m_recentMenu = nullptr;
    QString m_lastFindString;
};

#endif
