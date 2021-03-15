#include "../interface.h"
#include "cmdShellClient.h"

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

#define	COMMAND_STR_LEN			2048
#define	PIPE_CONNECTION_TIMEOUT	20000

void __cdecl wmain()
{
	HANDLE	hPipe		= NULL;
	HANDLE hStdOut 		= GetStdHandle( STD_OUTPUT_HANDLE );
	HANDLE hHeap		= GetProcessHeap();
	wchar_t* wInputBuf	= ( wchar_t* )HeapAlloc( hHeap, 0, COMMAND_STR_LEN * sizeof( wchar_t ) );
	BOOL   	fSuccess 		= FALSE; 
	DWORD	dwMode = 0, dwCharsToRead = 0, dwCharsRead = 0;
	
	SetConsoleTitle( L"cmd.exe client" );
	system( "cls" );
	wprintf( L"Client for remote connection to cmd.exe at server via named pipe.\r\n" );
	wprintf( L"Kremenchuk Mykhailo Ostrohradskyi National University\r\n" );
	wprintf( L"Computer and Information System Department\r\n" );
	wprintf( L"Copyright (c) 2018 ANTON GLADYR.\r\n\r\n" );
	
	// Try to open a named pipe; wait for it, if necessary.
	while ( 1 ) { 
		hPipe = CreateFile( 
			lpszPipename,   	// pipe name 
			GENERIC_READ |  	// read and write access 
			GENERIC_WRITE, 
			0,              			// no sharing 
			NULL,           		// default security attributes
			OPEN_EXISTING,  	// opens existing pipe 
			0,              			// default attributes 
			NULL         		// no template file 
		); 
		
		// Break if the pipe handle is valid. 
		
		if ( hPipe != INVALID_HANDLE_VALUE ) 
		break; 
		
		// Exit if an error other than ERROR_PIPE_BUSY occurs. 
		
		if ( GetLastError() != ERROR_PIPE_BUSY ) {
			wprintf( L"Could not open pipe. GLE=%d\r\n", GetLastError() ); 
			wprintf( L"\r\nPress <ENTER> to continue..." );
    			while( getwchar() != '\r\n' );
			return;
		}
		
		// All pipe instances are busy, so wait for 20 seconds. 
		
		if ( ! WaitNamedPipe( lpszPipename, PIPE_CONNECTION_TIMEOUT ) ) { 
			wprintf( L"Could not open pipe: 20 second wait timed out." ); 
			wprintf( L"\r\nPress <ENTER> to continue..." );
    			while( getwchar() != '\r\n' );
			return;
		} 
	}
	
	wprintf( L"SUCCESSFULLY CONNECTED!\r\n\r\n" );
	
	// The pipe connected; change to message-read mode.
	dwMode = PIPE_READMODE_MESSAGE; 
	fSuccess = SetNamedPipeHandleState( 
		hPipe,		// pipe handle 
		&dwMode,	// new pipe mode 
		NULL,		// don't set maximum bytes 
		NULL		// don't set maximum time 
	);

	if ( ! fSuccess) {
		wprintf( L"SetNamedPipeHandleState failed. GLE=%d\r\n", GetLastError() ); 
		wprintf( L"\r\nPress <ENTER> to continue..." );
		while( getwchar() != '\r\n' );
		return;
	}
	
	
	while( 1 ) {
		wmemset( wInputBuf, 0, lstrlenW( wInputBuf ) + 1 );
		ReadFromNamedPipe( hPipe );
		fgetws( wInputBuf, COMMAND_STR_LEN, stdin );
		WriteToNamedPipe( hPipe, wInputBuf );
		if ( wcscmp( wInputBuf, TEXT( "exit\n" )) == 0 ) break;
	}
	
	wprintf( L"\r\n\r\nConnection to server successfully has been closed!\r\n\r\n");
	HeapFree( hHeap, 0, wInputBuf );
	CloseHandle( hPipe );
	
	return;
}

VOID ReadFromNamedPipe( HANDLE hNamedPipe ) {
	// Read from the pipe. 
	
	HANDLE hStdOut 		= GetStdHandle( STD_OUTPUT_HANDLE );
	HANDLE hHeap		= GetProcessHeap();
	wchar_t* wchRespnStr	= ( wchar_t* )HeapAlloc( hHeap, 0, PIPE_BUFFER_BYTES );
	BOOL	fSuccess = FALSE;
	DWORD	cbRead = 0;
	
	for ( ;; ) {
		fSuccess = ReadFile(
			hNamedPipe,    				// pipe handle
			wchRespnStr,    				// buffer to receive reply 
			PIPE_BUFFER_BYTES, 			// number of byets to read    PIPE_BUFFER_BYTES
			&cbRead,  					// number of bytes read 
			NULL    						// not overlapped 
		);
			
		wprintf( L"%s", wchRespnStr );
		
		// Dirty hack !!!
		// Checking for '>' symbol and null-terminating symbol
		
		if ( *( wchRespnStr + lstrlenW( wchRespnStr )  - 1 ) == L'>'  &&
			*( wchRespnStr + lstrlenW( wchRespnStr ) ) == 0 ) {
			break;
		}
		
		wmemset( wchRespnStr, 0, lstrlenW( wchRespnStr ) );
	}
	
	HeapFree( hHeap, 0, wchRespnStr );
}

VOID WriteToNamedPipe( HANDLE hNamedPipe, wchar_t *wchCmdStr ) {
	// Send a message to the pipe server. 
	
	BOOL   	fSuccess = FALSE; 
	DWORD	cbToWrite = 0, cbWritten = 0;
	
	cbToWrite = lstrlenW( wchCmdStr )  * sizeof( wchar_t );
	
	fSuccess = WriteFile( 
		hNamedPipe,		// pipe handle 
		wchCmdStr,		// message 
		cbToWrite,		// message length 
		&cbWritten,		// bytes written 
		NULL			// not overlapped 
	);
	if ( ! fSuccess) {
		wprintf( L"WriteFile to pipe failed. GLE=%d\r\n", GetLastError() ); 
		wprintf( L"\r\nPress <ENTER> to continue..." );
		while( getwchar() != '\r\n' );
		return;
	}
}