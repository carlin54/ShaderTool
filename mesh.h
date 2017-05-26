#ifndef MESH_H
#define MESH_H

#include <stdlib.h>
#include <stdio.h>

#include <cmath>
#include <iostream>
#include <fstream>
#include <algorithm>
#include <string>
#include <glm/glm.hpp>

#include <QFile>
#include <QtGlobal>
#include <QVector>
#include <QOpenGLShader>
#include <QOpenGLFunctions>
#include <QOpenGLWidget>
#include <QOpenGLBuffer>
#include <QOpenGLTexture>
#include <QOpenGLShaderProgram>
#include <QOpenGLBuffer>
#include <QOpenGLVertexArrayObject>

#include <QVector>
#include <QVector2D>
#include <QVector3D>
#include <assimp/scene.h>
#include <assimp/postprocess.h>
#include <assimp/Importer.hpp>




class Mesh
{

public:

    Mesh();
    ~Mesh();

    QOpenGLVertexArrayObject mVao;
    QOpenGLBuffer mVboVerts;
    QOpenGLBuffer mVboNormals;
    QOpenGLBuffer mVboTexCoords;
    QOpenGLBuffer mVboColors;
    QOpenGLBuffer mVboIndexBuffer;

    float mScaleFactor;
    unsigned int mNumIndices;
    const aiScene* mScene;
    aiVector3D mBbMin, mBbMax;

    QVector3D mTranslation;
    QVector3D mRotation;
    QVector3D mScale;

    void Clear();
    void ConnectVao();

    // void LoadCube();
    // void LoadMesh(const std::string& filePath);

};



#endif // MESH_H
