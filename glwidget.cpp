#include "glwidget.h"

GLWidget::GLWidget(QWidget *parent) : QOpenGLWidget(parent)
{
    vertices = NULL;
    normals = NULL;
    texCoords = NULL;
    tangents = NULL;
    indices = std::vector<unsigned int>(0, 0);

    vboVertices = NULL;
    vboNormals = NULL;
    vboTexCoords = NULL;
    vboTangents = NULL;
    vboIndices = NULL;

    shaderProgram = NULL;
    vertexShader = NULL;
    fragmentShader = NULL;
    currentShader = 0;

    zoom = 0.0;
}

GLWidget::~GLWidget()
{
    destroyVBOs();
    destroyShaders();
}

void GLWidget::initializeGL(){
    GLWidget::setFocusPolicy(Qt::StrongFocus);

    initializeOpenGLFunctions();
    setUpdatesEnabled(true);
    glEnable(GL_DEPTH_TEST);

    QImage texColor = QImage(":/textures/textures/1.jpg");
    QImage texNormal = QImage(":/textures/textures/2.jpg");
    QImage readyTexColor = texColor.convertToFormat(QImage::Format_RGBA8888);
    QImage readyTexNormal = texNormal.convertToFormat(QImage::Format_RGBA8888);

    glGenTextures(2, texID);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, texID[0]);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, readyTexColor.width(), readyTexColor.height(), 0, GL_RGBA, GL_UNSIGNED_BYTE, readyTexColor.constBits());
    glGenerateMipmap(GL_TEXTURE_2D);

    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, texID[1]);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, readyTexNormal.width(), readyTexNormal.height(), 0, GL_RGBA, GL_UNSIGNED_BYTE, readyTexNormal.constBits());
    glGenerateMipmap(GL_TEXTURE_2D);

    connect(&timer, SIGNAL(timeout()), this, SLOT(animate()));
    timer.start(0);
}

void GLWidget::paintGL(){
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    if(!vboVertices) return;

    //Model-view transformations
    modelViewMatrix.setToIdentity();
    modelViewMatrix.lookAt(camera.eye, camera.at, camera.up);
    modelViewMatrix.translate(0, 0, zoom);
    modelViewMatrix.rotate(trackBall.getRotation());

    //Binding of the shader program
    shaderProgram->bind();

    //Uploading of the uniform data to the GPU
    shaderProgram->setUniformValue("modelViewMatrix", modelViewMatrix);
    shaderProgram->setUniformValue("projectionMatrix", projectionMatrix);
    shaderProgram->setUniformValue("normalMatrix", modelViewMatrix.normalMatrix());

    QVector4D ambientProduct = light.ambient * material.ambient;
    QVector4D diffuseProduct = light.diffuse * material.diffuse;
    QVector4D specularProduct = light.specular * material.specular;

    shaderProgram->setUniformValue("lightPosition", light.position);
    shaderProgram->setUniformValue("ambientProduct", ambientProduct);
    shaderProgram->setUniformValue("diffuseProduct", diffuseProduct);
    shaderProgram->setUniformValue("specularProduct", specularProduct);
    shaderProgram->setUniformValue("shininess", static_cast<GLfloat>(material.shininess));

    shaderProgram->setUniformValue("texColorMap", 0);
    shaderProgram->setUniformValue("texNormalMap", 1);

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, texID[0]);
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, texID[1]);

    //Binding of buffer objects and textures to the GPU
    vboVertices->bind();
    shaderProgram->enableAttributeArray("vPosition");
    shaderProgram->setAttributeBuffer("vPosition", GL_FLOAT, 0, 4, 0);

    vboNormals->bind();
    shaderProgram->enableAttributeArray("vNormal");
    shaderProgram->setAttributeBuffer("vNormal", GL_FLOAT, 0, 3, 0);

    vboTexCoords->bind();
    shaderProgram->enableAttributeArray("vTexCoord");
    shaderProgram->setAttributeBuffer("vTexCoord", GL_FLOAT, 0, 2, 0);

    vboTangents->bind();
    shaderProgram->enableAttributeArray("vTangent");
    shaderProgram->setAttributeBuffer("vTangent", GL_FLOAT, 0, 4, 0);

    vboIndices->bind();

    glDrawElements(GL_TRIANGLES, numFaces * 3, GL_UNSIGNED_INT, 0);

    //Releasing of the buffer objects and the shader program
    vboIndices->release();
    vboTangents->release();
    vboTexCoords->release();
    vboNormals->release();
    vboVertices->release();
    shaderProgram->release();
}

