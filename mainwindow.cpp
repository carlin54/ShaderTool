#include "mainwindow.h"
#include "ui_mainwindow.h"

#define GLSL(version, shader)  "#version " #version "\n" #shader

Assimp::Importer MainWindow::importer;

//
void GetBoundingBox(const aiMesh* mesh, aiVector3D* min, aiVector3D* max)
{
    min->x = min->y = min->z =  1e10f;
    max->x = max->y = max->z = -1e10f;

    for (unsigned int t = 0; t < mesh->mNumVertices; ++t){
        aiVector3D tmp = mesh->mVertices[t];

        min->x = std::min(min->x,tmp.x);
        min->y = std::min(min->y,tmp.y);
        min->z = std::min(min->z,tmp.z);

        max->x = std::max(max->x,tmp.x);
        max->y = std::max(max->y,tmp.y);
        max->z = std::max(max->z,tmp.z);
    }

}
void GetBoundingBoxForNode (const aiScene* scene, const aiNode* nd, aiVector3D* min, aiVector3D* max)
{
    unsigned int n = 0, t;

    for (; n < nd->mNumMeshes; ++n) {
        const aiMesh* mesh = scene->mMeshes[nd->mMeshes[n]];
        for (t = 0; t < mesh->mNumVertices; ++t){
            aiVector3D tmp = mesh->mVertices[t];

            min->x = std::min(min->x,tmp.x);
            min->y = std::min(min->y,tmp.y);
            min->z = std::min(min->z,tmp.z);

            max->x = std::max(max->x,tmp.x);
            max->y = std::max(max->y,tmp.y);
            max->z = std::max(max->z,tmp.z);
        }
    }

    for (n = 0; n < nd->mNumChildren; ++n) {
        GetBoundingBoxForNode(scene, nd->mChildren[n],min,max);
    }
}
void GetBoundingBox (const aiScene* scene, aiVector3D* min, aiVector3D* max)
{
    min->x = min->y = min->z =  1e10f;
    max->x = max->y = max->z = -1e10f;
    GetBoundingBoxForNode(scene, scene->mRootNode, min, max);
}
void BufferIndexedVerts(Mesh& meshdata)
{

    meshdata.Clear();

    //
    //Q_ASSERT(meshdata.mVao.create());
    //meshdata.mVao.bind();

    const aiMesh* aimesh = meshdata.mScene->mMeshes[0];
    unsigned int numFaces = aimesh->mNumFaces;

    unsigned int *faceArray;
    faceArray = (unsigned int *)malloc(sizeof(unsigned int) * numFaces * 3);
    unsigned int faceIndex = 0;

    for (unsigned int t = 0; t < numFaces; ++t){
        const aiFace* face = &aimesh->mFaces[t];
        memcpy(&faceArray[faceIndex], face->mIndices, 3 * sizeof(unsigned int));
        faceIndex += 3;
    }

    //Buffer indices
    meshdata.mVboIndexBuffer.create();
    meshdata.mVboIndexBuffer.setUsagePattern(QOpenGLBuffer::StaticDraw);
    meshdata.mVboIndexBuffer.bind();
    meshdata.mVboIndexBuffer.allocate(faceArray, sizeof(unsigned int) * numFaces * 3);
    free(faceArray);

    //glGenBuffers(1, &meshdata.mIndexBuffer);
    //glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, meshdata.mIndexBuffer);
    //glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(unsigned int) * numFaces * 3, faceArray, GL_STATIC_DRAW);
    //free(faceArray);

    //Buffer vertices
    if (aimesh->HasPositions()) {
        meshdata.mVboVerts.create();
        meshdata.mVboVerts.setUsagePattern(QOpenGLBuffer::StaticDraw);
        meshdata.mVboVerts.bind();
        meshdata.mVboVerts.allocate(aimesh->mVertices, sizeof(float) * 3 * aimesh->mNumVertices);

        //glGenBuffers(1, &meshdata.mVboVerts);
        //glBindBuffer(GL_ARRAY_BUFFER, meshdata.mVboVerts);
        //glBufferData(GL_ARRAY_BUFFER, sizeof(float) * 3 * mesh->mNumVertices, aimesh->mVertices, GL_STATIC_DRAW);
        //glEnableVertexAttribArray(pos_loc);
        //glVertexAttribPointer(pos_loc, 3, GL_FLOAT, 0, 0, 0);
    }

    // buffer for vertex texture coordinates
    if (aimesh->HasTextureCoords(0)) {
        float *texCoords = (float *)malloc(sizeof(float) * 2 * aimesh->mNumVertices);
        for (unsigned int k = 0; k < aimesh->mNumVertices; ++k) {
            texCoords[k*2]   = aimesh->mTextureCoords[0][k].x;
            texCoords[k*2+1] = aimesh->mTextureCoords[0][k].y;
        }

        meshdata.mVboTexCoords.create();
        meshdata.mVboTexCoords.setUsagePattern(QOpenGLBuffer::StaticDraw);
        meshdata.mVboTexCoords.bind();
        meshdata.mVboTexCoords.allocate(texCoords, sizeof(float) * 2 * aimesh->mNumVertices);

        //glGenBuffers(1, &meshdata.mVboTexCoords);
        //glBindBuffer(GL_ARRAY_BUFFER, meshdata.mVboTexCoords);
        //glBufferData(GL_ARRAY_BUFFER, sizeof(float) * 2 * aimesh->mNumVertices, texCoords, GL_STATIC_DRAW);
        //glEnableVertexAttribArray(tex_coord_loc);
        //glVertexAttribPointer(tex_coord_loc, 2, GL_FLOAT, 0, 0, 0);
        free(texCoords);
    }

    if (aimesh->HasNormals()) {
        float *normals = (float *)malloc(sizeof(float) * 3 * aimesh->mNumVertices);
        for (unsigned int k = 0; k < aimesh->mNumVertices; ++k) {
            normals[k*3]   = aimesh->mNormals[k].x;
            normals[k*3+1] = aimesh->mNormals[k].y;
            normals[k*3+2] = aimesh->mNormals[k].z;
        }

        meshdata.mVboNormals.create();
        meshdata.mVboNormals.setUsagePattern(QOpenGLBuffer::StaticDraw);
        meshdata.mVboNormals.bind();
        meshdata.mVboNormals.allocate(normals, sizeof(float) * 3 * aimesh->mNumVertices);

        //glGenBuffers(1, &meshdata.mVboNormals);
        //glBindBuffer(GL_ARRAY_BUFFER, meshdata.mVboNormals);
        //glBufferData(GL_ARRAY_BUFFER, sizeof(float) * 3 * aimesh->mNumVertices, normals, GL_STATIC_DRAW);
        //glEnableVertexAttribArray(normal_loc);
        //glVertexAttribPointer(normal_loc, 3, GL_FLOAT, 0, 0, 0);
        free(normals);
    }

    Q_ASSERT(meshdata.mVao.create());

    //meshdata.mVao.release();

    //glBindVertexArray(0);
    //glBindBuffer(GL_ARRAY_BUFFER,0);
    //glBindBuffer(GL_ELEMENT_ARRAY_BUFFER,0);
}

