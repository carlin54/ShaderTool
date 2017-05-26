#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <string>
#include <iostream>
#include <QMainWindow>
#include <QSettings>
#include <QDesktopWidget>
#include <QFileDialog>
#include <QOpenGLShaderProgram>
#include "assimp/Importer.hpp"
#include "shadereditor.h"
#include "glslhighlighter.h"
#include "mesh.h"

namespace Ui {
class MainWindow;
}

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = 0);
    ~MainWindow();

private:

    static Assimp::Importer importer;

    int m_iE;   QVector<ShaderEditor*>      m_Editors;
    int m_iM;   QVector<Mesh*>              m_Meshes;
    int m_iT;   QVector<QOpenGLTexture*>    m_Textures;

    bool hasEditor();
    bool hasManyEditors();

    bool hasMesh();
    bool hasManyMeshes();

    bool hasTexture();
    bool hasManyTextures();

    bool isShaderProgramActive();

    void readSettings();
    template<class T> void next(int& i, const QVector<T>& container);
    template<class T> void previous(int& i, const QVector<T>& container);

    ShaderEditor* getEditor();
    Mesh* getMesh();
    QOpenGLTexture* getTexture();

    void setNewestEditor();
    void setNewestMesh();
    void setNewestTexture();

private slots:

    // Ui
    void handleRefresh();
    void handleVertexToggle(bool set);
    void handleControlToggle(bool set);
    void handleEvaluationToggle(bool set);
    void handleGeometryToggle(bool set);
    void handleFragmentToggle(bool set);

    void handleCompile();
    void handleShaderNext();
    void handleShaderPrevious();

    void handleEditorNext();
    void handleEditorDelete();
    void handleEditorPrevious();

    void handleMeshNext();
    void handleMeshDelete();
    void handleMeshPrevious();

    void handleMeshTranslateSetX();
    void handleMeshTranslateSetY();
    void handleMeshTranslateSetZ();
    void handleMeshRotateSetX();
    void handleMeshRotateSetY();
    void handleMeshRotateSetZ();
    void handleMeshScaleSetX();
    void handleMeshScaleSetY();
    void handleMeshScaleSetZ();

    // Texture
    void handleTextureLoadFile();
    void handleTextureNext();
    void handleTextureDelete();
    void handleTexturePrevious();

    // Mesh
    void handleAddMeshLoadFile();
    void handleAddMeshTriangle();
    void handleAddMesh2DPlane();
    void handleAddMesh3DCube();
    void handleAddMesh3DSphere();
    void handleAddMesh3DIcosahedron();

    // Shaders
    void handleAddShaderEmpty();
    void handleAddShaderPlain();
    void handleAddShaderCookTorr();
    void handleAddShaderPhong();
    void handleAddShaderBlingPhong();
    void handleAddShaderToon();

    //Reset
    void handleSettingsUniformsReset();
    void handleSettingsCameraReset();

private:

    Ui::MainWindow *ui;
    GLSLHighlighter* m_pHighlighter;
    void setupEditor();


};

#endif // MAINWINDOW_H