void GLWidget::resizeGL(int w, int h){
    //w = width / h = height
    glViewport(0, 0, w, h);
    projectionMatrix.setToIdentity();
    projectionMatrix.perspective(60.0,
                        static_cast<qreal>(w) /
                        static_cast<qreal>(h), 0.1, 20.0);

    trackBall.resizeViewport(w, h);

    update();
}

void GLWidget::browseFormFile()
{
    formFilePath = QDir::toNativeSeparators(QFileDialog::getOpenFileName(this, tr("Carregar forma"), QDir::currentPath(),QString("%1 Files (*.%2)").arg(QString("OFF")).arg(QString("off"))));
    if (!formFilePath.isEmpty()) {
        readOFFFile(formFilePath);
        genNormals();
        genTexCoordsCylinder();
        genTangents();
        createVBOs();
        createShaders();
        update();
  }
}

void GLWidget::toggleBackgroundColor()
{
    QColor corFundo = QColorDialog::getColor(Qt::white, this, "Selecione a cor de fundo");
    makeCurrent();
    glClearColor(corFundo.redF(), corFundo.greenF(), corFundo.blueF(), corFundo.alphaF());
    update();
}

void GLWidget::readOFFFile(const QString &fileName)
{
    std::ifstream stream;
    stream.open(fileName.toLatin1(), std::ifstream::in);

    if(!stream.is_open()) {
        qWarning("Cannot open file.");
        return;
    }
    std::string line;

    stream >> line;
    stream >> numVertices >> numFaces >> line;

    delete[] vertices;
    vertices = new QVector4D[numVertices];

    indices.clear();
    indices = std::vector<unsigned int>(0, 0);

    if(numVertices > 0) {
        double minLim = std::numeric_limits <double>::min();
        double maxLim = std::numeric_limits <double>::max();
        QVector4D max(minLim , minLim , minLim , 1.0);
        QVector4D min(maxLim , maxLim , maxLim , 1.0);

        for(unsigned int i = 0; i < numVertices; i++) {
            float x, y, z;
            stream >> x >> y >> z;
            max.setX(qMax(max.x(), x));
            max.setY(qMax(max.y(), y));
            max.setZ(qMax(max.z(), z));
            min.setX(qMin(min.x(), x));
            min.setY(qMin(min.y(), y));
            min.setZ(qMin(min.z(), z));

            vertices[i] = QVector4D(x, y, z, 1.0);
        }

        QVector4D midpoint = (min + max) * 0.5;
        double invdiag = 1 / (max - min).length();

        for(unsigned int i = 0; i < numVertices; i++) {
            vertices[i] = (vertices[i] - midpoint)*invdiag;
            vertices[i].setW(1);
        }
    }

    unsigned int newNumFaces = 0;
    for(unsigned int i = 0; i < numFaces; i++)
    {
        int numVerticesFace;
        stream >> numVerticesFace;
        std::vector<unsigned int> faceAtual = std::vector<unsigned int>(0,0);
        for(int j = 0; j < numVerticesFace; j++){
            unsigned int numero;
            stream >> numero;
            faceAtual.push_back(numero);
        }
        for(int j = 0; j < numVerticesFace-2; j++){
            newNumFaces++;
            indices.push_back(faceAtual[0]);
            indices.push_back(faceAtual[j+1]);
            indices.push_back(faceAtual[j+2]);
        }
    }
    numFaces = newNumFaces;

    stream.close();

    emit statusBarMessage(QString("Samples %1, Faces %2")
                            .arg(numVertices)
                            .arg(numFaces));
}

void GLWidget::genNormals()
{
    delete[] normals;
    normals = new QVector3D[numVertices];

    for(unsigned int i = 0; i < numFaces ; i ++) {
        unsigned int i1 = indices[i * 3 ];
        unsigned int i2 = indices[i * 3 + 1];
        unsigned int i3 = indices[i * 3 + 2];

        QVector3D v1 = vertices[i1].toVector3D();
        QVector3D v2 = vertices[i2].toVector3D();
        QVector3D v3 = vertices[i3].toVector3D();

        QVector3D faceNormal = QVector3D::crossProduct(v2 -v1 , v3 - v1);
        normals[i1] += faceNormal;
        normals[i2] += faceNormal;
        normals[i3] += faceNormal;
    }

    for (unsigned int i = 0; i < numVertices ; i ++) {
        normals[i].normalize();
    }
}