MainWindow::MainWindow(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::MainWindow)
{
    ui->setupUi(this);
    setupEditor();
    handleRefresh();
}
MainWindow::~MainWindow()
{
    delete ui;
}
void MainWindow::readSettings(){
    QSettings settings(QCoreApplication::organizationName(), QCoreApplication::applicationName());
    const QByteArray geometry = settings.value("geometry", QByteArray()).toByteArray();
    if (geometry.isEmpty()) {
        const QRect availableGeometry = QApplication::desktop()->availableGeometry(this);
        resize(availableGeometry.width() / 3, availableGeometry.height() / 2);
        move((availableGeometry.width() - width()) / 2,
             (availableGeometry.height() - height()) / 2);
    } else {
        restoreGeometry(geometry);
    }
}
void MainWindow::setupEditor(){
    m_pHighlighter = new GLSLHighlighter(ui->pteShaderCode->document());

    m_iE = 0;
    m_iM = 0;
    m_iT = 0;

    //
    connect(ui->btnMeshNext, &QPushButton::pressed, this, &MainWindow::handleMeshNext);
    connect(ui->btnMeshNext, &QPushButton::released, this, &MainWindow::handleRefresh);

    connect(ui->btnMeshDelete, &QPushButton::pressed, this, &MainWindow::handleMeshDelete);
    connect(ui->btnMeshDelete, &QPushButton::released, this, &MainWindow::handleRefresh);

    connect(ui->btnMeshPrevious, &QPushButton::pressed, this, &MainWindow::handleMeshPrevious);
    connect(ui->btnMeshPrevious, &QPushButton::released, this, &MainWindow::handleRefresh);

    //
    connect(ui->btnTextureNext, &QPushButton::pressed, this, &MainWindow::handleTextureNext);
    connect(ui->btnTextureNext, &QPushButton::released, this, &MainWindow::handleRefresh);

    connect(ui->btnTextureDelete, &QPushButton::pressed, this, &MainWindow::handleTextureDelete);
    connect(ui->btnTextureDelete, &QPushButton::released, this, &MainWindow::handleRefresh);

    connect(ui->btnTexturePrevious, &QPushButton::pressed, this, &MainWindow::handleTexturePrevious);
    connect(ui->btnTexturePrevious, &QPushButton::released, this, &MainWindow::handleRefresh);


    //
    connect(ui->btnEditorNext, &QPushButton::pressed, this, &MainWindow::handleEditorNext);
    connect(ui->btnEditorNext, &QPushButton::released, this, &MainWindow::handleRefresh);

    connect(ui->btnEditorDelete, &QPushButton::pressed, this, &MainWindow::handleEditorDelete);
    connect(ui->btnEditorDelete, &QPushButton::released, this, &MainWindow::handleRefresh);

    connect(ui->btnEditorPrevious, &QPushButton::pressed, this, &MainWindow::handleEditorPrevious);
    connect(ui->btnEditorPrevious, &QPushButton::released, this, &MainWindow::handleRefresh);

    //
    connect(ui->cbVertex, &QCheckBox::clicked, this, &MainWindow::handleVertexToggle);
    // connect(ui->cbVertex, &QCheckBox::released, this, &MainWindow::handleRefresh);

    connect(ui->cbEvaluation, &QCheckBox::clicked, this, &MainWindow::handleEvaluationToggle);
    // connect(ui->cbEvaluation, &QCheckBox::released, this, &MainWindow::handleRefresh);

    connect(ui->cbControl, &QCheckBox::clicked, this, &MainWindow::handleControlToggle);
    // connect(ui->cbControl, &QCheckBox::released, this, &MainWindow::handleRefresh);

    connect(ui->cbGeometry, &QCheckBox::clicked, this, &MainWindow::handleGeometryToggle);
    // connect(ui->cbGeometry, &QCheckBox::released, this, &MainWindow::handleRefresh);

    connect(ui->cbFragment, &QCheckBox::clicked, this, &MainWindow::handleFragmentToggle);
    // connect(ui->cbFragment, &QCheckBox::released, this, &MainWindow::handleRefresh);



    connect(ui->btnShaderNext, &QPushButton::released, this, &MainWindow::handleShaderNext);
    connect(ui->btnShaderNext, &QPushButton::released, this, &MainWindow::handleRefresh);

    connect(ui->btnShaderPrevious, &QPushButton::released, this, &MainWindow::handleShaderPrevious);
    connect(ui->btnShaderPrevious, &QPushButton::released, this, &MainWindow::handleRefresh);

    connect(ui->btnCompile, &QPushButton::released, this, &MainWindow::handleCompile);
    connect(ui->btnCompile, &QPushButton::released, this, &MainWindow::handleRefresh);

    // connect(ui->sbX, &QDoubleSpinBox::editingFinished, this, &MainWindow::handleMeshRotatingSetX);
    // connect(ui->sbX, &QDoubleSpinBox::editingFinished, this, &MainWindow::handleRefresh);

    // connect(ui->sbY, &QDoubleSpinBox::editingFinished, this, &MainWindow::handleMeshRotatingSetY);
    // connect(ui->sbY, &QDoubleSpinBox::editingFinished, this, &MainWindow::handleRefresh);

    // connect(ui->sbZ, &QDoubleSpinBox::editingFinished, this, &MainWindow::handleMeshRotatingSetZ);
    // connect(ui->sbZ, &QDoubleSpinBox::editingFinished, this, &MainWindow::handleRefresh);

    connect(ui->dsbMeshTranslationX, &QDoubleSpinBox::editingFinished, this, &MainWindow::handleMeshTranslateSetX);
    connect(ui->dsbMeshTranslationX, &QDoubleSpinBox::editingFinished, this, &MainWindow::handleRefresh);

    connect(ui->dsbMeshTranslationY, &QDoubleSpinBox::editingFinished, this, &MainWindow::handleMeshTranslateSetY);
    connect(ui->dsbMeshTranslationY, &QDoubleSpinBox::editingFinished, this, &MainWindow::handleRefresh);

    connect(ui->dsbMeshTranslationZ, &QDoubleSpinBox::editingFinished, this, &MainWindow::handleMeshTranslateSetZ);
    connect(ui->dsbMeshTranslationZ, &QDoubleSpinBox::editingFinished, this, &MainWindow::handleRefresh);


    connect(ui->dsbMeshRotateX, &QDoubleSpinBox::editingFinished, this, &MainWindow::handleMeshRotateSetX);
    connect(ui->dsbMeshRotateX, &QDoubleSpinBox::editingFinished, this, &MainWindow::handleRefresh);

    connect(ui->dsbMeshRotateY, &QDoubleSpinBox::editingFinished, this, &MainWindow::handleMeshRotateSetY);
    connect(ui->dsbMeshRotateY, &QDoubleSpinBox::editingFinished, this, &MainWindow::handleRefresh);

    connect(ui->dsbMeshRotateZ, &QDoubleSpinBox::editingFinished, this, &MainWindow::handleMeshRotateSetZ);
    connect(ui->dsbMeshRotateZ, &QDoubleSpinBox::editingFinished, this, &MainWindow::handleRefresh);


    connect(ui->dsbMeshScaleX, &QDoubleSpinBox::editingFinished, this, &MainWindow::handleMeshScaleSetX);
    connect(ui->dsbMeshScaleX, &QDoubleSpinBox::editingFinished, this, &MainWindow::handleRefresh);

    connect(ui->dsbMeshScaleY, &QDoubleSpinBox::editingFinished, this, &MainWindow::handleMeshScaleSetY);
    connect(ui->dsbMeshScaleY, &QDoubleSpinBox::editingFinished, this, &MainWindow::handleRefresh);

    connect(ui->dsbMeshScaleZ, &QDoubleSpinBox::editingFinished, this, &MainWindow::handleMeshScaleSetZ);
    connect(ui->dsbMeshScaleZ, &QDoubleSpinBox::editingFinished, this, &MainWindow::handleRefresh);

    ui->shaderframe->setTranslateDSB(ui->dsbMeshTranslationX,ui->dsbMeshTranslationY,ui->dsbMeshTranslationZ);
    ui->shaderframe->setScaleDSB(ui->dsbMeshScaleX,ui->dsbMeshScaleY,ui->dsbMeshScaleZ);
    ui->shaderframe->setRotateDSB(ui->dsbMeshRotateX,ui->dsbMeshRotateY,ui->dsbMeshRotateZ);


    /*Shaders*/{
        connect(ui->action_Shader_Empty, &QAction::triggered, this, &MainWindow::handleAddShaderEmpty);
        connect(ui->action_Shader_Empty, &QAction::triggered, this, &MainWindow::handleRefresh);
        connect(ui->action_Shader_Empty, &QAction::triggered, this, &MainWindow::setNewestEditor);

        connect(ui->action_Shader_Lighting_Plain, &QAction::triggered, this, &MainWindow::handleAddShaderPlain);
        connect(ui->action_Shader_Lighting_Plain, &QAction::triggered, this, &MainWindow::handleRefresh);
        connect(ui->action_Shader_Lighting_Plain, &QAction::triggered, this, &MainWindow::setNewestEditor);

        connect(ui->action_Shader_Lighting_Phong, &QAction::triggered, this, &MainWindow::handleAddShaderPhong);
        connect(ui->action_Shader_Lighting_Phong, &QAction::triggered, this, &MainWindow::handleRefresh);
        connect(ui->action_Shader_Lighting_Phong, &QAction::triggered, this, &MainWindow::setNewestEditor);

        connect(ui->action_Shader_Lighting_Bling_Phong, &QAction::triggered, this, &MainWindow::handleAddShaderBlingPhong);
        connect(ui->action_Shader_Lighting_Bling_Phong, &QAction::triggered, this, &MainWindow::handleRefresh);
        connect(ui->action_Shader_Lighting_Bling_Phong, &QAction::triggered, this, &MainWindow::setNewestEditor);

        connect(ui->action_Shader_Lighting_Cook_Torrance, &QAction::triggered, this, &MainWindow::handleAddShaderCookTorr);
        connect(ui->action_Shader_Lighting_Cook_Torrance, &QAction::triggered, this, &MainWindow::handleRefresh);
        connect(ui->action_Shader_Lighting_Cook_Torrance, &QAction::triggered, this, &MainWindow::setNewestEditor);

        connect(ui->actionToon, &QAction::triggered, this, &MainWindow::handleAddShaderToon);
        connect(ui->actionToon, &QAction::triggered, this, &MainWindow::handleRefresh);
        connect(ui->actionToon, &QAction::triggered, this, &MainWindow::setNewestEditor);
    }

    /*Mesh*/{
        connect(ui->action_Mesh_Load_File, &QAction::triggered, this, &MainWindow::handleAddMeshLoadFile);
        connect(ui->action_Mesh_Load_File, &QAction::triggered, this, &MainWindow::handleRefresh);
        connect(ui->action_Mesh_Load_File, &QAction::triggered, this, &MainWindow::setNewestMesh);

        connect(ui->action_Mesh_Shape2D_Plane, &QAction::triggered, this, &MainWindow::handleAddMesh2DPlane);
        connect(ui->action_Mesh_Shape2D_Plane, &QAction::triggered, this, &MainWindow::handleRefresh);
        connect(ui->action_Mesh_Shape2D_Plane, &QAction::triggered, this, &MainWindow::setNewestMesh);

        connect(ui->action_Mesh_Shape2D_Triangle, &QAction::triggered, this, &MainWindow::handleAddMeshTriangle);
        connect(ui->action_Mesh_Shape2D_Triangle, &QAction::triggered, this, &MainWindow::handleRefresh);
        connect(ui->action_Mesh_Shape2D_Triangle, &QAction::triggered, this, &MainWindow::setNewestMesh);

        connect(ui->action_Mesh_Shape3D_Cube, &QAction::triggered, this, &MainWindow::handleAddMesh3DCube);
        connect(ui->action_Mesh_Shape3D_Cube, &QAction::triggered, this, &MainWindow::handleRefresh);
        connect(ui->action_Mesh_Shape3D_Cube, &QAction::triggered, this, &MainWindow::setNewestMesh);

        connect(ui->action_Mesh_Shape3D_Icohedron, &QAction::triggered, this, &MainWindow::handleAddMesh3DIcosahedron);
        connect(ui->action_Mesh_Shape3D_Icohedron, &QAction::triggered, this, &MainWindow::handleRefresh);
        connect(ui->action_Mesh_Shape3D_Icohedron, &QAction::triggered, this, &MainWindow::setNewestMesh);

    }

    /*Settings*/{
        connect(ui->action_Settings_Uniforms_Reset, &QAction::triggered, this, &MainWindow::handleSettingsUniformsReset);
        connect(ui->action_Settings_Uniforms_Reset, &QAction::triggered, this, &MainWindow::handleRefresh);

        connect(ui->action_Settings_Camera_Reset, &QAction::triggered, this, &MainWindow::handleSettingsCameraReset);
        connect(ui->action_Settings_Camera_Reset, &QAction::triggered, this, &MainWindow::handleRefresh);
    }
}

