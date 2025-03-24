/*
 * Program 3 base code - includes modifications to shape and initGeom in preparation to load
 * multi shape objects 
 * CPE 471 Cal Poly Z. Wood + S. Sueda + I. Dunn
 */

#include <iostream>
#include <glad/glad.h>
#include <cstdlib>
#include <ctime>
#include <chrono>

#include "GLSL.h"
#include "Program.h"
#include "Shape.h"
#include "MatrixStack.h"
#include "WindowManager.h"
#include "Texture.h"
#include "Spline.h"
#include "Bezier.h"

#define TINYOBJLOADER_IMPLEMENTATION
#include <tiny_obj_loader/tiny_obj_loader.h>

// value_ptr for glm
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtc/matrix_transform.hpp>

using namespace std;
using namespace glm;

class Application : public EventCallbacks
{

public:

	WindowManager * windowManager = nullptr;

	// Our shader program - use this one for Blinn-Phong
	std::shared_ptr<Program> prog;

	//Our shader program for textures
	std::shared_ptr<Program> texProg;

	//our geometry
	shared_ptr<Shape> sphere;
	shared_ptr<Shape> theBunny;
	shared_ptr<Shape> bunnyNoNormals;
	shared_ptr<Shape> dog;

	// for final project
	// cat model i currently have has no texture coordinates
	vector<shared_ptr<Shape>> catModel;
	vector<shared_ptr<Shape>> catTree;
	shared_ptr<Shape> catBed;
	shared_ptr<Shape> floor;
	shared_ptr<Shape> cube;

	//global data for ground plane - direct load constant defined CPU data to GPU (not obj)
	GLuint GrndBuffObj, GrndNorBuffObj, GrndTexBuffObj, GIndxBuffObj;
	int g_GiboLen;
	//ground VAO
	GLuint GroundVertexArrayID;

	//the image to use as a texture (ground)
	shared_ptr<Texture> texture0;
	shared_ptr<Texture> texture1;
	shared_ptr<Texture> texture2;

	// final project textures
	shared_ptr<Texture> floor_texture;
	shared_ptr<Texture> cat_tree_post;
	shared_ptr<Texture> cat_tree_cover;
	shared_ptr<Texture> wall_texture;

	//global data (larger program should be encapsulated)
	vec3 gMin;
	float gRot = 0;
	float gCamH = 0;

	// for material toggling
	int matCode = 0;

	//animation data
	float lightTrans = 0;
	float gTrans = -3;
	float sTheta = 0;
	float eTheta = 0;
	float hTheta = 0;

	// animation data for cat model
	float catLegRot = 0;

	float rotationAngle = 0;
	float moveOffset = 0;

	bool isAnimating = true;

	// camera data
	vec3 cameraPos = glm::vec3(0.0f, 0.0f, 3.0f);
	vec3 cameraFront = glm::vec3(0.0f, 0.0f, -1.0f);
	vec3 cameraUp = glm::vec3(0.0f, 1.0f, 0.0f);
	glm::mat4 view;

	float radius = 3.0f;
	float yaw = -90.0f;  // Left/right rotation
	float pitch = 0.0f;  // Up/down rotation
	float scrollSens = 0.1f;  // scroll sensitivity

	float cameraSpeed = 0.5f;

	// spline curve
	double g_phi, g_theta;
	vec3 spline_view = vec3(0, 0, 1);
	vec3 strafe = vec3(1, 0, 0);
	vec3 g_eye = vec3(0, 1, 0);
	vec3 g_lookAt = vec3(0, 1, -4);

	Spline splinepath[2];
	bool goCamera = false;

	void keyCallback(GLFWwindow *window, int key, int scancode, int action, int mods)
	{
		if (key == GLFW_KEY_ESCAPE && action == GLFW_PRESS)
		{
			glfwSetWindowShouldClose(window, GL_TRUE);
		}

		// move camera forward or backward
		if (key == GLFW_KEY_W && action == GLFW_PRESS) {
			cameraPos += cameraSpeed * cameraFront;
			view = glm::lookAt(cameraPos, cameraPos + cameraFront, cameraUp);
		}
		if (key == GLFW_KEY_S && action == GLFW_PRESS) {
			cameraPos -= cameraSpeed * cameraFront;
			view = glm::lookAt(cameraPos, cameraPos + cameraFront, cameraUp);
		}

		// translate camera left/right (should not rotate)
		if (key == GLFW_KEY_A && action == GLFW_PRESS) {
			cameraPos -= glm::normalize(
				glm::cross(cameraFront, cameraUp)) * cameraSpeed;
			view = glm::lookAt(cameraPos, cameraPos + cameraFront, cameraUp);
		}
		if (key == GLFW_KEY_D && action == GLFW_PRESS) {
			cameraPos += glm::normalize(
				glm::cross(cameraFront, cameraUp)) * cameraSpeed;
			view = glm::lookAt(cameraPos, cameraPos + cameraFront, cameraUp);
		}
		if (key == GLFW_KEY_G && action == GLFW_RELEASE) {
			goCamera = !goCamera;
		}

		
		//update camera height
		if (key == GLFW_KEY_R && action == GLFW_PRESS){
			gCamH  += 0.25;
		}
		if (key == GLFW_KEY_F && action == GLFW_PRESS){
			gCamH  -= 0.25;
		}

		// change object material
		if (key == GLFW_KEY_M && action == GLFW_PRESS) {
			matCode += 1;
		}

		if (key == GLFW_KEY_Q && action == GLFW_PRESS){
			lightTrans -= 1.0;
		}
		if (key == GLFW_KEY_E && action == GLFW_PRESS){
			lightTrans += 1.0;
		}
		if (key == GLFW_KEY_Z && action == GLFW_PRESS) {
			glPolygonMode( GL_FRONT_AND_BACK, GL_LINE );
		}
		if (key == GLFW_KEY_Z && action == GLFW_RELEASE) {
			glPolygonMode( GL_FRONT_AND_BACK, GL_FILL );
		}
	}

	void mouseCallback(GLFWwindow *window, int button, int action, int mods)
	{
		double posX, posY;

		if (action == GLFW_PRESS)
		{
			 glfwGetCursorPos(window, &posX, &posY);
			 cout << "Pos X " << posX <<  " Pos Y " << posY << endl;
		}
	}

	void scrollCallback(GLFWwindow* window, double deltaX, double deltaY) {
		// code for pitch and yaw variable updates
		yaw += deltaX * scrollSens;
		pitch -= deltaY * scrollSens;

		// debugging scrolling changes
		//cout << deltaX << " " << deltaY << endl;
		//cout << "Yaw: " << yaw << ", Pitch: " << pitch << endl;

		// pitch clamping
		if (pitch > glm::radians(80.0f)) pitch = glm::radians(80.0f);
		if (pitch < glm::radians(-80.0f)) pitch = glm::radians(-80.0f);
		
		// look-at position
		vec3 lookAtPoint;
		lookAtPoint.x = radius * cos(pitch) * cos(yaw);
		lookAtPoint.y = radius * sin(pitch);
		lookAtPoint.z = radius * cos(pitch) * cos((3.14159/2.0) - yaw);
		cameraFront = glm::normalize(lookAtPoint);

		// cout << "Projected lookAtPoint: " << lookAtPoint.x << " " << lookAtPoint.y << " " << lookAtPoint.z << endl;

		view = glm::lookAt(cameraPos, cameraPos + cameraFront, cameraUp);
	}

