// a5dbg - minimal crash-reporting launcher for the VS2022 port (no debugger is installed with Build Tools).
// Runs a program under the Win32 debug API, echoes OutputDebugString text, and prints a symbolized
// stack for access violations / unhandled exceptions. Build (x86 developer prompt):
//   cl /nologo /EHsc /O1 a5dbg.cpp dbghelp.lib
// usage: a5dbg [-t seconds] [-cxx] <exe> [args...]      (working directory = current directory)
//   -t N   stop the program after N seconds (default: wait until it exits)
//   -cxx   also print stacks for first-chance C++ exceptions
#define _CRT_SECURE_NO_WARNINGS
#include <windows.h>
#include <dbghelp.h>
#include <stdio.h>
#include <string>

static HANDLE hProc;

static void PrintStack( DWORD dwThread )
{
	HANDLE hThread = OpenThread( THREAD_ALL_ACCESS, FALSE, dwThread );
	if ( !hThread )
		return;
	CONTEXT ctx = {};
	ctx.ContextFlags = CONTEXT_FULL;
	if ( !GetThreadContext( hThread, &ctx ) )
	{
		CloseHandle( hThread );
		return;
	}
	STACKFRAME64 f = {};
	f.AddrPC.Offset = ctx.Eip; f.AddrPC.Mode = AddrModeFlat;
	f.AddrFrame.Offset = ctx.Ebp; f.AddrFrame.Mode = AddrModeFlat;
	f.AddrStack.Offset = ctx.Esp; f.AddrStack.Mode = AddrModeFlat;
	for ( int i = 0; i < 40; ++i )
	{
		if ( !StackWalk64( IMAGE_FILE_MACHINE_I386, hProc, hThread, &f, &ctx, 0, SymFunctionTableAccess64, SymGetModuleBase64, 0 ) )
			break;
		if ( f.AddrPC.Offset == 0 )
			break;
		char buf[sizeof(SYMBOL_INFO) + 512] = {};
		SYMBOL_INFO *pSym = (SYMBOL_INFO*)buf;
		pSym->SizeOfStruct = sizeof(SYMBOL_INFO);
		pSym->MaxNameLen = 511;
		DWORD64 disp = 0;
		IMAGEHLP_MODULE64 mod = {};
		mod.SizeOfStruct = sizeof(mod);
		const char *pszMod = SymGetModuleInfo64( hProc, f.AddrPC.Offset, &mod ) ? mod.ModuleName : "?";
		IMAGEHLP_LINE64 line = {};
		line.SizeOfStruct = sizeof(line);
		DWORD ldisp = 0;
		if ( SymFromAddr( hProc, f.AddrPC.Offset, &disp, pSym ) )
		{
			if ( SymGetLineFromAddr64( hProc, f.AddrPC.Offset, &ldisp, &line ) )
				printf( "    #%02d %s!%s+0x%X  [%s:%lu]\n", i, pszMod, pSym->Name, (unsigned)disp, line.FileName, line.LineNumber );
			else
				printf( "    #%02d %s!%s+0x%X\n", i, pszMod, pSym->Name, (unsigned)disp );
		}
		else
			printf( "    #%02d %s!0x%08X\n", i, pszMod, (unsigned)f.AddrPC.Offset );
	}
	CloseHandle( hThread );
}

static void LoadModule( HANDLE hFile, void *pBase )
{
	char szName[MAX_PATH * 2] = {};
	if ( hFile )
		GetFinalPathNameByHandleA( hFile, szName, sizeof(szName) - 1, 0 );
	const char *pszName = szName;
	if ( !strncmp( pszName, "\\?\\", 4 ) )
		pszName += 4;
	SymLoadModuleEx( hProc, hFile, pszName[0] ? pszName : 0, 0, (DWORD64)(DWORD_PTR)pBase, 0, 0, 0 );
}

static std::string ReadRemote( const void *p, DWORD nLen, bool bUnicode )
{
	std::string s;
	if ( bUnicode )
	{
		std::wstring w( nLen, 0 );
		SIZE_T n = 0;
		ReadProcessMemory( hProc, p, &w[0], nLen * 2, &n );
		char buf[4096] = {};
		WideCharToMultiByte( CP_ACP, 0, w.c_str(), -1, buf, sizeof(buf) - 1, 0, 0 );
		s = buf;
	}
	else
	{
		s.resize( nLen );
		SIZE_T n = 0;
		ReadProcessMemory( hProc, p, &s[0], nLen, &n );
		s.resize( strlen( s.c_str() ) );
	}
	return s;
}