ShaderEditor* MainWindow::getEditor(){
    return m_Editors[m_iE];
}
Mesh* MainWindow::getMesh(){
    return m_Meshes[m_iM];
}
QOpenGLTexture* MainWindow::getTexture(){
    return m_Textures[m_iT];
}

//Ui
void MainWindow::handleRefresh(){

    if(hasEditor()){
        ui->lblProgram->setText(QString("Program(s) ") + QString().setNum(m_iE+1) + " of " + QString().setNum(m_Editors.size()));

        ui->shaderframe->setEditor(getEditor());

        ui->btnEditorDelete->setEnabled(true);

        ui->cbVertex->setEnabled(true);
        ui->cbEvaluation->setEnabled(true);
        ui->cbControl->setEnabled(true);
        ui->cbGeometry->setEnabled(true);
        ui->cbFragment->setEnabled(true);

        ui->cbVertex->setChecked(getEditor()->hasVertex());
        ui->cbEvaluation->setChecked(getEditor()->hasEvaluation());
        ui->cbControl->setChecked(getEditor()->hasControl());
        ui->cbGeometry->setChecked(getEditor()->hasGeometry());
        ui->cbFragment->setChecked(getEditor()->hasFragment());

        ui->sbPatches->setEnabled(getEditor()->willRequirePatches());
        if(getEditor()->hasProgram()){
            getEditor()->setPatches(ui->sbPatches->value());
        }

        if(hasManyEditors()){
            ui->btnEditorNext->setEnabled(true);
            ui->btnEditorPrevious->setEnabled(true);
        }else{
            ui->btnEditorNext->setEnabled(false);
            ui->btnEditorPrevious->setEnabled(false);
        }


        if(getEditor()->numShaders() > 0){
            ui->pteShaderCode->setEnabled(true);
            ui->pteShaderCode->setPlainText(getEditor()->getCurrentShader());
        }else{
            ui->pteShaderCode->setEnabled(false);
            ui->pteShaderCode->setPlainText(QString(""));
        }
        ui->lblShader->setText(getEditor()->currentShaderTypeName());

        if(getEditor()->numShaders() > 1){
            ui->btnShaderNext->setEnabled(true);
            ui->btnShaderPrevious->setEnabled(true);
        }else{
            ui->btnShaderNext->setEnabled(false);
            ui->btnShaderPrevious->setEnabled(false);
        }

        if(getEditor()->hasVertex() && getEditor()->hasFragment()){
            ui->btnCompile->setEnabled(true);
        }else{
            ui->btnCompile->setEnabled(false);
        }

    }else{
        ui->lblProgram->setText("Program");
        ui->pteShaderCode->setPlainText("");

        ui->shaderframe->setEditor(nullptr);
        ui->btnEditorDelete->setEnabled(false);
        ui->pteShaderCode->setEnabled(false);

        ui->cbVertex->setEnabled(false);
        ui->cbEvaluation->setEnabled(false);
        ui->cbControl->setEnabled(false);
        ui->cbGeometry->setEnabled(false);
        ui->cbFragment->setEnabled(false);

        ui->cbVertex->setChecked(false);
        ui->cbEvaluation->setChecked(false);
        ui->cbControl->setChecked(false);
        ui->cbGeometry->setChecked(false);
        ui->cbFragment->setChecked(false);

        ui->sbPatches->setEnabled(false);

        ui->btnEditorNext->setEnabled(false);
        ui->btnEditorPrevious->setEnabled(false);

        ui->btnShaderNext->setEnabled(false);
        ui->btnShaderPrevious->setEnabled(false);

        ui->btnCompile->setEnabled(false);
        ui->lblShader->setText("Shader");
    }


    //
    QFont font_positions = ui->lblMeshPositions->font();
    QFont font_normals = ui->lblMeshNormals->font();
    QFont font_colors = ui->lblMeshColors->font();
    QFont font_texture_coords = ui->lblMeshTextureCoords->font();
    ui->dsbMeshTranslationX->setEnabled(false);
    ui->dsbMeshTranslationY->setEnabled(false);
    ui->dsbMeshTranslationZ->setEnabled(false);
    ui->dsbMeshRotateX->setEnabled(false);
    ui->dsbMeshRotateY->setEnabled(false);
    ui->dsbMeshRotateZ->setEnabled(false);
    ui->dsbMeshScaleX->setEnabled(false);
    ui->dsbMeshScaleY->setEnabled(false);
    ui->dsbMeshScaleZ->setEnabled(false);

    if(hasMesh()){
        ui->lblMesh->setText(QString("Mesh(s) ") + QString().setNum(m_iM+1) + " of " + QString().setNum(m_Meshes.size()));
        //ui->sbX->setEnabled(true);
        //ui->sbY->setEnabled(true);
        //ui->sbZ->setEnabled(true);
        ui->btnMeshDelete->setEnabled(true);
        ui->shaderframe->setMesh(m_Meshes[m_iM]);
        if(hasManyMeshes()){
            ui->btnMeshNext->setEnabled(true);
            ui->btnMeshPrevious->setEnabled(true);
        }else{
            ui->btnMeshNext->setEnabled(false);
            ui->btnMeshPrevious->setEnabled(false);
        }

        ui->dsbMeshTranslationX->setEnabled(true);
        ui->dsbMeshTranslationY->setEnabled(true);
        ui->dsbMeshTranslationZ->setEnabled(true);
        ui->dsbMeshRotateX->setEnabled(true);
        ui->dsbMeshRotateY->setEnabled(true);
        ui->dsbMeshRotateZ->setEnabled(true);
        ui->dsbMeshScaleX->setEnabled(true);
        ui->dsbMeshScaleY->setEnabled(true);
        ui->dsbMeshScaleZ->setEnabled(true);

        ui->dsbMeshTranslationX->setValue(m_Meshes[m_iM]->mTranslation.x());
        ui->dsbMeshTranslationY->setValue(m_Meshes[m_iM]->mTranslation.y());
        ui->dsbMeshTranslationZ->setValue(m_Meshes[m_iM]->mTranslation.z());

        ui->dsbMeshRotateX->setValue((int)m_Meshes[m_iM]->mRotation.x() % 360);
        ui->dsbMeshRotateY->setValue((int)m_Meshes[m_iM]->mRotation.y() % 360);
        ui->dsbMeshRotateZ->setValue((int)m_Meshes[m_iM]->mRotation.z() % 360);

        ui->dsbMeshScaleX->setValue(m_Meshes[m_iM]->mScale.x());
        ui->dsbMeshScaleY->setValue(m_Meshes[m_iM]->mScale.y());
        ui->dsbMeshScaleZ->setValue(m_Meshes[m_iM]->mScale.z());

        font_positions.setStrikeOut(!m_Meshes[m_iM]->mVboVerts.isCreated());
        font_normals.setStrikeOut(!m_Meshes[m_iM]->mVboNormals.isCreated());
        font_colors.setStrikeOut(!m_Meshes[m_iM]->mVboColors.isCreated());
        font_texture_coords.setStrikeOut(!m_Meshes[m_iM]->mVboTexCoords.isCreated());


    }else{
        ui->lblMesh->setText("Mesh");

        ui->shaderframe->setMesh(nullptr);
        ui->btnMeshDelete->setEnabled(false);
        ui->btnMeshNext->setEnabled(false);
        ui->btnMeshPrevious->setEnabled(false);

        ui->dsbMeshTranslationX->setValue(0.0f);
        ui->dsbMeshTranslationY->setValue(0.0f);
        ui->dsbMeshTranslationZ->setValue(0.0f);
        ui->dsbMeshRotateX->setValue(0.0f);
        ui->dsbMeshRotateY->setValue(0.0f);
        ui->dsbMeshRotateZ->setValue(0.0f);
        ui->dsbMeshScaleX->setValue(1.0f);
        ui->dsbMeshScaleY->setValue(1.0f);
        ui->dsbMeshScaleZ->setValue(1.0f);

        font_positions.setStrikeOut(false);
        font_normals.setStrikeOut(false);
        font_colors.setStrikeOut(false);
        font_texture_coords.setStrikeOut(false);
    }

    ui->lblMeshPositions->setFont(font_positions);
    ui->lblMeshNormals->setFont(font_normals);
    ui->lblMeshColors->setFont(font_colors);
    ui->lblMeshTextureCoords->setFont(font_texture_coords);



    //
    if(hasTexture()){
        ui->lblMesh->setText(QString("Texture(s) ") + QString().setNum(m_iT+1) + " of " + QString().setNum(m_Textures.size()));
        ui->shaderframe->setTexture(m_Textures[m_iT]);

        ui->btnTextureDelete->setEnabled(true);
        ui->shaderframe->setTexture(m_Textures[m_iT]);
        if(hasManyTextures()){
            ui->btnTextureNext->setEnabled(true);
            ui->btnTexturePrevious->setEnabled(true);
        }else{
            ui->btnTextureNext->setEnabled(false);
            ui->btnTexturePrevious->setEnabled(false);
        }
    }else{
        ui->shaderframe->setTexture(nullptr);
        ui->btnTextureDelete->setEnabled(false);
        ui->btnTextureNext->setEnabled(false);
        ui->btnTexturePrevious->setEnabled(false);
    }
}