	void resizeCallback(GLFWwindow *window, int width, int height)
	{
		glViewport(0, 0, width, height);
	}

	void init(const std::string& resourceDirectory)
	{
		GLSL::checkVersion();

		// Set background color.
		glClearColor(.72f, .84f, 1.06f, 1.0f);
		// Enable z-buffer test.
		glEnable(GL_DEPTH_TEST);

		// Initialize the GLSL program that we will use for local shading
		prog = make_shared<Program>();
		prog->setVerbose(true);
		prog->setShaderNames(resourceDirectory + "/simple_vert.glsl", resourceDirectory + "/simple_frag.glsl");
		prog->init();
		prog->addUniform("P");
		prog->addUniform("V");
		prog->addUniform("M");
		prog->addUniform("MatAmb");
		prog->addUniform("MatDif");
		prog->addUniform("MatSpec");
		prog->addUniform("MatShine");
		prog->addUniform("lightPos");
		prog->addAttribute("vertPos");
		prog->addAttribute("vertNor");

		// Initialize the GLSL program that we will use for texture mapping
		texProg = make_shared<Program>();
		texProg->setVerbose(true);
		texProg->setShaderNames(resourceDirectory + "/tex_vert.glsl", resourceDirectory + "/tex_frag0.glsl");
		texProg->init();
		texProg->addUniform("P");
		texProg->addUniform("V");
		texProg->addUniform("M");
		texProg->addUniform("Texture0");
		texProg->addUniform("lightPos");
		texProg->addAttribute("vertPos");
		texProg->addAttribute("vertNor");
		texProg->addAttribute("vertTex");

		//read in a load the texture
		texture0 = make_shared<Texture>();
  		texture0->setFilename(resourceDirectory + "/grass.jpg");
  		texture0->init();
  		texture0->setUnit(0);
  		texture0->setWrapModes(GL_CLAMP_TO_EDGE, GL_CLAMP_TO_EDGE);

		texture1 = make_shared<Texture>();
		texture1->setFilename(resourceDirectory + "/flowers.jpg");
		texture1->init();
		texture1->setUnit(1);
		texture1->setWrapModes(GL_CLAMP_TO_EDGE, GL_CLAMP_TO_EDGE);

		texture2 = make_shared<Texture>();
		texture2->setFilename(resourceDirectory + "/crate.jpg");
		texture2->init();
		texture2->setUnit(2);
		texture2->setWrapModes(GL_CLAMP_TO_EDGE, GL_CLAMP_TO_EDGE);

		floor_texture = make_shared<Texture>();
		floor_texture->setFilename(resourceDirectory + "/wood_planks.jpg");
		floor_texture->init();
		floor_texture->setUnit(3);
		floor_texture->setWrapModes(GL_CLAMP_TO_EDGE, GL_CLAMP_TO_EDGE);

		wall_texture = make_shared<Texture>();
		wall_texture->setFilename(resourceDirectory + "/wall.jpg");
		wall_texture->init();
		wall_texture->setUnit(4);
		wall_texture->setWrapModes(GL_CLAMP_TO_EDGE, GL_CLAMP_TO_EDGE);

		cat_tree_post = make_shared<Texture>();
		cat_tree_post->setFilename(resourceDirectory + "/sisal_rope.jpg");
		cat_tree_post->init();
		cat_tree_post->setUnit(5);
		cat_tree_post->setWrapModes(GL_CLAMP_TO_EDGE, GL_CLAMP_TO_EDGE);

		cat_tree_cover = make_shared<Texture>();
		cat_tree_cover->setFilename(resourceDirectory + "/cat_tree_cover.jpg");
		cat_tree_cover->init();
		cat_tree_cover->setUnit(6);
		cat_tree_cover->setWrapModes(GL_CLAMP_TO_EDGE, GL_CLAMP_TO_EDGE);

		// init splines up and down
		splinepath[0] = Spline(glm::vec3(-6, 0, 5), glm::vec3(-1, -5, 5), glm::vec3(1, 5, 5), glm::vec3(2, 0, 5), 5);
		splinepath[1] = Spline(glm::vec3(2, 0, 5), glm::vec3(3, -2, 5), glm::vec3(-0.25, 0.25, 5), glm::vec3(0, 0, 5), 5);
	}

