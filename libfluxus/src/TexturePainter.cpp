// Copyright (C) 2005 Dave Griffiths
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software
// Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.

#include "OpenGL.h"
#include "State.h"
#include "TexturePainter.h"
#include "PNGLoader.h"
#include "SearchPaths.h"
#include <assert.h>

using namespace Fluxus;

TexturePainter *TexturePainter::m_Singleton=NULL;

TexturePainter::TexturePainter() :
m_MultitexturingEnabled(true)
{
	#ifndef DISABLE_MULTITEXTURING
	if (!GLEW_ARB_multitexture || (glActiveTexture == NULL) || (glClientActiveTexture == NULL))
	{
		Trace::Stream<<"Warning: Can't do multitexturing (no glActiveTexture or GLEW_ARB_multitexture not set)"<<endl;
		m_MultitexturingEnabled=false;
	}
	#else
	m_MultitexturingEnabled=false;
	#endif
}

TexturePainter::~TexturePainter()
{
	///\todo Shouldn't we delete all textures here?
}

void TexturePainter::Initialise()
{
#ifndef DISABLE_MULTITEXTURING
	if (m_MultitexturingEnabled)
	{
		for (int c=0; c<MAX_TEXTURES; c++)
		{
			glActiveTexture(GL_TEXTURE0+c);
			glClientActiveTexture(GL_TEXTURE0+c);
			glEnableClientState(GL_TEXTURE_COORD_ARRAY);
			glMatrixMode(GL_TEXTURE);
			glLoadIdentity();
		}
		glClientActiveTexture(GL_TEXTURE0);
	}
	else
#endif
	{
		glEnableClientState(GL_TEXTURE_COORD_ARRAY);
		glMatrixMode(GL_TEXTURE);
		glLoadIdentity();
	}
}

void TexturePainter::ClearCache()
{
	m_TextureMap.clear();
	m_LoadedMap.clear();
	m_LoadedCubeMap.clear();
}

unsigned int TexturePainter::LoadTexture(const string &Filename, CreateParams &params)
{
	string Fullpath = SearchPaths::Get()->GetFullPath(Filename);
	if (params.Type==GL_TEXTURE_CUBE_MAP_POSITIVE_X || params.Type==GL_TEXTURE_CUBE_MAP_NEGATIVE_X ||
		params.Type==GL_TEXTURE_CUBE_MAP_POSITIVE_Y || params.Type==GL_TEXTURE_CUBE_MAP_NEGATIVE_Y ||
		params.Type==GL_TEXTURE_CUBE_MAP_POSITIVE_Z || params.Type==GL_TEXTURE_CUBE_MAP_NEGATIVE_Z)
	{
		return LoadCubeMap(Fullpath, params);
	}
	
	// see if we've loaded this one already
	map<string,int>::iterator i=m_LoadedMap.find(Fullpath);
	if (i!=m_LoadedMap.end())
	{
		return i->second;
	}
	
	unsigned char *ImageData;
    TextureDesc desc;
    ImageData=PNGLoader::Load(Fullpath,desc.Width,desc.Height,desc.Format);
	
	if (ImageData!=NULL)
    {
		//\todo support more depths than 8bit
		
		// upload to card...
		glEnable(params.Type);
		
		if (params.ID==-1) // is this a new texture?
		{
			GLuint id;
			glGenTextures(1,&id);
			params.ID=id; // ahem
			//\todo this means mipmap levels won't be cached
    		m_TextureMap[params.ID]=desc;
			m_LoadedMap[Fullpath]=params.ID;					
		}
				
		UploadTexture(desc,params,ImageData);										
		delete[] ImageData;
		return params.ID;		
	}	
	m_LoadedMap[Fullpath]=0;
    return 0;
}

