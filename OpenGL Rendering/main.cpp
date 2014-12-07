//
//  main.cpp
//  OpenGL Rendering
//
//  Created by Emma Meersman on 11/27/14.
//  Copyright (c) 2014 Emma Meersman. All rights reserved.
//

#define _USE_MATH_DEFINES
#include <math.h>
#include <stdlib.h>

#include <OpenGL/gl.h>
#include <OpenGL/glu.h>
#include <GLUT/glut.h>

#include "float2.h"
#include "float3.h"
#include "Mesh.h"
#include <vector>
#include <map>

extern "C" unsigned char* stbi_load(char const *filename, int *x, int *y, int *comp, int req_comp);

float START_ROT = 90;
int NUM_TEAPOTS = 0;
bool gameWon;
bool balloonDrawn;
bool blastOff;

class LightSource
{
public:
	virtual float3 getpowerDensityAt  ( float3 x )=0;
	virtual float3 getLightDirAt  ( float3 x )=0;
	virtual float  getDistanceFrom( float3 x )=0;
	virtual void   apply( GLenum openglLightName )=0;
};

class DirectionalLight : public LightSource
{
	float3 dir;
	float3 powerDensity;
public:
	DirectionalLight(float3 dir, float3 powerDensity)
    :dir(dir), powerDensity(powerDensity){}
	float3 getpowerDensityAt  ( float3 x ){return powerDensity;}
	float3 getLightDirAt  ( float3 x ){return dir;}
	float  getDistanceFrom( float3 x ){return 900000000;}
	void   apply( GLenum openglLightName )
	{
		float aglPos[] = {dir.x, dir.y, dir.z, 0.0f};
        glLightfv(openglLightName, GL_POSITION, aglPos);
		float aglZero[] = {0.0f, 0.0f, 0.0f, 0.0f};
        glLightfv(openglLightName, GL_AMBIENT, aglZero);
		float aglIntensity[] = {powerDensity.x, powerDensity.y, powerDensity.z, 1.0f};
        glLightfv(openglLightName, GL_DIFFUSE, aglIntensity);
        glLightfv(openglLightName, GL_SPECULAR, aglIntensity);
        glLightf(openglLightName, GL_CONSTANT_ATTENUATION, 1.0f);
        glLightf(openglLightName, GL_LINEAR_ATTENUATION, 0.0f);
        glLightf(openglLightName, GL_QUADRATIC_ATTENUATION, 0.0f);
	}
};

class PointLight : public LightSource
{
	float3 pos;
	float3 power;
public:
	PointLight(float3 pos, float3 power)
    :pos(pos), power(power){}
	float3 getpowerDensityAt  ( float3 x ){return power*(1/(x-pos).norm2()*4*3.14);}
	float3 getLightDirAt  ( float3 x ){return (pos-x).normalize();}
	float  getDistanceFrom( float3 x ){return (pos-x).norm();}
	void   apply( GLenum openglLightName )
	{
		float aglPos[] = {pos.x, pos.y, pos.z, 1.0f};
        glLightfv(openglLightName, GL_POSITION, aglPos);
		float aglZero[] = {0.0f, 0.0f, 0.0f, 0.0f};
        glLightfv(openglLightName, GL_AMBIENT, aglZero);
		float aglIntensity[] = {power.x, power.y, power.z, 1.0f};
        glLightfv(openglLightName, GL_DIFFUSE, aglIntensity);
        glLightfv(openglLightName, GL_SPECULAR, aglIntensity);
        glLightf(openglLightName, GL_CONSTANT_ATTENUATION, 0.0f);
        glLightf(openglLightName, GL_LINEAR_ATTENUATION, 0.0f);
        glLightf(openglLightName, GL_QUADRATIC_ATTENUATION, 0.25f / 3.14f);
	}
};