int main( int argc, char **argv )
{
	int nTimeout = 0, i = 1;
	bool bCxx = false;
	for ( ; i < argc && argv[i][0] == '-'; ++i )
	{
		if ( !strcmp( argv[i], "-t" ) && i + 1 < argc )
			nTimeout = atoi( argv[++i] );
		else if ( !strcmp( argv[i], "-cxx" ) )
			bCxx = true;
	}
	if ( i >= argc )
	{
		printf( "usage: a5dbg [-t seconds] [-cxx] <exe> [args...]\n" );
		return 2;
	}
	std::string cmd;
	for ( ; i < argc; ++i )
		cmd += std::string( "\"" ) + argv[i] + "\" ";

	STARTUPINFOA si = { sizeof(si) };
	PROCESS_INFORMATION pi = {};
	if ( !CreateProcessA( 0, &cmd[0], 0, 0, FALSE, DEBUG_ONLY_THIS_PROCESS, 0, 0, &si, &pi ) )
	{
		printf( "CreateProcess failed: %lu\n", GetLastError() );
		return 2;
	}
	hProc = pi.hProcess;
	SymSetOptions( SYMOPT_LOAD_LINES | SYMOPT_UNDNAME );
	SymInitialize( hProc, 0, FALSE );
	DWORD dwStart = GetTickCount();
	int nCxx = 0, nAV = 0;
	bool bFirstBreak = true;
	for ( ;; )
	{
		DEBUG_EVENT ev = {};
		if ( !WaitForDebugEvent( &ev, 500 ) )
		{
			if ( nTimeout && GetTickCount() - dwStart > (DWORD)nTimeout * 1000 )
			{
				printf( "== still running after %d s - terminating (no crash)\n", nTimeout );
				TerminateProcess( hProc, 0 );
				return 0;
			}
			continue;
		}
		DWORD dwContinue = DBG_CONTINUE;
		switch ( ev.dwDebugEventCode )
		{
			case CREATE_PROCESS_DEBUG_EVENT:
				LoadModule( ev.u.CreateProcessInfo.hFile, ev.u.CreateProcessInfo.lpBaseOfImage );
				break;
			case LOAD_DLL_DEBUG_EVENT:
				LoadModule( ev.u.LoadDll.hFile, ev.u.LoadDll.lpBaseOfDll );
				break;
			case OUTPUT_DEBUG_STRING_EVENT:
			{
				std::string s = ReadRemote( ev.u.DebugString.lpDebugStringData, ev.u.DebugString.nDebugStringLength, ev.u.DebugString.fUnicode != 0 );
				printf( "[dbg] %s%s", s.c_str(), ( !s.empty() && s[s.size() - 1] == '\n' ) ? "" : "\n" );
				break;
			}
			case EXCEPTION_DEBUG_EVENT:
			{
				const EXCEPTION_RECORD &r = ev.u.Exception.ExceptionRecord;
				if ( r.ExceptionCode == EXCEPTION_BREAKPOINT && bFirstBreak )
				{
					bFirstBreak = false;
					break;
				}
				if ( r.ExceptionCode == 0x4000001F || r.ExceptionCode == 0x406D1388 ) // WOW64 breakpoint, thread name
					break;
				dwContinue = DBG_EXCEPTION_NOT_HANDLED;
				const bool bSecond = ev.u.Exception.dwFirstChance == 0;
				const bool bIsCxx = r.ExceptionCode == 0xE06D7363;
				if ( bIsCxx && !bSecond )
				{
					if ( ++nCxx <= 20 )
						printf( "== first-chance C++ exception at 0x%p (thread %lu)\n", r.ExceptionAddress, ev.dwThreadId );
					if ( bCxx && nCxx <= 5 )
						PrintStack( ev.dwThreadId );
					break;
				}
				if ( !bSecond && ++nAV > 3 )
					break;
				printf( "== %s exception 0x%08lX at 0x%p (thread %lu)", bSecond ? "UNHANDLED" : "first-chance", r.ExceptionCode, r.ExceptionAddress, ev.dwThreadId );
				if ( r.ExceptionCode == EXCEPTION_ACCESS_VIOLATION && r.NumberParameters >= 2 )
					printf( " %s address 0x%p", r.ExceptionInformation[0] ? "writing" : "reading", (void*)r.ExceptionInformation[1] );
				printf( "\n" );
				PrintStack( ev.dwThreadId );
				if ( bSecond )
				{
					TerminateProcess( hProc, r.ExceptionCode );
					printf( "== terminated after unhandled exception\n" );
					return 1;
				}
				break;
			}
			case EXIT_PROCESS_DEBUG_EVENT:
				printf( "== process exited with code 0x%08lX after %.1f s\n", ev.u.ExitProcess.dwExitCode, ( GetTickCount() - dwStart ) / 1000.0 );
				return 0;
		}
		fflush( stdout );
		ContinueDebugEvent( ev.dwProcessId, ev.dwThreadId, dwContinue );
	}
}