// Shader
bool MainWindow::hasEditor(){
    return !(m_Editors.isEmpty());
}
bool MainWindow::hasManyEditors(){
    return m_Editors.size() > 1;
}
void MainWindow::handleEditorNext()             {
    if(!m_Editors.isEmpty()){
        getEditor()->setCurrentShader(ui->pteShaderCode->document()->toPlainText());
        m_iE++;
        if(m_iE > m_Editors.size()-1){
            m_iE = 0;
        }
    }else{
        m_iE = 0;
    }
}
void MainWindow::handleEditorDelete()           {
    assert(hasEditor());
    delete getEditor();
    m_Editors.erase(m_Editors.begin()+m_iE);

    m_iE--;
    if(m_iE < 0){
        m_iE = 0;
    }

    if(hasEditor()){
        ui->shaderframe->setEditor(getEditor());
    }else{
        ui->shaderframe->setEditor(nullptr);
    }


}
void MainWindow::handleEditorPrevious()         {

    if(!m_Editors.isEmpty()){
        getEditor()->setCurrentShader(ui->pteShaderCode->document()->toPlainText());
        m_iE--;
        if(m_iE < 0){
            m_iE = m_Editors.size()-1;
        }
    }else{
        m_iE = 0;
    }

}
void MainWindow::setNewestEditor(){
    if(hasEditor()) m_iE = m_Editors.size()-1;
    else m_iE = 0;
}
void MainWindow::handleShaderNext()
{
    getEditor()->setCurrentShader(ui->pteShaderCode->document()->toPlainText());
    getEditor()->next();
}
void MainWindow::handleShaderPrevious()
{
    getEditor()->setCurrentShader(ui->pteShaderCode->document()->toPlainText());
    getEditor()->previous();
}
void MainWindow::handleVertexToggle(bool set)       {assert(hasEditor()); getEditor()->setShader(QOpenGLShader::Vertex, set);                   handleRefresh();}
void MainWindow::handleControlToggle(bool set)      {assert(hasEditor()); getEditor()->setShader(QOpenGLShader::TessellationControl, set);      handleRefresh();}
void MainWindow::handleEvaluationToggle(bool set)   {assert(hasEditor()); getEditor()->setShader(QOpenGLShader::TessellationEvaluation, set);   handleRefresh();}
void MainWindow::handleGeometryToggle(bool set)     {assert(hasEditor()); getEditor()->setShader(QOpenGLShader::Geometry, set);                 handleRefresh();}
void MainWindow::handleFragmentToggle(bool set)     {assert(hasEditor()); getEditor()->setShader(QOpenGLShader::Fragment, set);                 handleRefresh();}
void MainWindow::handleCompile(){
    try{
        getEditor()->setCurrentShader(ui->pteShaderCode->document()->toPlainText());
        getEditor()->compile();
        std::cout << "SUCCESS!!!" << std::endl;
        ui->shaderframe->setEditor(getEditor());
    }catch(QString error){
         std::cerr << error.toStdString() << std::endl;
    }


}
//
void MainWindow::handleAddShaderEmpty(){
    ShaderEditor* shader_empty = new ShaderEditor("","","","","");
    m_Editors.push_back(shader_empty);
    m_iE = m_Editors.size()-1;
}
void MainWindow::handleAddShaderPlain(){
    QString vert = GLSL(130,
        \n
        \nin vec3 position;
        \nuniform mat4 PVM;
        \n
        \nvoid main(){
        \n\tgl_Position = PVM*vec4(position, 1.0);
        \n}
    );
    QString frag = GLSL(130,
        \n
        \nout vec4 fragcolor;
        \n
        \nvoid main(){
        \n\tfragcolor = vec4(1.0, 0.0, 0.0, 1.0);
        \n}
    );

    ShaderEditor* shader_plain = new ShaderEditor(vert,"","","",frag);
    m_Editors.push_back(shader_plain);
    m_iE = m_Editors.size()-1;
}
void MainWindow::handleAddShaderPhong(){
    QString vert = GLSL(130,
        \n
        \nuniform mat4 P;
        \nuniform mat4 V;
        \nuniform mat4 M;
        \n
        \nin vec3 position;
        \nin vec3 normal;
        \n
        \nout vec3 v_n;
        \nout vec4 v_p;
        \n
        \nvoid main(void){
        \n\tv_p = M * vec4(position, 1.0);
        \n\tv_n = normalize(M * vec4(normal, 0.0)).xyz;
        \n\tgl_Position = P * V * v_p;
        \n}

    );

    QString frag = GLSL(130,
        \nuniform vec3 eye = vec3(0.0, 0.0, 1.0);
        \nuniform vec3 light_pos = vec3(30.0, 0.0, 0.0);
        \n
        \nuniform vec4 ka = vec4(0.13, 0.13, 0.13, 1.0);
        \nuniform vec4 La = vec4(1.0, 1.0, 1.0, 1.0);
        \n
        \nuniform vec4 kd = vec4(0.7, 1.0, 0.7, 1.0);
        \nuniform vec4 Ld = vec4(1.0, 1.0, 1.0, 1.0);
        \n
        \nuniform vec4 ks = vec4(0.5, 0.5, 0.5, 1.0);
        \nuniform vec4 Ls = vec4(1.0, 1.0, 1.0, 1.0);
        \n
        \nuniform float shininess = 20.0;
        \n
        \nin vec3 v_n;
        \nin vec4 v_p;
        \n
        \nout vec4 fragcolor;

        \nvoid main(void){
        \n\tvec3 n = normalize(v_n);
        \n\tvec3 l = normalize(light_pos - v_p.xyz);
        \n\tvec3 v = normalize(eye - v_p.xyz);
        \n\tvec3 r = reflect(-l, n);
        \n
        \n\tfragcolor = (ka*La + kd*Ld*max(dot(n, l), 0.0) + ks*Ls*max(pow(dot(r, v), shininess), 0.0));
        \n}

    );

    ShaderEditor* shader_phong = new ShaderEditor(vert,"","","",frag);
    m_Editors.push_back(shader_phong);
    m_iE = m_Editors.size()-1;
}
void MainWindow::handleAddShaderBlingPhong(){
    QString vert = GLSL(130, );
    QString frag = GLSL(130, );

    ShaderEditor* shader_plain = new ShaderEditor(vert,"","","",frag);
    m_Editors.push_back(shader_plain);
    m_iE = m_Editors.size()-1;
}
void MainWindow::handleAddShaderCookTorr(){
    QString vert = GLSL(130,
        \nuniform mat4 P;
        \nuniform mat4 V;
        \nuniform mat4 M;
        \nuniform float time;
        \n
        \nin vec3 position;
        \nin vec3 normal;
        \nin vec2 texture_coord;

        \nout vec3 n_vs;
        \nout vec4 p_vs;
        \nout vec4 cam_pos_vs;

        \nvoid main()
        \n{
        \n   p_vs = M*vec4(position, 1.0);
        \n   gl_Position = P*V*p_vs;
        \n
        \n   cam_pos_vs = inverse(V)*vec4(0.0, 0.0, 0.0, 1.0);
        \n   n_vs = normalize(M*vec4(normal, 0.0)).xyz;
        \n}
        \n
    );

    QString frag = GLSL(130,

        uniform vec3 light_pos = vec3(30.0, 0.0, 0.0);

        uniform vec4 ka = vec4(0.13, 0.13, 0.13, 1.0);
        uniform vec4 La = vec4(1.0, 1.0, 1.0, 1.0);

        uniform vec4 kd = vec4(0.7, 1.0, 0.7, 1.0);
        uniform vec4 Ld = vec4(1.0, 1.0, 1.0, 1.0);

        uniform vec4 ks = vec4(0.5, 0.5, 0.5, 1.0);
        uniform vec4 Ls = vec4(1.0, 1.0, 1.0, 1.0);

        in vec3 n_vs;
        in vec4 p_vs;
        in vec4 cam_pos_vs;

        out vec4 fragcolor;

        uniform float IOR = 1.0;
        uniform float m = 2.0;

        void main()
        {
            vec3 n = normalize(n_vs);
            vec3 l = normalize(light_pos - p_vs.xyz);
            vec3 v = normalize(cam_pos_vs.xyz - p_vs.xyz);
            vec3 r = reflect(-l, n);
            vec3 h = (l+v)/normalize(l+v);
            float c = dot(v,h);
            float g = sqrt(pow(IOR, 2.0) + pow(c, 2.0) - 1);
            float alpha = acos(dot(n, h));
            float flm = pow(((1-IOR)/(1+IOR)),2.0);
            float F = 1;
            float D = 1;
            float G = 1;

            G =	min((2 * dot(n, h) * dot(n, v))/dot(v, h), (2 * dot(n, h) * dot(n, l))/dot(v, h));
            fragcolor = ks*Ls*G;

            F = flm+(1-flm)*(1-pow(dot(n,v), 5.0));
            D = exp(pow(-(tan(alpha)/m),2.0))/(4*pow(m,2.0)*pow(dot(n,h), 4.0));
            G =	min((2 * dot(n, h) * dot(n, v))/dot(v, h), (2 * dot(n, h) * dot(n, l))/dot(v, h));
            fragcolor = ka*La + kd*Ld*max(dot(n, l), 0.0) + ks * Ls * ((F * D * G)/ ( 3.14 * dot(n, l) * dot(n, v) ) );
        }
    );

    ShaderEditor* shader_phong = new ShaderEditor(vert,"","","",frag);
    m_Editors.push_back(shader_phong);
    m_iE = m_Editors.size()-1;
}
void MainWindow::handleAddShaderToon(){
    QString vert = GLSL(130,
        \n
        \nuniform mat4 P;
        \nuniform mat4 V;
        \nuniform mat4 M;
        \n
        \nin vec3 normal;
        \nin vec3 position;
        \n
        \nout vec3 v_p;
        \nout vec3 v_n;
        \n
        \nvoid main()
        \n{
        \n\tv_p = mat3(M) * position;
        \n\tv_n = normalize(mat3(M) * normal);
        \n\tgl_Position = P*V*M*vec4(position,1);
        \n}
    );
    QString frag = GLSL(130,
        \nuniform vec3 light_position = vec3(5.0, 2.0, 0.0);
        \nuniform vec3 eye;
        \nuniform int shininess = 20;
        \nuniform float kd = 0.3;
        \nuniform float ks = 0.5;
        \n
        \nin vec3 v_p;
        \nin vec3 v_n;
        \n
        \nout vec4 fragcolor;
        \n
        \nvoid main()
        \n{
        \n\tconst int levels = 3;
        \n\tconst float scaleFactor = 1.0 / levels;
        \n\tvec3 La = vec3(1.0,1.0,1.0);
        \n\tvec3 Ld = vec3(1.0,1.0,1.0);
        \n\tvec3 color = vec3(0.8,0.6,0.4);
        \n
        \n\tvec3 L = normalize(light_position - v_p);
        \n\tvec3 V = normalize(eye - v_p);
        \n\tfloat diffuse = max(0, dot(L, v_n));
        \n\tLd = Ld * kd * floor(diffuse * levels) * scaleFactor;
        \n
        \n\tvec3 H = normalize(L + V);
        \n\tfloat specular = 0.0;
        \n\tif( dot(L,v_n) > 0.0){
        \n\tspecular = ks * pow(max(0, dot(H, v_n)), shininess);
        \n\t}
        \n
        \n\tfloat specMask = (pow(dot(H, v_n), shininess) > 0.5) ? 1 : 0;
        \n\tfloat edgeDetection = (dot(V, v_n) > 0.2) ? 1 : 0;
        \n\tcolor = edgeDetection * (color + Ld + specular * specMask);
        \n\tfragcolor = vec4(color,1);
        }
    );
    ShaderEditor* shader_empty = new ShaderEditor(vert,"","","",frag);
    m_Editors.push_back(shader_empty);
    m_iE = m_Editors.size()-1;
}


