#include "shaderframe.h"

ShaderFrame::ShaderFrame(QWidget* parent)
    :   QOpenGLWidget(parent)
    ,   QOpenGLFunctions()
    ,   m_Editor(nullptr)
    ,   m_Mesh(nullptr)
    ,   m_Texture(nullptr)
    ,   m_FrameNumber(0)
    ,   m_ClearColor(0.5, 0.5, 0.5, 1.0)
    ,   mMouseMoveSensitivity(0.6)
    ,   mMouseWheelSensitivity(0.15)
    ,   m_Time()
    ,   dsbTranslateX(nullptr)
    ,   dsbTranslateY(nullptr)
    ,   dsbTranslateZ(nullptr)
    ,   dsbRotateX(nullptr)
    ,   dsbRotateY(nullptr)
    ,   dsbRotateZ(nullptr)
    ,   dsbScaleX(nullptr)
    ,   dsbScaleY(nullptr)
    ,   dsbScaleZ(nullptr)
{

    defaultUniforms();
    m_Time.start();
}

ShaderFrame::~ShaderFrame()
{

}

void ShaderFrame::updateMesh(){
    //glGenBuffers(1, &meshdata.mVboVerts);
    //glBindBuffer(GL_ARRAY_BUFFER, meshdata.mVboVerts);
    //glBufferData(GL_ARRAY_BUFFER, sizeof(float) * 3 * mesh->mNumVertices, aimesh->mVertices, GL_STATIC_DRAW);
    //glEnableVertexAttribArray(pos_loc);
    //glVertexAttribPointer(pos_loc, 3, GL_FLOAT, 0, 0, 0);

    if(hasMesh() && hasProgram()){
        if(m_Mesh->mVboVerts.isCreated()){
            if(m_Editor->program().attributeLocation("position") != -1){
                m_Mesh->mVboVerts.bind();
                m_Editor->program().enableAttributeArray("position");
                m_Editor->program().setAttributeBuffer("position", GL_FLOAT, 0, 3);
                m_Mesh->mVboVerts.release();
            }else{
                //std::cerr << "position found in mesh but not shader" << std::endl;
            }
        }


        if(m_Mesh->mVboNormals.isCreated()){
            if(m_Editor->program().attributeLocation("normal") != -1){
                m_Mesh->mVboNormals.bind();
                m_Editor->program().enableAttributeArray("normal");
                m_Editor->program().setAttributeBuffer("normal", GL_FLOAT, 0, 3);
                m_Mesh->mVboNormals.release();
            }else{
                //std::cerr << "normal found in mesh but not shader" << std::endl;
            }
        }

        if(m_Mesh->mVboColors.isCreated()){
            if(m_Editor->program().attributeLocation("color") != -1){
                m_Mesh->mVboColors.bind();
                m_Editor->program().enableAttributeArray("color");
                m_Editor->program().setAttributeBuffer("color", GL_FLOAT, 0, 4);
                m_Mesh->mVboColors.release();
            }else{
                //std::cerr << "color found in mesh but not shader" << std::endl;
            }
        }

        if(m_Mesh->mVboTexCoords.isCreated()){
            if(m_Editor->program().attributeLocation("texture_coord") != -1){
                m_Mesh->mVboTexCoords.bind();
                m_Editor->program().enableAttributeArray("texture_coord");
                m_Editor->program().setAttributeBuffer("texture_coord", GL_FLOAT, 0, 2);
                m_Mesh->mVboTexCoords.release();
            }else{
                //std::cerr << "texture_coord found in mesh but not shader" << std::endl;
            }
        }
        m_Mesh->ConnectVao();

    }
}
void ShaderFrame::pushUniforms(){
    if(hasProgram()){
        m_Editor->program().setUniformValue("P", mUniforms.mCamera.mP);
        m_Editor->program().setUniformValue("fov", mUniforms.mCamera.mFov);
        m_Editor->program().setUniformValue("aspect_ratio", mUniforms.mCamera.mAspectRatio);
        m_Editor->program().setUniformValue("near", mUniforms.mCamera.mNear);
        m_Editor->program().setUniformValue("far", mUniforms.mCamera.mFar);


        m_Editor->program().setUniformValue("V", mUniforms.mCamera.mV);
        m_Editor->program().setUniformValue("eye", mUniforms.mCamera.mEye);
        m_Editor->program().setUniformValue("center", mUniforms.mCamera.mCenter);
        m_Editor->program().setUniformValue("up", mUniforms.mCamera.mUp);

        m_Editor->program().setUniformValue("M", mUniforms.mM);
        if(hasMesh()){
            m_Editor->program().setUniformValue("scale", getMesh().mScale);
            m_Editor->program().setUniformValue("rotation", getMesh().mRotation);
            m_Editor->program().setUniformValue("translation", getMesh().mTranslation);
        }
        m_Editor->program().setUniformValue("PVM", mUniforms.mPVM);

        m_Editor->program().setUniformValue("frame", mUniforms.mFrame);
        m_Editor->program().setUniformValue("time", mUniforms.mTime);
        m_Editor->program().setUniformValue("elapsed", mUniforms.mElapsed);
        m_Editor->program().setUniformValue("width", mUniforms.mWidth);
        m_Editor->program().setUniformValue("height", mUniforms.mHeight);
    }
}

