//=============================================================================================
// Mintaprogram: Zöld háromszög. Ervenyes 2019. osztol.
//
// A beadott program csak ebben a fajlban lehet, a fajl 1 byte-os ASCII karaktereket tartalmazhat, BOM kihuzando.
// Tilos:
// - mast "beincludolni", illetve mas konyvtarat hasznalni
// - faljmuveleteket vegezni a printf-et kiveve
// - Mashonnan atvett programresszleteket forrasmegjeloles nelkul felhasznalni es
// - felesleges programsorokat a beadott programban hagyni!!!!!!! 
// - felesleges kommenteket a beadott programba irni a forrasmegjelolest kommentjeit kiveve
// ---------------------------------------------------------------------------------------------
// A feladatot ANSI C++ nyelvu forditoprogrammal ellenorizzuk, a Visual Studio-hoz kepesti elteresekrol
// es a leggyakoribb hibakrol (pl. ideiglenes objektumot nem lehet referencia tipusnak ertekul adni)
// a hazibeado portal ad egy osszefoglalot.
// ---------------------------------------------------------------------------------------------
// A feladatmegoldasokban csak olyan OpenGL fuggvenyek hasznalhatok, amelyek az oran a feladatkiadasig elhangzottak 
// A keretben nem szereplo GLUT fuggvenyek tiltottak.
//
// NYILATKOZAT
// ---------------------------------------------------------------------------------------------
// Nev    : Mayer Ádám
// Neptun : XYJP9S
// ---------------------------------------------------------------------------------------------
// ezennel kijelentem, hogy a feladatot magam keszitettem, es ha barmilyen segitseget igenybe vettem vagy
// mas szellemi termeket felhasznaltam, akkor a forrast es az atvett reszt kommentekben egyertelmuen jeloltem.
// A forrasmegjeloles kotelme vonatkozik az eloadas foliakat es a targy oktatoi, illetve a
// grafhazi doktor tanacsait kiveve barmilyen csatornan (szoban, irasban, Interneten, stb.) erkezo minden egyeb
// informaciora (keplet, program, algoritmus, stb.). Kijelentem, hogy a forrasmegjelolessel atvett reszeket is ertem,
// azok helyessegere matematikai bizonyitast tudok adni. Tisztaban vagyok azzal, hogy az atvett reszek nem szamitanak
// a sajat kontribucioba, igy a feladat elfogadasarol a tobbi resz mennyisege es minosege alapjan szuletik dontes.
// Tudomasul veszem, hogy a forrasmegjeloles kotelmenek megsertese eseten a hazifeladatra adhato pontokat
// negativ elojellel szamoljak el es ezzel parhuzamosan eljaras is indul velem szemben.
//=============================================================================================
#include "framework.h"
#include "iostream"

using namespace std;

// vertex shader in GLSL: It is a Raw string (C++11) since it contains new line characters
const char * const vertexSource = R"(
	#version 330				// Shader 3.3
	precision highp float;		// normal floats, makes no difference on desktop computers

	uniform mat4 VP;			// uniform variable, the Model-View-Projection transformation matrix
	layout(location = 0) in vec3 cp;	// Varying input: cp = control point is expected in attrib array 0

	void main() {
		gl_Position = vec4(cp.x, cp.y, cp.z, 1) * VP;		// transform vp from modeling space to normalized device space
	}
)";

// fragment shader in GLSL
const char * const fragmentSource = R"(
	#version 330			// Shader 3.3
	precision highp float;	// normal floats, makes no difference on desktop computers
	
	uniform vec3 color;		// uniform variable, the color of the primitive
	out vec4 outColor;		// computed color of the current pixel

	void main() {
		outColor = vec4(color, 1);	// computed color is the color of the primitive
	}
)";

GPUProgram gpuProgram; // vertex and fragment shaders

enum CurveMode {
	LAGRANGE,
	BEZIER,
	CATMULLROM
};

CurveMode curveMode = LAGRANGE;

enum MoveMode {
	ENABLED, DISABLED
};

MoveMode moveMode = DISABLED;

class Camera {
	vec3 cam;
	float wsize;
	mat4 viewM, projM;
	mat4 invVP;

public:
	Camera() {
		cam = vec3(0, 0, 0);
		wsize = 30.0f;
	}

	void V() {
		float ratio = wsize / 30.0f;
		mat4 scale = { ratio, 0, 0, 0,
					   0, ratio, 0, 0,
					   0, 0, 1, 0,
					   0, 0, 0, 1 };

		mat4 trans = { 1, 0, 0, 0,
						   0, 1, 0, 0,
						   0, 0, 1, 0,
						   cam.x, 0, 0, 1
		};

		viewM = trans * scale;
		refreshVPinv();
	}

