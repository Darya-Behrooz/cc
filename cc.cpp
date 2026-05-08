#include <cerrno>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <string>
#include <Windows.h>


[[noreturn]] void errHandle(int errNo , const char* errLocation)
{
	LPWSTR errMsg;
	FormatMessage(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS | FORMAT_MESSAGE_MAX_WIDTH_MASK , NULL , errNo , NULL , reinterpret_cast<LPWSTR>(&errMsg) , NULL , NULL);

	#ifdef _DEBUG
		std::wcerr << "=== ERROR --- {" << errLocation << "} FAILED --- " << errNo << ": " << errMsg << "===" << std::endl;
		std::cerr << "=== PROGRAM TERMINATED {" << errNo << "} ===" << std::endl;
	#else
		std::wcerr << "[" << errNo << "] " << errMsg << std::endl;
	#endif

	LocalFree(errMsg);
	exit(errNo);
}


class clipboardRAII
{
	public:
		clipboardRAII()
		{
			if (!OpenClipboard(NULL))
			{
				errHandle(GetLastError() , "OpenClipboard(NULL)");
			}
		}

		~clipboardRAII()
		{
			if (!CloseClipboard())
			{
				errHandle(GetLastError() , "CloseClipboard()");
			}
		}
};


void help()
{
	std::cout <<
		"cc: writes output of the previous pipe to the shell, and also copies it to the clipboard\n\n"

		"Usage:\n"
		"<command> | cc\n"
		"The output of <command> will be copied to the clipboard and outputted\n"
		"No arguments are taken\n\n"

		"Flags:\n" <<
		"Show help: -?, /?, -h, /h, --help, /help\n";

	return;
}


int main(int argC , char* argV[])
{
	if (argC > 1)
	{
		if (!_stricmp(argV[1] , "-?") || !_stricmp(argV[1] , "/?") || !_stricmp(argV[1] , "-h") || !_stricmp(argV[1] , "/h") || !_stricmp(argV[1] , "--help") || !_stricmp(argV[1] , "/help"))
		{
			help();
			return 0;
		}

		std::cerr << "cc: no arguments taken" << std::endl;
		errHandle(EINVAL, "argV[1]");
	}


	const HANDLE stdinHandle = GetStdHandle(STD_INPUT_HANDLE);
	if (stdinHandle == INVALID_HANDLE_VALUE || !stdinHandle)
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
			break; //- write handle closed, treated as EOF
		}

		if (stdinBytes == 0)
		{
			break;
		}

		data.append(stdinBuf , stdinBytes);
	}


	const HANDLE stdoutHandle = GetStdHandle(STD_OUTPUT_HANDLE);
	if (stdoutHandle == INVALID_HANDLE_VALUE || !stdoutHandle)
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


	UINT stdinCodepage = GetConsoleOutputCP();
	if (!stdinCodepage)
	{
		errHandle(GetLastError() , "GetConsoleOutputCP()");
	}

	int convBuf = MultiByteToWideChar(stdinCodepage , 0 , data.data() , data.size() , nullptr , 0);
	if (!convBuf)
	{
		errHandle(GetLastError() , "MultiByteToWideChar(stdinCodepage , 0 , data.data() , data.size() , nullptr , 0)");
	}
	std::wstring dataUTF16;
	dataUTF16.resize(convBuf);
	if (!MultiByteToWideChar(stdinCodepage , 0 , data.data() , data.size() , dataUTF16.data() , convBuf))
	{
		errHandle(GetLastError() , "MultiByteToWideChar(stdinCodepage , 0 , data.data() , data.size() , dataUTF16.data() , convBuf)");
	}


	clipboardRAII clipboard;

	if (!EmptyClipboard())
	{
		errHandle(GetLastError() , "EmptyClipboard()");
	}

	HGLOBAL handleMem = GlobalAlloc(GHND , (dataUTF16.size() + 1) * sizeof(wchar_t));
	if (!handleMem)
	{
		errHandle(GetLastError() , "GlobalAlloc(GHND , (dataUTF16.size() + 1) * sizeof(wchar_t))");
	}

	LPVOID ptrMem = GlobalLock(handleMem);
	if (!ptrMem)
	{
		errHandle(GetLastError() , "GlobalLock(handleMem)");
	}

	memcpy(ptrMem , dataUTF16.data() , dataUTF16.size() * sizeof(wchar_t));
	reinterpret_cast<LPWSTR>(ptrMem)[dataUTF16.size()] = L'\0';

	while (true)
	{
		if (!GlobalUnlock(handleMem)) //! failure
		{
			if (GetLastError() != NO_ERROR)
			{
				errHandle(GetLastError() , "GlobalUnlock(handleMem)");
			}

			break; //- success
		}
		[[unlikely]]; //? still locked, pending success
	}

	if (!SetClipboardData(CF_UNICODETEXT , handleMem))
	{
		GlobalFree(handleMem);
		errHandle(GetLastError() , "SetClipboardData(CF_UNICODETEXT , handleMem)");
	}


	return 0;
}