	void initGeom(const std::string& resourceDirectory)
	{
		//EXAMPLE set up to read one shape from one obj file - convert to read several
		// Initialize mesh
		// Load geometry
 		// Some obj files contain material information.We'll ignore them for this assignment.
 		vector<tinyobj::shape_t> TOshapes;
 		vector<tinyobj::material_t> objMaterials;
 		string errStr;
		//load in the mesh and make the shape(s)
 		bool rc = tinyobj::LoadObj(TOshapes, objMaterials, errStr, (resourceDirectory + "/sphere.obj").c_str());
		if (!rc) {
			cerr << errStr << endl;
		} else {
			sphere = make_shared<Shape>();
			sphere->createShape(TOshapes[0]);
			sphere->measure();
			sphere->init();
		}
		//read out information stored in the shape about its size - something like this...
		//then do something with that information.....
		gMin.x = sphere->min.x;
		gMin.y = sphere->min.y;

		// Initialize bunny mesh.
		vector<tinyobj::shape_t> TOshapesB;
 		vector<tinyobj::material_t> objMaterialsB;
		//load in the mesh and make the shape(s)
 		rc = tinyobj::LoadObj(TOshapesB, objMaterialsB, errStr, (resourceDirectory + "/bunny.obj").c_str());
		if (!rc) {
			cerr << errStr << endl;
		} else {
			
			theBunny = make_shared<Shape>();
			theBunny->createShape(TOshapesB[0]);
			theBunny->measure();
			theBunny->init();
		}

		// Bunny without normals
		vector<tinyobj::shape_t> TOshapes3;
		vector<tinyobj::material_t> objMaterials3;
		rc = tinyobj::LoadObj(TOshapes3, objMaterials3, errStr, (resourceDirectory + "/bunnyNoNorm.obj").c_str());
		if (!rc) {
			cerr << errStr << endl;
		}
		else {
			bunnyNoNormals = make_shared<Shape>();
			bunnyNoNormals->computeNormals();
			bunnyNoNormals->createShape(TOshapes3[0]);
			bunnyNoNormals->measure();
			bunnyNoNormals->init();
		}

		// dog
		vector<tinyobj::shape_t> TOshapes4;
		vector<tinyobj::material_t> objMaterials4;
		rc = tinyobj::LoadObj(TOshapes4, objMaterials4, errStr, (resourceDirectory + "/dog.obj").c_str());
		if (!rc) {
			cerr << errStr << endl;
		}
		else {
			dog = make_shared<Shape>();
			dog->createShape(TOshapes4[0]);
			dog->measure();
			dog->init();
		}

		// cat model
		vector<tinyobj::shape_t> catShapes;
		vector<tinyobj::material_t> catObjMaterials;
		rc = tinyobj::LoadObj(catShapes, catObjMaterials, errStr, (resourceDirectory + "/cat_hierarchal_maybe.obj").c_str());
		if (!rc) {
			cerr << errStr << endl;
		}
		else {
			cout << catShapes.size();
			for (int i = 0; i < catShapes.size(); i++) {
				shared_ptr<Shape> tmp = make_shared<Shape>();
				tmp->createShape(catShapes[i]);
				tmp->measure();
				tmp->init();
				catModel.push_back(tmp);
			}
		}

		// cat tree
		vector<tinyobj::shape_t> TOshapes5;
		vector<tinyobj::material_t> objMaterials5;
		rc = tinyobj::LoadObj(TOshapes5, objMaterials5, errStr, (resourceDirectory + "/cat_tree.obj").c_str());
		if (!rc) {
			cerr << errStr << endl;
		}
		else {
			for (int i = 0; i < TOshapes5.size(); i++) {
				shared_ptr<Shape> tmp = make_shared<Shape>();
				tmp->createShape(TOshapes5[i]);
				tmp->measure();
				tmp->init();
				catTree.push_back(tmp);
			}
		}

		// cat bed
		vector<tinyobj::shape_t> TOshapes6;
		vector<tinyobj::material_t> objMaterials6;
		rc = tinyobj::LoadObj(TOshapes6, objMaterials6, errStr, (resourceDirectory + "/pet_bed.obj").c_str());
		if (!rc) {
			cerr << errStr << endl;
		}
		else {
			catBed = make_shared<Shape>();
			catBed->createShape(TOshapes6[0]);
			catBed->measure();
			catBed->init();
		}

		// floor
		vector<tinyobj::shape_t> TOshapes7;
		vector<tinyobj::material_t> objMaterials7;
		rc = tinyobj::LoadObj(TOshapes7, objMaterials7, errStr, (resourceDirectory + "/floor.obj").c_str());
		if (!rc) {
			cerr << errStr << endl;
		}
		else {
			floor = make_shared<Shape>();
			floor->createShape(TOshapes7[0]);
			floor->measure();
			floor->init();
		}

		// random cube to represent a box
		vector<tinyobj::shape_t> cubeShape;
		vector<tinyobj::material_t> cubeObjMat;
		rc = tinyobj::LoadObj(cubeShape, cubeObjMat, errStr, (resourceDirectory + "/cube.obj").c_str());
		if (!rc) {
			cerr << errStr << endl;
		}
		else {
			cube = make_shared<Shape>();
			cube->createShape(cubeShape[0]);
			cube->measure();
			cube->init();
		}

		//code to load in the ground plane (CPU defined data passed to GPU)
		initGround();
	}

