#include "../interface.h"
#include "cmdShellServer.h"

/*
	On computers running Microsoft Windows XP or later, the maximum length of the string
	that you can use at the command prompt is 8191 characters. On computers running
	Microsoft Windows 2000 or Windows NT 4.0, the maximum length of the string that you
	can use at the command prompt is 2047 characters.
	
	https://support.microsoft.com/en-us/help/830473/command-prompt-cmd-exe-command-line-string-limitation
*/

/*
	Windows Server 2003 and Windows XP:  Pipe write operations across a network are limited
	in size per write. The amount varies per platform. For x86 platforms it's 63.97 MB. For x64
	platforms it's 31.97 MB. For Itanium it's 63.95 MB.
	
	https://msdn.microsoft.com/en-us/library/windows/desktop/aa365747(v=vs.85).aspx
*/

const	LPCWSTR  	lpApplicationProcessName 	= TEXT( "C:\\WINDOWS\\SYSTEM32\\cmd.exe" );
const	LPCTSTR		lpCurrentDirectory			= TEXT( "C:\\WINDOWS\\SYSTEM32\\wbem\\" );

void __cdecl wmain()
{
	BOOL	fConnected 	= FALSE;
	DWORD	dwThreadId 	= 0;
	HANDLE	hNamedPipe = INVALID_HANDLE_VALUE, hThread = NULL;
	
	SetConsoleTitle( L"cmd.exe server" );
	system( "cls" );
	wprintf( L"Server side for handling client connections to cmd.exe via named pipe.\r\n" );
	wprintf( L"Kremenchuk Mykhailo Ostrohradskyi National University\r\n" );
	wprintf( L"Computer and Information System Department\r\n" );
	wprintf( L"Copyright (c) 2018 ANTON GLADYR.\r\n\r\n" );
	
	// The main loop creates an instance of the named pipe and 
	// then waits for a client to connect to it. When the client 
	// connects, a thread is created to handle communications 
	// with that client, and this loop is free to wait for the
	// next client connect request. It is an infinite loop.

	for ( ;; ) {
		wprintf( L"\r\nPipe Server: Main thread awaiting client connection on %s\r\n", lpszPipename );
		hNamedPipe = CreateNamedPipe( 
		          lpszPipename,             			// pipe name 
		          PIPE_ACCESS_DUPLEX,       		// read/write access 
		          PIPE_TYPE_MESSAGE |       		// message type pipe 
		          PIPE_READMODE_MESSAGE |   	// message-read mode 
		          PIPE_WAIT,                			// blocking mode 
		          PIPE_UNLIMITED_INSTANCES, 	// max. instances  
		          PIPE_BUFFER_BYTES,                  	// output buffer size in bytes
		          PIPE_BUFFER_BYTES,                  	// input buffer size in bytes
		          0,                        				// client time-out 
		          NULL						// default security attribute 
		);
		
		if ( hNamedPipe == INVALID_HANDLE_VALUE ) {
			wprintf( L"CreateNamedPipe failed, GLE=%d.\r\n", GetLastError() ); 
			return;
		}
		
		// Wait for the client to connect; if it succeeds, 
		// the function returns a nonzero value. If the function
		// returns zero, GetLastError returns ERROR_PIPE_CONNECTED.
		
		fConnected = ConnectNamedPipe( hNamedPipe, NULL ) ? 
			TRUE : ( GetLastError() == ERROR_PIPE_CONNECTED );
		
		if ( fConnected ) { 
			wprintf( L"Client connected, creating a processing thread.\r\n" ); 
		
			// Create a thread for this client. 
			hThread = CreateThread( 
				NULL,              		// no security attribute 
				0,                 		// default stack size 
				InstanceThread,    	// thread proc
				( LPVOID ) hNamedPipe,   // thread parameter 
				0,                 		// not suspended 
				&dwThreadId		// returns thread ID 
			);
			
			if ( hThread == NULL ) {
				wprintf( L"CreateThread failed, GLE=%d.\r\n", GetLastError() ); 
				return;
			}
			else CloseHandle( hThread ); 
		} 
		else 
		// The client could not connect, so close the pipe. 
		CloseHandle( hNamedPipe );
	}
	
	return;
}