class Material
{
public:
	float3 kd;			// diffuse reflection coefficient
	float3 ks;			// specular reflection coefficient
	float shininess;	// specular exponent
	Material()
	{
		kd = float3(0.5, 0.5, 0.5) + kd * 0.5;
		ks = float3(1, 1, 1);
		shininess = 15;
	}
	virtual void apply()
	{
		float aglDiffuse[] = {kd.x, kd.y, kd.z, 1.0f};
		glMaterialfv(GL_FRONT_AND_BACK, GL_DIFFUSE, aglDiffuse);
		float aglSpecular[] = {kd.x, kd.y, kd.z, 1.0f};
		glMaterialfv(GL_FRONT_AND_BACK, GL_SPECULAR, aglSpecular);
		if(shininess <= 128)
			glMaterialf(GL_FRONT_AND_BACK, GL_SHININESS, shininess);
		else
			glMaterialf(GL_FRONT_AND_BACK, GL_SHININESS, 128.0f);
	}
    virtual void bind(){};
};

class TexturedMaterial : public Material {
    unsigned int textureName;
public:
    TexturedMaterial(const char* filename,
                     GLint filtering = GL_LINEAR_MIPMAP_LINEAR
                     ){
        unsigned char* data;
        int width;
        int height;
        int nComponents = 4;
        
        data = stbi_load(filename, &width, &height, &nComponents, 0);
        
        if(data == NULL) return;
        
        // opengl texture creation below
        
        glGenTextures(1, &textureName);  // id generation
        glBindTexture(GL_TEXTURE_2D, textureName);      // binding
        
        if(nComponents == 4)
            gluBuild2DMipmaps(GL_TEXTURE_2D, GL_RGBA, width, height, GL_RGBA, GL_UNSIGNED_BYTE, data);
        else if(nComponents == 3)
            gluBuild2DMipmaps(GL_TEXTURE_2D, GL_RGB, width, height, GL_RGB, GL_UNSIGNED_BYTE, data);
        
        delete data;
    }
    void apply() {
        Material::apply();
        glEnable(GL_TEXTURE_2D);
        glBindTexture(GL_TEXTURE_2D, textureName);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
    }
};

// Object abstract base class.
class Object
{
protected:
	Material* material;
	float3 scaleFactor;
	float3 orientationAxis;
public:
    float orientationAngle;
    float3 position;
	Object(Material* material):material(material),position(0, 0, 0),orientationAngle(0.0f),scaleFactor(1.0,1.0,1.0),orientationAxis(0.0,1.0,0.0){}
    virtual ~Object(){}
    Object* translate(float3 offset){
        position += offset; return this;
    }
    Object* scale(float3 factor){
        scaleFactor *= factor; return this;
    }
    Object* rotate(float angle){
        orientationAngle += angle; return this; // degrees
    }
    virtual void draw()
    {
		material->apply();
        // apply scaling, translation and orientation
		glMatrixMode(GL_MODELVIEW);
		glPushMatrix();
        glTranslatef(position.x, position.y, position.z);
        glRotatef(orientationAngle, orientationAxis.x, orientationAxis.y, orientationAxis.z);
        glScalef(scaleFactor.x, scaleFactor.y, scaleFactor.z);
        drawModel();
		glPopMatrix();
    }
    virtual void drawModel()=0;
    virtual void drawShadow(float3 lightDir) {
        float shear[] = {
            1, 0, 0, 0,
            -lightDir.x, 1, -lightDir.z, 0,
            0, 0, 1, 0,
            0, 0, 0, 1 };
        
		glMatrixMode(GL_MODELVIEW);
		glPushMatrix();
    
        glMultMatrixf(shear);
        glTranslatef(0, 0.01, 0);
        glScalef(1,0,1);
        
        glTranslatef(position.x, position.y, position.z);
        glRotatef(orientationAngle, orientationAxis.x, orientationAxis.y, orientationAxis.z);
        glScalef(scaleFactor.x, scaleFactor.y, scaleFactor.z);
        drawModel();
		glPopMatrix();
    }
    virtual void move(double t, double dt){}
    virtual bool control(std::vector<bool>& keysPressed, std::vector<Object*>& spawn, std::vector<Object*>& objects){return false;}
};