	//directly pass quad for the ground to the GPU
	void initGround() {

		float g_groundSize = 20;
		float g_groundY = -0.25;

  		// A x-z plane at y = g_groundY of dimension [-g_groundSize, g_groundSize]^2
		float GrndPos[] = {
			-g_groundSize, g_groundY, -g_groundSize,
			-g_groundSize, g_groundY,  g_groundSize,
			g_groundSize, g_groundY,  g_groundSize,
			g_groundSize, g_groundY, -g_groundSize
		};

		float GrndNorm[] = {
			0, 1, 0,
			0, 1, 0,
			0, 1, 0,
			0, 1, 0,
			0, 1, 0,
			0, 1, 0
		};

		static GLfloat GrndTex[] = {
      		0, 0, // back
      		0, 1,
      		1, 1,
      		1, 0 };

      	unsigned short idx[] = {0, 1, 2, 0, 2, 3};

		//generate the ground VAO
      	glGenVertexArrays(1, &GroundVertexArrayID);
      	glBindVertexArray(GroundVertexArrayID);

      	g_GiboLen = 6;
      	glGenBuffers(1, &GrndBuffObj);
      	glBindBuffer(GL_ARRAY_BUFFER, GrndBuffObj);
      	glBufferData(GL_ARRAY_BUFFER, sizeof(GrndPos), GrndPos, GL_STATIC_DRAW);

      	glGenBuffers(1, &GrndNorBuffObj);
      	glBindBuffer(GL_ARRAY_BUFFER, GrndNorBuffObj);
      	glBufferData(GL_ARRAY_BUFFER, sizeof(GrndNorm), GrndNorm, GL_STATIC_DRAW);

      	glGenBuffers(1, &GrndTexBuffObj);
      	glBindBuffer(GL_ARRAY_BUFFER, GrndTexBuffObj);
      	glBufferData(GL_ARRAY_BUFFER, sizeof(GrndTex), GrndTex, GL_STATIC_DRAW);

      	glGenBuffers(1, &GIndxBuffObj);
     	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, GIndxBuffObj);
      	glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(idx), idx, GL_STATIC_DRAW);
      }

      //code to draw the ground plane
     void drawGround(shared_ptr<Program> curS) {
     	// curS->bind();
     	glBindVertexArray(GroundVertexArrayID);
     	floor_texture->bind(curS->getUniform("Texture0"));
		//draw the ground plane 
  		SetModel(vec3(0, -1, 0), 0, 0, 1, curS);
  		glEnableVertexAttribArray(0);
  		glBindBuffer(GL_ARRAY_BUFFER, GrndBuffObj);
  		glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0, 0);

  		glEnableVertexAttribArray(1);
  		glBindBuffer(GL_ARRAY_BUFFER, GrndNorBuffObj);
  		glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 0, 0);

  		glEnableVertexAttribArray(2);
  		glBindBuffer(GL_ARRAY_BUFFER, GrndTexBuffObj);
  		glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, 0, 0);

   		// draw!
  		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, GIndxBuffObj);
  		glDrawElements(GL_TRIANGLES, g_GiboLen, GL_UNSIGNED_SHORT, 0);

  		glDisableVertexAttribArray(0);
  		glDisableVertexAttribArray(1);
  		glDisableVertexAttribArray(2);
  		//curS->unbind();
     }

     //helper function to pass material data to the GPU
	void SetMaterial(shared_ptr<Program> curS, int i) {
    	switch (i) {
    		case 0: // polished oak wood
				/* blinn-phong values from chatgpt:
				Ambient (Ka): (0.4, 0.3, 0.2) (Oak has a slightly richer tone.)
				Diffuse (Kd): (0.7, 0.5, 0.3) (Oak reflects more light due to its lighter color.)
				Specular (Ks): (0.3, 0.3, 0.3) (Polished oak has noticeable highlights.)
				Shininess (alpha): 80–150 (A well-polished oak surface has sharp highlights.)
				*/
				glUniform3f(curS->getUniform("MatAmb"), 0.4, 0.3, 0.2);
				glUniform3f(curS->getUniform("MatDif"), 0.7, 0.5, 0.3);
				glUniform3f(curS->getUniform("MatSpec"), 0.3, 0.3, 0.3);
				glUniform1f(curS->getUniform("MatShine"), 130);
    		break;
    		case 1: // glass
    			glUniform3f(curS->getUniform("MatDif"), 0.1, 0.1, 0.1);
    			glUniform3f(curS->getUniform("MatAmb"), 0.1, 0.1, 0.1);
    			glUniform3f(curS->getUniform("MatSpec"), 1.0, 1.0, 1.0);
    			glUniform1f(curS->getUniform("MatShine"), 200);
    		break;
    		case 2: // metallic
				glUniform3f(curS->getUniform("MatDif"), 0.2, 0.2, 0.2);
				glUniform3f(curS->getUniform("MatAmb"), 0.3, 0.3, 0.3);
				glUniform3f(curS->getUniform("MatSpec"), 1.0, 1.0, 1.0);
    			glUniform1f(curS->getUniform("MatShine"), 250);
    		break;
  		}
	}

	/* helper function to set model trasnforms */
  	void SetModel(vec3 trans, float rotY, float rotX, float sc, shared_ptr<Program> curS) {
  		mat4 Trans = glm::translate( glm::mat4(1.0f), trans);
  		mat4 RotX = glm::rotate( glm::mat4(1.0f), rotX, vec3(1, 0, 0));
  		mat4 RotY = glm::rotate( glm::mat4(1.0f), rotY, vec3(0, 1, 0));
  		mat4 ScaleS = glm::scale(glm::mat4(1.0f), vec3(sc));
  		mat4 ctm = Trans*RotX*RotY*ScaleS;
  		glUniformMatrix4fv(curS->getUniform("M"), 1, GL_FALSE, value_ptr(ctm));
  	}

	void setModel(std::shared_ptr<Program> prog, std::shared_ptr<MatrixStack>M) {
		glUniformMatrix4fv(prog->getUniform("M"), 1, GL_FALSE, value_ptr(M->topMatrix()));
   	}

	/* camera controls - do not change */
	void SetView(shared_ptr<Program>  shader) {
		glm::mat4 Cam = glm::lookAt(g_eye, g_lookAt, vec3(0, 1, 0));
		glUniformMatrix4fv(shader->getUniform("V"), 1, GL_FALSE, value_ptr(Cam));
	}

	void updateUsingCameraPath(float frametime) {
		if (goCamera) {
			if (!splinepath[0].isDone()) {
				splinepath[0].update(frametime);
				g_eye = splinepath[0].getPosition();
			}
			else {
				splinepath[1].update(frametime);
				g_eye = splinepath[1].getPosition();
			}
		}
	}

   	/* code to draw waving hierarchical model */
   	void drawHierModel(shared_ptr<MatrixStack> Model) {
   		// draw hierarchical mesh - replace with your code if desired
		Model->pushMatrix();
			Model->loadIdentity();
			Model->translate(vec3(gTrans, 0, 6));
			
			//draw the torso with these transforms
			Model->pushMatrix();
			  Model->scale(vec3(1.15, 1.35, 1.0));
			  setModel(prog, Model);
			  sphere->draw(prog);
			Model->popMatrix();
			
		Model->popMatrix();
   	}

	void render(float frametime) {
		// Get current frame buffer size.
		int width, height;
		glfwGetFramebufferSize(windowManager->getHandle(), &width, &height);
		glViewport(0, 0, width, height);

		// Clear framebuffer
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

		//Use the matrix stack for Lab 6
		float aspect = width/(float)height;

		// Create the matrix stacks - please leave these alone for now
		auto Projection = make_shared<MatrixStack>();
		auto View = make_shared<MatrixStack>();
		auto Model = make_shared<MatrixStack>();

		updateUsingCameraPath(frametime);

		// Apply perspective projection.
		Projection->pushMatrix();
		Projection->perspective(45.0f, aspect, 0.01f, 100.0f);

		// View is global translation along negative z for now
		View->pushMatrix();
		View->loadIdentity();
		//camera up and down
		View->translate(vec3(0, gCamH, 0));
		//global rotate (the whole scene )
		View->rotate(gRot, vec3(0, 1, 0));

		//switch shaders to the texture mapping shader and draw the ground
		texProg->bind();
		glUniformMatrix4fv(texProg->getUniform("P"), 1, GL_FALSE, value_ptr(Projection->topMatrix()));
		glUniformMatrix4fv(texProg->getUniform("V"), 1, GL_FALSE, value_ptr(view));
		glUniformMatrix4fv(texProg->getUniform("M"), 1, GL_FALSE, value_ptr(Model->topMatrix()));
		glUniform3f(texProg->getUniform("lightPos"), 0.0, 7.0, 0.0);
		
		drawGround(texProg);
		texProg->unbind();

		floor_texture->unbind();
		texProg->unbind();

		// FOR FINAL PROJECT
		// draw cat tree
		texProg->bind();
		glUniformMatrix4fv(texProg->getUniform("P"), 1, GL_FALSE, value_ptr(Projection->topMatrix()));
		glUniformMatrix4fv(texProg->getUniform("V"), 1, GL_FALSE, value_ptr(view));
		glUniform3f(texProg->getUniform("lightPos"), 0.0, 1.0, -1.2);

		// cat trees
		Model->pushMatrix();
			Model->translate(vec3(-5, -1.2, -3.5));
			Model->rotate(radians(90.0f), vec3(0, 1, 0));
			Model->scale(vec3(0.35, 0.35, 0.35));
			glUniformMatrix4fv(texProg->getUniform("M"), 1, GL_FALSE, value_ptr(Model->topMatrix()));

			// base/platform wrapping
			cat_tree_cover->bind(texProg->getUniform("Texture0"));
			catTree[0]->draw(texProg);
			
			// inner padding
			texture1->bind(texProg->getUniform("Texture0"));
			catTree[1]->draw(texProg);

			// scratching poles
			cat_tree_post->bind(texProg->getUniform("Texture0"));
			catTree[2]->draw(texProg);
		Model->popMatrix();

		Model->pushMatrix();
			Model->translate(vec3(5, -1.2, -5.5));
			Model->scale(vec3(0.35, 0.35, 0.35));
			glUniformMatrix4fv(texProg->getUniform("M"), 1, GL_FALSE, value_ptr(Model->topMatrix()));

			// base/platform wrapping
			cat_tree_cover->bind(texProg->getUniform("Texture0"));
			catTree[0]->draw(texProg);

			// inner padding
			texture1->bind(texProg->getUniform("Texture0"));
			catTree[1]->draw(texProg);

			// scratching poles
			cat_tree_post->bind(texProg->getUniform("Texture0"));
			catTree[2]->draw(texProg);
		Model->popMatrix();


		Model->pushMatrix();
			Model->translate(vec3(-3, -1.2, 5.5));
			Model->rotate(radians(180.0f), vec3(0, 1, 0));
			Model->scale(vec3(0.35, 0.35, 0.35));
			glUniformMatrix4fv(texProg->getUniform("M"), 1, GL_FALSE, value_ptr(Model->topMatrix()));

			// base/platform wrapping
			cat_tree_cover->bind(texProg->getUniform("Texture0"));
			catTree[0]->draw(texProg);

			// inner padding
			texture1->bind(texProg->getUniform("Texture0"));
			catTree[1]->draw(texProg);

			// scratching poles
			cat_tree_post->bind(texProg->getUniform("Texture0"));
			catTree[2]->draw(texProg);
		Model->popMatrix();

		
		// cat bed (looks fine)
		Model->pushMatrix();
			Model->translate(vec3(3, -1.25, 5));
			Model->rotate(radians(180.0f), vec3(0, 1, 0));
			Model->scale(vec3(0.05, 0.05, 0.05));
			glUniformMatrix4fv(texProg->getUniform("M"), 1, GL_FALSE, value_ptr(Model->topMatrix()));

			cat_tree_cover->bind(texProg->getUniform("Texture0"));
			catBed->draw(texProg);
		Model->popMatrix();

		Model->pushMatrix();
			Model->translate(vec3(-5.2, -1.25, -1));
			Model->rotate(radians(90.0f), vec3(0, 1, 0));
			Model->scale(vec3(0.04, 0.04, 0.04));
			glUniformMatrix4fv(texProg->getUniform("M"), 1, GL_FALSE, value_ptr(Model->topMatrix()));

			cat_tree_cover->bind(texProg->getUniform("Texture0"));
			catBed->draw(texProg);
		Model->popMatrix();
		

		// cat cafe walls and ceiling (i can just use the obj for the floor)
		wall_texture->bind(texProg->getUniform("Texture0"));

		// left wall
		Model->pushMatrix();
			Model->translate(vec3(-6, 1, 0));
			Model->scale(vec3(0.05, 8, 2));
			glUniformMatrix4fv(texProg->getUniform("M"), 1, GL_FALSE, value_ptr(Model->topMatrix()));
			floor->draw(texProg);
		Model->popMatrix();

		// right wall
		Model->pushMatrix();
			Model->translate(vec3(6, 1, 0));
			Model->scale(vec3(0.05, 8, 2));
			glUniformMatrix4fv(texProg->getUniform("M"), 1, GL_FALSE, value_ptr(Model->topMatrix()));
			floor->draw(texProg);
		Model->popMatrix();

		// front wall
		Model->pushMatrix();
			Model->translate(vec3(0, 1, -6));
			Model->scale(vec3(2, 8, 0.05));
			glUniformMatrix4fv(texProg->getUniform("M"), 1, GL_FALSE, value_ptr(Model->topMatrix()));
			floor->draw(texProg);
		Model->popMatrix();

		// back wall
		Model->pushMatrix();
			Model->translate(vec3(0, 1, 6));
			Model->scale(vec3(2, 8, 0.05));
			glUniformMatrix4fv(texProg->getUniform("M"), 1, GL_FALSE, value_ptr(Model->topMatrix()));
			floor->draw(texProg);
		Model->popMatrix();

		// ceiling
		Model->pushMatrix();
			Model->translate(vec3(0, 6, 0));
			Model->scale(vec3(2, 0.5, 2));
			glUniformMatrix4fv(texProg->getUniform("M"), 1, GL_FALSE, value_ptr(Model->topMatrix()));
			floor->draw(texProg);
		Model->popMatrix();

		wall_texture->unbind();
		texProg->unbind();

		// cat models
		static float zPos = -4.0f;  // Initial position
		static bool movingForward = true;  // Start by moving forward
		static float rotationAngle2 = 0.0f;  // Initial rotation

		// Move forward or backward
		zPos += (movingForward ? 0.01f : -0.01f);

		// Reverse direction when reaching the bounds
		if (zPos >= 4.0f || zPos <= -4.0f) {
			movingForward = !movingForward;  // Switch direction
			rotationAngle2 += 180.0f;  // Instantly flip the cat
		}

		// animated models
		texProg->bind();
		glUniformMatrix4fv(texProg->getUniform("P"), 1, GL_FALSE, value_ptr(Projection->topMatrix()));
		glUniformMatrix4fv(texProg->getUniform("V"), 1, GL_FALSE, value_ptr(view));
		glUniform3f(texProg->getUniform("lightPos"), 0.0, 1.0, 2.0);

		texture1->bind(texProg->getUniform("Texture0"));

		Model->pushMatrix();
			Model->translate(vec3(5, -1.2, zPos));
			Model->rotate(radians(rotationAngle2), vec3(0, 1, 0));
			Model->scale(vec3(0.2, 0.2, 0.2));
			glUniformMatrix4fv(texProg->getUniform("M"), 1, GL_FALSE, value_ptr(Model->topMatrix()));
			
			// body
			for (int i = 0; i < 4; i++) {
				catModel[i]->draw(texProg);
			}

			// right front leg
			Model->pushMatrix();
				Model->translate(vec3(0, 1.1, 1.5));
				Model->rotate(catLegRot, vec3(1, 0, 0));
				Model->translate(vec3(0, -1.1, -1.5));
				glUniformMatrix4fv(texProg->getUniform("M"), 1, GL_FALSE, value_ptr(Model->topMatrix()));
				catModel[4]->draw(texProg);
				catModel[5]->draw(texProg);
			Model->popMatrix();
			
			// left front leg
			Model->pushMatrix();
				Model->translate(vec3(0, 1.1, 1.5));
				Model->rotate((catLegRot * -1), vec3(1, 0, 0));
				Model->translate(vec3(0, -1.1, -1.5));
				glUniformMatrix4fv(texProg->getUniform("M"), 1, GL_FALSE, value_ptr(Model->topMatrix()));
				catModel[6]->draw(texProg);
				catModel[7]->draw(texProg);
			Model->popMatrix();

			// right back leg
			Model->pushMatrix();
				Model->translate(vec3(0, 1, -1));
				Model->rotate(catLegRot, vec3(1, 0, 0));
				Model->translate(vec3(0, -1, 1));
				glUniformMatrix4fv(texProg->getUniform("M"), 1, GL_FALSE, value_ptr(Model->topMatrix()));
				catModel[8]->draw(texProg);
				catModel[9]->draw(texProg);
			Model->popMatrix();

			// left back leg
			Model->pushMatrix();
				Model->translate(vec3(0, 1, -1));
				Model->rotate((catLegRot * -1), vec3(1, 0, 0));
				Model->translate(vec3(0, -1, 1));
				glUniformMatrix4fv(texProg->getUniform("M"), 1, GL_FALSE, value_ptr(Model->topMatrix()));
				catModel[10]->draw(texProg);
				catModel[11]->draw(texProg);
			Model->popMatrix();
		Model->popMatrix();


		// for this model, look up.

		// rotation data for second model
		static float zPos2 = 0.0f;  // Initial position
		static bool movingForward2 = false;  // move backward
		static float rotationAngle3 = 180.0f;  // Initial rotation

		// Move forward or backward
		zPos2 += (movingForward2 ? 0.01f : -0.01f);

		// Reverse direction when reaching the bounds
		if (zPos2 >= 4.0f || zPos2 <= -4.0f) {
			movingForward2 = !movingForward2;  // Switch direction
			rotationAngle3 += 180.0f;  // Instantly flip the cat
		}

		Model->pushMatrix();
			Model->translate(vec3(0, 3.8, zPos2));
			Model->rotate(radians(rotationAngle3), vec3(0, 1, 0));
			Model->scale(vec3(0.2, 0.2, 0.2));
			glUniformMatrix4fv(texProg->getUniform("M"), 1, GL_FALSE, value_ptr(Model->topMatrix()));

			// body
			for (int i = 0; i < 4; i++) {
				catModel[i]->draw(texProg);
			}

			// right front leg
			Model->pushMatrix();
				Model->translate(vec3(0, 1.1, 1.5));
				Model->rotate(catLegRot, vec3(1, 0, 0));
				Model->translate(vec3(0, -1.1, -1.5));
				glUniformMatrix4fv(texProg->getUniform("M"), 1, GL_FALSE, value_ptr(Model->topMatrix()));
				catModel[4]->draw(texProg);
				catModel[5]->draw(texProg);
			Model->popMatrix();

			// left front leg
			Model->pushMatrix();
				Model->translate(vec3(0, 1.1, 1.5));
				Model->rotate((catLegRot * -1), vec3(1, 0, 0));
				Model->translate(vec3(0, -1.1, -1.5));
				glUniformMatrix4fv(texProg->getUniform("M"), 1, GL_FALSE, value_ptr(Model->topMatrix()));
				catModel[6]->draw(texProg);
				catModel[7]->draw(texProg);
			Model->popMatrix();

			// right back leg
			Model->pushMatrix();
				Model->translate(vec3(0, 1, -1));
				Model->rotate(catLegRot, vec3(1, 0, 0));
				Model->translate(vec3(0, -1, 1));
				glUniformMatrix4fv(texProg->getUniform("M"), 1, GL_FALSE, value_ptr(Model->topMatrix()));
				catModel[8]->draw(texProg);
				catModel[9]->draw(texProg);
			Model->popMatrix();

			// left back leg
			Model->pushMatrix();
				Model->translate(vec3(0, 1, -1));
				Model->rotate((catLegRot * -1), vec3(1, 0, 0));
				Model->translate(vec3(0, -1, 1));
				glUniformMatrix4fv(texProg->getUniform("M"), 1, GL_FALSE, value_ptr(Model->topMatrix()));
				catModel[10]->draw(texProg);
				catModel[11]->draw(texProg);
			Model->popMatrix();
		Model->popMatrix();


		// THIS ONE HAS THE ZOOMIES
		Model->pushMatrix();
			Model->translate(vec3(0.2 + moveOffset, -0.3, 0.2 + moveOffset));
			Model->rotate(radians((moveOffset >= 0) ? -145.0f : (180.0f - 145.0f)), vec3(0, 1, 0));
			Model->scale(vec3(0.2, 0.2, 0.2));
			glUniformMatrix4fv(texProg->getUniform("M"), 1, GL_FALSE, value_ptr(Model->topMatrix()));

			// body
			for (int i = 0; i < 4; i++) {
				catModel[i]->draw(texProg);
			}

			// right front leg
			Model->pushMatrix();
				Model->translate(vec3(0, 1.1, 1.5));
				Model->rotate(catLegRot, vec3(1, 0, 0));
				Model->translate(vec3(0, -1.1, -1.5));
				glUniformMatrix4fv(texProg->getUniform("M"), 1, GL_FALSE, value_ptr(Model->topMatrix()));
				catModel[4]->draw(texProg);
				catModel[5]->draw(texProg);
			Model->popMatrix();

			// left front leg
			Model->pushMatrix();
				Model->translate(vec3(0, 1.1, 1.5));
				Model->rotate((catLegRot * -1), vec3(1, 0, 0));
				Model->translate(vec3(0, -1.1, -1.5));
				glUniformMatrix4fv(texProg->getUniform("M"), 1, GL_FALSE, value_ptr(Model->topMatrix()));
				catModel[6]->draw(texProg);
				catModel[7]->draw(texProg);
			Model->popMatrix();

			// right back leg
			Model->pushMatrix();
				Model->translate(vec3(0.05, 1, -1));
				Model->rotate(catLegRot, vec3(1, 0, 0));
				Model->translate(vec3(0, -1, 1));
				glUniformMatrix4fv(texProg->getUniform("M"), 1, GL_FALSE, value_ptr(Model->topMatrix()));
				catModel[8]->draw(texProg);
				catModel[9]->draw(texProg);
			Model->popMatrix();

			// left back leg
			Model->pushMatrix();
				Model->translate(vec3(-0.05, 1, -1));
				Model->rotate((catLegRot * -1), vec3(1, 0, 0));
				Model->translate(vec3(0, -1, 1));
				glUniformMatrix4fv(texProg->getUniform("M"), 1, GL_FALSE, value_ptr(Model->topMatrix()));
				catModel[10]->draw(texProg);
				catModel[11]->draw(texProg);
			Model->popMatrix();
		Model->popMatrix();


		// THIS ONE TOO
		float moveOffset2 = sin(glfwGetTime()) * 4.0f;

		Model->pushMatrix();
			Model->translate(vec3(0 + moveOffset2, -1.2, -4.5));
			Model->rotate(radians((moveOffset2 >= 0) ? -90.f : (90.0f)), vec3(0, 1, 0));
			Model->scale(vec3(0.2, 0.2, 0.2));
			glUniformMatrix4fv(texProg->getUniform("M"), 1, GL_FALSE, value_ptr(Model->topMatrix()));

			// body
			for (int i = 0; i < 4; i++) {
				catModel[i]->draw(texProg);
			}

			// right front leg
			Model->pushMatrix();
				Model->translate(vec3(0, 1.1, 1.5));
				Model->rotate(catLegRot, vec3(1, 0, 0));
				Model->translate(vec3(0, -1.1, -1.5));
				glUniformMatrix4fv(texProg->getUniform("M"), 1, GL_FALSE, value_ptr(Model->topMatrix()));
				catModel[4]->draw(texProg);
				catModel[5]->draw(texProg);
			Model->popMatrix();

			// left front leg
			Model->pushMatrix();
				Model->translate(vec3(0, 1.1, 1.5));
				Model->rotate((catLegRot * -1), vec3(1, 0, 0));
				Model->translate(vec3(0, -1.1, -1.5));
				glUniformMatrix4fv(texProg->getUniform("M"), 1, GL_FALSE, value_ptr(Model->topMatrix()));
				catModel[6]->draw(texProg);
				catModel[7]->draw(texProg);
			Model->popMatrix();

			// right back leg
			Model->pushMatrix();
				Model->translate(vec3(0.05, 1, -1));
				Model->rotate(catLegRot, vec3(1, 0, 0));
				Model->translate(vec3(0, -1, 1));
				glUniformMatrix4fv(texProg->getUniform("M"), 1, GL_FALSE, value_ptr(Model->topMatrix()));
				catModel[8]->draw(texProg);
				catModel[9]->draw(texProg);
			Model->popMatrix();

			// left back leg
			Model->pushMatrix();
				Model->translate(vec3(-0.05, 1, -1));
				Model->rotate((catLegRot * -1), vec3(1, 0, 0));
				Model->translate(vec3(0, -1, 1));
				glUniformMatrix4fv(texProg->getUniform("M"), 1, GL_FALSE, value_ptr(Model->topMatrix()));
				catModel[10]->draw(texProg);
				catModel[11]->draw(texProg);
			Model->popMatrix();
		Model->popMatrix();


		// static models
		Model->pushMatrix();
			Model->translate(vec3(-5, -1.2, -1));
			Model->rotate(radians(90.0f), vec3(0, 1, 0));
			Model->scale(vec3(0.2, 0.2, 0.2));
			glUniformMatrix4fv(texProg->getUniform("M"), 1, GL_FALSE, value_ptr(Model->topMatrix()));

			// body
			for (int i = 0; i < 4; i++) {
				catModel[i]->draw(texProg);
			}

			// right front leg
			Model->pushMatrix();
				glUniformMatrix4fv(texProg->getUniform("M"), 1, GL_FALSE, value_ptr(Model->topMatrix()));
				catModel[4]->draw(texProg);
				catModel[5]->draw(texProg);
			Model->popMatrix();

			// left front leg
			Model->pushMatrix();
				glUniformMatrix4fv(texProg->getUniform("M"), 1, GL_FALSE, value_ptr(Model->topMatrix()));
				catModel[6]->draw(texProg);
				catModel[7]->draw(texProg);
			Model->popMatrix();

			// right back leg
			Model->pushMatrix();
				glUniformMatrix4fv(texProg->getUniform("M"), 1, GL_FALSE, value_ptr(Model->topMatrix()));
				catModel[8]->draw(texProg);
				catModel[9]->draw(texProg);
			Model->popMatrix();

			// left back leg
			Model->pushMatrix();
				glUniformMatrix4fv(texProg->getUniform("M"), 1, GL_FALSE, value_ptr(Model->topMatrix()));
				catModel[10]->draw(texProg);
				catModel[11]->draw(texProg);
			Model->popMatrix();
		Model->popMatrix();


		Model->pushMatrix();
			Model->translate(vec3(3, -1, 4.75));
			Model->rotate(radians(-140.0f), vec3(0, 1, 0));
			Model->scale(vec3(0.2, 0.2, 0.2));
			glUniformMatrix4fv(texProg->getUniform("M"), 1, GL_FALSE, value_ptr(Model->topMatrix()));

			// body
			for (int i = 0; i < 4; i++) {
				catModel[i]->draw(texProg);
			}

			// right front leg
			Model->pushMatrix();
				glUniformMatrix4fv(texProg->getUniform("M"), 1, GL_FALSE, value_ptr(Model->topMatrix()));
				catModel[4]->draw(texProg);
				catModel[5]->draw(texProg);
			Model->popMatrix();

			// left front leg
			Model->pushMatrix();
				glUniformMatrix4fv(texProg->getUniform("M"), 1, GL_FALSE, value_ptr(Model->topMatrix()));
				catModel[6]->draw(texProg);
				catModel[7]->draw(texProg);
			Model->popMatrix();

			// right back leg
			Model->pushMatrix();
				glUniformMatrix4fv(texProg->getUniform("M"), 1, GL_FALSE, value_ptr(Model->topMatrix()));
				catModel[8]->draw(texProg);
				catModel[9]->draw(texProg);
			Model->popMatrix();

			// left back leg
			Model->pushMatrix();
				glUniformMatrix4fv(texProg->getUniform("M"), 1, GL_FALSE, value_ptr(Model->topMatrix()));
				catModel[10]->draw(texProg);
				catModel[11]->draw(texProg);
			Model->popMatrix();
		Model->popMatrix();


		// this one's on a cat tree
		Model->pushMatrix();
			Model->translate(vec3(-3.38, 2.28, 5.35));
			Model->rotate(radians(90.0f), vec3(0, 1, 0));
			Model->scale(vec3(0.2, 0.2, 0.2));
			glUniformMatrix4fv(texProg->getUniform("M"), 1, GL_FALSE, value_ptr(Model->topMatrix()));

			// body
			for (int i = 0; i < 4; i++) {
				catModel[i]->draw(texProg);
			}

			// right front leg
			Model->pushMatrix();
				glUniformMatrix4fv(texProg->getUniform("M"), 1, GL_FALSE, value_ptr(Model->topMatrix()));
				catModel[4]->draw(texProg);
				catModel[5]->draw(texProg);
			Model->popMatrix();

			// left front leg
			Model->pushMatrix();
				glUniformMatrix4fv(texProg->getUniform("M"), 1, GL_FALSE, value_ptr(Model->topMatrix()));
				catModel[6]->draw(texProg);
				catModel[7]->draw(texProg);
			Model->popMatrix();

			// right back leg
			Model->pushMatrix();
				glUniformMatrix4fv(texProg->getUniform("M"), 1, GL_FALSE, value_ptr(Model->topMatrix()));
				catModel[8]->draw(texProg);
				catModel[9]->draw(texProg);
			Model->popMatrix();

			// left back leg
			Model->pushMatrix();
				glUniformMatrix4fv(texProg->getUniform("M"), 1, GL_FALSE, value_ptr(Model->topMatrix()));
				catModel[10]->draw(texProg);
				catModel[11]->draw(texProg);
			Model->popMatrix();
		Model->popMatrix();

		// same with this one
		Model->pushMatrix();
			Model->translate(vec3(5.3, 2.28, -5.45));
			Model->rotate(radians(-45.0f), vec3(0, 1, 0));
			Model->scale(vec3(0.2, 0.2, 0.2));
			glUniformMatrix4fv(texProg->getUniform("M"), 1, GL_FALSE, value_ptr(Model->topMatrix()));

			// body
			for (int i = 0; i < 4; i++) {
				catModel[i]->draw(texProg);
			}

			// right front leg
			Model->pushMatrix();
				glUniformMatrix4fv(texProg->getUniform("M"), 1, GL_FALSE, value_ptr(Model->topMatrix()));
				catModel[4]->draw(texProg);
				catModel[5]->draw(texProg);
			Model->popMatrix();

			// left front leg
			Model->pushMatrix();
				glUniformMatrix4fv(texProg->getUniform("M"), 1, GL_FALSE, value_ptr(Model->topMatrix()));
				catModel[6]->draw(texProg);
				catModel[7]->draw(texProg);
			Model->popMatrix();

			// right back leg
			Model->pushMatrix();
				glUniformMatrix4fv(texProg->getUniform("M"), 1, GL_FALSE, value_ptr(Model->topMatrix()));
				catModel[8]->draw(texProg);
				catModel[9]->draw(texProg);
			Model->popMatrix();

			// left back leg
			Model->pushMatrix();
				glUniformMatrix4fv(texProg->getUniform("M"), 1, GL_FALSE, value_ptr(Model->topMatrix()));
				catModel[10]->draw(texProg);
				catModel[11]->draw(texProg);
			Model->popMatrix();
		Model->popMatrix();
		
		texture1->unbind();

		// platforms on walls
		floor_texture->bind(texProg->getUniform("Texture0"));

		// right wall
		for (int i = 0; i < 3; i++) {
			Model->pushMatrix();
				Model->translate(vec3(5.4, (0.5 + i), (-3 + 3.5 * i)));
				Model->scale(vec3(0.08, 0.05, 0.16));
				glUniformMatrix4fv(texProg->getUniform("M"), 1, GL_FALSE, value_ptr(Model->topMatrix()));
				floor->draw(texProg);
			Model->popMatrix();
		}

		for (int i = 0; i < 2; i++) {
			Model->pushMatrix();
			Model->translate(vec3(5.5, (0.3 + 3.5 * i), (2.2 - 3.4 * i)));
				Model->scale(vec3(0.06, 0.05, (0.14 + 0.7 * i)));
				glUniformMatrix4fv(texProg->getUniform("M"), 1, GL_FALSE, value_ptr(Model->topMatrix()));
				floor->draw(texProg);
			Model->popMatrix();
		}

		Model->pushMatrix();
			Model->translate(vec3(0, 3.8, -5.5));
			Model->scale(vec3(1.2, 0.05, 0.06));
			glUniformMatrix4fv(texProg->getUniform("M"), 1, GL_FALSE, value_ptr(Model->topMatrix()));
			floor->draw(texProg);
		Model->popMatrix();


		// back wall platforms
		Model->pushMatrix();
			Model->translate(vec3(-1, 2.5, 5.5));
			Model->scale(vec3(0.15, 0.05, 0.06));
			glUniformMatrix4fv(texProg->getUniform("M"), 1, GL_FALSE, value_ptr(Model->topMatrix()));
			floor->draw(texProg);
		Model->popMatrix();

		Model->pushMatrix();
			Model->translate(vec3(1, 1.5, 5.5));
			Model->scale(vec3(0.15, 0.05, 0.06));
			glUniformMatrix4fv(texProg->getUniform("M"), 1, GL_FALSE, value_ptr(Model->topMatrix()));
			floor->draw(texProg);
		Model->popMatrix();

		Model->pushMatrix();
			Model->translate(vec3(3.5, 0.5, 5.5));
			Model->scale(vec3(0.15, 0.05, 0.06));
			glUniformMatrix4fv(texProg->getUniform("M"), 1, GL_FALSE, value_ptr(Model->topMatrix()));
			floor->draw(texProg);
		Model->popMatrix();


		// this one goes across the center of the room (z axis)
		Model->pushMatrix();
			Model->translate(vec3(0, 3.8, 0));
			Model->scale(vec3(0.08, 0.05, 1.2));
			glUniformMatrix4fv(texProg->getUniform("M"), 1, GL_FALSE, value_ptr(Model->topMatrix()));
			floor->draw(texProg);
		Model->popMatrix();

		floor_texture->unbind();

		// stuff on ground
		texture2->bind(texProg->getUniform("Texture0"));

		Model->pushMatrix();
			Model->translate(vec3(0, -0.6, 0));
			Model->scale(vec3(3, 0.6, 3));
			glUniformMatrix4fv(texProg->getUniform("M"), 1, GL_FALSE, value_ptr(Model->topMatrix()));
			cube->draw(texProg);
		Model->popMatrix();

		Model->pushMatrix();
			Model->translate(vec3(0, -0.95, 5.5));
			Model->scale(vec3(4, 0.6, 0.7));
			glUniformMatrix4fv(texProg->getUniform("M"), 1, GL_FALSE, value_ptr(Model->topMatrix()));
			cube->draw(texProg);
		Model->popMatrix();

		Model->pushMatrix();
			Model->translate(vec3(-5.5, -0.95, 3));
			Model->scale(vec3(0.7, 0.6, 6));
			glUniformMatrix4fv(texProg->getUniform("M"), 1, GL_FALSE, value_ptr(Model->topMatrix()));
			cube->draw(texProg);
		Model->popMatrix();

		texture2->unbind();

		texProg->unbind();

		//animation update example
		sTheta = sin(glfwGetTime());
		eTheta = std::max(0.0f, (float)sin(glfwGetTime()));
		hTheta = std::max(0.0f, (float)cos(glfwGetTime()));

		catLegRot = sin(glfwGetTime()) * 0.5;
		moveOffset = sin(glfwGetTime()) * -1.0f;

		// Pop matrix stacks.
		Projection->popMatrix();
		View->popMatrix();
	}
};

