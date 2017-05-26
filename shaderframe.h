#ifndef SHADERFRAME_H
#define SHADERFRAME_H

#include <cmath>
#include <QVector>
#include <QOpenGLShader>
#include <QTime>
#include <QMouseEvent>
#include <QOpenGLFunctions>
#include <QOpenGLWidget>
#include <QOpenGLVertexArrayObject>
#include <QOpenGLBuffer>
#include <QtGlobal>
#include <QMatrix4x4>
#include <QOpenGLTexture>
#include <QOpenGLShaderProgram>
#include <QDoubleSpinBox>
#include "mesh.h"
#include "shadereditor.h"

class ShaderFrame
    :   public QOpenGLWidget
    ,   public QOpenGLFunctions
{
private:
    ShaderEditor* m_Editor;
    Mesh* m_Mesh;
    QOpenGLTexture* m_Texture;

    unsigned int m_FrameNumber;
    QVector4D m_ClearColor;

    QTime m_Time;

    void updateMesh();
    void pushUniforms();
    void refreshUniforms();

    QDoubleSpinBox* dsbTranslateX;
    QDoubleSpinBox* dsbTranslateY;
    QDoubleSpinBox* dsbTranslateZ;
    QDoubleSpinBox* dsbRotateX;
    QDoubleSpinBox* dsbRotateY;
    QDoubleSpinBox* dsbRotateZ;
    QDoubleSpinBox* dsbScaleX;
    QDoubleSpinBox* dsbScaleY;
    QDoubleSpinBox* dsbScaleZ;



public:

    ShaderFrame(QWidget* parent = 0);
    ~ShaderFrame();

    float mMouseMoveSensitivity;
    float mMouseWheelSensitivity;


    void defaultUniforms();
    void resetCamera();
    void refreshCamera();

    bool hasEditor();
    bool hasProgram();
    bool hasMesh();
    bool hasTexture();

    ShaderEditor& getEditor();
    Mesh& getMesh();
    QOpenGLTexture& getTexture();

    void setEditor(ShaderEditor* set);
    void setMesh(Mesh* set);
    void setTexture(QOpenGLTexture* set);

    float getAspectRatio();
    unsigned int getFrameNumber();

    void initializeGL();
    void paintGL();
    void resizeGL(int w, int h);
    void wheelEvent(QWheelEvent* wheel);
    void mousePressEvent(QMouseEvent* mouse);
    void mouseMoveEvent(QMouseEvent* mouse);
    void mouseReleaseEvent(QMouseEvent* mouse);

    struct {

        struct {

            QMatrix4x4 mP;
            float mFov;
            float mAspectRatio;
            float mNear;
            float mFar;

            QMatrix4x4 mV;
            QVector3D mEye;
            QVector3D mCenter;
            QVector3D mUp;



        } mCamera;

        QMatrix4x4 mM;


        int mTime;
        int mElapsed;
        int mFrame;
        int mWidth;
        int mHeight;

        QMatrix4x4 mPVM;

    } mUniforms;

    bool isMouseLeftButtonDown;
    int mLastMouseX;
    int mLastMouseY;
    int mMouseX;
    int mMouseY;
    void updatePressMouse(int x, int y){
        mLastMouseX = x;mMouseX = x;
        mLastMouseY = y;mMouseY = y;
    }
    void updateMouse(int x, int y){
        mLastMouseX = mMouseX;mMouseX = x;
        mLastMouseY = mMouseY;mMouseY = y;
    }
    void updateReleaseMouse(int x, int y){
        mLastMouseX = x;mMouseX = x;
        mLastMouseY = y;mMouseY = y;
    }
    void updateDoubleSpinBoxes();

    void setTranslateDSB(QDoubleSpinBox* tx, QDoubleSpinBox* ty, QDoubleSpinBox* tz);
    void setScaleDSB(QDoubleSpinBox* sx, QDoubleSpinBox* sy, QDoubleSpinBox* sz);
    void setRotateDSB(QDoubleSpinBox* rx, QDoubleSpinBox* ry, QDoubleSpinBox* rz);
};

#endif // SHADERFRAME_H