class Teapot : public Object
{
public:
	Teapot(Material* material):Object(material){}
	void drawModel()
	{
		glutSolidTeapot(1.0f);
	}
};

class Ground : public Object {
    float3 start;
    float size;
public:
	Ground(Material* material, float3 startingLoc, float size)
        :Object(material), start(startingLoc), size(size){}
	void drawModel()
	{
        float scaleF = 50;
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        
        glEnable(GL_TEXTURE_2D);
        
        material->apply();
        glTexParameteri(GL_TEXTURE_2D,
                        GL_TEXTURE_MIN_FILTER,GL_LINEAR_MIPMAP_LINEAR);
        glTexParameteri(GL_TEXTURE_2D,
                        GL_TEXTURE_MAG_FILTER,GL_LINEAR_MIPMAP_LINEAR);
        glTexEnvi(GL_TEXTURE_ENV,
                  GL_TEXTURE_ENV_MODE, GL_REPLACE);
        
        glBegin(GL_QUADS);
        glTexCoord2d((-size+start.x)/scaleF,(-size+start.z)/scaleF);
        glVertex3d(-size+start.x,0+start.y,-size+start.z);
        glTexCoord2d((size+start.x)/scaleF,(-size+start.z)/scaleF);
        glVertex3d(size+start.x,0+start.y,-size+start.z);
        glTexCoord2d((size+start.x)/scaleF,(size+start.z)/scaleF);
        glVertex3d(size+start.x,0+start.y,size+start.z);
        glTexCoord2d((-size+start.x)/scaleF,(size+start.z)/scaleF);
        glVertex3d(-size+start.x,0+start.y,size+start.z);
        glEnd();
        
        glDisable(GL_TEXTURE_2D);
        glDisable(GL_BLEND);

	}
    void drawShadow(float3 lightDir) {
        position = {0,0,0};
    }
};

class MeshInstance : public Object
{
    Mesh* mesh;
    Material* material;
public:
	MeshInstance(Mesh* mesh, Material* material):Object(material){
        this->mesh = mesh;
        this->material = material;
    }
	void drawModel()
	{
		mesh->draw();
	}
};

class Balloon : public MeshInstance
{
    float3 acceleration;
    float angularVelocity;
    float angularAccel;
    float restitution;
public:
    float3 velocity;

    Balloon(Mesh* mesh, Material* material)
    :MeshInstance(mesh, material), velocity(float3{0,0,0}) {}
    
    void move(double t, double dt) {
        position = position + velocity*dt;
    }
};

class Avatar : public MeshInstance {
    float3 acceleration;
    float angularVelocity;
    float angularAccel;
    float restitution;
public:
    float3 velocity;

