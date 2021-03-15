#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <wchar.h>

#define	PIPE_BUFFER_CHARS			16384
#define	PIPE_BUFFER_BYTES			PIPE_BUFFER_CHARS * sizeof( wchar_t )

LPTSTR lpszPipename = TEXT("\\\\.\\pipe\\mynamedpipe");