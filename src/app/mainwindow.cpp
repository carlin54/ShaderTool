#include "mainwindow.h"

#include "previewvulkanwindow.h"
#include "rtpreviewwindow.h"
#include "raytracestages.h"
#include "previewpipeline.h"
#include "shadercompiler.h"
#include "enums.h"
#include "examples.h"
#include "exportdialog.h"
#include "pathutils.h"
#include "shaderprojectjson.h"
#include "stagepage.h"

#include <QAction>
#include <QApplication>
#include <QComboBox>
#include <QDockWidget>
#include <QFileDialog>
#include <QLabel>
#include <QLineEdit>
#include <QMenuBar>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QSplitter>
#include <QStackedWidget>
#include <QStatusBar>
#include <QTabWidget>
#include <QTextEdit>
#include <QToolBar>
#include <QPushButton>
#include <QVBoxLayout>
#include <QWidget>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonValue>
#include <QCloseEvent>
#include <QFileInfo>
#include <QSaveFile>
#include <QSettings>
#include <QVulkanInstance>
#include <QInputDialog>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QHash>

#include <algorithm>
#include <functional>

static ShaderProject defaultRasterProject()
{
    ShaderProject p;
    p.formatVersion = 1;
    p.hostUniformCatalogVersion = 1;
    p.name = QStringLiteral("Untitled");
    p.pipelineKind = QStringLiteral("raster");
    p.stages.append(ShaderStage{QStringLiteral("vertex"), QStringLiteral("VSMain"), DEFAULT_VERTEX});
    p.stages.append(ShaderStage{QStringLiteral("geometry"), QStringLiteral("GSMain"), DEFAULT_GEOMETRY});
    p.stages.append(ShaderStage{QStringLiteral("fragment"), QStringLiteral("PSMain"), DEFAULT_FRAGMENT});
    return p;
}

MainWindow::MainWindow(QVulkanInstance *vulkanInstance, QWidget *parent)
    : QMainWindow(parent)
    , m_vulkanInstance(vulkanInstance)
{
    setWindowTitle(QStringLiteral("ShaderTool"));
    {
        QSettings settings;
        if (settings.contains(QStringLiteral("geometry")))
            restoreGeometry(settings.value(QStringLiteral("geometry")).toByteArray());
        else
            resize(1280, 800);
        if (settings.contains(QStringLiteral("windowState")))
            restoreState(settings.value(QStringLiteral("windowState")).toByteArray());
    }

    m_previewWindow = new PreviewVulkanWindow;
    m_previewWindow->setVulkanInstance(m_vulkanInstance);

    m_rtPreviewWindow = new RtPreviewWindow(m_vulkanInstance);
    m_rtPreviewWindow->setVulkanInstance(m_vulkanInstance);
    {
        QSettings settings;
        m_rtPreviewWindow->setPreferredGpuName(settings.value(QStringLiteral("rtPreferredGpuName")).toString());
    }
    connect(m_rtPreviewWindow, &RtPreviewWindow::gpuInitFailed, this, [this](const QString &msg) {
        QMessageBox::warning(this, QStringLiteral("Ray tracing preview"), msg);
        statusBar()->showMessage(msg, 8000);
    });

    m_previewStack = new QStackedWidget;
    QWidget *rasterContainer = QWidget::createWindowContainer(m_previewWindow);
    rasterContainer->setMinimumSize(320, 240);
    QWidget *rtContainer = QWidget::createWindowContainer(m_rtPreviewWindow);
    rtContainer->setMinimumSize(320, 240);
    m_previewStack->addWidget(rasterContainer);
    m_previewStack->addWidget(rtContainer);

    auto *dock = new QDockWidget(QStringLiteral("Preview"), this);
    dock->setObjectName(QStringLiteral("PreviewDock"));
    dock->setAllowedAreas(Qt::AllDockWidgetAreas);
    dock->setFeatures(QDockWidget::DockWidgetMovable | QDockWidget::DockWidgetFloatable);
    dock->setWidget(m_previewStack);
    addDockWidget(Qt::RightDockWidgetArea, dock);

    setupDefaultUi();

    applyShaderProject(defaultRasterProject());
    m_dirty = false;
    m_loadedProject = shaderProjectFromEditors();

    tryCompileAndUpload(false);
    m_lastGoodSnapshot = snapshotKey();
    updateCompileUi();
}

MainWindow::~MainWindow() = default;