void GLWidget::genTexCoordsCylinder()
{
    delete[] texCoords;
    texCoords = new QVector2D[numVertices];

    double minLim = std::numeric_limits<double>::min();
    double maxLim = std::numeric_limits<double>::max();
    QVector2D max(minLim, minLim);
    QVector2D min(maxLim, maxLim);

    for (unsigned int i = 0; i < numVertices; i ++) {
        QVector2D pos = vertices[i].toVector2D() ;
        max.setX(qMax(max.x(), pos.x()));
        max.setY(qMax(max.y(), pos.y()));
        min.setX(qMin(min.x(), pos.x()));
        min.setY(qMin(min.y(), pos.y()));
    }

    QVector2D size = max - min;
    for (unsigned int i = 0; i < numVertices; i ++) {
        double x = 2.0 * (vertices[i].x() - min.x()) / size.x() - 1.0;
        texCoords[i] = QVector2D(acos(x) / M_PI, (vertices[i].y() - min.y()) / size.y());
    }
}

void GLWidget::genTangents()
{
    delete[] tangents;
    tangents = new QVector4D[numVertices];
    QVector3D *bitangents = new QVector3D[numVertices];

    for(unsigned int i = 0; i < numFaces; i ++) {
        unsigned int i1 = indices[i * 3 ];
        unsigned int i2 = indices[i * 3 + 1];
        unsigned int i3 = indices[i * 3 + 2];

        QVector3D E = vertices[i1].toVector3D();
        QVector3D F = vertices[i2].toVector3D();
        QVector3D G = vertices[i3].toVector3D();

        QVector2D stE = texCoords[i1];
        QVector2D stF = texCoords[i2];
        QVector2D stG = texCoords[i3];

        QVector3D P = F - E;
        QVector3D Q = G - E;

        QVector2D st1 = stF - stE;
        QVector2D st2 = stG - stE;

        QMatrix2x2 M;
        M(0 ,0) = st2.y();
        M(0 ,1) = -st1.y();
        M(1 ,0) = -st2.x();
        M(1 ,1) = st1.x();
        M *= (1.0 / (st1.x() * st2.y() - st2.x() * st1.y()));

        QVector4D T = QVector4D(M(0, 0) * P.x() + M(0, 1) * Q.x(),
                            M(0, 0) * P.y() + M(0, 1) * Q.y(),
                            M(0, 0) * P.z() + M(0, 1) * Q.z(), 0.0);

        QVector3D B = QVector3D(M(1 ,0) * P.x() + M(1 ,1) * Q.x(),
                            M(1, 0) * P.y() + M(1, 1) * Q.y(),
                            M(1, 0) * P.z() + M(1, 1) * Q.z());

        tangents[i1] += T;
        tangents[i2] += T;
        tangents[i3] += T;
        bitangents[i1] += B;
        bitangents[i2] += B;
        bitangents[i3] += B;
    }

    for(unsigned int i = 0; i < numVertices; i ++){
            const QVector3D & n = normals[i];
            const QVector4D & t = tangents[i];

            tangents[i] = (t - n * QVector3D::dotProduct(n, t.toVector3D())).normalized();
            QVector3D b = QVector3D::crossProduct(n, t.toVector3D());
            double hand = QVector3D::dotProduct(b, bitangents[i]);
            tangents[i].setW((hand < 0.0) ? -1.0 : 1.0);
    }

    delete[] bitangents;
}

void GLWidget::createShaders()
{
    destroyShaders();
    QString vertexShaderFile[] = {
        ":/shaders/shaders/vgouraud.glsl",
        ":/shaders/shaders/vphong.glsl",
        ":/shaders/shaders/vtexture.glsl",
        ":/shaders/shaders/vnormal.glsl"
    };
    QString fragmentShaderFile[] = {
        ":/shaders/shaders/fgouraud.glsl",
        ":/shaders/shaders/fphong.glsl",
        ":/shaders/shaders/ftexture.glsl",
        ":/shaders/shaders/fnormal.glsl"
    };

    vertexShader = new QOpenGLShader(QOpenGLShader::Vertex);
    if (!vertexShader->compileSourceFile(vertexShaderFile[currentShader])) {
        qWarning() << vertexShader->log();
    }

    fragmentShader = new QOpenGLShader(QOpenGLShader::Fragment);
    if (!fragmentShader->compileSourceFile(fragmentShaderFile[currentShader])) {
        qWarning() << fragmentShader->log();
    }

    shaderProgram = new QOpenGLShaderProgram;
    shaderProgram->addShader(vertexShader);
    shaderProgram->addShader(fragmentShader);

    if (!shaderProgram->link()) {
        qWarning() << shaderProgram->log() << " ERRORRRR " << Qt::endl;
    }
}

