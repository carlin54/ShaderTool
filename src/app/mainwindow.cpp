#include "mainwindow.h"

#include "previewvulkanwindow.h"
#include "rtpreviewwindow.h"
#include "computepreviewwindow.h"
#include "shaderprojectbundle.h"
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
#include <QDir>
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
#include <QTemporaryDir>
#include <QVulkanInstance>
#include <QInputDialog>
#include <QLineEdit>
#include <QListWidget>
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

    m_computePreviewWindow = new ComputePreviewWindow(m_vulkanInstance);
    m_computePreviewWindow->setVulkanInstance(m_vulkanInstance);
    connect(m_computePreviewWindow, &ComputePreviewWindow::gpuInitFailed, this, [this](const QString &msg) {
        QMessageBox::warning(this, QStringLiteral("Compute preview"), msg);
        statusBar()->showMessage(msg, 8000);
    });

    m_previewStack = new QStackedWidget;
    QWidget *rasterContainer = QWidget::createWindowContainer(m_previewWindow);
    rasterContainer->setMinimumSize(320, 240);
    QWidget *rtContainer = QWidget::createWindowContainer(m_rtPreviewWindow);
    rtContainer->setMinimumSize(320, 240);
    QWidget *computeContainer = QWidget::createWindowContainer(m_computePreviewWindow);
    computeContainer->setMinimumSize(320, 240);
    m_previewStack->addWidget(rasterContainer);
    m_previewStack->addWidget(rtContainer);
    m_previewStack->addWidget(computeContainer);

    auto *dock = new QDockWidget(QStringLiteral("Preview"), this);
    dock->setObjectName(QStringLiteral("PreviewDock"));
    dock->setAllowedAreas(Qt::AllDockWidgetAreas);
    dock->setFeatures(QDockWidget::DockWidgetMovable | QDockWidget::DockWidgetFloatable);
    dock->setWidget(m_previewStack);
    addDockWidget(Qt::RightDockWidgetArea, dock);

    {
        auto *texDock = new QDockWidget(QStringLiteral("Textures"), this);
        texDock->setObjectName(QStringLiteral("TexturesDock"));
        texDock->setAllowedAreas(Qt::AllDockWidgetAreas);
        texDock->setFeatures(QDockWidget::DockWidgetMovable | QDockWidget::DockWidgetFloatable
                             | QDockWidget::DockWidgetClosable);
        auto *texWidget = new QWidget;
        auto *texLay = new QVBoxLayout(texWidget);
        texLay->setContentsMargins(4, 4, 4, 4);
        m_textureList = new QListWidget;
        texLay->addWidget(m_textureList);
        auto *texButtons = new QHBoxLayout;
        auto *addBtn = new QPushButton(QStringLiteral("Add…"));
        auto *removeBtn = new QPushButton(QStringLiteral("Remove"));
        connect(addBtn, &QPushButton::clicked, this, &MainWindow::addTexture);
        connect(removeBtn, &QPushButton::clicked, this, &MainWindow::removeTexture);
        texButtons->addWidget(addBtn);
        texButtons->addWidget(removeBtn);
        texButtons->addStretch();
        texLay->addLayout(texButtons);
        texDock->setWidget(texWidget);
        addDockWidget(Qt::RightDockWidgetArea, texDock);
    }

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
    m_pipelineKindCombo->addItem(QStringLiteral("Compute"));
    connect(m_pipelineKindCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
            &MainWindow::onPipelineKindChanged);
    top->addWidget(m_pipelineKindCombo);
    m_blendCombo = new QComboBox;
    m_blendCombo->addItem(QStringLiteral("Blend: Off"));
    m_blendCombo->addItem(QStringLiteral("Blend: Alpha"));
    m_blendCombo->addItem(QStringLiteral("Blend: Additive"));
    connect(m_blendCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this]() {
        m_dirty = true;
        tryCompileAndUpload(false);
    });
    top->addWidget(m_blendCombo);
    auto *compileTopButton = new QPushButton(QStringLiteral("Compile"));
    connect(compileTopButton, &QPushButton::clicked, this, &MainWindow::compile);
    top->addWidget(compileTopButton);
    m_compileStatusIcon = new QLabel;
    m_compileStatusIcon->setFixedSize(18, 18);
    m_compileStatusIcon->setStyleSheet(QStringLiteral("background-color: #9e9e9e; border-radius: 9px;"));
    m_compileStatusIcon->setToolTip(QStringLiteral("Compile status"));
    top->addWidget(m_compileStatusIcon);
    auto *meshLabel = new QLabel(QStringLiteral("Mesh:"));
    meshLabel->setObjectName(QStringLiteral("meshLabel"));
    top->addWidget(meshLabel);
    m_meshStatusIcon = new QLabel;
    m_meshStatusIcon->setFixedSize(18, 18);
    m_meshStatusIcon->setStyleSheet(QStringLiteral("background-color: #9e9e9e; border-radius: 9px;"));
    m_meshStatusIcon->setToolTip(QStringLiteral("Mesh status"));
    top->addWidget(m_meshStatusIcon);
    m_meshNameLabel = new QLabel(QStringLiteral("None"));
    m_meshNameLabel->setMinimumWidth(80);
    top->addWidget(m_meshNameLabel);
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
        m_meshAbsolutePath = meshPath;
        m_meshNameLabel->setText(QFileInfo(meshPath).fileName());
        updateMeshStatus();
        m_dirty = true;
        m_compileLog->append(QStringLiteral("Mesh loaded: %1").arg(QFileInfo(meshPath).fileName()));
        statusBar()->showMessage(QStringLiteral("Mesh loaded: %1").arg(QFileInfo(meshPath).fileName()), 4000);
        tryCompileAndUpload(false);
    });
    fileMenu->addAction(QStringLiteral("Clear Mesh OBJ"), this, [this]() {
        m_meshAbsolutePath.clear();
        m_meshNameLabel->setText(QStringLiteral("None"));
        updateMeshStatus();
        m_dirty = true;
        m_compileLog->append(QStringLiteral("Mesh cleared."));
        statusBar()->showMessage(QStringLiteral("Mesh cleared."), 3000);
        tryCompileAndUpload(false);
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
    {
        QMenu *basics = examplesMenu->addMenu(QStringLiteral("Basics"));
        basics->addAction(QStringLiteral("Hello"), this, &MainWindow::loadExampleHello);
        basics->addAction(QStringLiteral("Warm tint"), this, &MainWindow::loadExampleWarm);
        basics->addAction(QStringLiteral("Mesh (OBJ test)"), this, &MainWindow::loadExampleMeshObj);

        QMenu *effects = examplesMenu->addMenu(QStringLiteral("Effects"));
        effects->addAction(QStringLiteral("Fire"), this, [this]() { loadExampleFromJson(Examples::jsonEffectsFire(), QStringLiteral("Effects: Fire")); });
        effects->addAction(QStringLiteral("Plasma"), this, [this]() { loadExampleFromJson(Examples::jsonEffectsPlasma(), QStringLiteral("Effects: Plasma")); });
        effects->addAction(QStringLiteral("Water ripples"), this, [this]() { loadExampleFromJson(Examples::jsonEffectsWaterRipples(), QStringLiteral("Effects: Water ripples")); });
        effects->addAction(QStringLiteral("Starfield"), this, [this]() { loadExampleFromJson(Examples::jsonEffectsStarfield(), QStringLiteral("Effects: Starfield")); });
        effects->addAction(QStringLiteral("Aurora"), this, [this]() { loadExampleFromJson(Examples::jsonEffectsAurora(), QStringLiteral("Effects: Aurora")); });
        effects->addAction(QStringLiteral("Lava lamp"), this, [this]() { loadExampleFromJson(Examples::jsonEffectsLavaLamp(), QStringLiteral("Effects: Lava lamp")); });

        QMenu *lighting = examplesMenu->addMenu(QStringLiteral("Lighting"));
        lighting->addAction(QStringLiteral("Toon / Cel shading"), this, [this]() { loadExampleFromJson(Examples::jsonLightingToon(), QStringLiteral("Lighting: Toon")); });
        lighting->addAction(QStringLiteral("Phong"), this, [this]() { loadExampleFromJson(Examples::jsonLightingPhong(), QStringLiteral("Lighting: Phong")); });
        lighting->addAction(QStringLiteral("Rim light"), this, [this]() { loadExampleFromJson(Examples::jsonLightingRimLight(), QStringLiteral("Lighting: Rim light")); });
        lighting->addAction(QStringLiteral("Matcap"), this, [this]() { loadExampleFromJson(Examples::jsonLightingMatcap(), QStringLiteral("Lighting: Matcap")); });
        lighting->addAction(QStringLiteral("Wireframe"), this, [this]() { loadExampleFromJson(Examples::jsonLightingWireframe(), QStringLiteral("Lighting: Wireframe")); });
        lighting->addAction(QStringLiteral("Normals visualizer"), this, [this]() { loadExampleFromJson(Examples::jsonLightingNormals(), QStringLiteral("Lighting: Normals")); });
        lighting->addAction(QStringLiteral("Dynamic point light"), this, [this]() { loadExampleFromJson(Examples::jsonLightingDynamicPointLight(), QStringLiteral("Lighting: Dynamic point light")); });
        lighting->addAction(QStringLiteral("Animated orbit light"), this, [this]() { loadExampleFromJson(Examples::jsonLightingOrbitLight(), QStringLiteral("Lighting: Orbit light")); });
        lighting->addAction(QStringLiteral("Multi-light"), this, [this]() { loadExampleFromJson(Examples::jsonLightingMultiLight(), QStringLiteral("Lighting: Multi-light")); });

        QMenu *textures = examplesMenu->addMenu(QStringLiteral("Textures"));
        textures->addAction(QStringLiteral("Textured quad"), this, [this]() { loadExampleFromJson(Examples::jsonTexturesTexturedQuad(), QStringLiteral("Textures: Textured quad")); });
        textures->addAction(QStringLiteral("Distortion"), this, [this]() { loadExampleFromJson(Examples::jsonTexturesDistortion(), QStringLiteral("Textures: Distortion")); });
        textures->addAction(QStringLiteral("Normal mapping"), this, [this]() { loadExampleFromJson(Examples::jsonTexturesNormalMap(), QStringLiteral("Textures: Normal mapping")); });
        textures->addAction(QStringLiteral("Multi-texture blend"), this, [this]() { loadExampleFromJson(Examples::jsonTexturesMultiBlend(), QStringLiteral("Textures: Multi-texture blend")); });

        QMenu *postprocess = examplesMenu->addMenu(QStringLiteral("Post-processing"));
        postprocess->addAction(QStringLiteral("CRT / Scanlines"), this, [this]() { loadExampleFromJson(Examples::jsonPostprocessCrt(), QStringLiteral("Post-processing: CRT")); });
        postprocess->addAction(QStringLiteral("Pixelation"), this, [this]() { loadExampleFromJson(Examples::jsonPostprocessPixelation(), QStringLiteral("Post-processing: Pixelation")); });
        postprocess->addAction(QStringLiteral("Chromatic aberration"), this, [this]() { loadExampleFromJson(Examples::jsonPostprocessChromaticAberration(), QStringLiteral("Post-processing: Chromatic aberration")); });
        postprocess->addAction(QStringLiteral("Vignette"), this, [this]() { loadExampleFromJson(Examples::jsonPostprocessVignette(), QStringLiteral("Post-processing: Vignette")); });
        postprocess->addAction(QStringLiteral("Edge detection"), this, [this]() { loadExampleFromJson(Examples::jsonPostprocessEdgeDetection(), QStringLiteral("Post-processing: Edge detection")); });
        postprocess->addAction(QStringLiteral("Glitch"), this, [this]() { loadExampleFromJson(Examples::jsonPostprocessGlitch(), QStringLiteral("Post-processing: Glitch")); });

        QMenu *patterns = examplesMenu->addMenu(QStringLiteral("Patterns"));
        patterns->addAction(QStringLiteral("Mandelbrot fractal"), this, [this]() { loadExampleFromJson(Examples::jsonPatternsMandelbrot(), QStringLiteral("Patterns: Mandelbrot")); });
        patterns->addAction(QStringLiteral("Voronoi cells"), this, [this]() { loadExampleFromJson(Examples::jsonPatternsVoronoi(), QStringLiteral("Patterns: Voronoi")); });
        patterns->addAction(QStringLiteral("Truchet tiles"), this, [this]() { loadExampleFromJson(Examples::jsonPatternsTruchet(), QStringLiteral("Patterns: Truchet")); });
        patterns->addAction(QStringLiteral("SDF raymarching"), this, [this]() { loadExampleFromJson(Examples::jsonPatternsSdfRaymarch(), QStringLiteral("Patterns: SDF raymarch")); });
        patterns->addAction(QStringLiteral("Kaleidoscope"), this, [this]() { loadExampleFromJson(Examples::jsonPatternsKaleidoscope(), QStringLiteral("Patterns: Kaleidoscope")); });
        patterns->addAction(QStringLiteral("Moire patterns"), this, [this]() { loadExampleFromJson(Examples::jsonPatternsMoire(), QStringLiteral("Patterns: Moire")); });

        QMenu *multipass = examplesMenu->addMenu(QStringLiteral("Multipass"));
        multipass->addAction(QStringLiteral("Bloom"), this, [this]() { loadExampleFromJson(Examples::jsonMultipassBloom(), QStringLiteral("Multipass: Bloom")); });
        multipass->addAction(QStringLiteral("Deferred shading"), this, [this]() { loadExampleFromJson(Examples::jsonMultipassDeferred(), QStringLiteral("Multipass: Deferred")); });
        multipass->addAction(QStringLiteral("Shadow map"), this, [this]() { loadExampleFromJson(Examples::jsonMultipassShadowMap(), QStringLiteral("Multipass: Shadow map")); });
        multipass->addAction(QStringLiteral("Edge glow"), this, [this]() { loadExampleFromJson(Examples::jsonMultipassEdgeGlow(), QStringLiteral("Multipass: Edge glow")); });

        QMenu *blending = examplesMenu->addMenu(QStringLiteral("Blending"));
        blending->addAction(QStringLiteral("Additive glow"), this, [this]() { loadExampleFromJson(Examples::jsonBlendAdditiveGlow(), QStringLiteral("Blending: Additive glow")); });
        blending->addAction(QStringLiteral("Transparent layers"), this, [this]() { loadExampleFromJson(Examples::jsonBlendTransparentLayers(), QStringLiteral("Blending: Transparent layers")); });

        QMenu *compute = examplesMenu->addMenu(QStringLiteral("Compute"));
        compute->addAction(QStringLiteral("Game of Life"), this, [this]() { loadExampleFromJson(Examples::jsonComputeGameOfLife(), QStringLiteral("Compute: Game of Life")); });
        compute->addAction(QStringLiteral("Fluid simulation"), this, [this]() { loadExampleFromJson(Examples::jsonComputeFluidSim(), QStringLiteral("Compute: Fluid sim")); });
        compute->addAction(QStringLiteral("Image blur"), this, [this]() { loadExampleFromJson(Examples::jsonComputeImageBlur(), QStringLiteral("Compute: Image blur")); });

        QMenu *tessellation = examplesMenu->addMenu(QStringLiteral("Tessellation"));
        tessellation->addAction(QStringLiteral("Displacement mapping"), this, [this]() { loadExampleFromJson(Examples::jsonTessellationDisplacement(), QStringLiteral("Tessellation: Displacement")); });
        tessellation->addAction(QStringLiteral("PN-Triangles"), this, [this]() { loadExampleFromJson(Examples::jsonTessellationPnTriangles(), QStringLiteral("Tessellation: PN-Triangles")); });
        tessellation->addAction(QStringLiteral("Adaptive LOD"), this, [this]() { loadExampleFromJson(Examples::jsonTessellationAdaptiveLod(), QStringLiteral("Tessellation: Adaptive LOD")); });

        QMenu *subgroup = examplesMenu->addMenu(QStringLiteral("Subgroup / Wave"));
        subgroup->addAction(QStringLiteral("Wave reduction"), this, [this]() { loadExampleFromJson(Examples::jsonSubgroupWaveReduction(), QStringLiteral("Subgroup: Wave reduction")); });
        subgroup->addAction(QStringLiteral("Wave prefix scan"), this, [this]() { loadExampleFromJson(Examples::jsonSubgroupWavePrefixScan(), QStringLiteral("Subgroup: Wave prefix scan")); });

        QMenu *rt = examplesMenu->addMenu(QStringLiteral("Ray tracing"));
        rt->addAction(QStringLiteral("Minimal (compile test)"), this, &MainWindow::loadExampleRtMinimal);
        rt->addAction(QStringLiteral("Reflections"), this, [this]() { loadExampleFromJson(Examples::jsonRtReflections(), QStringLiteral("RT: Reflections")); });
        rt->addAction(QStringLiteral("Shadows"), this, [this]() { loadExampleFromJson(Examples::jsonRtShadows(), QStringLiteral("RT: Shadows")); });
        rt->addAction(QStringLiteral("Procedural geometry"), this, [this]() { loadExampleFromJson(Examples::jsonRtProceduralGeo(), QStringLiteral("RT: Procedural geo")); });
        rt->addAction(QStringLiteral("Callable materials"), this, [this]() { loadExampleFromJson(Examples::jsonRtCallableMaterials(), QStringLiteral("RT: Callable materials")); });
        rt->addAction(QStringLiteral("Any-hit transparency"), this, [this]() { loadExampleFromJson(Examples::jsonRtAnyhitTransparency(), QStringLiteral("RT: Any-hit transparency")); });
    }

    QMenu *exportMenu = menuBar()->addMenu(QStringLiteral("Export"));
    exportMenu->addAction(QStringLiteral("Record animation…"), this, &MainWindow::showExportDialog);
    exportMenu->addAction(QStringLiteral("Export as Bundle (.stproj)…"), this, &MainWindow::exportBundle);

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
    const int idx = m_pipelineKindCombo ? m_pipelineKindCombo->currentIndex() : 0;
    if (m_previewStack)
        m_previewStack->setCurrentIndex(idx);
    const bool showMesh = (idx == 0);
    if (m_meshNameLabel)
        m_meshNameLabel->setVisible(showMesh);
    if (m_meshStatusIcon)
        m_meshStatusIcon->setVisible(showMesh);
    if (auto *ml = findChild<QLabel *>(QStringLiteral("meshLabel")))
        ml->setVisible(showMesh);
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
    switch (m_pipelineKindCombo->currentIndex()) {
    case 1:  p.pipelineKind = QStringLiteral("raytrace"); break;
    case 2:  p.pipelineKind = QStringLiteral("compute"); break;
    default: p.pipelineKind = QStringLiteral("raster"); break;
    }
    p.meshPath = m_meshAbsolutePath;
    p.textures = m_loadedProject.textures;
    switch (m_blendCombo->currentIndex()) {
    case 1:  p.blend = QStringLiteral("alpha"); break;
    case 2:  p.blend = QStringLiteral("additive"); break;
    default: p.blend = QStringLiteral("off"); break;
    }
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
    int kindIdx = 0;
    if (p.pipelineKind == QStringLiteral("raytrace")) kindIdx = 1;
    else if (p.pipelineKind == QStringLiteral("compute")) kindIdx = 2;
    m_pipelineKindCombo->setCurrentIndex(kindIdx);
    int blendIdx = 0;
    if (p.blend == QStringLiteral("alpha")) blendIdx = 1;
    else if (p.blend == QStringLiteral("additive")) blendIdx = 2;
    m_blendCombo->setCurrentIndex(blendIdx);
    if (!p.meshPath.isEmpty()) {
        m_meshAbsolutePath = p.meshPath;
        m_meshNameLabel->setText(QFileInfo(p.meshPath).fileName());
    }
    updateMeshStatus();
    rebuildStageEditors(p);
    updatePreviewStackForPipelineKind();

    // Sync texture list widget
    if (m_textureList) {
        m_textureList->clear();
        for (int i = 0; i < p.textures.size(); ++i) {
            const QJsonObject o = p.textures[i].toObject();
            const QString path = o.value(QStringLiteral("path")).toString();
            m_textureList->addItem(QStringLiteral("[%1] %2").arg(i).arg(QFileInfo(path).fileName()));
        }
    }
}

bool MainWindow::openProjectFromPath(const QString &path, QString *errorOut)
{
    if (ShaderProjectBundle::isBundle(path)) {
        ShaderProjectBundle::cleanupTempDir(m_bundleTempDir);
        m_bundleTempDir.clear();

        auto result = ShaderProjectBundle::extractToTemp(path);
        if (!result.ok) {
            if (errorOut)
                *errorOut = result.errorMessage;
            return false;
        }
        m_bundleTempDir = result.tempDir;

        QString schemaErr;
        if (!ShaderProjectJson::validateAgainstSchema(result.projectJson, &schemaErr)) {
            if (errorOut) *errorOut = schemaErr;
            return false;
        }
        QString err;
        ShaderProject p = ShaderProject::fromJson(result.projectJson, &err);
        if (!err.isEmpty()) {
            if (errorOut) *errorOut = err;
            return false;
        }
        const QString sem = ShaderProject::validationMessage(p);
        if (!sem.isEmpty()) {
            if (errorOut) *errorOut = sem;
            return false;
        }
        applyShaderProject(p);
        statusBar()->showMessage(QStringLiteral("Opened bundle: %1").arg(QFileInfo(path).fileName()), 5000);
        return true;
    }

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

    if (!p.meshPath.isEmpty() && !QFileInfo(p.meshPath).isAbsolute())
        p.meshPath.clear();

    applyShaderProject(p);
    m_dirty = true;
    setWindowTitle(titleForDirty + QStringLiteral(" — ShaderTool"));
    tryCompileAndUpload(true);

    // Status bar hint for mesh-based examples without a mesh loaded
    if (p.meshPath.isEmpty()) {
        for (const ShaderStage &s : p.stages) {
            if (s.source.contains(QStringLiteral("POSITION")) && s.source.contains(QStringLiteral("NORMAL"))) {
                statusBar()->showMessage(
                    QStringLiteral("This example works best with a mesh — File → Open Mesh OBJ"), 8000);
                break;
            }
        }
    }
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
    const int kindIdx = m_pipelineKindCombo->currentIndex();
    QStringList kinds;
    if (kindIdx == 0) {
        kinds << QStringLiteral("vertex") << QStringLiteral("fragment") << QStringLiteral("geometry")
              << QStringLiteral("tessellation_control") << QStringLiteral("tessellation_evaluation");
    } else if (kindIdx == 2) {
        kinds << QStringLiteral("compute");
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
    static const QHash<QString, QString> defaultEntryCompute = {
        {QStringLiteral("compute"), QStringLiteral("CSMain")},
    };
    QString entryDef;
    if (kindIdx == 2)
        entryDef = defaultEntryCompute.value(kind, QStringLiteral("CSMain"));
    else if (kindIdx == 0)
        entryDef = defaultEntryRaster.value(kind, QStringLiteral("main"));
    else
        entryDef = defaultEntryRt.value(kind, QStringLiteral("main"));
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
        setCompileStatus(false);
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
            setCompileStatus(false);
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
    setCompileStatus(true);
    m_shaderNeedsMesh = false;
    updateMeshStatus();
    m_compileLog->setPlainText(QStringLiteral("Ray trace compile OK.\n") + log);
    if (m_rtPreviewWindow)
        m_rtPreviewWindow->setRayTracePayload(bins, doc.maxPipelineRayRecursionDepth);
    m_lastGoodSnapshot = snapshotKey();
    updateCompileUi();
}

void MainWindow::compileComputeProject(bool showErrors)
{
    StagePage *cs = findStagePage(QStringLiteral("compute"));
    if (!cs) {
        const QString msg = QStringLiteral("Compute pipeline requires a compute stage.");
        m_compileLog->setPlainText(msg);
        setCompileStatus(false);
        if (showErrors)
            QMessageBox::warning(this, QStringLiteral("Compile"), msg);
        return;
    }

    ShaderCompileResult csc = ShaderCompiler::compileHLSL(
        cs->source(), ShaderStageHelpers::dxcProfile(QStringLiteral("compute")), cs->entry());
    if (!csc.ok) {
        m_compileLog->setPlainText(csc.stderrText);
        setCompileStatus(false);
        if (showErrors)
            QMessageBox::warning(this, QStringLiteral("Compute shader"), csc.stderrText);
        return;
    }

    setCompileStatus(true);
    m_shaderNeedsMesh = false;
    updateMeshStatus();
    m_compileLog->setPlainText(QStringLiteral("Compute compile OK.\n") + csc.stderrText);

    ComputeStageBinary bin;
    bin.spirv = csc.spirv;
    bin.entry = cs->entry();
    if (m_computePreviewWindow)
        m_computePreviewWindow->setComputePayload(bin);
    m_lastGoodSnapshot = snapshotKey();
    updateCompileUi();
}

void MainWindow::setCompileStatus(bool ok)
{
    if (!m_compileStatusIcon)
        return;
    if (ok) {
        m_compileStatusIcon->setStyleSheet(
            QStringLiteral("background-color: #4caf50; border-radius: 9px;"));
        m_compileStatusIcon->setToolTip(QStringLiteral("Compile succeeded"));
    } else {
        m_compileStatusIcon->setStyleSheet(
            QStringLiteral("background-color: #f44336; border-radius: 9px;"));
        m_compileStatusIcon->setToolTip(QStringLiteral("Compile failed"));
    }
}

void MainWindow::updateMeshStatus()
{
    if (!m_meshStatusIcon)
        return;
    if (!m_shaderNeedsMesh) {
        m_meshStatusIcon->setStyleSheet(
            QStringLiteral("background-color: #9e9e9e; border-radius: 9px;"));
        m_meshStatusIcon->setToolTip(QStringLiteral("Mesh not required by shader"));
    } else if (m_meshAbsolutePath.isEmpty()) {
        m_meshStatusIcon->setStyleSheet(
            QStringLiteral("background-color: #f44336; border-radius: 9px;"));
        m_meshStatusIcon->setToolTip(QStringLiteral("No mesh loaded"));
    } else if (!QFileInfo::exists(m_meshAbsolutePath)) {
        m_meshStatusIcon->setStyleSheet(
            QStringLiteral("background-color: #f44336; border-radius: 9px;"));
        m_meshStatusIcon->setToolTip(QStringLiteral("Mesh file not found"));
    } else {
        m_meshStatusIcon->setStyleSheet(
            QStringLiteral("background-color: #4caf50; border-radius: 9px;"));
        m_meshStatusIcon->setToolTip(QStringLiteral("Mesh loaded"));
    }
}

void MainWindow::tryCompileAndUpload(bool showErrors)
{
    const ShaderProject doc = shaderProjectFromEditors();

    if (doc.pipelineKind == QStringLiteral("raytrace")) {
        compileRaytraceProject(showErrors);
        return;
    }

    if (doc.pipelineKind == QStringLiteral("compute")) {
        compileComputeProject(showErrors);
        return;
    }

    StagePage *vs = findStagePage(QStringLiteral("vertex"));
    StagePage *fs = findStagePage(QStringLiteral("fragment"));
    if (!vs || !fs) {
        const QString msg = QStringLiteral("Raster pipeline requires vertex and fragment stages.");
        m_compileLog->setPlainText(msg);
        setCompileStatus(false);
        if (showErrors)
            QMessageBox::warning(this, QStringLiteral("Compile"), msg);
        return;
    }

    ShaderCompileResult vsc = ShaderCompiler::compileHLSL(
        vs->source(), ShaderStageHelpers::dxcProfile(QStringLiteral("vertex")), vs->entry());
    if (!vsc.ok) {
        m_compileLog->setPlainText(vsc.stderrText);
        setCompileStatus(false);
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
            setCompileStatus(false);
            if (showErrors)
                QMessageBox::warning(this, QStringLiteral("Geometry shader"), gsc.stderrText);
            return;
        }
    }

    ShaderCompileResult tcsc;
    StagePage *tcs = findStagePage(QStringLiteral("tessellation_control"));
    if (tcs && !tcs->source().trimmed().isEmpty()) {
        tcsc = ShaderCompiler::compileHLSL(tcs->source(),
                                            ShaderStageHelpers::dxcProfile(QStringLiteral("tessellation_control")),
                                            tcs->entry());
        if (!tcsc.ok) {
            m_compileLog->setPlainText(tcsc.stderrText);
            setCompileStatus(false);
            if (showErrors)
                QMessageBox::warning(this, QStringLiteral("Tessellation control shader"), tcsc.stderrText);
            return;
        }
    }

    ShaderCompileResult tesc;
    StagePage *tes = findStagePage(QStringLiteral("tessellation_evaluation"));
    if (tes && !tes->source().trimmed().isEmpty()) {
        tesc = ShaderCompiler::compileHLSL(tes->source(),
                                            ShaderStageHelpers::dxcProfile(QStringLiteral("tessellation_evaluation")),
                                            tes->entry());
        if (!tesc.ok) {
            m_compileLog->setPlainText(tesc.stderrText);
            setCompileStatus(false);
            if (showErrors)
                QMessageBox::warning(this, QStringLiteral("Tessellation evaluation shader"), tesc.stderrText);
            return;
        }
    }

    ShaderCompileResult fsc = ShaderCompiler::compileHLSL(
        fs->source(), ShaderStageHelpers::dxcProfile(QStringLiteral("fragment")), fs->entry());
    if (!fsc.ok) {
        m_compileLog->setPlainText(fsc.stderrText);
        setCompileStatus(false);
        if (showErrors)
            QMessageBox::warning(this, QStringLiteral("Fragment shader"), fsc.stderrText);
        return;
    }

    setCompileStatus(true);
    m_shaderNeedsMesh = vs->source().contains(QStringLiteral("POSITION"))
                     && vs->source().contains(QStringLiteral("NORMAL"));
    updateMeshStatus();

    QString log = QStringLiteral("Compile OK.\n") + vsc.stderrText;
    if (gs && !gs->source().trimmed().isEmpty())
        log += gsc.stderrText;
    if (tcs && !tcs->source().trimmed().isEmpty())
        log += tcsc.stderrText;
    if (tes && !tes->source().trimmed().isEmpty())
        log += tesc.stderrText;
    log += fsc.stderrText;
    m_compileLog->setPlainText(log);

    QVector<RasterStageBinary> bins;
    RasterStageBinary vb{};
    vb.stage = VK_SHADER_STAGE_VERTEX_BIT;
    vb.spirv = vsc.spirv;
    vb.entry = vs->entry();
    bins.append(vb);
    if (tcs && !tcs->source().trimmed().isEmpty()) {
        RasterStageBinary tcb{};
        tcb.stage = VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT;
        tcb.spirv = tcsc.spirv;
        tcb.entry = tcs->entry();
        bins.append(tcb);
    }
    if (tes && !tes->source().trimmed().isEmpty()) {
        RasterStageBinary teb{};
        teb.stage = VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT;
        teb.spirv = tesc.spirv;
        teb.entry = tes->entry();
        bins.append(teb);
    }
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
        if (QFileInfo(doc.meshPath).isAbsolute()) {
            meshAbs = doc.meshPath;
        } else {
            meshAbs = PathUtils::safeResolveUnderProject(m_currentPath, doc.meshPath);
            if (meshAbs.isEmpty() && !m_currentPath.isEmpty())
                meshAbs = QFileInfo(m_currentPath).absolutePath() + QDir::separator() + doc.meshPath;
        }
        if (!QFileInfo::exists(meshAbs))
            meshAbs.clear();
    }

    RasterPreviewPipeline::BlendMode bm = RasterPreviewPipeline::BlendOff;
    if (doc.blend == QStringLiteral("alpha")) bm = RasterPreviewPipeline::BlendAlpha;
    else if (doc.blend == QStringLiteral("additive")) bm = RasterPreviewPipeline::BlendAdditive;
    m_previewWindow->setRasterPayload(bins, texImages, meshAbs, bm);
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
    ShaderProjectBundle::cleanupTempDir(m_bundleTempDir);
    m_bundleTempDir.clear();
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
                                                      QStringLiteral("Shader project (*.json *.stproj);;All files (*)"));
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
    if (!p.meshPath.isEmpty() && QFileInfo(p.meshPath).isAbsolute()) {
        const QDir projDir = QFileInfo(m_currentPath).absoluteDir();
        p.meshPath = projDir.relativeFilePath(p.meshPath);
    }
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