void MainWindow::setupDefaultUi()
{
    auto *central = new QWidget;
    auto *lay = new QVBoxLayout(central);

    auto *top = new QHBoxLayout;
    top->addWidget(new QLabel(QStringLiteral("Pipeline:")));
    m_pipelineKindCombo = new QComboBox;
    m_pipelineKindCombo->addItem(QStringLiteral("Raster"));
    m_pipelineKindCombo->addItem(QStringLiteral("Ray trace"));
    connect(m_pipelineKindCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
            &MainWindow::onPipelineKindChanged);
    top->addWidget(m_pipelineKindCombo);
    auto *compileTopButton = new QPushButton(QStringLiteral("Compile"));
    connect(compileTopButton, &QPushButton::clicked, this, &MainWindow::compile);
    top->addWidget(compileTopButton);
    top->addWidget(new QLabel(QStringLiteral("Mesh (OBJ):")));
    m_meshPathEdit = new QLineEdit;
    m_meshPathEdit->setPlaceholderText(QStringLiteral("optional, path relative to project file"));
    connect(m_meshPathEdit, &QLineEdit::textChanged, this, &MainWindow::onEditorChanged);
    top->addWidget(m_meshPathEdit, 1);
    lay->addLayout(top);

    auto *split = new QSplitter(Qt::Vertical);

    m_stageTabs = new QTabWidget;
    m_compileLog = new QTextEdit;
    m_compileLog->setReadOnly(true);
    m_compileLog->setMaximumHeight(160);

    split->addWidget(m_stageTabs);
    split->addWidget(m_compileLog);
    split->setStretchFactor(0, 3);
    split->setStretchFactor(1, 1);
    lay->addWidget(split);
    setCentralWidget(central);

    QMenu *fileMenu = menuBar()->addMenu(QStringLiteral("File"));
    fileMenu->addAction(QStringLiteral("New"), this, &MainWindow::newProject, QKeySequence::New);
    fileMenu->addAction(QStringLiteral("Open…"), this, &MainWindow::openProject, QKeySequence::Open);
    fileMenu->addSeparator();
    fileMenu->addAction(QStringLiteral("Open Mesh OBJ…"), this, [this]() {
        QString startDir;
        if (!m_currentPath.isEmpty())
            startDir = QFileInfo(m_currentPath).absolutePath();
        const QString meshPath = QFileDialog::getOpenFileName(
            this, QStringLiteral("Open mesh OBJ"), startDir,
            QStringLiteral("Wavefront OBJ (*.obj)"));
        if (meshPath.isEmpty())
            return;
        m_meshPathEdit->setText(meshPath);
        statusBar()->showMessage(QStringLiteral("Mesh selected: %1").arg(QFileInfo(meshPath).fileName()), 4000);
    });
    fileMenu->addAction(QStringLiteral("Clear Mesh OBJ"), this, [this]() {
        m_meshPathEdit->clear();
        statusBar()->showMessage(QStringLiteral("Mesh cleared."), 3000);
    });
    fileMenu->addSeparator();
    m_recentMenu = fileMenu->addMenu(QStringLiteral("Open Recent"));
    rebuildRecentMenu();
    fileMenu->addSeparator();
    fileMenu->addAction(QStringLiteral("Save"), this, &MainWindow::saveProject, QKeySequence::Save);
    fileMenu->addAction(QStringLiteral("Save As…"), this, &MainWindow::saveProjectAs, QKeySequence::SaveAs);

    QMenu *editMenu = menuBar()->addMenu(QStringLiteral("Edit"));
    auto addEdit = [this, editMenu](const QString &text, const std::function<void()> &fn,
                                    const QKeySequence &seq) {
        QAction *a = new QAction(text, this);
        a->setShortcut(seq);
        connect(a, &QAction::triggered, this, [fn](bool) { fn(); });
        editMenu->addAction(a);
    };
    addEdit(QStringLiteral("Undo"),
            [this]() {
                if (auto *p = qobject_cast<QPlainTextEdit *>(QApplication::focusWidget()))
                    p->undo();
            },
            QKeySequence::Undo);
    addEdit(QStringLiteral("Redo"),
            [this]() {
                if (auto *p = qobject_cast<QPlainTextEdit *>(QApplication::focusWidget()))
                    p->redo();
            },
            QKeySequence::Redo);
    editMenu->addSeparator();
    addEdit(QStringLiteral("Cut"),
            [this]() {
                if (auto *p = qobject_cast<QPlainTextEdit *>(QApplication::focusWidget()))
                    p->cut();
            },
            QKeySequence::Cut);
    addEdit(QStringLiteral("Copy"),
            [this]() {
                if (auto *p = qobject_cast<QPlainTextEdit *>(QApplication::focusWidget()))
                    p->copy();
            },
            QKeySequence::Copy);
    addEdit(QStringLiteral("Paste"),
            [this]() {
                if (auto *p = qobject_cast<QPlainTextEdit *>(QApplication::focusWidget()))
                    p->paste();
            },
            QKeySequence::Paste);
    addEdit(QStringLiteral("Select All"),
            [this]() {
                if (auto *p = qobject_cast<QPlainTextEdit *>(QApplication::focusWidget()))
                    p->selectAll();
            },
            QKeySequence::SelectAll);
    editMenu->addSeparator();
    editMenu->addAction(QStringLiteral("Find…"), this, &MainWindow::findInEditor, QKeySequence::Find);
    editMenu->addAction(QStringLiteral("Find Next"), this, &MainWindow::findNext,
                        QKeySequence(QStringLiteral("F3")));

    QMenu *examplesMenu = menuBar()->addMenu(QStringLiteral("Examples"));
    QMenu *basics = examplesMenu->addMenu(QStringLiteral("Basics"));
    basics->addAction(QStringLiteral("Hello"), this, &MainWindow::loadExampleHello);
    basics->addAction(QStringLiteral("Warm tint"), this, &MainWindow::loadExampleWarm);
    basics->addAction(QStringLiteral("Mesh (OBJ test)"), this, &MainWindow::loadExampleMeshObj);
    QMenu *rt = examplesMenu->addMenu(QStringLiteral("Ray tracing"));
    rt->addAction(QStringLiteral("Minimal (compile test)"), this, &MainWindow::loadExampleRtMinimal);

    QMenu *exportMenu = menuBar()->addMenu(QStringLiteral("Export"));
    exportMenu->addAction(QStringLiteral("Record animation…"), this, &MainWindow::showExportDialog);

    QMenu *buildMenu = menuBar()->addMenu(QStringLiteral("Build"));
    m_compileAction = buildMenu->addAction(QStringLiteral("Compile"), this, &MainWindow::compile, QKeySequence::Refresh);
    buildMenu->addSeparator();
    buildMenu->addAction(QStringLiteral("Add shader stage…"), this, &MainWindow::addShaderStage);
    buildMenu->addAction(QStringLiteral("Remove current stage"), this, &MainWindow::removeCurrentStage);
    buildMenu->addSeparator();
    buildMenu->addAction(QStringLiteral("Set RT preferred GPU…"), this, [this]() {
        QSettings settings;
        const QString cur = settings.value(QStringLiteral("rtPreferredGpuName")).toString();
        bool ok = false;
        const QString next = QInputDialog::getText(
            this, QStringLiteral("RT preferred GPU"),
            QStringLiteral("Optional GPU name filter (substring match; empty = auto-pick compatible device):"),
            QLineEdit::Normal, cur, &ok);
        if (!ok)
            return;
        settings.setValue(QStringLiteral("rtPreferredGpuName"), next.trimmed());
        if (m_rtPreviewWindow) {
            m_rtPreviewWindow->setPreferredGpuName(next);
            m_rtPreviewWindow->resetGpuErrorAnnouncement();
        }
        statusBar()->showMessage(next.trimmed().isEmpty()
                                     ? QStringLiteral("RT preferred GPU cleared.")
                                     : QStringLiteral("RT preferred GPU set to \"%1\".").arg(next.trimmed()),
                                 4000);
    });

    connect(m_compileAction, &QAction::changed, this, [this, compileTopButton]() {
        compileTopButton->setEnabled(m_compileAction->isEnabled());
    });
    compileTopButton->setEnabled(m_compileAction->isEnabled());

    statusBar()->showMessage(QStringLiteral("Ready."));
}