void ShaderFrame::refreshUniforms(){
    if(hasMesh()){
        mUniforms.mCamera.mP.setToIdentity();
        mUniforms.mCamera.mV.setToIdentity();
        mUniforms.mM.setToIdentity();
        mUniforms.mCamera.mAspectRatio = getAspectRatio();

        mUniforms.mCamera.mP.perspective(mUniforms.mCamera.mFov, mUniforms.mCamera.mAspectRatio, mUniforms.mCamera.mNear, mUniforms.mCamera.mFar);
        mUniforms.mCamera.mV.lookAt(mUniforms.mCamera.mEye, mUniforms.mCamera.mCenter, mUniforms.mCamera.mUp);
        QMatrix4x4 T; T.translate(getMesh().mTranslation);
        QMatrix4x4 R; R.rotate(QQuaternion().fromEulerAngles(getMesh().mRotation));
        QMatrix4x4 S; S.scale(getMesh().mScale);
        mUniforms.mM = T*R*S;
        mUniforms.mPVM = mUniforms.mCamera.mP * mUniforms.mCamera.mV * mUniforms.mM;
    }
}
void ShaderFrame::defaultUniforms(){
    mUniforms.mCamera.mFov = 90.0;
    mUniforms.mCamera.mAspectRatio = getAspectRatio();
    mUniforms.mCamera.mNear = 0.1;
    mUniforms.mCamera.mFar = 100.0;
    mUniforms.mCamera.mP.perspective(mUniforms.mCamera.mFov, mUniforms.mCamera.mAspectRatio, mUniforms.mCamera.mNear, mUniforms.mCamera.mFar);

    mUniforms.mCamera.mEye = QVector3D(0, 0, -1);
    mUniforms.mCamera.mCenter = QVector3D(0, 0, 0);
    mUniforms.mCamera.mUp = QVector3D(0, 1, 0);
    mUniforms.mCamera.mV.lookAt(mUniforms.mCamera.mEye, mUniforms.mCamera.mCenter, mUniforms.mCamera.mUp);

    mUniforms.mM.setToIdentity();
    if(hasMesh()){
        QMatrix4x4 T; T.translate(getMesh().mTranslation);
        QMatrix4x4 R; R.rotate(QQuaternion().fromEulerAngles(getMesh().mRotation));
        QMatrix4x4 S; S.scale(getMesh().mScale);
        mUniforms.mM = T*R*S;
    }
    mUniforms.mFrame = 0;

}

