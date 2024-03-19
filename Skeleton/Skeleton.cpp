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

using namespace std;

// vertex shader in GLSL: It is a Raw string (C++11) since it contains new line characters
const char * const vertexSource = R"(
	#version 330				// Shader 3.3
	precision highp float;		// normal floats, makes no difference on desktop computers

	uniform mat4 MVP;			// uniform variable, the Model-View-Projection transformation matrix
	layout(location = 0) in vec3 cp;	// Varying input: vp = control point is expected in attrib array 0

	void main() {
		gl_Position = vec4(cp.x, cp.y, cp.z, 1) * MVP;		// transform vp from modeling space to normalized device space
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

enum State {
	IDLE,
	LAGRANGE,
	BEZIER,
	CATMULLROM
};

State mode = IDLE;

class Camera {
	vec3 cam;
	float size = 30;
	mat4 viewM, projM;
public:
	Camera() {
		cam = vec3(0, 0, 0);
	}

	void V() {
		float ratio = size / (float)30;
		viewM = { ratio, 0, 0, 0,
				  0, ratio, 0, 0,
				  0, 0, ratio, 0,
				  cam.x, cam.y, cam.z, 1 };
	}

	void P() {
		float ratio = (float)1 / (float)15;
		projM = { ratio, 0, 0, 0,
				  0, ratio, 0, 0,
				  0, 0, ratio, 0,
				  0, 0, 0, ratio };
	}

	void pan(int panvalue) {
		cam.y += panvalue;
		V();
	}

	void zoom(int zoomvalue) {
		size *= zoomvalue;
		V();
	}
};

class Curve {
	unsigned int vao, vbo[2]; // vbo[0] -> cps, vbo[1] -> gorbe
protected:
	vector<vec3> cps;
public:
	Curve() {
		glGenVertexArrays(1, &vao);
		glBindVertexArray(vao);

		glGenBuffers(2, vbo);

		glEnableVertexAttribArray(0);

		glBindBuffer(GL_ARRAY_BUFFER, vbo[0]);
		glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0, NULL);

		glBindBuffer(GL_ARRAY_BUFFER, vbo[1]);
		glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0, NULL);
	}

	void updateGPU(int vboid) {
		glBindVertexArray(vao);
		glBindBuffer(GL_ARRAY_BUFFER, vbo[vboid]);
		glBufferData(GL_ARRAY_BUFFER, cps.size() * sizeof(vec3), &cps[0], GL_DYNAMIC_DRAW);
	}

	void Draw(int type, vec3 color) {
		if (cps.size() > 0) {
			glBindVertexArray(vao);
			gpuProgram.setUniform(color, "color");
			glDrawArrays(type, 0, cps.size());
		}
	}
};

class Lagrange : Curve {
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

	void AddControlPoint(vec3 cp) {
		float ti = cps.size(); // or something better
		cps.push_back(cp);
		ts.push_back(ti);
	}

	vec3 r(float t) {
		vec3 rt(0, 0, 0);
		for (int i = 0; i < cps.size(); i++) {
			rt = rt + cps[i] * L(i, t);
		}
		return rt;
	}

};

class Bezier : Curve {
	vector<vec3> cps; // control pts
	float B(int i, float t) {
		int n = cps.size() - 1; // n+1 pts!
		float choose = 1;
		for (int j = 1; j <= i; j++) choose *= (float)(n - j + 1) / j;
		return choose * pow(t, i) * pow(1 - t, n - i);
	}
public:
	void AddControlPoint(vec3 cp) {
		cps.push_back(cp);
	}
	vec3 r(float t) {
		vec3 rt(0, 0, 0);
		for (int i = 0; i < cps.size(); i++) {
			rt = rt + (cps[i] * B(i, t));
		}
		return rt;
	}

};

class CatmullRom : Curve {

};

Lagrange* lagrange;



// Initialization, create an OpenGL context
void onInitialization() {
	glViewport(0, 0, windowWidth, windowHeight);

	lagrange = new Lagrange;

	// create program for the GPU
	gpuProgram.create(vertexSource, fragmentSource, "outColor");
}

// Window has become invalid: Redraw
void onDisplay() {
	glClearColor(0, 0, 0, 0);     // background color
	glClear(GL_COLOR_BUFFER_BIT); // clear frame buffer

	// Set color to (0, 1, 0) = green
	int location = glGetUniformLocation(gpuProgram.getId(), "color");
	glUniform3f(location, 0.0f, 1.0f, 0.0f); // 3 floats

	float MVPtransf[4][4] = { 1, 0, 0, 0,    // MVP matrix, 
							  0, 1, 0, 0,    // row-major!
							  0, 0, 1, 0,
							  0, 0, 0, 1 };

	location = glGetUniformLocation(gpuProgram.getId(), "MVP");	// Get the GPU location of uniform variable MVP
	glUniformMatrix4fv(location, 1, GL_TRUE, &MVPtransf[0][0]);	// Load a 4x4 row-major float matrix to the specified location

	//lagrange->Draw();

	glutSwapBuffers(); // exchange buffers for double buffering
}

// Key of ASCII code pressed
void onKeyboard(unsigned char key, int pX, int pY) {
	switch (key)
	{
	case 'l':
		mode = LAGRANGE;
		break;
	case 'b':
		mode = BEZIER;
		break;
	case 'c':
		mode = CATMULLROM;
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
	printf("Mouse moved to (%3.2f, %3.2f)\n", cX, cY);
}

// Mouse click event
void onMouse(int button, int state, int pX, int pY) { // pX, pY are the pixel coordinates of the cursor in the coordinate system of the operation system
	// Convert to normalized device space
	float cX = 2.0f * pX / windowWidth - 1;	// flip y axis
	float cY = 1.0f - 2.0f * pY / windowHeight;

	if (state == GLUT_DOWN) {
		switch (mode)
		{
		case IDLE:
			break;
		case LAGRANGE:

			break;
		case BEZIER:
			break;
		case CATMULLROM:
			break;
		}
	}

	/*switch (button) {
	case GLUT_LEFT_BUTTON:   printf("Left button %s at (%3.2f, %3.2f)\n", buttonStat, cX, cY);   break;
	case GLUT_MIDDLE_BUTTON: printf("Middle button %s at (%3.2f, %3.2f)\n", buttonStat, cX, cY); break;
	case GLUT_RIGHT_BUTTON:  printf("Right button %s at (%3.2f, %3.2f)\n", buttonStat, cX, cY);  break;
	}*/
}

// Idle event indicating that some time elapsed: do animation here
void onIdle() {
	long time = glutGet(GLUT_ELAPSED_TIME); // elapsed time since the start of the program
}