void MainWindow::onPipelineKindChanged(int)
{
    m_dirty = true;
    if (m_pipelineKindCombo && m_pipelineKindCombo->currentIndex() == 1 && m_rtPreviewWindow)
        m_rtPreviewWindow->resetGpuErrorAnnouncement();
    updatePreviewStackForPipelineKind();
    updateCompileUi();
}

void MainWindow::updatePreviewStackForPipelineKind()
{
    const bool rt = m_pipelineKindCombo && m_pipelineKindCombo->currentIndex() == 1;
    if (m_previewStack)
        m_previewStack->setCurrentIndex(rt ? 1 : 0);
    if (m_meshPathEdit)
        m_meshPathEdit->setVisible(!rt);
}

void MainWindow::rebuildStageEditors(const ShaderProject &p)
{
    while (m_stageTabs->count()) {
        QWidget *w = m_stageTabs->widget(0);
        m_stageTabs->removeTab(0);
        delete w;
    }

    ShaderProject src = p;
    if (src.stages.isEmpty())
        src = defaultRasterProject();

    for (const ShaderStage &s : src.stages) {
        auto *page = new StagePage(s.kind, s.entry, s.source, this);
        m_stageTabs->addTab(page, s.kind + QStringLiteral(" · ") + s.entry);
        connect(page, &StagePage::contentChanged, this, &MainWindow::onEditorChanged);
    }
}