void MainWindow::addTexture()
{
    QString startDir;
    if (!m_currentPath.isEmpty())
        startDir = QFileInfo(m_currentPath).absolutePath();
    const QString path = QFileDialog::getOpenFileName(
        this, QStringLiteral("Add texture"), startDir,
        QStringLiteral("Images (*.png *.jpg *.jpeg *.bmp *.tga);;All files (*)"));
    if (path.isEmpty())
        return;

    const int slot = m_loadedProject.textures.size();
    QJsonObject entry;
    entry[QStringLiteral("path")] = path;
    entry[QStringLiteral("slot")] = slot;
    m_loadedProject.textures.append(entry);

    m_textureList->addItem(QStringLiteral("[%1] %2").arg(slot).arg(QFileInfo(path).fileName()));
    m_dirty = true;
    tryCompileAndUpload(false);
}

void MainWindow::removeTexture()
{
    const int row = m_textureList->currentRow();
    if (row < 0 || row >= m_loadedProject.textures.size())
        return;

    m_loadedProject.textures.removeAt(row);
    delete m_textureList->takeItem(row);

    // Renumber slots sequentially
    for (int i = 0; i < m_loadedProject.textures.size(); ++i) {
        QJsonObject o = m_loadedProject.textures[i].toObject();
        o[QStringLiteral("slot")] = i;
        m_loadedProject.textures[i] = o;
    }
    // Rebuild list display
    m_textureList->clear();
    for (int i = 0; i < m_loadedProject.textures.size(); ++i) {
        const QJsonObject o = m_loadedProject.textures[i].toObject();
        const QString p = o.value(QStringLiteral("path")).toString();
        m_textureList->addItem(QStringLiteral("[%1] %2").arg(i).arg(QFileInfo(p).fileName()));
    }

    m_dirty = true;
    tryCompileAndUpload(false);
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
    ShaderProjectBundle::cleanupTempDir(m_bundleTempDir);
    QMainWindow::closeEvent(e);
}

