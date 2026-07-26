#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QString>

#include "shaderproject.h"

class QCloseEvent;
class QLabel;
class QTextEdit;
class QVulkanInstance;
class PreviewVulkanWindow;
class RtPreviewWindow;
class ComputePreviewWindow;
class QStackedWidget;
class QAction;
class QMenu;
class QTabWidget;
class QComboBox;
class QLineEdit;
class QListWidget;
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
    void addTexture();
    void removeTexture();
    void exportBundle();

private:
    void setupDefaultUi();
    void rebuildStageEditors(const ShaderProject &p);
    StagePage *findStagePage(const QString &kind) const;
    void applyShaderProject(const ShaderProject &p);
    ShaderProject shaderProjectFromEditors() const;
    QString snapshotKey() const;
    void tryCompileAndUpload(bool showErrors);
    void compileRaytraceProject(bool showErrors);
    void compileComputeProject(bool showErrors);
    void updateCompileUi();
    void setCompileStatus(bool ok);
    void updateMeshStatus();
    void updatePreviewStackForPipelineKind();
    void addRecentFile(const QString &path);
    void rebuildRecentMenu();
    bool openProjectFromPath(const QString &path, QString *errorOut);
    void loadExampleFromJson(const QByteArray &json, const QString &titleForDirty);

    QVulkanInstance *m_vulkanInstance = nullptr;
    PreviewVulkanWindow *m_previewWindow = nullptr;
    RtPreviewWindow *m_rtPreviewWindow = nullptr;
    ComputePreviewWindow *m_computePreviewWindow = nullptr;
    QStackedWidget *m_previewStack = nullptr;

    QTabWidget *m_stageTabs = nullptr;
    QComboBox *m_pipelineKindCombo = nullptr;
    QComboBox *m_blendCombo = nullptr;
    QLabel *m_meshNameLabel = nullptr;
    QLabel *m_meshStatusIcon = nullptr;
    QString m_meshAbsolutePath;
    bool m_shaderNeedsMesh = false;
    QTextEdit *m_compileLog = nullptr;

    QString m_currentPath;
    bool m_dirty = false;
    QString m_lastGoodSnapshot;

    ShaderProject m_loadedProject;

    QListWidget *m_textureList = nullptr;

    QAction *m_compileAction = nullptr;
    QLabel *m_compileStatusIcon = nullptr;
    QMenu *m_recentMenu = nullptr;
    QString m_lastFindString;
    QString m_bundleTempDir;
};

#endif