void ShaderFrame::initializeGL(){
    this->makeCurrent();
    initializeOpenGLFunctions();
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_CULL_FACE);
    m_ClearColor = QVector4D(0.5, 0.5, 0.5, 1.0);
    glClearColor(m_ClearColor.x(), m_ClearColor.y(), m_ClearColor.z(), m_ClearColor.w());
    this->doneCurrent();
}
void ShaderFrame::paintGL(){
    //this->makeCurrent();
    mUniforms.mFrame++;

    mUniforms.mElapsed = m_Time.elapsed();
    mUniforms.mTime = m_Time.second();

    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    if(hasProgram() && hasMesh()){
        std::cout << "test! " << mUniforms.mFrame << std::endl;
        assert(m_Editor->program().isLinked());
        assert(m_Editor->program().bind());

        updateMesh();
        refreshUniforms();
        pushUniforms();

        if(hasTexture())
            m_Texture->bind();

        m_Mesh->mVao.bind();
        if(m_Editor->requiresPatches()){
            glDrawElements(GL_PATCHES, m_Mesh->mNumIndices, GL_UNSIGNED_INT, (void*)0);
        }else{
            glDrawElements(GL_TRIANGLES, m_Mesh->mNumIndices, GL_UNSIGNED_INT, (void*)0);
        }
        m_Mesh->mVao.release();

        if(hasTexture())
            m_Texture->release();

        m_Editor->program().release();
    }


    update();

    //this->doneCurrent();
}
void ShaderFrame::resizeGL(int w, int h){
    this->makeCurrent();
    mUniforms.mWidth = w;
    mUniforms.mHeight = h;
    update();
    this->doneCurrent();
}

float ShaderFrame::getAspectRatio()                     { return (float)mUniforms.mWidth / (float)mUniforms.mHeight;           }
unsigned int ShaderFrame::getFrameNumber()              { return m_FrameNumber;                 }

bool ShaderFrame::hasEditor()                           { return m_Editor != nullptr;           }
bool ShaderFrame::hasProgram()                          {
    if(hasEditor()){
        return m_Editor->hasProgram();
    }
    return false;
}
bool ShaderFrame::hasMesh()                             { return m_Mesh != nullptr;             }
bool ShaderFrame::hasTexture()                          { return m_Texture != nullptr;          }

ShaderEditor& ShaderFrame::getEditor()                  { return *m_Editor;                     }
Mesh& ShaderFrame::getMesh()                            { return *m_Mesh;                       }
QOpenGLTexture& ShaderFrame::getTexture()               { return *m_Texture;                    }

void ShaderFrame::setEditor(ShaderEditor* set)          { m_Editor = set;  updateMesh();        }
void ShaderFrame::setMesh(Mesh* set)                    { m_Mesh = set;  updateMesh();          }
void ShaderFrame::setTexture(QOpenGLTexture* set)       { m_Texture = set;                      }

void ShaderFrame::mousePressEvent(QMouseEvent* mouse){
    if(mouse->button() == Qt::MouseButton::LeftButton){
        updatePressMouse(mouse->x(), mouse->y());
        isMouseLeftButtonDown = true;
    }
    refreshCamera();
}
void ShaderFrame::mouseMoveEvent(QMouseEvent* mouse){
        updateMouse(mouse->x(), mouse->y());
        refreshCamera();
}
void ShaderFrame::mouseReleaseEvent(QMouseEvent* mouse){
    if(mouse->button() == Qt::MouseButton::LeftButton){
        updateReleaseMouse(mouse->x(), mouse->y());
        isMouseLeftButtonDown = false;
    }
    updateDoubleSpinBoxes();
    refreshCamera();
}
void ShaderFrame::wheelEvent(QWheelEvent* wheel){
    int numDegrees = wheel->delta() / 8;
    int numSteps = numDegrees / 15;
    QVector3D dirToCenter = mUniforms.mCamera.mEye - mUniforms.mCamera.mCenter;
    dirToCenter.normalize();
    mUniforms.mCamera.mEye -= dirToCenter * numSteps * mMouseWheelSensitivity;

    refreshCamera();
}

