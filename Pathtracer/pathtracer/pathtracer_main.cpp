#ifdef WIN32
#include <windows.h>
#endif

#include <GL/glew.h>
#include <GL/glut.h>
#include <cstdlib>
#include <iostream>
#include <sstream>
#include "glutil.h"
#include "Pathtracer.h"
#include "FreeImage.h"

Pathtracer g_pathtracer; 

GLuint vertexArrayObject;	
GLuint shaderProgram;

#ifdef _DEBUG
int g_subsample = 8; 
#else 
int g_subsample = 1; 
#endif

#define MAX_SAMPLES_PER_PIXEL 2048

using namespace std; 

void initGL()
{
	// Initialize GLEW, which provides access to OpenGL Extensions
	glewInit();  
	// Print information about GL and ensure that we've got GL.30
	startupGLDiagnostics();
	// Workaround for AMD, which hopefully will not be necessary in the near future...
	if (!glBindFragDataLocation)
	{
		glBindFragDataLocation = glBindFragDataLocationEXT;
	}
	// Define the positions for each of the three vertices of the triangle
	const float positions[] = {
		 1.0f,   -1.0f, 0.0f,
		 1.0f,	  1.0f, 0.0f, 
		-1.0f,	 -1.0f, 0.0f, 
		-1.0f,	  1.0f, 0.0f
	};
	// Create a handle for the position vertex buffer object
	GLuint positionBuffer; 
	glGenBuffers( 1, &positionBuffer );
	// Set the newly created buffer as the current one
	glBindBuffer( GL_ARRAY_BUFFER, positionBuffer );
	// Send the vertex position data to the current buffer
	glBufferData( GL_ARRAY_BUFFER, sizeof(positions), positions, GL_STATIC_DRAW );

	//******* Connect triangle data with the vertex array object *******
	glGenVertexArrays(1, &vertexArrayObject);
	// Bind the vertex array object
	// The following calls will affect this vertex array object.
	glBindVertexArray(vertexArrayObject);
	// Makes positionBuffer the current array buffer for subsequent calls.
	glBindBuffer( GL_ARRAY_BUFFER, positionBuffer );
	// Attaches positionBuffer to vertexArrayObject, in the 0th attribute location
	glVertexAttribPointer(0, 3, GL_FLOAT, false/*normalized*/, 0/*stride*/, 0/*offset*/ );	
	glEnableVertexAttribArray(0); // Enable the vertex position attribute

	///////////////////////////////////////////////////////////////////////////
	// Create shaders
	///////////////////////////////////////////////////////////////////////////	
	GLuint vertexShader = glCreateShader(GL_VERTEX_SHADER);
	GLuint fragmentShader = glCreateShader(GL_FRAGMENT_SHADER);

	// Invoke helper functions (in glutil.h/cpp) to load text files for vertex and fragment shaders.
	const char *vs = textFileRead("simple.vert"); // On mac, use "../../simple.vert"
	const char *fs = textFileRead("simple.frag"); // On mac, use "../../simple.frag"

	glShaderSource(vertexShader, 1, &vs, NULL);
	glShaderSource(fragmentShader, 1, &fs, NULL);

	// we are now done with the source and can free the file data, textFileRead uses new [] to.
	// allocate the memory so we must free it using delete [].
	delete [] vs;;
	delete [] fs;

	// Comile the shader, translates into internal representation and checks for errors.
	glCompileShader(vertexShader);
	int compileOK;
	// check for compiler errors in vertex shader.
	glGetShaderiv(vertexShader, GL_COMPILE_STATUS, &compileOK);
	if(!compileOK) {
		std::string err = GetShaderInfoLog(vertexShader);
		fatal_error( err );
		return;
	}

	// Comile the shader, translates into internal representation and checks for errors.
	glCompileShader(fragmentShader);
	// check for compiler errors in fragment shader.
	glGetShaderiv(fragmentShader, GL_COMPILE_STATUS, &compileOK);
	if(!compileOK) {
		std::string err = GetShaderInfoLog(fragmentShader);
		fatal_error( err );
		return;
	}

	// Create a program object and attach the two shaders we have compiled, the program object contains
	// both vertex and fragment shaders as well as information about uniforms and attributes common to both.
	shaderProgram = glCreateProgram();
	glAttachShader(shaderProgram, fragmentShader);
	glAttachShader(shaderProgram, vertexShader);

	// Now that the fragment and vertex shader has been attached, we no longer need these two separate objects and should delete them.
	// The attachment to the shader program will keep them alive, as long as we keep the shaderProgram.
	glDeleteShader( vertexShader );
	glDeleteShader( fragmentShader );

	// We have previously (in the glVertexAttribPointer calls) decided that our 
	// vertex position data will be the 0th attribute. Bind the attribute with 
	// name "position" to the 0th stream
	glBindAttribLocation(shaderProgram, 0, "position"); 

	// This tells OpenGL which draw buffer the fragment shader out varaible 'fragmentColor' will end up in.
	// Since we only use one output and draw buffer this is actually redundant, as the default will be correct.
	glBindFragDataLocation(shaderProgram, 0, "fragmentColor");

	// Link the different shaders that are bound to this program, this creates a final shader that 
	// we can use to render geometry with.
	glLinkProgram(shaderProgram);

	// Check for linker errors, many errors, such as mismatched in and out variables between 
	// vertex/fragment shaders,  do not appear before linking.
	{
		GLint linkOk = 0;
		glGetProgramiv(shaderProgram, GL_LINK_STATUS, &linkOk);
		if(!linkOk) 
		{
			std::string err = GetShaderInfoLog(shaderProgram);
			fatal_error( err );
			return;
		}
	}
	CHECK_GL_ERROR();
}

