#ifndef SHADEREDITOR_H
#define SHADEREDITOR_H

#include <cassert>
#include <QString>
#include <QOpenGLShader>
#include <QOpenGLShaderProgram>
#include <iostream>
#include "mesh.h"

#define NUM_TYPES_SHADERS 5

class ShaderEditor
{
private:
    int m_I;
    QOpenGLShaderProgram* m_Program;
    QString* m_Code[NUM_TYPES_SHADERS];
    bool m_isShaderEnabled[NUM_TYPES_SHADERS];
    bool m_requiresPatches;
    unsigned int m_patches;

    static const QOpenGLShader::ShaderType SHADER_TYPES[NUM_TYPES_SHADERS];
    static const QString SHADER_TYPE_NAMES[NUM_TYPES_SHADERS+1];

public:
    bool hasVertex();
    bool hasControl();
    bool hasEvaluation();
    bool hasGeometry();
    bool hasFragment();
    bool hasShader();

    void compile() throw(QString);
    unsigned numShaders();

    const QString& currentShaderTypeName();

    void setPatches(const unsigned& patches);
    unsigned getPatches();
    bool requiresPatches();
    bool willRequirePatches();

    void setShader(const QOpenGLShader::ShaderType& type, const bool& set);
    void switchShader(const QOpenGLShader::ShaderType& type);
    void updateShader(QOpenGLShader::ShaderType type, const QString& code);

    void next();
    bool hasProgram()const;
    void setCurrentShader(const QString& code);
    const QString& getCurrentShader();
    QOpenGLShaderProgram& program()const;
    void previous();

    ShaderEditor(const QString& vert, const QString& cont, const QString& eval, const QString& geom, const QString& frag);
    ~ShaderEditor();

};

#endif // SHADEREDITOR_H
