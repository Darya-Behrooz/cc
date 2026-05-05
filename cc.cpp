#include <iostream>
#include <string>
#include <Windows.h>


[[noreturn]] void errHandle(int errNo , const char* errLocation)
{
	LPWSTR errMsg;

	FormatMessage(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS | FORMAT_MESSAGE_MAX_WIDTH_MASK , NULL , errNo , NULL , reinterpret_cast<LPWSTR>(&errMsg) , NULL , NULL);

	std::wcerr << "=== ERROR --- {" << errLocation << "} FAILED --- " << errNo << ": " << errMsg << "===" << std::endl;
	std::cerr << "=== PROGRAM TERMINATED {" << errNo << "} ===" << std::endl;

	LocalFree(errMsg);
	exit(errNo);
}


int main()
{
	const HANDLE stdinHandle = GetStdHandle(STD_INPUT_HANDLE);
	if (stdinHandle == INVALID_HANDLE_VALUE)
	{
		errHandle(GetLastError() , "GetStdHandle(STD_INPUT_HANDLE)");
	}

	const DWORD stdinType = GetFileType(stdinHandle);
	if (stdinType != FILE_TYPE_PIPE)
	{
		if (stdinType == FILE_TYPE_UNKNOWN)
		{
			if (GetLastError() != NO_ERROR)
			{
				errHandle(GetLastError() , "GetFileType(stdinHandle)");
			}
		}
		errHandle(ERROR_BAD_PIPE , "GetFileType(stdinHandle)");
	}

	std::string data;
	char stdinBuf[4096];
	DWORD stdinBytes;
	while (true)
	{
		if (!ReadFile(stdinHandle , stdinBuf , sizeof(stdinBuf) , &stdinBytes , NULL))
		{
			if (GetLastError() != ERROR_BROKEN_PIPE)
			{
				errHandle(GetLastError() , "ReadFile(stdinHandle , stdinBuf , sizeof(stdinBuf) , &stdinBytes , NULL)");
			}
			break;
		}

		if (stdinBytes == 0)
		{
			break;
		}

		data.append(stdinBuf , stdinBytes);
	}


	const HANDLE stdoutHandle = GetStdHandle(STD_OUTPUT_HANDLE);
	if (stdoutHandle == INVALID_HANDLE_VALUE)
	{
		errHandle(GetLastError() , "GetStdHandle(STD_OUTPUT_HANDLE)");
	}

	const DWORD stdoutType = GetFileType(stdoutHandle);
	if (stdoutType != FILE_TYPE_CHAR && stdoutType != FILE_TYPE_DISK && stdoutType != FILE_TYPE_PIPE)
	{
		if (stdoutType == FILE_TYPE_UNKNOWN)
		{
			if (GetLastError() != NO_ERROR)
			{
				errHandle(GetLastError() , "GetFileType(stdoutHandle)");
			}
		}
		errHandle(ERROR_INVALID_HANDLE , "GetFileType(stdoutHandle)");
	}

	DWORD stdoutBytes;
	if (!WriteFile(stdoutHandle , data.data() , data.size() , &stdoutBytes , NULL))
	{
		errHandle(GetLastError() , "WriteFile(stdoutHandle , data.data() , data.size() , &stdoutBytes , NULL)");
	}


	if (!OpenClipboard(NULL))
	{
		errHandle(GetLastError() , "OpenClipboard(NULL)");
	}

	if (!EmptyClipboard())
	{
		errHandle(GetLastError() , "EmptyClipboard()");
	}

	HGLOBAL handleMem = GlobalAlloc(GHND , data.size() + 1);
	if (!handleMem)
	{
		errHandle(GetLastError() , "GlobalAlloc(GHND , data.size() + 1)");
	}

	LPVOID ptrMem = GlobalLock(handleMem);
	if (!ptrMem)
	{
		errHandle(GetLastError() , "GlobalLock(handleMem)");
	}

	memcpy(ptrMem , data.data() , data.size() + 1);

	while (true)
	{
		if (!GlobalUnlock(handleMem))
		{
			if (GetLastError() != NO_ERROR)
			{
				errHandle(GetLastError() , "GlobalUnlock(handleMem)");
			}

			break;
		}
		[[unlikely]];
	}

	if (!SetClipboardData(CF_TEXT , handleMem))
	{
		GlobalFree(handleMem);
		errHandle(GetLastError() , "SetClipboardData(CF_TEXT , handleMem)");
	}

	if (!CloseClipboard())
	{
		errHandle(GetLastError() , "CloseClipboard()");
	}


	return 0;
}