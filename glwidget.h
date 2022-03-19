#ifndef GLWIDGET_H
#define GLWIDGET_H
#define _USE_MATH_DEFINES

#include <cmath>
#include <QColorDialog>
#include <QWidget>
#include <QOpenGLWidget>
#include <QOpenGLFunctions>
#include <QDir>
#include <QFileDialog>
#include <QString>
#include <QOpenGLBuffer>
#include <QOpenGLShader>
#include <QOpenGLShaderProgram>
#include <QTimer>
#include <QOpenGLFunctions>
#include <QFileDialog>
#include <QKeyEvent>
#include <QApplication>
#include <iostream>
#include <fstream>
#include <math.h>
#include <limits>
#include <camera.h>
#include <trackball.h>
#include <light.h>
#include <material.h>

class GLWidget : public QOpenGLWidget, protected QOpenGLFunctions
{
    Q_OBJECT
public:
    explicit GLWidget(QWidget *parent = nullptr);  
    virtual ~GLWidget();
    QString formFilePath;

protected:
    void initializeGL() override;
    void paintGL() override;
    void resizeGL(int w, int h) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseReleaseEvent( QMouseEvent *event) override;
    void wheelEvent(QWheelEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;

private:
    void readOFFFile(const QString &fileName);
    void genNormals();
    void genTexCoordsCylinder();
    void genTangents();
    void createVBOs();
    void destroyVBOs();
    void createShaders();
    void destroyShaders();

    unsigned int numVertices;
    unsigned int numFaces;
    unsigned int currentShader;

    QPointF pixelPosToViewPos(const QPointF &p);
    QVector4D *vertices;
    QVector3D *normals;
    QVector2D *texCoords;
    QVector4D *tangents;
    std::vector<unsigned int> indices;

    QOpenGLBuffer *vboVertices;
    QOpenGLBuffer *vboNormals;
    QOpenGLBuffer *vboTexCoords;
    QOpenGLBuffer *vboTangents;
    QOpenGLBuffer *vboIndices;

    QOpenGLShader *vertexShader;
    QOpenGLShader *fragmentShader;
    QOpenGLShaderProgram *shaderProgram;
    GLuint texID[2];
    QMatrix4x4 modelViewMatrix;
    QMatrix4x4 projectionMatrix;

    Camera camera;
    Light light;
    Material material;
    TrackBall trackBall;

    double zoom;

    QTimer timer;

signals:
    void statusBarMessage(QString ns);

public slots:
    void toggleBackgroundColor();
    void browseFormFile();
    void animate();

};

#endif // GLWIDGET_H
