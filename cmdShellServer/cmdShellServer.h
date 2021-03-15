/*  PROTOTYPES */
DWORD WINAPI InstanceThread( LPVOID ); // the __stdcall convention
VOID GetAnswerToRequest( HANDLE, HANDLE );
VOID RequestToChildProc( HANDLE, HANDLE, HANDLE,  BOOL* );