unsigned int TexturePainter::LoadCubeMap(const string &Fullpath, CreateParams &params)
{	
	// see if we've loaded this one already
	map<string,int>::iterator i=m_LoadedCubeMap.find(Fullpath);
	if (i!=m_LoadedCubeMap.end())
	{
		return i->second;
	}

	unsigned char *ImageData;
    TextureDesc desc;
    ImageData=PNGLoader::Load(Fullpath,desc.Width,desc.Height,desc.Format);
	
	if (ImageData!=NULL)
    {
		//\todo support more depths than 8bit
		
		// upload to card...
		glEnable(params.Type);
		
		if (params.ID==-1) // is this a new texture?
		{
			GLuint id;
			glGenTextures(1,&id);
			params.ID=id; // ahem
			//\todo this means cubemaps won't be cached
    		m_TextureMap[params.ID]=desc;
			m_LoadedCubeMap[Fullpath]=params.ID;
						
			CubeMapDesc newcubemap;
			m_CubeMapMap[params.ID] = newcubemap;
				
			switch (params.Type) // record the cubemap face
			{
				case GL_TEXTURE_CUBE_MAP_POSITIVE_X: m_CubeMapMap[params.ID].Positive[0]=params.ID; break;
				case GL_TEXTURE_CUBE_MAP_NEGATIVE_X: m_CubeMapMap[params.ID].Negative[0]=params.ID; break;
				case GL_TEXTURE_CUBE_MAP_POSITIVE_Y: m_CubeMapMap[params.ID].Positive[1]=params.ID; break;
				case GL_TEXTURE_CUBE_MAP_NEGATIVE_Y: m_CubeMapMap[params.ID].Negative[1]=params.ID; break;
				case GL_TEXTURE_CUBE_MAP_POSITIVE_Z: m_CubeMapMap[params.ID].Positive[2]=params.ID; break;
				case GL_TEXTURE_CUBE_MAP_NEGATIVE_Z: m_CubeMapMap[params.ID].Negative[2]=params.ID; break;
				default: assert(0); break;
			}
		}
		else // we have an existing texture id specified
		{
			// make a new texture id for this face
			// record the primary cubemap id
			unsigned int primary = params.ID;
			GLuint id;
			glGenTextures(1,&id);
			params.ID=id; // ahem
    		m_TextureMap[params.ID]=desc;
			m_LoadedCubeMap[Fullpath]=params.ID;

			switch (params.Type) // record the cubemap face
			{
				case GL_TEXTURE_CUBE_MAP_POSITIVE_X: m_CubeMapMap[primary].Positive[0]=params.ID; break;
				case GL_TEXTURE_CUBE_MAP_NEGATIVE_X: m_CubeMapMap[primary].Negative[0]=params.ID; break;
				case GL_TEXTURE_CUBE_MAP_POSITIVE_Y: m_CubeMapMap[primary].Positive[1]=params.ID; break;
				case GL_TEXTURE_CUBE_MAP_NEGATIVE_Y: m_CubeMapMap[primary].Negative[1]=params.ID; break;
				case GL_TEXTURE_CUBE_MAP_POSITIVE_Z: m_CubeMapMap[primary].Positive[2]=params.ID; break;
				case GL_TEXTURE_CUBE_MAP_NEGATIVE_Z: m_CubeMapMap[primary].Negative[2]=params.ID; break;
				default: assert(0); break;
			}
		}
			
		UploadTexture(desc,params,ImageData);					
		delete[] ImageData;
		return params.ID;		
	}	
	m_LoadedMap[Fullpath]=0;
    return 0;
}

void TexturePainter::UploadTexture(TextureDesc desc, CreateParams params, const unsigned char *ImageData)
{
	glBindTexture(params.Type,params.ID);
		
	if (params.GenerateMipmaps)
	{
		if (desc.Format==RGB)
    	{
			gluBuild2DMipmaps(params.Type,3,desc.Width,desc.Height,GL_RGB,GL_UNSIGNED_BYTE,ImageData);
    	}
    	else
    	{
    		gluBuild2DMipmaps(params.Type,4,desc.Width,desc.Height,GL_RGBA,GL_UNSIGNED_BYTE,ImageData);
    	}
	}
	else
	{		
		//\todo check power of two
		//\todo and scale?
		if (desc.Format==RGB)
    	{				
			glTexImage2D(params.Type,params.MipLevel,3,desc.Width,desc.Height,params.Border,
				GL_RGB,GL_UNSIGNED_BYTE,ImageData);			
		}
    	else
    	{
			glTexImage2D(params.Type,params.MipLevel,4,desc.Width,desc.Height,params.Border,
				GL_RGBA,GL_UNSIGNED_BYTE,ImageData);
		}
	}		
}