StagePage *MainWindow::findStagePage(const QString &kind) const
{
    for (int i = 0; i < m_stageTabs->count(); ++i) {
        auto *page = qobject_cast<StagePage *>(m_stageTabs->widget(i));
        if (page && page->kind() == kind)
            return page;
    }
    return nullptr;
}

void MainWindow::rebuildRecentMenu()
{
    m_recentMenu->clear();
    QSettings s;
    const QStringList files = s.value(QStringLiteral("recentFiles")).toStringList();
    for (const QString &path : files) {
        if (!QFileInfo::exists(path))
            continue;
        QAction *a = m_recentMenu->addAction(path);
        connect(a, &QAction::triggered, this, [this, path]() {
            QString err;
            if (!openProjectFromPath(path, &err)) {
                QMessageBox::warning(this, QStringLiteral("Open Recent"), err);
                return;
            }
            m_currentPath = path;
            m_dirty = false;
            tryCompileAndUpload(true);
        });
    }
    if (m_recentMenu->isEmpty())
        m_recentMenu->addAction(QStringLiteral("(none)"))->setEnabled(false);
}

void MainWindow::addRecentFile(const QString &path)
{
    if (path.isEmpty())
        return;
    QSettings s;
    QStringList files = s.value(QStringLiteral("recentFiles")).toStringList();
    files.removeAll(path);
    files.prepend(path);
    while (files.size() > 10)
        files.removeLast();
    s.setValue(QStringLiteral("recentFiles"), files);
    rebuildRecentMenu();
}

QString MainWindow::snapshotKey() const
{
    return QString::fromUtf8(QJsonDocument(shaderProjectFromEditors().toJson()).toJson(QJsonDocument::Compact));
}

ShaderProject MainWindow::shaderProjectFromEditors() const
{
    ShaderProject p;
    p.formatVersion = 1;
    p.hostUniformCatalogVersion = m_loadedProject.hostUniformCatalogVersion;
    QString name = QFileInfo(m_currentPath).baseName();
    if (name.isEmpty())
        name = QStringLiteral("Untitled");
    p.name = name;
    p.pipelineKind = m_pipelineKindCombo->currentIndex() == 0 ? QStringLiteral("raster")
                                                                : QStringLiteral("raytrace");
    p.meshPath = m_meshPathEdit->text().trimmed();
    p.textures = m_loadedProject.textures;
    p.maxPipelineRayRecursionDepth = m_loadedProject.maxPipelineRayRecursionDepth;

    for (int i = 0; i < m_stageTabs->count(); ++i) {
        auto *page = qobject_cast<StagePage *>(m_stageTabs->widget(i));
        if (!page)
            continue;
        ShaderStage st;
        st.kind = page->kind();
        QString ent = page->entry().trimmed();
        if (ent.isEmpty())
            ent = QStringLiteral("main");
        st.entry = ent;
        st.source = page->source();
        p.stages.append(st);
    }

    if (p.pipelineKind == QStringLiteral("raster")) {
        for (const ShaderStage &s : m_loadedProject.stages) {
            if (s.kind == QStringLiteral("vertex") || s.kind == QStringLiteral("fragment")
                || s.kind == QStringLiteral("geometry"))
                continue;
            p.stages.append(s);
        }
    }

    return p;
}

void MainWindow::applyShaderProject(const ShaderProject &p)
{
    m_loadedProject = p;
    m_pipelineKindCombo->setCurrentIndex(p.pipelineKind == QStringLiteral("raytrace") ? 1 : 0);
    m_meshPathEdit->setText(p.meshPath);
    rebuildStageEditors(p);
    updatePreviewStackForPipelineKind();
}

