#include <cwchar>

#define WIN32_LEAN_AND_MEAN
#define _WIN32_DCOM

#define sprintf_s sprintf

#include <windows.h>
#include <comutil.h>
#include <stringapiset.h>
#include <tlhelp32.h>
#include <wbemidl.h>

void
win32_setenv_shell_impl(void);

void
win32_setenv_shell_impl(void)
{
	unsigned pid = GetCurrentProcessId();
	int ppid = -1;
	HANDLE h = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
	PROCESSENTRY32 pe = { 0 };
	IWbemLocator* wbem_locator       = nullptr;
	IWbemServices* wbem_services     = nullptr;
	IEnumWbemClassObject* enum_wbem  = nullptr;

	pe.dwSize = sizeof(PROCESSENTRY32);

	if (Process32First(h, &pe)) {
		do {
			if (pe.th32ProcessID == pid)
				ppid = pe.th32ParentProcessID;
		} while (Process32Next(h, &pe));
	}

	CloseHandle(h);

	CoInitializeEx(0, COINIT_MULTITHREADED);
	CoInitializeSecurity(nullptr, -1, nullptr, nullptr, RPC_C_AUTHN_LEVEL_DEFAULT, RPC_C_IMP_LEVEL_IMPERSONATE, nullptr, EOAC_NONE, nullptr);
	CoCreateInstance(CLSID_WbemLocator, 0, CLSCTX_INPROC_SERVER, IID_IWbemLocator, (LPVOID*)&wbem_locator);

	wbem_locator->ConnectServer(L"ROOT\\CIMV2", nullptr, nullptr, nullptr, 0, nullptr, nullptr, &wbem_services);
	wchar_t* query = new wchar_t[4096];
	swprintf(query, 4096, L"select commandline from win32_process where processid = %d", ppid);
	wbem_services->ExecQuery(L"WQL", query, WBEM_FLAG_FORWARD_ONLY, nullptr, &enum_wbem);
	delete[] query;

	if (enum_wbem) {
		IWbemClassObject *result = nullptr;
		ULONG returned_count = 0;

		if(enum_wbem->Next(WBEM_INFINITE, 1, &result, &returned_count) == S_OK) {
			VARIANT process_id;
			VARIANT command_line;

			result->Get(L"CommandLine", 0, &command_line, 0, 0);

			wchar_t* command_line_utf16 = command_line.bstrVal;
			size_t size = WideCharToMultiByte(CP_UTF8, 0, command_line_utf16, -1, nullptr, 0, nullptr, nullptr) + 1;
			char* command_line_utf8 = new char[size];

			WideCharToMultiByte(CP_UTF8, 0, command_line_utf16, -1, command_line_utf8, size, nullptr, nullptr);

			SysFreeString(command_line_utf16);

			setenv("SHELL", command_line_utf8, 1);

			delete[] command_line_utf8;

			result->Release();
		}
	}
}

extern "C" {

void
win32_setenv_shell(void);

void
win32_setenv_shell(void)
{
	win32_setenv_shell_impl();
}

}