///todo: all pdata texture load/save is to 8 bit RGB or RGBA - need to deal with arbitrary channels and bit depths
bool TexturePainter::LoadPData(const string &Filename, unsigned int &w, unsigned int &h, TypedPData<dColour> &pixels)
{
	string Fullpath = SearchPaths::Get()->GetFullPath(Filename);

	unsigned char *ImageData;
    TextureDesc desc;
    ImageData=PNGLoader::Load(Fullpath,desc.Width,desc.Height,desc.Format);
	
	if (ImageData!=NULL)
    {
		pixels.Resize(desc.Width*desc.Height);
		w=desc.Width;
		h=desc.Height;

		if (desc.Format==RGB)
		{
			for (unsigned int n=0; n<desc.Width*desc.Height; n++)
			{
				pixels.m_Data[n]=dColour(ImageData[n*3]/255.0f, 
				                         ImageData[n*3+1]/255.0f,
										 ImageData[n*3+2]/255.0f,1.0f);
				
			}	
		}	
		else if (desc.Format==RGBA)
		{
			for (unsigned int n=0; n<desc.Width*desc.Height; n++)
			{
				pixels.m_Data[n]=dColour(ImageData[n*4]/255.0f, 
				                         ImageData[n*4+1]/255.0f,
										 ImageData[n*4+2]/255.0f,
										 ImageData[n*4+3]/255.0f);
				
			}	
		}
		else
		{
			delete[] ImageData;
			return false;
		}
			
		delete[] ImageData;
		return true;
	}
	
	return false;
}

bool TexturePainter::SavePData(const string &Filename, unsigned int w, unsigned int h, const TypedPData<dColour> &pixels)
{
	// save as 8bit rgba 
	unsigned char *ImageData=new unsigned char[w*h*4];
	for (unsigned int n=0; n<w*h; n++)
	{
		ImageData[n*4]=(unsigned char)(pixels.m_Data[n].r*255.0f);
		ImageData[n*4+1]=(unsigned char)(pixels.m_Data[n].g*255.0f);
		ImageData[n*4+2]=(unsigned char)(pixels.m_Data[n].b*255.0f);
		ImageData[n*4+3]=(unsigned char)(pixels.m_Data[n].a*255.0f);
	}
	
	PNGLoader::Save(Filename, w, h, RGBA, ImageData);
	
	delete[] ImageData;
	return true;
}

unsigned int TexturePainter::MakeTexture(unsigned int w, unsigned int h, PData *data)
{
	GLuint ID=0;
	TypedPData<dColour> *pixels = dynamic_cast<TypedPData<dColour>*>(data);
	if (pixels)
	{
		// upload to card...
		glGenTextures(1,&ID);
    	glBindTexture(GL_TEXTURE_2D,ID);
    	gluBuild2DMipmaps(GL_TEXTURE_2D,4,w,h,GL_RGBA,GL_FLOAT,&pixels->m_Data[0]);
		return ID;
	}
    return 0;
}