bool MainWindow::openProjectFromPath(const QString &path, QString *errorOut)
{
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly)) {
        if (errorOut)
            *errorOut = QStringLiteral("Could not read file.");
        return false;
    }
    QJsonParseError pe{};
    QJsonDocument doc = QJsonDocument::fromJson(f.readAll(), &pe);
    f.close();
    if (pe.error != QJsonParseError::NoError || !doc.isObject()) {
        if (errorOut)
            *errorOut = QStringLiteral("Invalid JSON.");
        return false;
    }
    QString schemaErr;
    if (!ShaderProjectJson::validateAgainstSchema(doc.object(), &schemaErr)) {
        if (errorOut)
            *errorOut = schemaErr;
        return false;
    }
    QString err;
    ShaderProject p = ShaderProject::fromJson(doc.object(), &err);
    if (!err.isEmpty()) {
        if (errorOut)
            *errorOut = err;
        return false;
    }
    const QString sem = ShaderProject::validationMessage(p);
    if (!sem.isEmpty()) {
        if (errorOut)
            *errorOut = sem;
        return false;
    }

    bool hasUnsupported = false;
    for (const ShaderStage &s : p.stages) {
        if (p.pipelineKind == QStringLiteral("raster") && !ShaderStageHelpers::isSupportedInRasterPreview(s.kind)) {
            hasUnsupported = true;
            break;
        }
    }
    if (hasUnsupported) {
        QMessageBox::information(
            this, QStringLiteral("Project"),
            QStringLiteral("This project contains shader stages not used by the raster preview "
                           "(e.g. tessellation). Those stages are preserved on save but not compiled."));
    }
    if (!p.meshPath.trimmed().isEmpty()) {
        const QString relMesh = p.meshPath.trimmed();
        if (QFileInfo(relMesh).isAbsolute() || PathUtils::safeResolveUnderProject(path, relMesh).isEmpty()) {
            if (errorOut) {
                *errorOut = QStringLiteral(
                    "Mesh path must be relative to the project directory and must not escape it: %1")
                                .arg(relMesh);
            }
            return false;
        }
    }
    for (const QJsonValue &tv : p.textures) {
        const QJsonObject t = tv.toObject();
        const QString rel = t.value(QStringLiteral("path")).toString();
        if (rel.isEmpty())
            continue;
        if (QFileInfo(rel).isAbsolute() || PathUtils::safeResolveUnderProject(path, rel).isEmpty()) {
            if (errorOut) {
                *errorOut = QStringLiteral(
                    "Texture path must be relative to the project directory and must not escape it: %1")
                                .arg(rel);
            }
            return false;
        }
    }
    applyShaderProject(p);
    return true;
}

void MainWindow::loadExampleFromJson(const QByteArray &json, const QString &titleForDirty)
{
    if (m_dirty) {
        auto r = QMessageBox::question(this, QStringLiteral("Examples"),
                                       QStringLiteral("Discard unsaved changes and load example?"));
        if (r != QMessageBox::Yes)
            return;
    }
    QJsonParseError pe{};
    QJsonDocument doc = QJsonDocument::fromJson(json, &pe);
    if (pe.error != QJsonParseError::NoError || !doc.isObject()) {
        QMessageBox::warning(this, QStringLiteral("Examples"), QStringLiteral("Invalid example JSON."));
        return;
    }
    QString schemaErr;
    if (!ShaderProjectJson::validateAgainstSchema(doc.object(), &schemaErr)) {
        QMessageBox::warning(this, QStringLiteral("Examples"), schemaErr);
        return;
    }
    QString err;
    ShaderProject p = ShaderProject::fromJson(doc.object(), &err);
    if (!err.isEmpty()) {
        QMessageBox::warning(this, QStringLiteral("Examples"), err);
        return;
    }
    const QString sem = ShaderProject::validationMessage(p);
    if (!sem.isEmpty()) {
        QMessageBox::warning(this, QStringLiteral("Examples"), sem);
        return;
    }
    m_currentPath.clear();
    applyShaderProject(p);
    m_dirty = true;
    setWindowTitle(titleForDirty + QStringLiteral(" — ShaderTool"));
    tryCompileAndUpload(true);
}

void MainWindow::loadExampleHello()
{
    loadExampleFromJson(Examples::jsonBasicsHello(), QStringLiteral("Examples: Hello"));
}

void MainWindow::loadExampleWarm()
{
    loadExampleFromJson(Examples::jsonBasicsWarm(), QStringLiteral("Examples: Warm tint"));
}

void MainWindow::loadExampleMeshObj()
{
    loadExampleFromJson(Examples::jsonBasicsMeshObj(), QStringLiteral("Examples: Mesh OBJ test"));
}

void MainWindow::loadExampleRtMinimal()
{
    loadExampleFromJson(Examples::jsonRtMinimalCompileTest(), QStringLiteral("Examples: RT minimal"));
}