void ShaderFrame::refreshCamera(){
    if(hasMesh()){
        if(isMouseLeftButtonDown){
            getMesh().mRotation.setY(getMesh().mRotation.y() - (mLastMouseX - mMouseX) * mMouseMoveSensitivity);
            getMesh().mRotation.setX(getMesh().mRotation.x() + (mLastMouseY - mMouseY) * mMouseMoveSensitivity);
        }
    }
}
void ShaderFrame::resetCamera(){
    mUniforms.mCamera.mEye      = QVector3D(0, 0,-1);
    mUniforms.mCamera.mCenter   = QVector3D(0, 0, 0);
    mUniforms.mCamera.mUp       = QVector3D(0, 1, 0);
}
void ShaderFrame::setTranslateDSB(QDoubleSpinBox* tx, QDoubleSpinBox* ty, QDoubleSpinBox* tz){
    assert(tx != nullptr), assert(ty != nullptr), assert(tz != nullptr);
    dsbTranslateX = tx;dsbTranslateY = ty;dsbTranslateZ = tz;
}
void ShaderFrame::setScaleDSB(QDoubleSpinBox* sx, QDoubleSpinBox* sy, QDoubleSpinBox* sz){
    assert(sx != nullptr), assert(sy != nullptr), assert(sz != nullptr);
    dsbScaleX = sx;dsbScaleY = sy;dsbScaleZ = sz;
}
void ShaderFrame::setRotateDSB(QDoubleSpinBox* rx, QDoubleSpinBox* ry, QDoubleSpinBox* rz){
    assert(rx != nullptr), assert(ry != nullptr), assert(rz != nullptr);
    dsbRotateX = rx;dsbRotateY = ry;dsbRotateZ = rz;
}

void ShaderFrame::updateDoubleSpinBoxes(){
    if(hasMesh()){

        if(dsbTranslateX != nullptr && dsbTranslateX->isEnabled())
            {dsbTranslateX->setValue(getMesh().mTranslation.x());  dsbTranslateX->editingFinished();}
        if(dsbTranslateY != nullptr && dsbTranslateY->isEnabled())
            {dsbTranslateY->setValue(getMesh().mTranslation.y());  dsbTranslateY->editingFinished();}
        if(dsbTranslateZ != nullptr && dsbTranslateZ->isEnabled())
            {dsbTranslateZ->setValue(getMesh().mTranslation.z());  dsbTranslateZ->editingFinished();}

        if(dsbRotateX != nullptr && dsbRotateX->isEnabled())
            {dsbRotateX->setValue((int)getMesh().mRotation.x() % 360);  dsbRotateX->editingFinished();}
        if(dsbRotateY != nullptr && dsbRotateY->isEnabled())
            {dsbRotateY->setValue((int)getMesh().mRotation.y() % 360);  dsbRotateY->editingFinished();}
        if(dsbRotateZ != nullptr && dsbRotateZ->isEnabled())
            {dsbRotateZ->setValue((int)getMesh().mRotation.z() % 360);  dsbRotateZ->editingFinished();}

        if(dsbScaleX != nullptr && dsbScaleX->isEnabled())
            {dsbScaleX->setValue(getMesh().mScale.x()); dsbScaleX->editingFinished();}
        if(dsbScaleY != nullptr && dsbScaleY->isEnabled())
            {dsbScaleY->setValue(getMesh().mScale.y()); dsbScaleY->editingFinished();}
        if(dsbScaleZ != nullptr && dsbScaleZ->isEnabled())
            {dsbScaleZ->setValue(getMesh().mScale.z()); dsbScaleZ->editingFinished();}

    }
}


