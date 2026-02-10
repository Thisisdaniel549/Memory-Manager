#include "memory.hpp"
#include "../logging/logging.hpp"

#ifndef NT_SUCCESS
#define NT_SUCCESS(Status) (((NTSTATUS)(Status)) >= 0)
#endif

typedef NTSTATUS(WINAPI* pNtReadVirtualMemory)(HANDLE ProcessHandle, PVOID BaseAddress, PVOID Buffer, ULONG NumberOfBytesToRead, PULONG NumberOfBytesRead);
typedef NTSTATUS(WINAPI* pNtWriteVirtualMemory)(HANDLE Processhandle, PVOID BaseAddress, PVOID Buffer, ULONG NumberOfBytesToWrite, PULONG NumberOfBytesWritten);

pNtReadVirtualMemory NtReadVirtualMemory = nullptr;
pNtWriteVirtualMemory NtWriteVirtualMemory = nullptr;

bool c_memory_manager::initialize() {
	HMODULE ntdll = GetModuleHandleA("ntdll.dll");
	NtReadVirtualMemory = (pNtReadVirtualMemory)GetProcAddress(ntdll, "NtReadVirtualMemory");
	NtWriteVirtualMemory = (pNtWriteVirtualMemory)GetProcAddress(ntdll, "NtWriteVirtualMemory");
	
	if (!NtReadVirtualMemory || !NtWriteVirtualMemory) {
		logging::error("failed to get NtReadVirtualMemory or NtWriteVirtualMemory");
		return false;
	}

	this->process_name = "csgo.exe";
	this->module_name = "csgo.exe";
	this->base_module = this->get_base(this->module_name);
	this->base_size = this->get_base_size(this->module_name);
	if (!this->base_module || !this->base_size) {
		logging::error("failed to get base module or base size for %s", this->module_name.c_str());
		return false;
	}

	this->process_id = this->get_process_id(this->process_name);
	if (!this->process_id) {
		logging::error("failed to get process id for %s", this->process_name.c_str());
		return false;
	}

	this->handle = OpenProcess(PROCESS_ALL_ACCESS, TRUE, (DWORD)this->process_id);
	if (!this->handle) {
		logging::error("failed to open process with id %d", this->process_id);
	    return false;
	}

	return true;
}

uintptr_t c_memory_manager::get_base(const std::string& module_name) {
	HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE | TH32CS_SNAPMODULE32, this->process_id);

	if (snap == INVALID_HANDLE_VALUE)
		return 0;

	MODULEENTRY32 me{};
	me.dwSize = sizeof(me);

	if (Module32First(snap, &me)) {
		do {
			if (!_stricmp(me.szModule, module_name.c_str())) {
				CloseHandle(snap);
				return (uintptr_t)me.modBaseAddr;
			}
		} while (Module32Next(snap, &me));
	}

	CloseHandle(snap);
	return 0;
}

uintptr_t c_memory_manager::get_base_size(const std::string& module_name) {
	HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE | TH32CS_SNAPMODULE32, this->process_id);

	if (snap == INVALID_HANDLE_VALUE)
		return 0;

	MODULEENTRY32 me{};
	me.dwSize = sizeof(me);

	if (Module32First(snap, &me)) {
		do {
			if (!_stricmp(me.szModule, module_name.c_str())) {
				CloseHandle(snap);
				return (uintptr_t)me.modBaseSize;
			}
		} while (Module32Next(snap, &me));
	}

	CloseHandle(snap);
	return 0;
}

uintptr_t c_memory_manager::get_process_id(const std::string& process_name) {
	PROCESSENTRY32 pt;
	HANDLE hsnap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
	pt.dwSize = sizeof(PROCESSENTRY32);
	if (Process32First(hsnap, &pt)) {
		do {
			if (!lstrcmpi(pt.szExeFile, process_name.c_str())) {
				CloseHandle(hsnap);
				process_id = pt.th32ProcessID;
				return pt.th32ProcessID;
			}
		} while (Process32Next(hsnap, &pt));
	}
	CloseHandle(hsnap);
	return NULL;
}

template<typename X>
X c_memory_manager::read(uintptr_t address) {
	X buffer{};

	NtReadVirtualMemory(this->handle, (PVOID)address, &buffer, sizeof(X), nullptr);
	return buffer;
}

template<typename X>
void c_memory_manager::write(uintptr_t address, const X& value) {
	NtWriteVirtualMemory(this->handle, (PVOID)address, (PVOID)&value, sizeof(X), nullptr);
}

bool c_memory_manager::find_pattern(const char* pattern, const char* mask, uintptr_t& out_address) {
	uintptr_t start_address = this->base_module;
	uintptr_t end_address = this->base_module + this->base_size;
	size_t pattern_length = strlen(mask);

	for (uintptr_t current_address = start_address; current_address <= end_address - pattern_length; ++current_address)
	{
		bool found = true;

		for (size_t i = 0; i < pattern_length; ++i) {
			if (mask[i] == '?')
				continue;

			char byte = 0;
			NTSTATUS status = NtReadVirtualMemory(
				this->handle,
				(PVOID)(current_address + i),
				&byte,
				sizeof(byte),
				nullptr
			);

			if (!NT_SUCCESS(status) || byte != pattern[i]) {
				found = false;
				break;
			}
		}

		if (found) {
			out_address = current_address;
			return true;
		}
	}

	return false;
}