void GLWidget::destroyShaders()
{
    delete vertexShader;
    vertexShader = NULL;
    delete fragmentShader;
    fragmentShader = NULL;

    if (shaderProgram) {
        shaderProgram->release();
        delete shaderProgram;
        shaderProgram = NULL;
    }
}

void GLWidget::createVBOs()
{
    destroyVBOs();

    vboVertices = new QOpenGLBuffer(QOpenGLBuffer::VertexBuffer);
    vboVertices->create();
    vboVertices->bind();
    vboVertices->setUsagePattern(QOpenGLBuffer::StaticDraw);
    vboVertices->allocate(vertices, numVertices * sizeof(QVector4D));
    delete[] vertices;
    vertices = NULL;

    vboNormals = new QOpenGLBuffer(QOpenGLBuffer::VertexBuffer);
    vboNormals->create();
    vboNormals->bind();
    vboNormals->setUsagePattern(QOpenGLBuffer::StaticDraw);
    vboNormals->allocate(normals, numVertices * sizeof(QVector3D));
    delete[] normals;
    normals = NULL;

    vboTexCoords = new QOpenGLBuffer(QOpenGLBuffer::VertexBuffer);
    vboTexCoords->create();
    vboTexCoords->bind();
    vboTexCoords->setUsagePattern(QOpenGLBuffer::StaticDraw);
    vboTexCoords->allocate(texCoords, numVertices * sizeof(QVector2D));
    delete[] texCoords;
    texCoords = NULL;

    vboTangents = new QOpenGLBuffer(QOpenGLBuffer::VertexBuffer);
    vboTangents->create();
    vboTangents->bind();
    vboTangents->setUsagePattern(QOpenGLBuffer::StaticDraw);
    vboTangents->allocate(tangents, numVertices * sizeof(QVector4D));
    delete[] tangents;
    tangents = NULL;

    vboIndices = new QOpenGLBuffer(QOpenGLBuffer::IndexBuffer);
    vboIndices->create();
    vboIndices->bind();
    vboIndices->setUsagePattern(QOpenGLBuffer::StaticDraw);

    int* copia = new int[indices.size()];
    std::copy(indices.begin(), indices.end(), copia);
    vboIndices->allocate(copia, numFaces * 3 * sizeof(unsigned int));
    indices.clear();
    indices = std::vector<unsigned int>(0, 0);
}

void GLWidget::destroyVBOs()
{
    if (vboVertices) {
        vboVertices->release();
        delete vboVertices;
        vboVertices = NULL;
    }

    if (vboNormals) {
        vboNormals->release();
        delete vboNormals;
        vboNormals = NULL;
    }

    if (vboTexCoords) {
        vboTexCoords->release();
        delete vboTexCoords;
        vboTexCoords = NULL;
    }

    if (vboTangents) {
        vboTangents->release();
        delete vboTangents;
        vboTangents = NULL;
    }

    if (vboIndices) {
        vboIndices->release();
        delete vboIndices;
        vboIndices = NULL;
    }
}

void GLWidget::keyPressEvent(QKeyEvent *event)
{
    switch (event->key())
    {
        case Qt::Key_1:
            currentShader = 0;
            createShaders();
            update();
            break;
        case Qt::Key_2:
            currentShader = 1;
            createShaders();
            update();
            break;
        case Qt::Key_3:
            currentShader = 2;
            createShaders();
            update();
            break;
        case Qt::Key_4:
            currentShader = 3;
            createShaders();
            update();
            break;
        case Qt::Key_Escape:
            qApp->quit();
    }
}

void GLWidget::mouseMoveEvent(QMouseEvent *event)
{
    trackBall.mouseMove(event->pos());
}

void GLWidget::mousePressEvent(QMouseEvent *event)
{
    if (event->button() & Qt::LeftButton)
        trackBall.mousePress(event->pos());
}

void GLWidget::mouseReleaseEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton)
        trackBall.mouseRelease(event->pos());
}

void GLWidget::wheelEvent(QWheelEvent *event)
{
    zoom += 0.001 * event->angleDelta().y();
}

void GLWidget::animate()
{
    makeCurrent();
    update();
}