DWORD WINAPI InstanceThread( LPVOID lpvParam ) {
	// This routine is a thread processing function to read from and reply to a client
	// via the open pipe connection passed from the main loop. Note this allows
	// the main loop to continue executing, potentially creating more threads of
	// of this procedure to run concurrently, depending on the number of incoming
	// client connections.
	
	HANDLE 	hNamedPipe  			= NULL;
	HANDLE 	g_hChildStd_IN_Rd  	= NULL;
	HANDLE 	g_hChildStd_IN_Wr  	= NULL;
	HANDLE 	g_hChildStd_OUT_Rd  	= NULL;
	HANDLE 	g_hChildStd_OUT_Wr  	= NULL;
	BOOL	fSuccess 	= FALSE, isExitCommand = FALSE;
	
	
	SECURITY_ATTRIBUTES  saAttr;
	STARTUPINFO StartInfo;
	PROCESS_INFORMATION ProcessInfo;
	
	// Do some extra error checking since the app will keep running even if this
   	// thread fails.
   	
	if ( lpvParam == NULL ) {
		wprintf( L"\r\nERROR - Pipe Server Failure:\r\n" );
		wprintf( L"   InstanceThread got an unexpected NULL value in lpvParam.\r\n" );
		wprintf( L"   InstanceThread exitting.\r\n" );
		return ( DWORD ) -1;
	}

	// Print verbose messages. In production code, this should be for debugging only.
	wprintf( L"\r\nInstanceThread   %d    created, receiving and processing messages.\r\n", GetCurrentThreadId() );
	
	// The thread's parameter is a handle to a pipe object instance. 
	hNamedPipe = ( HANDLE ) lpvParam;
	
	// Create new process for cmd.exe
	GetStartupInfo( &StartInfo );
	StartInfo.wShowWindow = SW_HIDE;
	StartInfo.dwFlags =  STARTF_USESHOWWINDOW + STARTF_USESTDHANDLES;
	
	saAttr.nLength = sizeof( saAttr );
	saAttr.lpSecurityDescriptor =  NULL;
	saAttr.bInheritHandle = TRUE;
	CreatePipe( &g_hChildStd_IN_Rd, &g_hChildStd_IN_Wr, &saAttr, 0 );
	CreatePipe( &g_hChildStd_OUT_Rd, &g_hChildStd_OUT_Wr, &saAttr, 0 );
	StartInfo.hStdOutput = g_hChildStd_OUT_Wr;
	StartInfo.hStdInput = g_hChildStd_IN_Rd;
	StartInfo.hStdError = g_hChildStd_OUT_Wr;
	
	fSuccess =  CreateProcess(
		lpApplicationProcessName,		// use command line
		NULL,						// Command line
		NULL,						// Process handle not inheritable
		NULL,						// Thread handle not inheritable
		TRUE,						// Set handle inheritance to TRUE
		CREATE_NEW_CONSOLE, 		// The new process has a new console, instead of inheriting its parent's console (the default)
		NULL,						// Use parent's environment block
		lpCurrentDirectory,			// Use parent's starting directory
		&StartInfo,					// Pointer to STARTUPINFO structure
		&ProcessInfo					// Pointer to PROCESS_INFORMATION structure
	);
	
	if ( !fSuccess ) {
		wprintf( L"CreateProcess Error. GLE=%d\r\n", GetLastError() );
		DisconnectNamedPipe( hNamedPipe ); 
		CloseHandle( hNamedPipe ); 
		CloseHandle( ProcessInfo.hProcess );
		CloseHandle( ProcessInfo.hThread );
	}
	
	// Loop until done reading
	while ( 1 ) { 
		GetAnswerToRequest( hNamedPipe, g_hChildStd_OUT_Rd );
		RequestToChildProc(	 hNamedPipe, 
							g_hChildStd_IN_Wr,
							 g_hChildStd_OUT_Rd,
							 &isExitCommand 
						);
		if ( isExitCommand ) break;
	}
	
	// Flush the pipe to allow the client to read the pipe's contents 
	// before disconnecting. Then disconnect the pipe, and close the 
	// handle to this pipe instance. 
	// Close all handels, connections. Release of resources.
	FlushFileBuffers( hNamedPipe );
	DisconnectNamedPipe( hNamedPipe ); 
	CloseHandle( hNamedPipe ); 
	CloseHandle( ProcessInfo.hProcess );
	CloseHandle( ProcessInfo.hThread );
	
	wprintf( L"\r\n\r\nInstanceThread   %d  exitting.\r\n", GetCurrentThreadId() );

	return 1;
}