int main(int argc, char *argv[])
{
	// Where the resources are loaded from
	std::string resourceDir = "../resources";

	if (argc >= 2)
	{
		resourceDir = argv[1];
	}

	Application *application = new Application();

	// Your main will always include a similar set up to establish your window
	// and GL context, etc.

	WindowManager *windowManager = new WindowManager();
	windowManager->init(1920, 1080);
	windowManager->setEventCallbacks(application);
	application->windowManager = windowManager;

	// This is the code that will likely change program to program as you
	// may need to initialize or set up different data and state

	application->init(resourceDir);
	application->initGeom(resourceDir);

	auto lastTime = chrono::high_resolution_clock::now();
	// Loop until the user closes the window.
	while (! glfwWindowShouldClose(windowManager->getHandle()))
	{
		// save current time for next frame
		auto nextLastTime = chrono::high_resolution_clock::now();

		// get time since last frame
		float deltaTime =
			chrono::duration_cast<std::chrono::microseconds>(
				chrono::high_resolution_clock::now() - lastTime)
			.count();
		// convert microseconds (weird) to seconds (less weird)
		deltaTime *= 0.000001;

		// reset lastTime so that we can calculate the deltaTime
		// on the next frame
		lastTime = nextLastTime;

		// Render scene.
		application->render(deltaTime);

		// Swap front and back buffers.
		glfwSwapBuffers(windowManager->getHandle());
		// Poll for and process events.
		glfwPollEvents();
	}

	// Quit program.
	windowManager->shutdown();
	return 0;
}