    Avatar(float rest, Mesh* mesh, Material* material)
    :MeshInstance(mesh, material), velocity(float3{0,0,0}) {
        //this->velocity = vel;
        //this->angularVelocity = angVel;
        this->restitution = rest;
    }
    void move(double t, double dt) {
        float3 projPos = position + velocity*dt;
        if(position.x > 99) {
            if(projPos.x <= position.x) {
                position = projPos;
                velocity = velocity + acceleration*dt;
            } else {
                velocity = float3(0,0,0);
            }
        } else if(position.x < -99) {
            if(projPos.x >= position.x) {
                position = projPos;
                velocity = velocity + acceleration*dt;
            } else {
                velocity = float3(0,0,0);
            }
        } else if(position.z > 99) {
            if(projPos.z <= position.z) {
                position = projPos;
                velocity = velocity + acceleration*dt;
            } else {
                velocity = float3(0,0,0);
            }
        } else if(position.z < -99) {
            if(projPos.z >= position.z) {
                position = projPos;
                velocity = velocity + acceleration*dt;
            } else {
                velocity = float3(0,0,0);
            }
        } else {
            velocity = velocity + acceleration*dt;
            position = position + velocity*dt;
        }
        if(position.y < 0) {
            velocity.y *= -restitution;
            position.y = 0;
        }
        
        velocity *= pow(0.8, dt);
        
        angularVelocity = angularVelocity + angularAccel*dt;
        orientationAngle = orientationAngle + angularVelocity*dt;
        angularVelocity *= pow(0.8, dt);

    }
    virtual Object* rotate(float angle){
        orientationAngle += angle; return this; // degrees
    }
    virtual bool control(std::vector<bool>& keysPressed, std::vector<Object*>& spawn,
                         std::vector<Object*>& objects){
        if(keysPressed['a'] == true) {
            angularAccel = 100;
        } else if (keysPressed['d'] == true) {
            angularAccel = -100;
        } else {
            angularAccel = 0;
        }
        // convert orientationAngle to radians from degrees
        float tempOrientationAngle = orientationAngle * (M_PI/180);

        if(keysPressed['w']) {
            acceleration = float3((-cos(tempOrientationAngle)*10), -10, (sin(tempOrientationAngle) *10));
        } else if (keysPressed['s']) {
            acceleration = float3((cos(tempOrientationAngle) *10), -10, (-sin(tempOrientationAngle) *10));
        } else {
            acceleration = {0,-10,0};
        }
        return false;
    }
};

Avatar *player = nullptr;
Balloon* balloon;

// Skeletal Camera class. Feel free to add custom initialization, set aspect ratio to fit viewport dimensions, or animation.
class Camera
{
    float3 lookAt;
    
	float3 right;
	float3 up;
    
    float2 lastMousePos;
    float2 mouseDelta;
    
	float fov;
	float aspect;
public:
    float3 eye;
    float3 ahead;

	float3 getEye()
	{
		return eye;
	}
	Camera()
	{
		//eye = float3(0, 5, 5);
        eye = float3(0,3,0);
		ahead = float3(0, -1, 0);
		right = float3(1, 0, 0);
		up = float3(0, 1, 0);
		fov = 1.5;
		aspect = 1;
	}
    
	void apply()
	{
		glMatrixMode(GL_PROJECTION);
		glLoadIdentity();
		gluPerspective(fov / 3.14 * 180, aspect, 0.1, 200);
		glMatrixMode(GL_MODELVIEW);
		glLoadIdentity();
		gluLookAt(eye.x, eye.y, eye.z, lookAt.x, lookAt.y, lookAt.z, 0.0, 1.0, 0.0);
	}
    
    void startDrag(int x, int y) {
        lastMousePos = float2(x, y);
    }
    void drag(int x, int y){
        float2 mousePos(x, y);
        mouseDelta = mousePos - lastMousePos;
        lastMousePos = mousePos;
    }
    void endDrag(){
        mouseDelta = float2(0, 0);
    }
    
    void move(float3 loc, float rot, float dt, std::vector<bool>& keysPressed) {
        eye = float3(loc.x, loc.y+20, loc.z);
        
        float yaw = atan2f( ahead.x, ahead.z );
        float pitch = -atan2f( ahead.y,
                              sqrtf(ahead.x * ahead.x + ahead.z * ahead.z) );
        
        yaw = (rot-START_ROT)/(57.2);
        //yaw -= mouseDelta.x * 0.02f;
        //pitch += mouseDelta.y * 0.02f;
        pitch = 0.77;
        if(pitch > 3.14/2) pitch = 3.14/2;
        if(pitch < -3.14/2) pitch = -3.14/2;
        
        mouseDelta = float2(0, 0);
        
        ahead = float3(sin(yaw)*cos(pitch), -sin(pitch),
                       cos(yaw)*cos(pitch) );
        right = ahead.cross(float3(0, 1, 0)).normalize();
        up = right.cross(ahead);
        lookAt = eye + ahead;
    }
    
