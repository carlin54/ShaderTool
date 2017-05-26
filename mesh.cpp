#include "mesh.h"



void Mesh::Clear(){
    if(mVao.isCreated())            mVao.destroy();
    if(mVboVerts.isCreated())       mVboVerts.destroy();
    if(mVboNormals.isCreated())     mVboNormals.destroy();
    if(mVboColors.isCreated())      mVboColors.destroy();
    if(mVboTexCoords.isCreated())   mVboTexCoords.destroy();
    if(mVboIndexBuffer.isCreated()) mVboIndexBuffer.destroy();

}

void Mesh::ConnectVao(){

    if(mVao.isCreated())
    {
        mVao.bind();
        if(mVboVerts.isCreated())           mVboVerts.bind();
        if(mVboNormals.isCreated())         mVboNormals.bind();
        if(mVboColors.isCreated())          mVboColors.bind();
        if(mVboTexCoords.isCreated())       mVboTexCoords.bind();
        if(mVboIndexBuffer.isCreated())     mVboIndexBuffer.bind();
        mVao.release();
    }
}




Mesh::Mesh()
    :   mVao                ()
    ,   mVboVerts           (QOpenGLBuffer::VertexBuffer)
    ,   mVboNormals         (QOpenGLBuffer::VertexBuffer)
    ,   mVboTexCoords       (QOpenGLBuffer::VertexBuffer)
    ,   mVboColors          (QOpenGLBuffer::VertexBuffer)
    ,   mVboIndexBuffer     (QOpenGLBuffer::IndexBuffer)
    ,   mTranslation(0, 0, 0)
    ,   mRotation(0, 0, 0)
    ,   mScale(1, 1, 1)
{

}

Mesh::~Mesh(){
    Clear();
}
/*
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

void Mesh::LoadMesh(const std::string& pFile)
{

    //check if file exists
    std::ifstream fin(pFile.c_str());
    if(!fin.fail()){
        fin.close();
    }else{
        printf("Couldn't open file: %s\n", pFile.c_str());
        printf("%s\n", importer.GetErrorString());
    }

    this->mScene = importer.ReadFile( pFile, aiProcessPreset_TargetRealtime_Quality);//|aiProcess_FlipWindingOrder);

    // If the import failed, report it
    if(!this->mScene){
        printf("%s\n", importer.GetErrorString());
    }

    // Now we can access the file's contents.
    printf("Import of scene %s succeeded.", pFile.c_str());

    GetBoundingBox(this->mScene, &this->mBbMin, &this->mBbMax);
    aiVector3D diff = this->mBbMax-this->mBbMin;
    float w = std::max(diff.x, std::max(diff.y, diff.z));

    this->mScaleFactor = 1.0f / w;

    BufferIndexedVerts(*this);
    this->mNumIndices = this->mScene->mMeshes[0]->mNumFaces* 3;

    this->ConnectVao();

}



void Mesh::LoadCube(){



}*/