void MainWindow::exportBundle()
{
    const QString outPath = QFileDialog::getSaveFileName(
        this, QStringLiteral("Export as Bundle"),
        QString(),
        QStringLiteral("ShaderTool bundle (*.stproj)"));
    if (outPath.isEmpty())
        return;

    ShaderProject p = shaderProjectFromEditors();
    QByteArray projectJson = QJsonDocument(p.toJson()).toJson(QJsonDocument::Indented);

    QTemporaryDir tmpDir;
    if (!tmpDir.isValid()) {
        QMessageBox::warning(this, QStringLiteral("Export"), QStringLiteral("Failed to create temp directory."));
        return;
    }
    QString tmpProjectPath = tmpDir.path() + QStringLiteral("/project.json");
    {
        QFile f(tmpProjectPath);
        f.open(QIODevice::WriteOnly);
        f.write(projectJson);
    }

    QStringList assetPaths;
    if (!m_meshAbsolutePath.isEmpty() && QFileInfo::exists(m_meshAbsolutePath)) {
        assetPaths.append(m_meshAbsolutePath);
    } else if (!p.meshPath.isEmpty() && !m_currentPath.isEmpty()) {
        QString resolved = QFileInfo(m_currentPath).absolutePath() + QLatin1Char('/') + p.meshPath;
        if (QFileInfo::exists(resolved))
            assetPaths.append(resolved);
    }
    for (int i = 0; i < m_textureList->count(); ++i) {
        QString texPath = m_textureList->item(i)->text();
        if (QFileInfo::exists(texPath))
            assetPaths.append(texPath);
    }

    QString err;
    if (!ShaderProjectBundle::createBundle(outPath, tmpProjectPath, assetPaths, &err)) {
        QMessageBox::warning(this, QStringLiteral("Export"), err);
        return;
    }
    statusBar()->showMessage(QStringLiteral("Exported bundle: %1").arg(QFileInfo(outPath).fileName()), 5000);
}