bool TexturePainter::SetCurrent(unsigned int *ids, TextureState *states)
{
	bool ret=false;

	int tcount = (m_MultitexturingEnabled ? MAX_TEXTURES : 1);
	for (int c = 0; c < tcount; c++)
	{
	#ifndef DISABLE_MULTITEXTURE
		if (m_MultitexturingEnabled)
		{
			glActiveTexture(GL_TEXTURE0+c);
		}
	#endif

		if (ids[c]!=0)
		{
			map<unsigned int,CubeMapDesc>::iterator i=m_CubeMapMap.find(ids[c]);

			if (i!=m_CubeMapMap.end()) // cubemap texture path
			{
				glBindTexture(GL_TEXTURE_CUBE_MAP_POSITIVE_X,i->second.Positive[0]);
				glBindTexture(GL_TEXTURE_CUBE_MAP_NEGATIVE_X,i->second.Negative[0]);
				glBindTexture(GL_TEXTURE_CUBE_MAP_POSITIVE_Y,i->second.Positive[1]);
				glBindTexture(GL_TEXTURE_CUBE_MAP_NEGATIVE_Y,i->second.Negative[1]);
				glBindTexture(GL_TEXTURE_CUBE_MAP_POSITIVE_Z,i->second.Positive[2]);
				glBindTexture(GL_TEXTURE_CUBE_MAP_NEGATIVE_Z,i->second.Negative[2]);

				glEnable(GL_TEXTURE_CUBE_MAP);
				ApplyState(GL_TEXTURE_CUBE_MAP,states[c],true);
			}
			else // normal 2D texture path
			{
				glEnable(GL_TEXTURE_2D);
				glBindTexture(GL_TEXTURE_2D,ids[c]);
				ApplyState(GL_TEXTURE_2D,states[c],false);
			}

			ret=true;
		}
		else
		{
			glDisable(GL_TEXTURE_2D);
			glDisable(GL_TEXTURE_CUBE_MAP);
		}
	}

	#ifndef DISABLE_MULTITEXTURE
	if (m_MultitexturingEnabled)
	{
		glActiveTexture(GL_TEXTURE0);
	}
	#endif
	
	return ret;
}

void TexturePainter::ApplyState(int type, TextureState &state, bool cubemap)
{
	glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, state.TexEnv);
	glTexEnvfv(GL_TEXTURE_ENV, GL_TEXTURE_ENV_COLOR, state.EnvColour.arr());
	glTexParameteri(type, GL_TEXTURE_MIN_FILTER, state.Min);
	glTexParameteri(type, GL_TEXTURE_MAG_FILTER, state.Mag);	
    glTexParameteri(type, GL_TEXTURE_WRAP_S, state.WrapS);
	glTexParameteri(type, GL_TEXTURE_WRAP_T, state.WrapT);
	if (cubemap) glTexParameteri(type, GL_TEXTURE_WRAP_R, state.WrapT);
	glTexParameterfv(type, GL_TEXTURE_BORDER_COLOR, state.BorderColour.arr());
	glTexParameterf(type, GL_TEXTURE_PRIORITY, state.Priority);
	glTexParameterf(type, GL_TEXTURE_MIN_LOD, state.MinLOD);
	glTexParameterf(type, GL_TEXTURE_MAX_LOD, state.MaxLOD);
}

void TexturePainter::DisableAll()
{
	#ifndef DISABLE_MULTITEXTURE
	if (m_MultitexturingEnabled)
	{
		for (int c=0; c<MAX_TEXTURES; c++)
		{
			glActiveTexture(GL_TEXTURE0+c);
			glDisable(GL_TEXTURE_2D);
			glDisable(GL_TEXTURE_CUBE_MAP);
		}
		glClientActiveTexture(GL_TEXTURE0);
	}
	else
	#endif
	{
		glDisable(GL_TEXTURE_2D);
		glDisable(GL_TEXTURE_CUBE_MAP);
	}
}

void TexturePainter::Dump()
{
	for (map<string,int>::iterator i=m_LoadedMap.begin(); i!=m_LoadedMap.end(); ++i)
	{
		TextureDesc info = m_TextureMap[i->second];
		Trace::Stream<<i->first<<" "<<info.Width<<"X"<<info.Height<<" ";
		if (info.Format==RGB) Trace::Stream<<"RGB"<<endl;
		else if (info.Format==RGBA) Trace::Stream<<"RGBA"<<endl;
	}
}