// Mesh
bool MainWindow::hasMesh(){
    return !(m_Meshes.isEmpty());
}
bool MainWindow::hasManyMeshes(){
    return m_Meshes.size() > 1;
}
void MainWindow::handleMeshNext(){
    if(!m_Meshes.isEmpty()){
        m_iM++;
        if(m_iM > m_Meshes.size()-1){
            m_iM = 0;
        }
    }else{
        m_iM = 0;
    }
}
void MainWindow::handleMeshDelete(){
    assert(hasMesh());
    delete getMesh();
    m_Meshes.erase(m_Meshes.begin()+m_iM);

    m_iM--;
    if(m_iM < 0){
        m_iM = 0;
    }

    if(hasMesh()){
        ui->shaderframe->setMesh(m_Meshes[m_iM]);
    }else{
        ui->shaderframe->setMesh(nullptr);
    }

}
void MainWindow::handleMeshPrevious(){
    if(!m_Meshes.isEmpty()){
        m_iM--;
        if(m_iM < 0){
            m_iM = m_Meshes.size()-1;
        }
    }else{
        m_iM = 0;
    }
}
void MainWindow::setNewestMesh(){
    if(hasMesh()) m_iM = m_Meshes.size()-1;
    else m_iM = 0;
}

void MainWindow::handleMeshTranslateSetX(){
    if(hasMesh())   m_Meshes[m_iM]->mTranslation.setX(ui->dsbMeshTranslationX->value());
}
void MainWindow::handleMeshTranslateSetY(){
    if(hasMesh())   m_Meshes[m_iM]->mTranslation.setY(ui->dsbMeshTranslationY->value());
}
void MainWindow::handleMeshTranslateSetZ(){
    if(hasMesh())   m_Meshes[m_iM]->mTranslation.setZ(ui->dsbMeshTranslationZ->value());
}
void MainWindow::handleMeshRotateSetX(){
    if(hasMesh())   m_Meshes[m_iM]->mRotation.setX(ui->dsbMeshRotateX->value());
}
void MainWindow::handleMeshRotateSetY(){
    if(hasMesh())   m_Meshes[m_iM]->mRotation.setY(ui->dsbMeshRotateY->value());
}
void MainWindow::handleMeshRotateSetZ(){
    if(hasMesh())   m_Meshes[m_iM]->mRotation.setZ(ui->dsbMeshRotateZ->value());
}
void MainWindow::handleMeshScaleSetX(){
    if(hasMesh()){ m_Meshes[m_iM]->mScale.setX(ui->dsbMeshScaleX->value()); }

}
void MainWindow::handleMeshScaleSetY(){
    if(hasMesh())   m_Meshes[m_iM]->mScale.setY(ui->dsbMeshScaleY->value());
}
void MainWindow::handleMeshScaleSetZ(){
    if(hasMesh())   m_Meshes[m_iM]->mScale.setZ(ui->dsbMeshScaleZ->value());
}
//
void MainWindow::handleAddMeshLoadFile(){

    QString location = QFileDialog::getOpenFileName(this, "Open File", QDir::currentPath(), QString("*.obj"), 0, QFileDialog::DontUseNativeDialog);

    if (!location.isEmpty()){

        Mesh* mesh = new Mesh();

        //check if file exists
        std::ifstream fin(location.toStdString().c_str());
        if(!fin.fail()){
            fin.close();
            mesh->mScene = importer.ReadFile( location.toStdString(), aiProcessPreset_TargetRealtime_Quality);//|aiProcess_FlipWindingOrder);

            // If the import failed, report it
            if(!mesh->mScene){
                std::cout << importer.GetErrorString() << std::endl;
                printf("%s\n", importer.GetErrorString());
            }

            GetBoundingBox(mesh->mScene, &mesh->mBbMin, &mesh->mBbMax);
            aiVector3D diff = mesh->mBbMax-mesh->mBbMin;
            float w = std::max(diff.x, std::max(diff.y, diff.z));

            mesh->mScaleFactor = 1.0f / w;

            BufferIndexedVerts(*mesh);
            mesh->mNumIndices = mesh->mScene->mMeshes[0]->mNumFaces* 3;

            mesh->ConnectVao();

            m_Meshes.push_back(mesh);


        }else{
            std::cout << "Could not open file" << location.toStdString() << std::endl;
            std::cout << importer.GetErrorString() << std::endl;
        }


    }
}
void MainWindow::handleAddMeshTriangle(){
    Mesh* triangle = new Mesh();
    triangle->Clear();
    GLfloat positions[3*8] =
    {
        // front
         0.0,  1.0,  0.0,
        -1.0,  1.0,  0.0,
         1.0, -1.0,  0.0,
    };
    Q_ASSERT(triangle->mVboVerts.create());
    triangle->mVboVerts.setUsagePattern(QOpenGLBuffer::StaticDraw);
    triangle->mVboVerts.bind();
    triangle->mVboVerts.allocate(positions, sizeof(positions));
    triangle->mVboVerts.release();

    GLfloat colors[4*8] =
    {
        1.0f, 0.0f, 0.0f, 1.0f,
        0.0f, 1.0f, 0.0f, 1.0f,
        0.0f, 0.0f, 1.0f, 1.0f
    };
    Q_ASSERT(triangle->mVboColors.create());
    triangle->mVboColors.setUsagePattern(QOpenGLBuffer::StaticDraw);
    triangle->mVboColors.bind();
    triangle->mVboColors.allocate(colors, sizeof(colors));
    triangle->mVboColors.release();

    GLfloat normals[3*8] =
    {
        -1.0f,-1.0f, 1.0f,
         1.0f,-1.0f, 1.0f,
         1.0f, 1.0f, 1.0f,
        -1.0f, 1.0f, 1.0f,
        -1.0f,-1.0f,-1.0f,
         1.0f,-1.0f,-1.0f,
         1.0f, 1.0f,-1.0f,
        -1.0f, 1.0f,-1.0f
    };

    Q_ASSERT(triangle->mVboNormals.create());
    triangle->mVboNormals.setUsagePattern(QOpenGLBuffer::StaticDraw);
    triangle->mVboNormals.bind();
    triangle->mVboNormals.allocate(normals, sizeof(normals));
    triangle->mVboNormals.release();

    GLuint indices[3*1] =
    {
        0, 1, 2
    };

    Q_ASSERT(triangle->mVboIndexBuffer.create());
    triangle->mVboIndexBuffer.setUsagePattern(QOpenGLBuffer::StaticDraw);
    triangle->mVboIndexBuffer.bind();
    triangle->mVboIndexBuffer.allocate(indices, sizeof(indices));
    triangle->mVboIndexBuffer.release();
    triangle->mNumIndices = sizeof(indices) / sizeof(GLuint);

    triangle->mVao.create();
    triangle->mVao.bind();
    {
        if(triangle->mVboVerts.isCreated())        triangle->mVboVerts.bind();
        if(triangle->mVboNormals.isCreated())      triangle->mVboNormals.bind();
        if(triangle->mVboColors.isCreated())       triangle->mVboColors.bind();
        if(triangle->mVboTexCoords.isCreated())    triangle->mVboTexCoords.bind();
        if(triangle->mVboIndexBuffer.isCreated())  triangle->mVboIndexBuffer.bind();
    }
    triangle->mVao.release();


    if(triangle->mVboVerts.isCreated())             triangle->mVboVerts.release();
    if(triangle->mVboNormals.isCreated())           triangle->mVboNormals.release();
    if(triangle->mVboColors.isCreated())            triangle->mVboColors.release();
    if(triangle->mVboTexCoords.isCreated())         triangle->mVboTexCoords.release();
    if(triangle->mVboIndexBuffer.isCreated())       triangle->mVboIndexBuffer.release();

    m_Meshes.push_back(triangle);
    m_iM = m_Meshes.size()-1;
}
void MainWindow::handleAddMesh2DPlane(){
    Mesh* plane = new Mesh();
    plane->Clear();
    GLfloat positions[3*4] =
    {
        -1.0f, 1.0f, 0.0f,
        -1.0f,-1.0f, 0.0f,
         1.0f,-1.0f, 0.0f,
         1.0f, 1.0f, 0.0f
    };
    Q_ASSERT(plane->mVboVerts.create());
    plane->mVboVerts.setUsagePattern(QOpenGLBuffer::StaticDraw);
    plane->mVboVerts.bind();
    plane->mVboVerts.allocate(positions, sizeof(positions));
    plane->mVboVerts.release();

    GLfloat colors[4*4] =
    {
        1.0f, 0.0f, 0.0f, 1.0f,
        0.0f, 1.0f, 0.0f, 1.0f,
        0.0f, 0.0f, 1.0f, 1.0f,
        1.0f, 0.0f, 0.0f, 1.0f
    };
    Q_ASSERT(plane->mVboColors.create());
    plane->mVboColors.setUsagePattern(QOpenGLBuffer::StaticDraw);
    plane->mVboColors.bind();
    plane->mVboColors.allocate(colors, sizeof(colors));
    plane->mVboColors.release();

    GLfloat normals[3*4] =
    {
        0.0f, 0.0f,-1.0f,
        0.0f, 0.0f,-1.0f,
        0.0f, 0.0f,-1.0f,
        0.0f, 0.0f,-1.0f
    };

    Q_ASSERT(plane->mVboNormals.create());
    plane->mVboNormals.setUsagePattern(QOpenGLBuffer::StaticDraw);
    plane->mVboNormals.bind();
    plane->mVboNormals.allocate(normals, sizeof(normals));
    plane->mVboNormals.release();

    GLuint indices[3*2] =
    {
        0, 1, 2,    0, 2, 3
    };

    Q_ASSERT(plane->mVboIndexBuffer.create());
    plane->mVboIndexBuffer.setUsagePattern(QOpenGLBuffer::StaticDraw);
    plane->mVboIndexBuffer.bind();
    plane->mVboIndexBuffer.allocate(indices, sizeof(indices));
    plane->mVboIndexBuffer.release();
    plane->mNumIndices = sizeof(indices) / sizeof(GLuint);
    plane->mVao.create();
    plane->mVao.bind();
    {
        if(plane->mVboVerts.isCreated())        plane->mVboVerts.bind();
        if(plane->mVboNormals.isCreated())      plane->mVboNormals.bind();
        if(plane->mVboColors.isCreated())       plane->mVboColors.bind();
        if(plane->mVboTexCoords.isCreated())    plane->mVboTexCoords.bind();
        if(plane->mVboIndexBuffer.isCreated())  plane->mVboIndexBuffer.bind();
    }
    plane->mVao.release();


    if(plane->mVboVerts.isCreated())             plane->mVboVerts.release();
    if(plane->mVboNormals.isCreated())           plane->mVboNormals.release();
    if(plane->mVboColors.isCreated())            plane->mVboColors.release();
    if(plane->mVboTexCoords.isCreated())         plane->mVboTexCoords.release();
    if(plane->mVboIndexBuffer.isCreated())       plane->mVboIndexBuffer.release();

    m_Meshes.push_back(plane);
}
void MainWindow::handleAddMesh3DCube(){
    Mesh* cube = new Mesh();
    cube->Clear();
    GLfloat positions[3*8] =
    {
        // front
        -1.0, -1.0,  1.0,
         1.0, -1.0,  1.0,
         1.0,  1.0,  1.0,
        -1.0,  1.0,  1.0,
        // back
        -1.0, -1.0, -1.0,
         1.0, -1.0, -1.0,
         1.0,  1.0, -1.0,
        -1.0,  1.0, -1.0,
    };
    Q_ASSERT(cube->mVboVerts.create());
    cube->mVboVerts.setUsagePattern(QOpenGLBuffer::StaticDraw);
    cube->mVboVerts.bind();
    cube->mVboVerts.allocate(positions, sizeof(positions));
    cube->mVboVerts.release();

    GLfloat colors[4*8] =
    {
        1.0f, 0.0f, 0.0f, 1.0f,
        0.0f, 1.0f, 0.0f, 1.0f,
        0.0f, 0.0f, 1.0f, 1.0f,
        1.0f, 0.0f, 0.0f, 1.0f,

        0.0f, 1.0f, 0.0f, 1.0f,
        0.0f, 0.0f, 1.0f, 1.0f,
        1.0f, 0.0f, 0.0f, 1.0f,
        0.0f, 1.0f, 0.0f, 1.0f,
    };
    Q_ASSERT(cube->mVboColors.create());
    cube->mVboColors.setUsagePattern(QOpenGLBuffer::StaticDraw);
    cube->mVboColors.bind();
    cube->mVboColors.allocate(colors, sizeof(colors));
    cube->mVboColors.release();

    GLfloat normals[3*8] =
    {
        -1.0f,-1.0f, 1.0f,
         1.0f,-1.0f, 1.0f,
         1.0f, 1.0f, 1.0f,
        -1.0f, 1.0f, 1.0f,
        -1.0f,-1.0f,-1.0f,
         1.0f,-1.0f,-1.0f,
         1.0f, 1.0f,-1.0f,
        -1.0f, 1.0f,-1.0f
    };

    Q_ASSERT(cube->mVboNormals.create());
    cube->mVboNormals.setUsagePattern(QOpenGLBuffer::StaticDraw);
    cube->mVboNormals.bind();
    cube->mVboNormals.allocate(normals, sizeof(normals));
    cube->mVboNormals.release();

    GLuint indices[2*3*8] =
    {
        // front
        0, 1, 2,    2, 3, 0,
        // top
        1, 5, 6,    6, 2, 1,
        // back
        7, 6, 5,    5, 4, 7,
        // bottom
        4, 0, 3,    3, 7, 4,
        // left
        4, 5, 1,    1, 0, 4,
        // right
        3, 2, 6,    6, 7, 3,
    };

    Q_ASSERT(cube->mVboIndexBuffer.create());
    cube->mVboIndexBuffer.setUsagePattern(QOpenGLBuffer::StaticDraw);
    cube->mVboIndexBuffer.bind();
    cube->mVboIndexBuffer.allocate(indices, sizeof(indices));
    cube->mVboIndexBuffer.release();
    cube->mNumIndices = sizeof(indices) / sizeof(GLuint);

    cube->mVao.create();
    cube->mVao.bind();
    {
        if(cube->mVboVerts.isCreated())        cube->mVboVerts.bind();
        if(cube->mVboNormals.isCreated())      cube->mVboNormals.bind();
        if(cube->mVboColors.isCreated())       cube->mVboColors.bind();
        if(cube->mVboTexCoords.isCreated())    cube->mVboTexCoords.bind();
        if(cube->mVboIndexBuffer.isCreated())  cube->mVboIndexBuffer.bind();
    }
    cube->mVao.release();


    if(cube->mVboVerts.isCreated())             cube->mVboVerts.release();
    if(cube->mVboNormals.isCreated())           cube->mVboNormals.release();
    if(cube->mVboColors.isCreated())            cube->mVboColors.release();
    if(cube->mVboTexCoords.isCreated())         cube->mVboTexCoords.release();
    if(cube->mVboIndexBuffer.isCreated())       cube->mVboIndexBuffer.release();

    m_Meshes.push_back(cube);

}
void MainWindow::handleAddMesh3DSphere(){}
void MainWindow::handleAddMesh3DIcosahedron(){
    Mesh* icosahedron = new Mesh();
    icosahedron->Clear();


    GLfloat positions[] = {
         0.000f,  0.000f,  1.000f,
         0.894f,  0.000f,  0.447f,
         0.276f,  0.851f,  0.447f,
        -0.724f,  0.526f,  0.447f,
        -0.724f, -0.526f,  0.447f,
         0.276f, -0.851f,  0.447f,
         0.724f,  0.526f, -0.447f,
        -0.276f,  0.851f, -0.447f,
        -0.894f,  0.000f, -0.447f,
        -0.276f, -0.851f, -0.447f,
         0.724f, -0.526f, -0.447f,
         0.000f,  0.000f, -1.000f
    };



    Q_ASSERT(icosahedron->mVboVerts.create());
    icosahedron->mVboVerts.setUsagePattern(QOpenGLBuffer::StaticDraw);
    icosahedron->mVboVerts.bind();
    icosahedron->mVboVerts.allocate(positions, sizeof(positions));
    icosahedron->mVboVerts.release();

    GLfloat colors[4*8] =
    {
        1.0f, 0.0f, 0.0f, 1.0f,
        0.0f, 1.0f, 0.0f, 1.0f,
        0.0f, 0.0f, 1.0f, 1.0f,
        1.0f, 0.0f, 0.0f, 1.0f,

        0.0f, 1.0f, 0.0f, 1.0f,
        0.0f, 0.0f, 1.0f, 1.0f,
        1.0f, 0.0f, 0.0f, 1.0f,
        0.0f, 1.0f, 0.0f, 1.0f,
    };
    Q_ASSERT(icosahedron->mVboColors.create());
    icosahedron->mVboColors.setUsagePattern(QOpenGLBuffer::StaticDraw);
    icosahedron->mVboColors.bind();
    icosahedron->mVboColors.allocate(colors, sizeof(colors));
    icosahedron->mVboColors.release();

    GLfloat normals[3*8] =
    {
        -1.0f,-1.0f, 1.0f,
         1.0f,-1.0f, 1.0f,
         1.0f, 1.0f, 1.0f,
        -1.0f, 1.0f, 1.0f,
        -1.0f,-1.0f,-1.0f,
         1.0f,-1.0f,-1.0f,
         1.0f, 1.0f,-1.0f,
        -1.0f, 1.0f,-1.0f
    };

    Q_ASSERT(icosahedron->mVboNormals.create());
    icosahedron->mVboNormals.setUsagePattern(QOpenGLBuffer::StaticDraw);
    icosahedron->mVboNormals.bind();
    icosahedron->mVboNormals.allocate(normals, sizeof(normals));
    icosahedron->mVboNormals.release();

    GLuint indices[3*5*4] = {
        2, 1, 0,
        3, 2, 0,
        4, 3, 0,
        5, 4, 0,
        1, 5, 0,

        11, 6, 7,
        11, 7, 8,
        11, 8, 9,
        11, 9, 10,
        11, 10, 6,

        1, 2, 6,
        2, 3, 7,
        3, 4, 8,
        4, 5, 9,
        5, 1, 10,

        2,  7, 6,
        3,  8, 7,
        4,  9, 8,
        5, 10, 9,
        1, 6, 10
    };

    Q_ASSERT(icosahedron->mVboIndexBuffer.create());
    icosahedron->mVboIndexBuffer.setUsagePattern(QOpenGLBuffer::StaticDraw);
    icosahedron->mVboIndexBuffer.bind();
    icosahedron->mVboIndexBuffer.allocate(indices, sizeof(indices));
    icosahedron->mVboIndexBuffer.release();
    icosahedron->mNumIndices = sizeof(indices) / sizeof(GLuint);

    icosahedron->mVao.create();
    icosahedron->mVao.bind();
    {
        if(icosahedron->mVboVerts.isCreated())        icosahedron->mVboVerts.bind();
        if(icosahedron->mVboNormals.isCreated())      icosahedron->mVboNormals.bind();
        if(icosahedron->mVboColors.isCreated())       icosahedron->mVboColors.bind();
        if(icosahedron->mVboTexCoords.isCreated())    icosahedron->mVboTexCoords.bind();
        if(icosahedron->mVboIndexBuffer.isCreated())  icosahedron->mVboIndexBuffer.bind();
    }
    icosahedron->mVao.release();


    if(icosahedron->mVboVerts.isCreated())             icosahedron->mVboVerts.release();
    if(icosahedron->mVboNormals.isCreated())           icosahedron->mVboNormals.release();
    if(icosahedron->mVboColors.isCreated())            icosahedron->mVboColors.release();
    if(icosahedron->mVboTexCoords.isCreated())         icosahedron->mVboTexCoords.release();
    if(icosahedron->mVboIndexBuffer.isCreated())       icosahedron->mVboIndexBuffer.release();

    m_Meshes.push_back(icosahedron);
}


