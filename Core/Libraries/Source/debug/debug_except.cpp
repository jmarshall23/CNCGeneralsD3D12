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

/////////////////////////////////////////////////////////////////////////EA-V1
// $File: //depot/GeneralsMD/Staging/code/Libraries/Source/debug/debug_except.cpp $
// $Author: mhoffe $
// $Revision: #1 $
// $DateTime: 2003/07/03 11:55:26 $
//
// (c) 2003 Electronic Arts
//
// Unhandled exception handler
//////////////////////////////////////////////////////////////////////////////
#include "debug.h"
#include "internal_except.h"
#include <windows.h>
#include <commctrl.h>

DebugExceptionhandler::DebugExceptionhandler()
{
  // don't do anything here!
}

const char *DebugExceptionhandler::GetExceptionType(struct _EXCEPTION_POINTERS *exptr, char *explanation)
{
  #define EX(code,text)   \
    case EXCEPTION_##code: strcpy(explanation,text); return "EXCEPTION_" #code;

  switch(exptr->ExceptionRecord->ExceptionCode)
  {
		case EXCEPTION_ACCESS_VIOLATION:
      wsprintf(explanation,
             "The thread tried to read from or write to a virtual\n"
             "address for which it does not have the appropriate access.\n"
             "Access address 0x%08x was %s.",
                exptr->ExceptionRecord->ExceptionInformation[1],
                exptr->ExceptionRecord->ExceptionInformation[0]?"written to":"read from");
      return "EXCEPTION_ACCESS_VIOLATION";
		EX(ARRAY_BOUNDS_EXCEEDED,"The thread tried to access an array element that\n"
                             "is out of bounds and the underlying hardware\n"
                             "supports bounds checking.")
		EX(BREAKPOINT,"A breakpoint was encountered.")
		EX(DATATYPE_MISALIGNMENT,"The thread tried to read or write data that is\n"
                             "misaligned on hardware that does not provide alignment.\n"
                             "For example, 16-bit values must be aligned on\n"
                             "2-byte boundaries; 32-bit values on 4-byte\n"
                             "boundaries, and so on.")
		EX(FLT_DENORMAL_OPERAND,"One of the operands in a floating-point operation is\n"
                            "denormal. A denormal value is one that is too small\n"
                            "to represent as a standard floating-point value.")
		EX(FLT_DIVIDE_BY_ZERO,"The thread tried to divide a floating-point\n"
                          "value by a floating-point divisor of zero.")
		EX(FLT_INEXACT_RESULT,"The result of a floating-point operation\n"
                          "cannot be represented exactly as a decimal fraction.")
		EX(FLT_INVALID_OPERATION,"Some strange unknown floating point operation was attempted.")
		EX(FLT_OVERFLOW,"The exponent of a floating-point operation is greater\n"
                    "than the magnitude allowed by the corresponding type.")
		EX(FLT_STACK_CHECK,"The stack overflowed or underflowed as the result\n"
                       "of a floating-point operation.")
		EX(FLT_UNDERFLOW,"The exponent of a floating-point operation is less\n"
                     "than the magnitude allowed by the corresponding type.")
    EX(GUARD_PAGE,"A guard page was accessed.")
		EX(ILLEGAL_INSTRUCTION,"The thread tried to execute an invalid instruction.")
		EX(IN_PAGE_ERROR,"The thread tried to access a page that was not\n"
                     "present, and the system was unable to load the page.\n"
                     "For example, this exception might occur if a network "
                     "connection is lost while running a program over the network.")
		EX(INT_DIVIDE_BY_ZERO,"The thread tried to divide an integer value by\n"
                          "an integer divisor of zero.")
		EX(INT_OVERFLOW,"The result of an integer operation caused a carry\n"
                    "out of the most significant bit of the result.")
		EX(INVALID_DISPOSITION,"An exception handler returned an invalid disposition\n"
                           "to the exception dispatcher. Programmers using a\n"
                           "high-level language such as C should never encounter\n"
                           "this exception.")
    EX(INVALID_HANDLE,"An invalid Windows handle was used.")
		EX(NONCONTINUABLE_EXCEPTION,"The thread tried to continue execution after\n"
                                "a noncontinuable exception occurred.")
		EX(PRIV_INSTRUCTION,"The thread tried to execute an instruction whose\n"
                        "operation is not allowed in the current machine mode.")
		EX(SINGLE_STEP,"A trace trap or other single-instruction mechanism\n"
                   "signaled that one instruction has been executed.")
		EX(STACK_OVERFLOW,"The thread used up its stack.")
    case 0xE06D7363: strcpy(explanation,"Microsoft C++ Exception"); return "EXCEPTION_MS";
    default:
      wsprintf(explanation,"Unknown exception code 0x%08x",exptr->ExceptionRecord->ExceptionCode);
      return "EXCEPTION_UNKNOWN";
  }

  #undef EX
}

void DebugExceptionhandler::LogExceptionLocation(Debug &dbg, struct _EXCEPTION_POINTERS *exptr)
{

}

void DebugExceptionhandler::LogRegisters(Debug &dbg, struct _EXCEPTION_POINTERS *exptr)
{
 
}

void DebugExceptionhandler::LogFPURegisters(Debug &dbg, struct _EXCEPTION_POINTERS *exptr)
{
 
}

// include exception dialog box
#include "rc_exception.inl"

// stupid dialog box function needs this
static struct _EXCEPTION_POINTERS *exPtrs;

// and this saves us from re-generating register/version info again...
static char regInfo[1024],verInfo[256];

// and this saves us from doing a stack walk twice
static DebugStackwalk::Signature sig;

static BOOL CALLBACK ExceptionDlgProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
 
  return TRUE;
}

LONG __stdcall DebugExceptionhandler::ExceptionFilter(struct _EXCEPTION_POINTERS* pExPtrs)
{
  
  // Now die
  return EXCEPTION_EXECUTE_HANDLER;
}