VOID GetAnswerToRequest(	HANDLE hNamedPipe,
						HANDLE g_hChildStd_OUT_Rd ) {
	 // Read output from cmd.exe and write to NamedPipe
	BOOL	fSuccess 		= FALSE;
	HANDLE hHeap		= GetProcessHeap();
	wchar_t* pchReply	= ( wchar_t* )HeapAlloc( hHeap, 0, PIPE_BUFFER_BYTES );
	CHAR 	stdOutBuffer[PIPE_BUFFER_CHARS] = { 0 };
	DWORD 	cbBytesRead = 0, cbWritten = 0, cbCharsWritten = 0;
	
	if ( pchReply == NULL ) {
		wprintf( L"\r\nERROR - Pipe Server Failure:\r\n" );
		wprintf( L"   InstanceThread got an unexpected NULL heap allocation.\r\n" );
		wprintf( L"   InstanceThread exitting.\r\n" );
		return;
	}
	
	for ( ;; ) {	
		fSuccess = ReadFile(
			g_hChildStd_OUT_Rd,
			stdOutBuffer,
			PIPE_BUFFER_CHARS,		// number of bytes to read         sizeof( stdOutBuffer )
			&cbBytesRead,			// number of bytes read
			NULL
		);
		
		if ( !fSuccess || cbBytesRead == 0 ) {
			wprintf( L"ReadFile failed. GLE=%d\r\n", GetLastError() );
			break;
		}
		
		cbCharsWritten = MultiByteToWideChar( // returns the number of characters written to pchReply
			CP_UTF8,
			0,
			stdOutBuffer,
			-1, //the function processes the entire input string , including terminating null character
			pchReply,
			PIPE_BUFFER_CHARS	// Size in characters of the buffer ( pchReply )
		);
		
		if ( cbCharsWritten == 0 ) {
			wprintf( L"MultiByteToWideChar failed. GLE=%d\r\n", GetLastError() );
			break;
		}
		
		fSuccess = WriteFile(
			hNamedPipe,
			pchReply,
			cbCharsWritten * sizeof(wchar_t),	// number of bytes to write
			&cbWritten,
			NULL
		);
		if ( !fSuccess ) {
			wprintf( L"WriteFile hNamedPipe failed. GLE=%d\r\n", GetLastError() );
			break;
		}
		
		// wprintf( L"%s", pchReply ); 	//Debug printf 
		
		// Dirty hack !!!
		// Checking for '>' symbol and null-terminating symbol
		
		if ( stdOutBuffer[cbBytesRead - 1] == '>' &&
			stdOutBuffer[cbBytesRead] == 0 ) {
			break;
		}
		
		memset( stdOutBuffer, 0, strlen( stdOutBuffer ) ); // clean buffer
		wmemset( pchReply, 0, ( lstrlenW( pchReply ) + 1 ) * sizeof( wchar_t ) );
	}
	
	HeapFree( hHeap, 0, pchReply );
}


VOID RequestToChildProc(	HANDLE hNamedPipe,
						HANDLE g_hChildStd_IN_Wr,
						HANDLE g_hChildStd_OUT_Rd,
						BOOL *isExitCommand  ) {
	// Read client requests from the pipe. This simplistic code only allows messages
	// up to BUFSIZE characters in length.
	BOOL	fSuccess 		= FALSE;
	HANDLE	 hStdOut 		= GetStdHandle( STD_OUTPUT_HANDLE );
	HANDLE	 hHeap		= GetProcessHeap();
	wchar_t* pchRequest	= ( wchar_t* )HeapAlloc( hHeap, 0, PIPE_BUFFER_BYTES );
	CHAR 	stdOutBuffer[PIPE_BUFFER_CHARS] = { 0 };
	DWORD 	cbBytesRead = 0, cbWritten = 0, cbBytesWritten = 0;
	
	if ( pchRequest == NULL ) {
		wprintf( L"\r\nERROR - Pipe Server Failure:\r\n" );
		wprintf( L"   InstanceThread got an unexpected NULL heap allocation.\r\n" );
		wprintf( L"   InstanceThread exitting.\r\n" );
		return;
	}
	
	fSuccess = ReadFile( 
		hNamedPipe,        			// handle to pipe 
		pchRequest,    			// buffer to receive data 
		PIPE_BUFFER_BYTES,  		// number of bytes to read
		&cbBytesRead, 			// number of bytes read 
		NULL        				// not overlapped I/O
	);
	
	
	if ( !fSuccess || cbBytesRead == 0 ) {   
		if ( GetLastError() == ERROR_BROKEN_PIPE ) {
			wprintf( L"InstanceThread: client disconnected. GLE=%d.\r\n", GetLastError() ); 
		}
		else
		{
			wprintf( L"InstanceThread ReadFile failed, GLE=%d.\r\n", GetLastError() ); 
		}
		HeapFree( hHeap, 0, pchRequest );
		return;
	}
	
	cbBytesWritten = WideCharToMultiByte(
		CP_UTF8,
		0,
		pchRequest,
		cbBytesRead / sizeof( wchar_t ),	// size in characters of the string
		stdOutBuffer,
		sizeof( stdOutBuffer ),	// size in bytes of the buffer
		NULL,
		NULL
	);
	if ( cbBytesWritten == 0 ) {
		wprintf( L"WideCharToMultiByte failed. GLE=%d\r\n", GetLastError() );
	}

	fSuccess = WriteFile(
		g_hChildStd_IN_Wr,
		stdOutBuffer,
		cbBytesWritten,   // number of bytes to write
		&cbWritten,
		NULL
	);
	
	if ( strcmp( stdOutBuffer, "exit\n" ) == 0 ) {
		*isExitCommand = TRUE;
		HeapFree( hHeap, 0, pchRequest );
		return;
	}
	
	// Dirty hack for cleaning duplicated command
	fSuccess = ReadFile(
					g_hChildStd_OUT_Rd,
					stdOutBuffer,
					cbBytesWritten,			// number of bytes to read
					&cbBytesRead,			// number of bytes read
					NULL
	);
	
	HeapFree( hHeap, 0, pchRequest );
}