void printString(int x, int y, string str)
{
	int currentx = x; 
	for(unsigned int i=0; i<str.size(); i++) {
		glWindowPos2i(currentx,y); 
		glutBitmapCharacter(GLUT_BITMAP_HELVETICA_12, str[i]); 
		currentx += glutBitmapWidth(GLUT_BITMAP_HELVETICA_12, str[i]); 
	}
}

void display(void)
{
	glClearColor(0.2,0.2,0.8,1.0);						// Set clear color
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT); // Clears the color buffer and the z-buffer

	int w = glutGet((GLenum)GLUT_WINDOW_WIDTH);
	int h = glutGet((GLenum)GLUT_WINDOW_HEIGHT);
	glViewport(0, 0, w, h);						// Set viewport
	glDisable(GL_CULL_FACE);

	// Shader Program
	glUseProgram( shaderProgram );			// Set the shader program to use for this draw call

	///////////////////////////////////////////////////////////////////////////
	// Trace one path per pixel
	///////////////////////////////////////////////////////////////////////////
	if(g_pathtracer.m_frameBufferSamples < MAX_SAMPLES_PER_PIXEL)
		g_pathtracer.tracePrimaryRays(); 

	///////////////////////////////////////////////////////////////////////////
	// Create and upload raytracer framebuffer as a texture
	///////////////////////////////////////////////////////////////////////////
	static GLuint framebufferTexture = 0;
	if(framebufferTexture == 0) glGenTextures(1, &framebufferTexture); 
	glActiveTexture(GL_TEXTURE0); 
	glBindTexture(GL_TEXTURE_2D, framebufferTexture); 
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB32F, 
				 g_pathtracer.m_frameBufferSize.x, 
				 g_pathtracer.m_frameBufferSize.y,
				 0, GL_RGB, GL_FLOAT, g_pathtracer.m_frameBuffer); 
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR); 
	glUniform1i(glGetUniformLocation(shaderProgram, "framebuffer"), 0); 
	glUniform1i(glGetUniformLocation(shaderProgram, "framebufferSamples"), g_pathtracer.m_frameBufferSamples); 

	// Bind the vertex array object that contains all the vertex data.
	glBindVertexArray(vertexArrayObject);
	// enable vertex attribute arrays 0 and 1 for the currently bound vertex array object.
	glDrawArrays( GL_TRIANGLE_STRIP, 0, 4 );

	glUseProgram( 0 );						// "unsets" the current shader program. Not really necessary.
	CHECK_GL_ERROR();

	///////////////////////////////////////////////////////////////////////////
	// Print some useful information
	///////////////////////////////////////////////////////////////////////////
	{
		static int lastFrameTime = glutGet(GLUT_ELAPSED_TIME); 
		stringstream ss; 
		ss << "Seconds per frame: " << 0.001 * float(glutGet(GLUT_ELAPSED_TIME) - lastFrameTime); 
		printString(10, 10+14+14, ss.str()); 
		lastFrameTime = glutGet(GLUT_ELAPSED_TIME); 
	}
	{
		stringstream ss; 
		ss << "Samples per pixel: " << g_pathtracer.m_frameBufferSamples; 
		printString(10, 10+14, ss.str()); 
	}
	{
		stringstream ss; 
		ss << "Subsampling: " << 1.0f / float(g_subsample); 
		printString(10, 10+0, ss.str()); 
	}


	glutSwapBuffers();  // swap front and back buffer. This frame will now been displayed.
}

