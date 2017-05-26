#include "shadereditor.h"

const QOpenGLShader::ShaderType ShaderEditor::SHADER_TYPES[NUM_TYPES_SHADERS] = {
    QOpenGLShader::Vertex,
    QOpenGLShader::TessellationControl,
    QOpenGLShader::TessellationEvaluation,
    QOpenGLShader::Geometry,
    QOpenGLShader::Fragment,
};
const QString ShaderEditor::SHADER_TYPE_NAMES[NUM_TYPES_SHADERS+1] = {
    "Vertex","Control","Evaluation","Geometry","Fragment","Shader"
};
bool ShaderEditor::hasVertex()    {   return m_isShaderEnabled[0];  }
bool ShaderEditor::hasControl()   {   return m_isShaderEnabled[1];  }
bool ShaderEditor::hasEvaluation(){   return m_isShaderEnabled[2];  }
bool ShaderEditor::hasGeometry()  {   return m_isShaderEnabled[3];  }
bool ShaderEditor::hasFragment()  {   return m_isShaderEnabled[4];  }
bool ShaderEditor::hasShader()    {   return hasVertex() || hasControl() || hasEvaluation() || hasGeometry() || hasFragment(); }

const QString& ShaderEditor::currentShaderTypeName(){
    return SHADER_TYPE_NAMES[m_I];
}

void ShaderEditor::next(){
    if(numShaders() == 0){
        m_I = 5;
    }else{
        m_I++;
        while(!m_isShaderEnabled[m_I]){
            m_I++;
            if(m_I > NUM_TYPES_SHADERS){
                m_I = 0;
            }

        }
    }
}
void ShaderEditor::previous(){
    if(numShaders() == 0){
        m_I = 5;
    }else{
        m_I--;
        while(!m_isShaderEnabled[m_I]){
            m_I--;
            if(m_I < 0){
                m_I = 4;
            }
        }
    }
}
unsigned ShaderEditor::numShaders(){
    int numOfShaders = 0;
    for(int i = 0; i < NUM_TYPES_SHADERS; i++){
        if(m_isShaderEnabled[i] == true){
            numOfShaders++;
        }
    }
    return numOfShaders;
}

void ShaderEditor::compile() throw(QString)
{
    QOpenGLShaderProgram* newProgram = new QOpenGLShaderProgram();

    std::cout << "Compiling... " << std::endl;
    for(unsigned i = 0; i < NUM_TYPES_SHADERS; i++){
        if(m_isShaderEnabled[i]){
            std::cout << "[ " << SHADER_TYPE_NAMES[i].toStdString() << " ]";
            newProgram->addShaderFromSourceCode(SHADER_TYPES[i], *m_Code[i]);
        }
        if(newProgram->log() != ""){
            throw QString(newProgram->log());
        }
    }
    std::cout << std::endl;
    std::cout << "Linking... ";
    newProgram->link();
    if(newProgram->log() != ""){
        throw QString(newProgram->log());
    }else{
        if(m_Program != nullptr){
            delete m_Program;
        }
        m_Program = newProgram;
    }
    assert(m_Program->isLinked());
    m_requiresPatches = willRequirePatches();
    std::cout << "Done!" << std::endl;

}
void ShaderEditor::setShader(const QOpenGLShader::ShaderType& type, const bool& set){

    for(int i = 0; i < NUM_TYPES_SHADERS; i++){
        if(SHADER_TYPES[i] == type){
            m_isShaderEnabled[i] = set;
            if(numShaders() == 0){
                m_I = 5;
            }else{
                if(m_I == 5) m_I = 0;
                if(!m_isShaderEnabled[m_I]) next();
            }
            break;
        }
    }
}

void ShaderEditor::switchShader(const QOpenGLShader::ShaderType& type){
    for(int i = 0; i < NUM_TYPES_SHADERS; i++){
        if(SHADER_TYPES[i] == type){
            m_isShaderEnabled[i] = !m_isShaderEnabled[i];
            if(numShaders() == 0){
                m_I = 5;
            }
            break;
        }
    }
}

bool ShaderEditor::hasProgram()const{
    if(m_Program != nullptr)
        assert(m_Program->isLinked());
    return m_Program != nullptr;
}
QOpenGLShaderProgram& ShaderEditor::program()const{
    return *m_Program;
}

const QString& ShaderEditor::getCurrentShader(){
    return *m_Code[m_I];
}
void ShaderEditor::setCurrentShader(const QString& code){
    if(m_I != 5){
        delete m_Code[m_I];
        m_Code[m_I] = new QString(code);
    }
}

bool ShaderEditor::willRequirePatches(){
    return hasControl() || hasEvaluation();
}
void ShaderEditor::setPatches(const unsigned& patches){
    assert(hasProgram());
    m_Program->setPatchVertexCount(patches);
}
unsigned ShaderEditor::getPatches(){
    assert(hasProgram());
    return m_Program->patchVertexCount();
}
bool ShaderEditor::requiresPatches(){
    return m_requiresPatches;
}

ShaderEditor::ShaderEditor(const QString& vert, const QString& cont, const QString& eval, const QString& geom, const QString& frag)
{
    const QString* shader_code[NUM_TYPES_SHADERS] = {&vert, &cont, &eval, &geom, &frag};
    m_I = 5;
    m_Program = nullptr;
    m_patches = 3;
    for(unsigned i = 0; i < NUM_TYPES_SHADERS; i++){
        m_isShaderEnabled[i] = (*shader_code[i] != "");
        if(m_I == 5 && (*shader_code[i] != "")) m_I = i;
        m_Code[i] = new QString(*shader_code[i]);
    }
}
ShaderEditor::~ShaderEditor(){
    for(unsigned i = 0; i < NUM_TYPES_SHADERS; i++){
        if(m_Code[i] != nullptr){
            delete m_Code[i];
        }
    }
    if(m_Program != nullptr){
        delete m_Program;
    }
}