    void setAspectRatio(float ar)  {
        aspect = ar;
    }
};

class Scene
{
	Camera camera;
	std::vector<LightSource*> lightSources;
	std::vector<Material*> materials;
    std::vector<Mesh*> meshes;
public:
    std::vector<Object*> objects;
    std::vector<Object*> teapots;

	Scene()
	{
		lightSources.push_back(new DirectionalLight(float3(10, 1, 0),
                                                    float3(1, 0.5, 1)));
        lightSources.push_back(new DirectionalLight(float3(-10, 1, 0),
                                                    float3(1, 0.5, 1)));
		lightSources.push_back(new PointLight(float3(-1, -1, 1),
                                              float3(0.2, 0.1, 0.1)));
	}
	~Scene()
	{
		for (std::vector<LightSource*>::iterator iLightSource = lightSources.begin(); iLightSource != lightSources.end(); ++iLightSource)
			delete *iLightSource;
		for (std::vector<Material*>::iterator iMaterial = materials.begin(); iMaterial != materials.end(); ++iMaterial)
			delete *iMaterial;
		for (std::vector<Object*>::iterator iObject = objects.begin(); iObject != objects.end(); ++iObject)
			delete *iObject;
        for (std::vector<Mesh*>::iterator iMesh = meshes.begin(); iMesh != meshes.end(); ++iMesh)
			delete *iMesh;
        for (std::vector<Object*>::iterator iTeapot = teapots.begin(); iTeapot != teapots.end(); ++iTeapot)
			delete *iTeapot;
	}
    
public:
	Camera& getCamera()
	{
		return camera;
	}
    
	void draw()
	{
		camera.apply();
		unsigned int iLightSource=0;
        float3 lightDir = float3(0,0,0);
		for (; iLightSource<lightSources.size(); iLightSource++)
		{
			glEnable(GL_LIGHT0 + iLightSource);
			lightSources.at(iLightSource)->apply(GL_LIGHT0 + iLightSource);
            lightDir = lightDir + lightSources.at(iLightSource)->getLightDirAt(float3(0, 0, 0));

		}
		for (; iLightSource<GL_MAX_LIGHTS; iLightSource++)
			glDisable(GL_LIGHT0 + iLightSource);
        
        for (unsigned int iObject=0; iObject<objects.size(); iObject++)
			objects.at(iObject)->draw();
        for (unsigned int iTeapot=0; iTeapot<teapots.size(); iTeapot++)
			teapots.at(iTeapot)->draw();
        
        glDisable(GL_TEXTURE_2D);
        glDisable(GL_LIGHTING);

        glColor3d(0,0,0);
        for(Object *o : objects) {
            
            o->drawShadow(lightDir);
        }
        for(Object *t : teapots) {
            
            t->drawShadow(lightDir);
        }
        
        glEnable(GL_TEXTURE_2D);
        glEnable(GL_LIGHTING);
        
	}
    
