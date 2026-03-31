/*
**	Command & Conquer Generals Zero Hour(tm)
**	Copyright 2025 Electronic Arts Inc.
**
**	This program is free software: you can redistribute it and/or modify
**	it under the terms of the GNU General Public License as published by
**	the Free Software Foundation, either version 3 of the License, or
**	(at your option) any later version.
**
**	This program is distributed in the hope that it will be useful,
**	but WITHOUT ANY WARRANTY; without even the implied warranty of
**	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
**	GNU General Public License for more details.
**
**	You should have received a copy of the GNU General Public License
**	along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

////////////////////////////////////////////////////////////////////////////////
//																																						//
//  (c) 2001-2003 Electronic Arts Inc.																				//
//																																						//
////////////////////////////////////////////////////////////////////////////////

#include "PreRTS.h"	// This must go first in EVERY cpp file in the GameEngine

#if defined(RTS_DEBUG) || defined(IG_DEBUG_STACKTRACE)

#pragma pack(push, 8)

#include "Common/StackDump.h"
#include "Common/Debug.h"

#include "DbgHelpLoader.h"

//*****************************************************************************
//	Prototypes
//*****************************************************************************
BOOL InitSymbolInfo();
void MakeStackTrace(DWORD myeip,DWORD myesp,DWORD myebp, int skipFrames, void (*callback)(const char*));
void GetFunctionDetails(void *pointer, char*name, char*filename, unsigned int* linenumber, unsigned int* address);
void WriteStackLine(void*address, void (*callback)(const char*));

//*****************************************************************************
//	Mis-named globals :-)
//*****************************************************************************
static CONTEXT gsContext;


//*****************************************************************************
//*****************************************************************************
void StackDumpDefaultHandler(const char*line)
{
	DEBUG_LOG((line));
}


//*****************************************************************************
//*****************************************************************************
void StackDump(void (*callback)(const char*))
{

}


//*****************************************************************************
//*****************************************************************************
void StackDumpFromContext(DWORD eip,DWORD esp,DWORD ebp, void (*callback)(const char*))
{
	if (callback == nullptr)
	{
		callback = StackDumpDefaultHandler;
	}

	if (!InitSymbolInfo())
		return;

	MakeStackTrace(eip,esp,ebp, 0,  callback);
}


//*****************************************************************************
//*****************************************************************************
BOOL InitSymbolInfo()
{
	if (DbgHelpLoader::isLoaded())
		return TRUE;

	if (DbgHelpLoader::isFailed())
		return FALSE;

	if (!DbgHelpLoader::load())
	{
		atexit(DbgHelpLoader::unload);
		return FALSE;
	}

	char pathname[_MAX_PATH+1];
	char drive[10];
	char directory[_MAX_PATH+1];
	HANDLE process;

	DbgHelpLoader::symSetOptions(SYMOPT_DEFERRED_LOADS | SYMOPT_UNDNAME | SYMOPT_LOAD_LINES | SYMOPT_OMAP_FIND_NEAREST);

	process = GetCurrentProcess();

	//Get the apps name
	::GetModuleFileName(nullptr, pathname, _MAX_PATH);

	// turn it into a search path
	_splitpath(pathname, drive, directory, nullptr, nullptr);
	sprintf(pathname, "%s:\\%s", drive, directory);

	// append the current directory to build a search path for SymInit
	::lstrcat(pathname, ";.;");

	if(DbgHelpLoader::symInitialize(process, pathname, FALSE))
	{
		// regenerate the name of the app
		::GetModuleFileName(nullptr, pathname, _MAX_PATH);
		if(DbgHelpLoader::symLoadModule(process, nullptr, pathname, nullptr, 0, 0))
		{
				//Load any other relevant modules (ie dlls) here
				atexit(DbgHelpLoader::unload);
				return TRUE;
		}
	}

	DbgHelpLoader::unload();
	return FALSE;
}


//*****************************************************************************
//*****************************************************************************
void MakeStackTrace(DWORD myeip,DWORD myesp,DWORD myebp, int skipFrames, void (*callback)(const char*))
{

}


//*****************************************************************************
//*****************************************************************************
void GetFunctionDetails(void *pointer, char*name, char*filename, unsigned int* linenumber, unsigned int* address)
{
	if (!InitSymbolInfo())
		return;

	if (name)
	{
		strcpy(name, "<Unknown>");
	}
	if (filename)
	{
		strcpy(filename, "<Unknown>");
	}
	if (linenumber)
	{
		*linenumber = 0xFFFFFFFF;
	}
	if (address)
	{
		*address = 0xFFFFFFFF;
	}

	ULONG displacement = 0;

    HANDLE process = ::GetCurrentProcess();

    char symbol_buffer[512 + sizeof(IMAGEHLP_SYMBOL)];
    memset(symbol_buffer, 0, sizeof(symbol_buffer));

    PIMAGEHLP_SYMBOL psymbol = (PIMAGEHLP_SYMBOL)symbol_buffer;
    psymbol->SizeOfStruct = sizeof(symbol_buffer);
    psymbol->MaxNameLength = 512;

	if (DbgHelpLoader::symGetSymFromAddr(process, (DWORD) pointer, &displacement, psymbol))
	{
		if (name)
		{
			strcpy(name, psymbol->Name);
			strcat(name, "();");
		}

		// Get line now

		IMAGEHLP_LINE line;
		memset(&line,0,sizeof(line));
		line.SizeOfStruct = sizeof(line);

		if (DbgHelpLoader::symGetLineFromAddr(process, (DWORD) pointer, &displacement, &line))
		{
			if (filename)
			{
				strcpy(filename, line.FileName);
			}
			if (linenumber)
			{
				*linenumber = (unsigned int)line.LineNumber;
			}
			if (address)
			{
				*address = (unsigned int)line.Address;
			}
		}
	}
}


//*****************************************************************************
// Gets last x addresses from the stack
//*****************************************************************************
void FillStackAddresses(void**addresses, unsigned int count, unsigned int skip)
{
	
}



//*****************************************************************************
// Do full stack dump using an address array
//*****************************************************************************
void StackDumpFromAddresses(void**addresses, unsigned int count, void (*callback)(const char *))
{
	if (callback == nullptr)
	{
		callback = StackDumpDefaultHandler;
	}

	if (!InitSymbolInfo())
		return;

	while ((count--) && (*addresses!=nullptr))
	{
		WriteStackLine(*addresses,callback);
		addresses++;
	}
}


AsciiString g_LastErrorDump;
//*****************************************************************************
//*****************************************************************************
void WriteStackLine(void*address, void (*callback)(const char*))
{
	static char line[MAX_PATH];
	static char function_name[512];
	static char filename[MAX_PATH];
	unsigned int linenumber;
	unsigned int addr;

	GetFunctionDetails(address, function_name, filename, &linenumber, &addr);
    sprintf(line, "  %s(%d) : %s 0x%08p", filename, linenumber, function_name, address);
		if (g_LastErrorDump.isNotEmpty()) {
			g_LastErrorDump.concat(line);
			g_LastErrorDump.concat("\n");
		}
	callback(line);
}


//*****************************************************************************
//*****************************************************************************
void DumpExceptionInfo( unsigned int u, EXCEPTION_POINTERS* e_info )
{
	
}


#pragma pack(pop)

#endif