void reshape(int w, int h)
{
	g_pathtracer.resize(w/g_subsample, h/g_subsample);
}

void idle()
{
	glutPostRedisplay(); 
}

void handleKeys(unsigned char key, int /*x*/, int /*y*/)
{
	switch( key )
	{
		// Key 27 => Escape. 
		case 27:
		{
			exit( 0 );
			break;
		}
		case 'c': 
			g_pathtracer.m_selectedCamera += 1; 
			g_pathtracer.restart(); 
			break; 
		case 's':
			g_subsample += 1; 
			g_pathtracer.resize(glutGet(GLUT_WINDOW_WIDTH)/g_subsample, glutGet(GLUT_WINDOW_HEIGHT)/g_subsample);
			break;
		case 'S':
			g_subsample = max(1, g_subsample - 1); 
			g_pathtracer.resize(glutGet(GLUT_WINDOW_WIDTH)/g_subsample, glutGet(GLUT_WINDOW_HEIGHT)/g_subsample);
			break;
		case 'p':
			{
				int w = g_pathtracer.m_frameBufferSize.x;
				int h = g_pathtracer.m_frameBufferSize.y;

				// Allocate a 32-bit dib
				FIBITMAP *dib = FreeImage_Allocate(w, h, 32, FI_RGBA_RED_MASK, FI_RGBA_GREEN_MASK, FI_RGBA_BLUE_MASK);
				// Calculate the number of bytes per pixel (3 for 24-bit or 4 for 32-bit)
				int bytespp = FreeImage_GetLine(dib) / FreeImage_GetWidth(dib);
				for(unsigned y = 0; y < FreeImage_GetHeight(dib); y++) {
					BYTE *bits = FreeImage_GetScanLine(dib, y);
					for(unsigned x = 0; x < FreeImage_GetWidth(dib); x++) {
						float r = min(1.0f, pow(g_pathtracer.m_frameBuffer[y*w + x].x/g_pathtracer.m_frameBufferSamples, 1.0f/2.2f));
						float g = min(1.0f, pow(g_pathtracer.m_frameBuffer[y*w + x].y/g_pathtracer.m_frameBufferSamples, 1.0f/2.2f));
						float b = min(1.0f, pow(g_pathtracer.m_frameBuffer[y*w + x].z/g_pathtracer.m_frameBufferSamples, 1.0f/2.2f));
						bits[FI_RGBA_RED]   = BYTE(r * 255.0);
						bits[FI_RGBA_GREEN] = BYTE(g * 255.0);
						bits[FI_RGBA_BLUE]  = BYTE(b * 255.0);
						bits[FI_RGBA_ALPHA] = 255;
						// jump to next pixel
						bits += bytespp;
					}
				}
				if(!FreeImage_Save(FIF_PNG, dib, "screenshot.png", 0)) {
					cout << "Failed to save screenshot\n";
				}
				FreeImage_Unload(dib); 
			}
			break;
	}
}

int main(int argc, char *argv[])
{
#	if defined(__linux__)
	linux_initialize_cwd();
#	endif // ! __linux__

	glutInit(&argc, argv);
	/* open window of size 512x512 with double buffering, RGB colors, and Z-buffering */
	glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGB | GLUT_DEPTH);
	glutInitWindowSize(512,512);
	glutCreateWindow("Simple Pathtracer");
	glutKeyboardFunc(handleKeys);
	glutReshapeFunc(reshape);
	glutIdleFunc(idle); 
	/* the display function is called once when the gluMainLoop is called,
	* but also each time the window has to be redrawn due to window 
	* changes (overlap, resize, etc).  
	*/
	glutDisplayFunc(display);	// Set the main redraw function

	initGL();

	OBJModel model; 
	model.load("../scenes/cornell.obj"); 
	//model.load("../scenes/cornell_textured.obj"); 
	//model.load("../scenes/cornellbottle2.obj"); 
	g_pathtracer.m_scene = new Scene; 
	g_pathtracer.m_scene->buildFromObj(&model); 
	g_pathtracer.m_selectedCamera = 0; 

	glEnable(GL_FRAMEBUFFER_SRGB);
	glutMainLoop();  /* start the program main loop */

	return 0;          
}