	void P() {
		float ratio = 2.0f / 30.0f;
		projM = { ratio, 0, 0, 0,
				  0, ratio, 0, 0,
				  0, 0, 1, 0,
				  0, 0, 0, 1 };
		refreshVPinv();
	}

	void uploadMx() const {
		mat4 VP = viewM * projM;
		int location = glGetUniformLocation(gpuProgram.getId(), "VP");
		glUniformMatrix4fv(location, 1, GL_TRUE, &VP[0][0]);
	}

	void refreshVPinv() {
		float pRatio = 30.0f / 2.0f;
		mat4 invP = { pRatio, 0, 0, 0,
					  0, pRatio, 0, 0,
					  0, 0, 1, 0,
					  0, 0, 0, 1
		};

		float vRatio = 30.0f / wsize;
		mat4 invScale = { vRatio, 0, 0, 0,
						  0, vRatio, 0, 0,
						  0, 0, 1, 0,
						  0, 0, 0, 1
		};

		mat4 invTrans = { 1, 0, 0, 0,
						  0, 1, 0, 0,
						  0, 0, 1, 0,
						  -1*cam.x, 0, 0, 1
		};

		invVP = invP * invScale * invTrans;
	}

	vec3 inverseVP(vec3 cursorpos) const {
		//cout << invVP[0][0] << " " << invVP[1][1] << " " << invVP[2][2] << " " << invVP[3][0] << endl;
		
		vec4 wC = vec4(cursorpos.x, cursorpos.y, cursorpos.z, 1) * invVP;
		//cout << wC.x << " " << wC.y << " " << endl << endl;
		return vec3(wC.x, wC.y, wC.z);
	}

	void pan(int panvalue) {
		cam.x += panvalue;
		V(); P(); uploadMx();
	}

	void zoom(float zoomvalue) {
		wsize *= zoomvalue;
		V(); 
		P(); 
		uploadMx();
	}
};

Camera* camera;

class Curve {
protected:
	unsigned int vaoCP, vboCP, vaoSpl, vboSpl;
	vector<vec3> cps;
	vector<vec3> curveVertices;
	vec3* selectedPoint;
public:
	Curve() {
		glGenVertexArrays(1, &vaoCP);
		glBindVertexArray(vaoCP);
		glEnableVertexAttribArray(0);
		glGenBuffers(1, &vboCP);
		glBindBuffer(GL_ARRAY_BUFFER, vboCP);
		glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0, NULL);
		

		glGenVertexArrays(1, &vaoSpl);
		glBindVertexArray(vaoSpl);
		glEnableVertexAttribArray(0);
		glGenBuffers(1, &vboSpl);
		glBindBuffer(GL_ARRAY_BUFFER, vboSpl);
		glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0, NULL);

		selectedPoint = nullptr;
	}

	void selectPoint(vec3 cursor) {
		for (vec3& vertex : cps) {
			if (fabs(length(cursor - vertex)) <= 0.1f) {
				selectedPoint = &vertex;
			}
		}
	}

	void deselectCP() {
		selectedPoint = nullptr;
	}

	void moveCP(vec3 cursor) {
		if (selectedPoint != nullptr) {
			selectedPoint->x = cursor.x;
			selectedPoint->y = cursor.y;
			selectedPoint->z = cursor.z;
			vectorize();
		}
	}

	virtual void AddControlPoint(vec3 cp) = 0;
	virtual void setTension(float tau) = 0;
	virtual vec3 r(float t) = 0;

	void vectorize() {
		curveVertices.clear();
		for (float i = 0; i < 1; i += 0.01f) {
			vec3 vtx = r(i);
			vtx.z = 1;
			curveVertices.push_back(vtx);
		}
		glutPostRedisplay();
	}

	void del() {
		cps.clear(); curveVertices.clear(); selectedPoint = nullptr;
	}

	void Draw() {
		glBindVertexArray(vaoSpl);
		glBindBuffer(GL_ARRAY_BUFFER, vboSpl);
		glBufferData(GL_ARRAY_BUFFER, curveVertices.size() * sizeof(vec3), &curveVertices[0], GL_DYNAMIC_DRAW);
		gpuProgram.setUniform(vec3(1.0f, 1.0f, 0.0f), "color");
		glDrawArrays(GL_LINE_STRIP, 0, curveVertices.size());

		glBindVertexArray(vaoCP);
		glBindBuffer(GL_ARRAY_BUFFER, vboCP);
		glBufferData(GL_ARRAY_BUFFER, cps.size() * sizeof(vec3), &cps[0], GL_DYNAMIC_DRAW);
		gpuProgram.setUniform(vec3(1.0f, 0.0f, 0.0f), "color");
		glDrawArrays(GL_POINTS, 0, cps.size());
	}
};

class Lagrange : public Curve {
	vector<float> ts; // knots

