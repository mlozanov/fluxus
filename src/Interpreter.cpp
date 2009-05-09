// Copyright (C) 2007 Dave Griffiths
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

#include <iostream>
#include "Interpreter.h"
#include "Repl.h"

#ifdef STATIC_LINK
#include "../modules/fluxus-engine/src/FluxusEngine.h"
#include "../modules/fluxus-audio/src/FluxusAudio.h"
#include "../modules/fluxus-osc/src/FluxusOSC.h"
#include "../modules/fluxus-midi/src/FluxusMIDI.h"
#endif

// include the scheme bytecode, created by mzc in SConstruct
#include "base.c"

using namespace std;
using namespace fluxus;

static const int LOG_SIZE=256;

static const string STARTUP_SCRIPT="(define plt-collects-location \"%s\") " \
									"(define fluxus-collects-location \"%s\") " \
									"(define fluxus-version \"%d%d\") " \
									"(define fluxus-data-location \"%s\") " \
									"(define static-link \"%s\") "\
									"(define fluxus-platform '\"%s\") "\
									"(load (string-append fluxus-collects-location \"/fluxus-\" " \
										"fluxus-version \"/boot.scm\")) ";

Scheme_Env *Interpreter::m_Scheme=NULL;
Repl *Interpreter::m_Repl=NULL;
Scheme_Object *Interpreter::m_OutReadPort=NULL;
Scheme_Object *Interpreter::m_ErrReadPort=NULL;
Scheme_Object *Interpreter::m_OutWritePort=NULL;
Scheme_Object *Interpreter::m_ErrWritePort=NULL;
std::string Interpreter::m_Language;
	
void Interpreter::Register()
{
	MZ_GC_DECL_REG(0);	
    MZ_GC_REG();

	MZ_REGISTER_STATIC(Interpreter::m_Scheme);
	MZ_REGISTER_STATIC(Interpreter::m_OutReadPort);
	MZ_REGISTER_STATIC(Interpreter::m_ErrReadPort);
	MZ_REGISTER_STATIC(Interpreter::m_OutWritePort);
	MZ_REGISTER_STATIC(Interpreter::m_ErrWritePort);
	
    MZ_GC_UNREG();
}

void Interpreter::Initialise()
{
	Scheme_Config *config;
	Scheme_Object *v = NULL;

	MZ_GC_DECL_REG(2);
	MZ_GC_VAR_IN_REG(0, v);
    MZ_GC_VAR_IN_REG(1, config);
	MZ_GC_REG();

	m_Scheme=scheme_basic_env();

	scheme_pipe(&m_OutReadPort,&m_OutWritePort);
	scheme_pipe(&m_ErrReadPort,&m_ErrWritePort);
	config = scheme_current_config();
	scheme_set_param(config, MZCONFIG_OUTPUT_PORT, m_OutWritePort);
	scheme_set_param(config, MZCONFIG_ERROR_PORT, m_ErrWritePort);

	declare_modules(m_Scheme);
    v = scheme_intern_symbol("scheme/base");
    scheme_namespace_require(v);

	// figure out what we are running on
	#ifdef WIN32
	string platform = "win32";
	#else
	#ifdef __APPLE__
	string platform = "apple";
	#else
	string platform = "linux";
	#endif
	#endif

#ifdef STATIC_LINK
	engine_scheme_reload(m_Scheme);
	audio_scheme_reload(m_Scheme);
	osc_scheme_reload(m_Scheme);
	midi_scheme_reload(m_Scheme);
	
	// need the cwd to hack the collects path to 'relative'
	char cwd[1024];
	#ifndef __APPLE__
	getcwd(cwd, 1024);
	#endif
	string LocalCollects=string(cwd)+string("/collects");	
#endif

	// load the startup script
	char startup[1024];
	
	#ifdef STATIC_LINK
	// insert the version number etc
	snprintf(startup,1024,STARTUP_SCRIPT.c_str(),
		LocalCollects.c_str(),
		LocalCollects.c_str(),
		FLUXUS_MAJOR_VERSION,
		FLUXUS_MINOR_VERSION,
		cwd,
		"#t",
		platform.c_str()
		);
#else
	// insert the version number etc
	snprintf(startup,1024,STARTUP_SCRIPT.c_str(),
		PLT_COLLECTS_LOCATION,
		FLUXUS_COLLECTS_LOCATION,
		FLUXUS_MAJOR_VERSION,
		FLUXUS_MINOR_VERSION,
		DATA_LOCATION,
		"#f",
		platform.c_str()
		);
#endif

	Interpret(startup,NULL,true);

    MZ_GC_UNREG();
}

void Interpreter::SetRepl(Repl *s) 
{ 
	m_Repl=s; 
}

int fill_from_port(Scheme_Object* port, char *dest, long size)
{
	MZ_GC_DECL_REG(2);
	MZ_GC_VAR_IN_REG(0, port);
	MZ_GC_VAR_IN_REG(1, dest);
	MZ_GC_REG();

	long pos=0;
	while (scheme_char_ready(port) && pos<size)
	{
		dest[pos++]=scheme_getc(port);
	}
	dest[pos]=0;

	// check if there are more characters available
	int r=scheme_char_ready(port);
	MZ_GC_UNREG();
	return r;
}

string Interpreter::SetupLanguage(const string &str)
{
	if (m_Language.empty()) return str;
	return "(module foo "+m_Language+" "+str+") (require foo)";
}

bool Interpreter::Interpret(const string &str, Scheme_Object **ret, bool abort)
{
	char msg[LOG_SIZE];
	mz_jmp_buf * volatile save = NULL, fresh;

	MZ_GC_DECL_REG(1);
	MZ_GC_VAR_IN_REG(0, msg);
	MZ_GC_REG();

	string code = SetupLanguage(str);

	save = scheme_current_thread->error_buf;
	scheme_current_thread->error_buf = &fresh;

	if (scheme_setjmp(scheme_error_buf))
	{
		scheme_current_thread->error_buf = save;
		if (m_ErrReadPort!=NULL)
		{
			int char_available;
			do
			{
				char_available = fill_from_port(m_ErrReadPort, msg, LOG_SIZE);
				if (strlen(msg)>0)
				{
					if (m_Repl==NULL) cerr<<msg<<endl;
					else m_Repl->Print(string(msg));
				}
			} while (char_available);
		}
		if (abort) exit(-1);
		MZ_GC_UNREG();
		return false;
	}
	else
	{
		if (ret==NULL)
		{
			scheme_eval_string_all(code.c_str(), m_Scheme, 1);
		}
		else
		{
			*ret = scheme_eval_string_all(code.c_str(), m_Scheme, 1);
		}
		scheme_current_thread->error_buf = save;
	}

	if (m_OutReadPort!=NULL)
	{
		int char_available;
		do
		{
			char_available = fill_from_port(m_OutReadPort, msg, LOG_SIZE);
			if (strlen(msg)>0)
			{
				if (m_Repl==NULL) cerr<<msg<<endl;
				else m_Repl->Print(string(msg));
			}
		} while (char_available);
	}

	MZ_GC_UNREG();
	return true;
}