    void initialize() {
        
        TexturedMaterial* balloonSkin = new TexturedMaterial("/Users/emeersman/Documents/AIT/Graphics/OpenGL Rendering/OpenGL Rendering/balloon.png", GL_LINEAR);
        materials.push_back(balloonSkin);
        
        Mesh* balloonMesh = new Mesh("/Users/emeersman/Documents/AIT/Graphics/OpenGL Rendering/OpenGL Rendering/balloon.obj");
        meshes.push_back(balloonMesh);
        
        Material* red = new Material();
        red->kd = float3(1, 0, 0);
		materials.push_back(red);
        
        Material* orange = new Material();
        orange->kd = float3(1, 0.6, 0);
		materials.push_back(orange);
        
        Material* yellow = new Material();
        yellow->kd = float3(1, 1, 0.8);
		materials.push_back(yellow);
        
        Material* green = new Material();
        green->kd = float3(0.3, 1, 0.5);
		materials.push_back(green);
        
        Material* blue = new Material();
        blue->kd = float3(0.2, .5, 1);
		materials.push_back(blue);
        
        Material* purple = new Material();
        purple->kd = float3(0.6, 0.2, 1);
		materials.push_back(purple);
        
		teapots.push_back((new Teapot(red))
                          ->translate(float3(-90, 1, -40))
                          ->scale(float3(1.5, 1.5, 1.5)) );
		teapots.push_back((new Teapot(orange))
                          ->translate(float3(-20, 1, 10))
                          ->scale(float3(1.5, 1.5, 1.5)) );
        teapots.push_back((new Teapot(yellow))
                          ->translate(float3(40, 1, 70))
                          ->scale(float3(1.5, 1.5, 1.5)) );
		teapots.push_back((new Teapot(green))
                          ->translate(float3(80, 1, -30))
                          ->scale(float3(1.5, 1.5, 1.5)) );
        teapots.push_back((new Teapot(blue))
                          ->translate(float3(0, 1, -70))
                          ->scale(float3(1.5, 1.5, 1.5)) );
		teapots.push_back((new Teapot(purple))
                          ->translate(float3(60, 1, 10))
                          ->scale(float3(1.5, 1.5, 1.5)) );
        NUM_TEAPOTS = teapots.size();
        

        TexturedMaterial* sand = new TexturedMaterial("/Users/emeersman/Documents/AIT/Graphics/OpenGL Rendering/OpenGL Rendering/sand.jpg", GL_LINEAR);
        materials.push_back(sand);
        
        TexturedMaterial* water = new TexturedMaterial("/Users/emeersman/Documents/AIT/Graphics/OpenGL Rendering/OpenGL Rendering/water.jpg", GL_LINEAR);
        materials.push_back(water);
        
        objects.push_back(new Ground(sand, float3(0,0,0), 100));
        
        objects.push_back(new Ground(water, float3(300,0,0), 200));
        objects.push_back(new Ground(water, float3(-300,0,0), 200));
        objects.push_back(new Ground(water, float3(0,0,300), 200));
        objects.push_back(new Ground(water, float3(0,0,-300), 200));


        Mesh* tigger = new Mesh("/Users/emeersman/Documents/AIT/Graphics/OpenGL Rendering/OpenGL Rendering/tigger.obj");
        meshes.push_back(tigger);
        
        TexturedMaterial* tiggerSkin = new TexturedMaterial("/Users/emeersman/Documents/AIT/Graphics/OpenGL Rendering/OpenGL Rendering/tigger.png", GL_LINEAR);
        materials.push_back(tiggerSkin);
        
        player = new Avatar(1,tigger,tiggerSkin);
        player->scale(float3(0.2,0.2,0.2));
        player->translate(float3(0,2,0));
        objects.push_back(player);
    }
    
    void move(double t, double dt) {
        for(Object *o : objects) {
            o->move(t, dt);
        }
    }
    
    void control(std::vector<bool>& keysPressed)
    {
        std::vector<Object*> spawn;
        for (unsigned int iObject=0; iObject<objects.size(); iObject++)
            objects.at(iObject)->control(keysPressed, spawn, objects);
        for (unsigned int iTeapot=0; iTeapot<teapots.size(); iTeapot++)
            teapots.at(iTeapot)->control(keysPressed, spawn, teapots);
    }
    
    int collide() {
        for (unsigned int iTeapot=0; iTeapot<teapots.size(); iTeapot++){
            float dist = sqrt((pow(teapots.at(iTeapot)->position.x - player->position.x, 2)) +
                              (pow(teapots.at(iTeapot)->position.z - player->position.z, 2)));
            if(dist < 5) {
                return iTeapot;
            }
        }
        return -1;
    }
    