void MainWindow::addShaderStage()
{
    const bool raster = m_pipelineKindCombo->currentIndex() == 0;
    QStringList kinds;
    if (raster) {
        kinds << QStringLiteral("vertex") << QStringLiteral("fragment") << QStringLiteral("geometry")
              << QStringLiteral("tessellation_control") << QStringLiteral("tessellation_evaluation");
    } else {
        kinds << QStringLiteral("raygen") << QStringLiteral("miss") << QStringLiteral("closest_hit")
              << QStringLiteral("any_hit") << QStringLiteral("intersection") << QStringLiteral("callable");
    }
    bool ok = false;
    const QString kind = QInputDialog::getItem(this, QStringLiteral("Add stage"), QStringLiteral("Stage kind:"),
                                                 kinds, 0, false, &ok);
    if (!ok)
        return;

    static const QHash<QString, QString> defaultEntryRaster = {
        {QStringLiteral("vertex"), QStringLiteral("VSMain")},
        {QStringLiteral("fragment"), QStringLiteral("PSMain")},
        {QStringLiteral("geometry"), QStringLiteral("GSMain")},
        {QStringLiteral("tessellation_control"), QStringLiteral("HSMain")},
        {QStringLiteral("tessellation_evaluation"), QStringLiteral("DSMain")},
    };
    static const QHash<QString, QString> defaultEntryRt = {
        {QStringLiteral("raygen"), QStringLiteral("RayGen")},
        {QStringLiteral("miss"), QStringLiteral("Miss")},
        {QStringLiteral("closest_hit"), QStringLiteral("ClosestHit")},
        {QStringLiteral("any_hit"), QStringLiteral("AnyHit")},
        {QStringLiteral("intersection"), QStringLiteral("Intersection")},
        {QStringLiteral("callable"), QStringLiteral("Callable")},
    };
    const QString entryDef = raster ? defaultEntryRaster.value(kind, QStringLiteral("main"))
                                    : defaultEntryRt.value(kind, QStringLiteral("main"));
    const QString entry = QInputDialog::getText(this, QStringLiteral("Entry point"),
                                                QStringLiteral("Shader entry point:"), QLineEdit::Normal,
                                                entryDef, &ok);
    if (!ok)
        return;

    auto *page = new StagePage(kind, entry, QStringLiteral("// New stage\n"), this);
    m_stageTabs->addTab(page, kind + QStringLiteral(" · ") + entry);
    connect(page, &StagePage::contentChanged, this, &MainWindow::onEditorChanged);
    m_dirty = true;
    updateCompileUi();
}

void MainWindow::findInEditor()
{
    auto *page = qobject_cast<StagePage *>(m_stageTabs->currentWidget());
    if (!page)
        return;
    QPlainTextEdit *ed = page->sourceEdit();
    bool ok = false;
    const QString s = QInputDialog::getText(this, QStringLiteral("Find"), QStringLiteral("Search for:"),
                                            QLineEdit::Normal, m_lastFindString, &ok);
    if (!ok)
        return;
    m_lastFindString = s;
    if (s.isEmpty()) {
        statusBar()->showMessage(QStringLiteral("Empty search."), 2000);
        return;
    }
    if (!ed->find(s))
        statusBar()->showMessage(QStringLiteral("Not found."), 2000);
    else
        ed->setFocus();
}

void MainWindow::findNext()
{
    if (m_lastFindString.isEmpty()) {
        findInEditor();
        return;
    }
    auto *page = qobject_cast<StagePage *>(m_stageTabs->currentWidget());
    if (!page)
        return;
    QPlainTextEdit *ed = page->sourceEdit();
    if (!ed->find(m_lastFindString))
        statusBar()->showMessage(QStringLiteral("Not found."), 2000);
    else
        ed->setFocus();
}

void MainWindow::removeCurrentStage()
{
    const int i = m_stageTabs->currentIndex();
    if (i < 0)
        return;

    auto *page = qobject_cast<StagePage *>(m_stageTabs->widget(i));
    if (!page)
        return;

    const bool raster = m_pipelineKindCombo->currentIndex() == 0;
    if (m_stageTabs->count() <= 1) {
        QMessageBox::information(this, QStringLiteral("Remove stage"),
                                 QStringLiteral("Cannot remove the last stage."));
        return;
    }

    if (raster) {
        const QString k = page->kind();
        QStringList kinds;
        for (int t = 0; t < m_stageTabs->count(); ++t) {
            if (t == i)
                continue;
            if (auto *p = qobject_cast<StagePage *>(m_stageTabs->widget(t)))
                kinds.append(p->kind());
        }
        if (k == QStringLiteral("vertex") && !kinds.contains(QStringLiteral("vertex"))) {
            QMessageBox::warning(this, QStringLiteral("Remove stage"),
                                 QStringLiteral("Raster projects need at least one vertex stage."));
            return;
        }
        if (k == QStringLiteral("fragment") && !kinds.contains(QStringLiteral("fragment"))) {
            QMessageBox::warning(this, QStringLiteral("Remove stage"),
                                 QStringLiteral("Raster projects need at least one fragment stage."));
            return;
        }
    }

    m_stageTabs->removeTab(i);
    delete page;
    m_dirty = true;
    updateCompileUi();
}

void MainWindow::showExportDialog()
{
    ExportDialog dlg(m_previewWindow, this);
    dlg.exec();
}