	float L(int i, float t) {
		float Li = 1.0f;
		for (int j = 0; j < cps.size(); j++)
			if (j != i) {
				Li = Li * ((t - ts[j]) / (ts[i] - ts[j]));
			}
		return Li;
	}
public:
	Lagrange() : Curve() {}

	void AddControlPoint(vec3 cp) override {
		ts.clear();
		ts.push_back(0);

		cps.push_back(cp);

		if (cps.size() <= 1) {
			return;
		}
		else {
			float fullDist = 0;
			for (int i = 1; i < cps.size(); i++) {
				fullDist += length(cps[i] - cps[i - 1]);
			}

			for (int i = 1; i < cps.size(); i++) {
				float ithLength = 0;
				for (int j = 1; j <= i; j++) {
					ithLength += length(cps[j] - cps[j - 1]);
				}
				float knot = ithLength / fullDist;
				ts.push_back(knot);
			}
		}
		vectorize();
	}

	vec3 r(float t) override {
		vec3 rt(0, 0, 0);
		for (int i = 0; i < cps.size(); i++) {
			rt = rt + cps[i] * L(i, t);
		}
		return rt;
	}

	void setTension(float tau) override {
		return;
	}
};

class Bezier : public Curve {
	float B(int i, float t) {
		int n = cps.size() - 1; // n+1 pts!
		float choose = 1;
		for (int j = 1; j <= i; j++) {
			choose *= (float)(n - j + 1) / j;
		}
		return choose * powf(t, i) * powf(1.0f - t, n - i);
	}
public:
	Bezier() : Curve() {}
	void AddControlPoint(vec3 cp) {
		cps.push_back(cp);
		vectorize();
	}

	vec3 r(float t) override {
		vec3 rt(0, 0, 0);
		for (int i = 0; i < cps.size(); i++) {
			rt = rt + cps[i] * B(i, t);
		}
		return rt;
	}

	void setTension(float tau) override {
		return;
	}
};

class CatmullRom : public Curve {
	vector<float> ts;
	float tension = 0;

	vec3 Hermite(vec3 p0, vec3 v0, float t0, vec3 p1, vec3 v1, float t1, float t) {
		float deltat = t1 - t0;
		vec3 a0 = p0;
		vec3 a1 = v0;
		vec3 a2 = ((3 * (p1 - p0)) / (powf(deltat, 2))) - ((v1 + 2 * v0) / deltat);
		vec3 a3 = ((2 * (p0 - p1)) / (powf(deltat, 3))) + (v1 + v0) / (powf(deltat, 2));
		deltat = t - t0;
		return (a3 * powf(deltat, 3)) + (a2 * powf(deltat, 2)) + (a1 * deltat) + a0;
	}public:	void AddControlPoint(vec3 cp) override {
		ts.clear();
		ts.push_back(0);

		cps.push_back(cp);

		if (cps.size() <= 1) {
			return;
		}
		else {
			float fullDist = 0;
			for (int i = 1; i < cps.size(); i++) {
				fullDist += length(cps[i] - cps[i - 1]);
			}

			for (int i = 1; i < cps.size(); i++) {
				float ithLength = 0;
				for (int j = 1; j <= i; j++) {
					ithLength += length(cps[j] - cps[j - 1]);
				}
				float knot = ithLength / fullDist;
				ts.push_back(knot);
			}
		}

		vectorize();
	}	vec3 firstPoint(int i) {		return (cps[i + 1] - cps[i]) / (ts[i + 1] - ts[i]);	}	vec3 lastPoint(int i) {		return (cps[i] - cps[i - 1]) / (ts[i] - ts[i - 1]);	}	vec3 middlePoint(int i) {		return ((1-tension)/2)			 * ((cps[i + 1] - cps[i]) / (ts[i + 1] - ts[i]) 			 + (cps[i] - cps[i - 1]) / (ts[i] - ts[i - 1]));	}
	vec3 r(float t) override {
		for (int i = 0; i < cps.size() - 1; i++) {
			if (ts[i] <= t && t <= ts[i + 1]) {
				vec3 v0, v1;
				if (cps.size() <= 2) {
					v0 = firstPoint(i);
					v1 = lastPoint(i + 1);
				}
				else {
					if (i > 0 && i < cps.size() - 2) {
						v0 = middlePoint(i);
						v1 = middlePoint(i + 1);
					}
					else if (i == 0) {
						v0 = firstPoint(i);
						v1 = middlePoint(i + 1);
					}
					else if (i == cps.size() - 2) {
						v0 = middlePoint(i);
						v1 = lastPoint(i + 1);
					}
				}
				return Hermite(cps[i], v0, ts[i], cps[i + 1], v1, ts[i + 1], t);
			}
		}
	}