    void endGame(double t, double dt) {
        if(!balloonDrawn) {
            balloon = new Balloon(meshes.at(0),materials.at(0));
            balloon->translate(float3(0,26,0));
            balloon->scale(float3(1,1.5,1));
            objects.push_back(balloon);
            balloonDrawn = true;
        }
        
        float dist = sqrt((pow(balloon->position.x - player->position.x, 2)) +
                          (pow(balloon->position.z - player->position.z, 2)));
        if(dist < 5) {
            blastOff = true;
            player->position = float3(0,balloon->position.y - 24,0);
            player->velocity = float3(0,4,0);
            balloon->velocity = float3(0,4,0);
            getCamera().eye = float3(15,3,0);
            getCamera().ahead = float3(-15, 30, 0);
        }
    }
};

// global application data

// screen resolution
const int screenWidth = 600;
const int screenHeight = 600;

//scene object
Scene scene;

//global
std::vector<bool> keysPressed;

void onKeyboard(unsigned char key, int x, int y) {
    keysPressed.at(key) = true;
}

void onKeyboardUp(unsigned char key, int x, int y) {
    keysPressed.at(key) = false;
}

void onMouse(int button, int state, int x, int y) {
    if(button == GLUT_LEFT_BUTTON)
        if(state == GLUT_DOWN)
            scene.getCamera().startDrag(x, y);
        else
            scene.getCamera().endDrag();
}

void onMouseMotion(int x, int y) {
    scene.getCamera().drag(x, y);
}

void onReshape(int winWidth, int winHeight) {
    glViewport(0, 0, winWidth, winHeight);
    scene.getCamera().setAspectRatio(
                                     (float)winWidth/winHeight);
}

// Displays the image.
void onDisplay( ) {
    glClearColor(0.1f, 0.2f, 0.3f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT); // clear screen
    
	scene.draw();
    
    glutSwapBuffers(); // drawing finished
}

int score;

void onIdle() {
    double t = glutGet(GLUT_ELAPSED_TIME) * 0.001;
    static double lastTime = 0.0;
    double dt = t - lastTime;
    lastTime = t;
    
    scene.control(keysPressed);
    scene.move(t, dt);
    
    int collided = scene.collide();
    if(collided != -1) {
        scene.teapots.erase(scene.teapots.begin() + collided);
        ++score;
    }
    
    if(score == NUM_TEAPOTS) {
        gameWon = true;
    }
    
    if(gameWon) {
        scene.endGame(t,dt);
    }
    
    float3 loc = player->position;
    float rot = player->orientationAngle;
    if(!blastOff){
        scene.getCamera().move(loc, rot, dt, keysPressed);
    }
    glutPostRedisplay();
}

int main(int argc, char **argv) {
    glutInit(&argc, argv);						// initialize GLUT
    glutInitWindowSize(screenWidth, screenHeight);				// startup window size
    glutInitWindowPosition(100, 100);           // where to put window on screen
    glutInitDisplayMode(GLUT_RGBA | GLUT_DOUBLE | GLUT_DEPTH);    // 8 bit R,G,B,A + double buffer + depth buffer
    
    glutCreateWindow("OpenGL teapots");				// application window is created and displayed
    
    glViewport(0, 0, screenWidth, screenHeight);
    
    glutDisplayFunc(onDisplay);					// register callback
    glutIdleFunc(onIdle);						// register callback
    glutKeyboardFunc(onKeyboard);               // register callback
    glutKeyboardUpFunc(onKeyboardUp);           // register callback
    for(int i=0; i<256; i++)
        keysPressed.push_back(false);
    glutMouseFunc(onMouse);                     // register callback
    glutMotionFunc(onMouseMotion);              // register callback
    glutReshapeFunc(onReshape);                 // register callback
    
	glEnable(GL_LIGHTING);
	glEnable(GL_DEPTH_TEST);
    
    score = 0;
    gameWon = false;
    balloonDrawn = false;
    blastOff = false;
    
    
    scene.initialize();
    
    glutMainLoop();								// launch event handling loop
    
    return 0;
}