void MainWindow::compileRaytraceProject(bool showErrors)
{
    const ShaderProject doc = shaderProjectFromEditors();
    const QString verr = ShaderProject::validationMessage(doc);
    if (!verr.isEmpty()) {
        m_compileLog->setPlainText(verr);
        if (showErrors)
            QMessageBox::warning(this, QStringLiteral("Compile"), verr);
        return;
    }

    QString log;
    QVector<RayTraceStageBinary> bins;
    for (int i = 0; i < m_stageTabs->count(); ++i) {
        auto *page = qobject_cast<StagePage *>(m_stageTabs->widget(i));
        if (!page)
            continue;
        const QString prof = ShaderStageHelpers::dxcProfile(page->kind());
        ShaderCompileResult r = ShaderCompiler::compileHLSL(page->source(), prof, page->entry());
        log += QStringLiteral("\n--- %1 / %2 ---\n").arg(page->kind(), page->entry());
        if (!r.ok) {
            m_compileLog->setPlainText(log + r.stderrText);
            if (showErrors)
                QMessageBox::warning(this, QStringLiteral("Compile"), r.stderrText);
            return;
        }
        log += r.stderrText;
        RayTraceStageBinary b{};
        b.kind = page->kind();
        b.spirv = r.spirv;
        b.entry = page->entry();
        bins.append(b);
    }
    m_compileLog->setPlainText(QStringLiteral("Ray trace compile OK.\n") + log);
    if (m_rtPreviewWindow)
        m_rtPreviewWindow->setRayTracePayload(bins, doc.maxPipelineRayRecursionDepth);
    m_lastGoodSnapshot = snapshotKey();
    updateCompileUi();
}

void MainWindow::tryCompileAndUpload(bool showErrors)
{
    const ShaderProject doc = shaderProjectFromEditors();

    if (doc.pipelineKind == QStringLiteral("raytrace")) {
        compileRaytraceProject(showErrors);
        return;
    }

    StagePage *vs = findStagePage(QStringLiteral("vertex"));
    StagePage *fs = findStagePage(QStringLiteral("fragment"));
    if (!vs || !fs) {
        const QString msg = QStringLiteral("Raster pipeline requires vertex and fragment stages.");
        m_compileLog->setPlainText(msg);
        if (showErrors)
            QMessageBox::warning(this, QStringLiteral("Compile"), msg);
        return;
    }

    ShaderCompileResult vsc = ShaderCompiler::compileHLSL(
        vs->source(), ShaderStageHelpers::dxcProfile(QStringLiteral("vertex")), vs->entry());
    if (!vsc.ok) {
        m_compileLog->setPlainText(vsc.stderrText);
        if (showErrors)
            QMessageBox::warning(this, QStringLiteral("Vertex shader"), vsc.stderrText);
        return;
    }

    ShaderCompileResult gsc;
    StagePage *gs = findStagePage(QStringLiteral("geometry"));
    if (gs && !gs->source().trimmed().isEmpty()) {
        gsc = ShaderCompiler::compileHLSL(gs->source(), ShaderStageHelpers::dxcProfile(QStringLiteral("geometry")),
                                           gs->entry());
        if (!gsc.ok) {
            m_compileLog->setPlainText(gsc.stderrText);
            if (showErrors)
                QMessageBox::warning(this, QStringLiteral("Geometry shader"), gsc.stderrText);
            return;
        }
    }

    ShaderCompileResult fsc = ShaderCompiler::compileHLSL(
        fs->source(), ShaderStageHelpers::dxcProfile(QStringLiteral("fragment")), fs->entry());
    if (!fsc.ok) {
        m_compileLog->setPlainText(fsc.stderrText);
        if (showErrors)
            QMessageBox::warning(this, QStringLiteral("Fragment shader"), fsc.stderrText);
        return;
    }

    QString log = QStringLiteral("Compile OK.\n") + vsc.stderrText;
    if (gs && !gs->source().trimmed().isEmpty())
        log += gsc.stderrText;
    log += fsc.stderrText;
    m_compileLog->setPlainText(log);

    QVector<RasterStageBinary> bins;
    RasterStageBinary vb{};
    vb.stage = VK_SHADER_STAGE_VERTEX_BIT;
    vb.spirv = vsc.spirv;
    vb.entry = vs->entry();
    bins.append(vb);
    if (gs && !gs->source().trimmed().isEmpty()) {
        RasterStageBinary gb{};
        gb.stage = VK_SHADER_STAGE_GEOMETRY_BIT;
        gb.spirv = gsc.spirv;
        gb.entry = gs->entry();
        bins.append(gb);
    }
    RasterStageBinary fb{};
    fb.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
    fb.spirv = fsc.spirv;
    fb.entry = fs->entry();
    bins.append(fb);

    struct TexEntry {
        int slot = 0;
        QString path;
    };
    QVector<TexEntry> texList;
    for (const QJsonValue &tv : doc.textures) {
        const QJsonObject o = tv.toObject();
        TexEntry te;
        te.slot = o.value(QStringLiteral("slot")).toInt(0);
        te.path = o.value(QStringLiteral("path")).toString();
        if (!te.path.isEmpty())
            texList.append(te);
    }
    std::sort(texList.begin(), texList.end(),
              [](const TexEntry &a, const TexEntry &b) { return a.slot < b.slot; });

    QVector<QImage> texImages;
    for (const TexEntry &te : texList) {
        QString abs = te.path;
        if (!QFileInfo(te.path).isAbsolute()) {
            abs = PathUtils::safeResolveUnderProject(m_currentPath, te.path);
            if (abs.isEmpty())
                abs = te.path;
        }
        QImage img(abs);
        if (!img.isNull())
            texImages.append(img);
    }

    QString meshAbs;
    if (!doc.meshPath.isEmpty()) {
        meshAbs = PathUtils::safeResolveUnderProject(m_currentPath, doc.meshPath);
        if (meshAbs.isEmpty())
            meshAbs = doc.meshPath;
        if (!QFileInfo::exists(meshAbs))
            meshAbs.clear();
    }

    m_previewWindow->setRasterPayload(bins, texImages, meshAbs);
    m_lastGoodSnapshot = snapshotKey();
    updateCompileUi();
}