// Texture
bool MainWindow::hasTexture(){
    return !(m_Textures.isEmpty());
}
bool MainWindow::hasManyTextures(){
    return m_Textures.size() > 1;
}
void MainWindow::handleTextureLoadFile(){
    QString location = QFileDialog::getOpenFileName(this, "Open File", QDir::currentPath(), QString("*.*"), 0, QFileDialog::DontUseNativeDialog);
    if(!location.isEmpty()){
        QOpenGLTexture* text = new QOpenGLTexture(QImage(location));
        m_Textures.push_back(text);
    }else{
        std::cout << "Could not load texture!" << std::endl;
    }

}
void MainWindow::handleTextureNext(){
    m_iT++;
    if(m_iT > m_Textures.size()-1){
        m_iT = 0;
    }
}
void MainWindow::handleTextureDelete(){
    assert(hasTexture());
    delete getTexture();
    m_Textures.erase(m_Textures.begin()+m_iT);
    m_iT--;
    if(m_iT < 0){
        m_iT = 0;
    }

    if(hasTexture()){
        ui->shaderframe->setTexture(m_Textures[m_iT]);
    }else{
        ui->shaderframe->setTexture(nullptr);
    }

}
void MainWindow::handleTexturePrevious(){
    m_iT--;
    if(m_iT < 0){
        m_iT = m_Textures.size()-1;
    }
}
void MainWindow::setNewestTexture(){
    if(hasTexture()) m_iT = m_Textures.size()-1;
    else m_iT = 0;
}

//Settings
void MainWindow::handleSettingsUniformsReset(){
    ui->shaderframe->defaultUniforms();
}
void MainWindow::handleSettingsCameraReset(){
    ui->shaderframe->resetCamera();
}