	void setTension(float tau) override {
		tension += tau;
		vectorize();
	}
};

Curve* curve;
Lagrange* lagrange;
Bezier* bezier;
CatmullRom* catmullrom;


// Initialization, create an OpenGL context
void onInitialization() {
	glViewport(0, 0, windowWidth, windowHeight);

	glLineWidth(2);
	glPointSize(10);

	//curve = new Curve();
	camera = new Camera();
	lagrange = new Lagrange();
	bezier = new Bezier();
	catmullrom = new CatmullRom();

	curve = bezier;

	
	curve->AddControlPoint(vec3(5, 0, 1));
	curve->AddControlPoint(vec3(10, 4, 1));
	curve->AddControlPoint(vec3(10, 6, 1));
	curve->AddControlPoint(vec3(5, 10, 1));

	// create program for the GPU
	gpuProgram.create(vertexSource, fragmentSource, "outColor");
}

// Window has become invalid: Redraw
void onDisplay() {
	glClearColor(0, 0, 0, 1);
	glClear(GL_COLOR_BUFFER_BIT);

	camera->V(); camera->P();
	camera->uploadMx();

	curve->Draw();

	glutSwapBuffers();
}

// Key of ASCII code pressed
void onKeyboard(unsigned char key, int pX, int pY) {
	switch (key)
	{
	case 'l':
		curve->del();
		curve = lagrange;
		glutPostRedisplay();
		curveMode = LAGRANGE;
		break;
	case 'b':
		curve->del();
		curve = bezier;
		glutPostRedisplay();
		curveMode = BEZIER;
		break;
	case 'c':
		curve->del();
		curve = catmullrom;
		glutPostRedisplay();
		curveMode = CATMULLROM;
		break;
	case 't':
		if (curveMode == CATMULLROM) {
			curve->setTension(-0.1f);
		}
		break;
	case 'T':
		if (curveMode == CATMULLROM) {
			curve->setTension(0.1f);
		}
		break;
	case 'z':
		camera->zoom(1.1f);
		//curve->printTranslated();
		glutPostRedisplay();
		break;
	case 'Z':
		camera->zoom(1.0f / 1.1f);
		//curve->printTranslated();
		glutPostRedisplay();
		break;
	case 'p':
		camera->pan(1);
		glutPostRedisplay();
		break;
	case 'P':
		camera->pan(-1);
		glutPostRedisplay();
		break;
	}
}

// Key of ASCII code released
void onKeyboardUp(unsigned char key, int pX, int pY) {
}

// Move mouse with key pressed
void onMouseMotion(int pX, int pY) {	// pX, pY are the pixel coordinates of the cursor in the coordinate system of the operation system
	// Convert to normalized device space
	float cX = 2.0f * pX / windowWidth - 1;	// flip y axis
	float cY = 1.0f - 2.0f * pY / windowHeight;

	if (moveMode == ENABLED) {
		curve->moveCP(camera->inverseVP(vec3(cX, cY, 1)));
		glutPostRedisplay();
	}
}

// Mouse click event
void onMouse(int button, int state, int pX, int pY) { // pX, pY are the pixel coordinates of the cursor in the coordinate system of the operation system
	// Convert to normalized device space
	float cX = 2.0f * pX / windowWidth - 1;	// flip y axis
	float cY = 1.0f - 2.0f * pY / windowHeight;

	if (button == GLUT_LEFT_BUTTON && state == GLUT_DOWN) {
		switch (curveMode)
		{
		case LAGRANGE:
			if (button == GLUT_LEFT_BUTTON) {
				lagrange->AddControlPoint(camera->inverseVP(vec3(cX, cY, 1)));
				glutPostRedisplay();
			}
			break;
		case BEZIER:
			if (button == GLUT_LEFT_BUTTON) {
				bezier->AddControlPoint(camera->inverseVP(vec3(cX, cY, 1)));
				glutPostRedisplay();
			}
			break;
		case CATMULLROM:
			if (button == GLUT_LEFT_BUTTON) {
				catmullrom->AddControlPoint(camera->inverseVP(vec3(cX, cY, 1)));
				glutPostRedisplay();
			}
			break;
		}
	}
	if (button == GLUT_RIGHT_BUTTON && state == GLUT_DOWN) {
		curve->selectPoint(camera->inverseVP(vec3(cX, cY, 1)));
		moveMode = ENABLED;
	}
	if (button == GLUT_RIGHT_BUTTON && state == GLUT_UP) {
		curve->deselectCP();
		moveMode = DISABLED;
	}
}

// Idle event indicating that some time elapsed: do animation here
void onIdle() {
	long time = glutGet(GLUT_ELAPSED_TIME); // elapsed time since the start of the program
}

vec3 Curve::r(float t)
{
	return vec3();
}