void MainWindow::updateCompileUi()
{
    const bool upToDate = (snapshotKey() == m_lastGoodSnapshot);
    statusBar()->showMessage(upToDate ? QStringLiteral("Up to date.") : QStringLiteral("Sources changed — press Compile."));
    if (m_compileAction)
        m_compileAction->setEnabled(!upToDate);
}

void MainWindow::compile()
{
    tryCompileAndUpload(true);
}

void MainWindow::onEditorChanged()
{
    m_dirty = true;
    updateCompileUi();
}

void MainWindow::newProject()
{
    if (m_dirty) {
        auto r = QMessageBox::question(this, QStringLiteral("New project"),
                                       QStringLiteral("Discard unsaved changes?"));
        if (r != QMessageBox::Yes)
            return;
    }
    m_currentPath.clear();
    m_loadedProject = ShaderProject();
    applyShaderProject(defaultRasterProject());
    m_dirty = false;
    setWindowTitle(QStringLiteral("ShaderTool"));
    tryCompileAndUpload(false);
}

void MainWindow::openProject()
{
    if (m_dirty) {
        auto r = QMessageBox::question(this, QStringLiteral("Open"),
                                       QStringLiteral("Discard unsaved changes?"));
        if (r != QMessageBox::Yes)
            return;
    }
    const QString path = QFileDialog::getOpenFileName(this, QStringLiteral("Open shader project"),
                                                      QString(),
                                                      QStringLiteral("Shader project (*.json);;All files (*)"));
    if (path.isEmpty())
        return;

    QString err;
    if (!openProjectFromPath(path, &err)) {
        QMessageBox::warning(this, QStringLiteral("Open"), err);
        return;
    }
    m_currentPath = path;
    m_dirty = false;
    setWindowTitle(QFileInfo(path).fileName() + QStringLiteral(" — ShaderTool"));
    addRecentFile(path);
    tryCompileAndUpload(true);
}

void MainWindow::saveProject()
{
    if (m_currentPath.isEmpty()) {
        saveProjectAs();
        return;
    }
    ShaderProject p = shaderProjectFromEditors();
    QSaveFile f(m_currentPath);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        QMessageBox::warning(this, QStringLiteral("Save"), QStringLiteral("Could not write file."));
        return;
    }
    f.write(QJsonDocument(p.toJson()).toJson(QJsonDocument::Indented));
    if (!f.commit()) {
        QMessageBox::warning(this, QStringLiteral("Save"), QStringLiteral("Commit failed."));
        return;
    }
    m_dirty = false;
}

void MainWindow::saveProjectAs()
{
    QString path = QFileDialog::getSaveFileName(this, QStringLiteral("Save shader project"),
                                                m_currentPath.isEmpty() ? QStringLiteral("project.json") : m_currentPath,
                                                QStringLiteral("Shader project (*.json)"));
    if (path.isEmpty())
        return;
    if (!path.endsWith(QStringLiteral(".json"), Qt::CaseInsensitive))
        path += QStringLiteral(".json");
    m_currentPath = path;
    saveProject();
    setWindowTitle(QFileInfo(path).fileName() + QStringLiteral(" — ShaderTool"));
    addRecentFile(path);
}

void MainWindow::closeEvent(QCloseEvent *e)
{
    if (m_dirty) {
        auto r = QMessageBox::question(this, QStringLiteral("Quit"),
                                       QStringLiteral("Discard unsaved changes?"));
        if (r != QMessageBox::Yes) {
            e->ignore();
            return;
        }
    }
    {
        QSettings settings;
        settings.setValue(QStringLiteral("geometry"), saveGeometry());
        settings.setValue(QStringLiteral("windowState"), saveState());
    }
    QMainWindow::closeEvent(e);
}
