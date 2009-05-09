
#ifdef WIN32
#define _STDCALL_SUPPORTED
#define _M_IX86
#endif

extern "C" {
#include "GL/glew.h"

#ifndef __APPLE__
#include "GL/glut.h"
#include <GL/gl.h>
#include <GL/glu.h>
#include <GL/glext.h>
#else
#include "GLUT/glut.h"
#include <OpenGL/gl.h>
#include <OpenGL/glu.h>
#include <OpenGL/glext.h>
#endif
}
