#include "memory_manager.hpp"

typedef NTSTATUS(WINAPI* pNtReadVirtualMemory)(HANDLE ProcessHandle, PVOID BaseAddress, PVOID Buffer, ULONG NumberOfBytesToRead, PULONG NumberOfBytesRead);
typedef NTSTATUS(WINAPI* pNtWriteVirtualMemory)(HANDLE Processhandle, PVOID BaseAddress, PVOID Buffer, ULONG NumberOfBytesToWrite, PULONG NumberOfBytesWritten);

pNtReadVirtualMemory* NtReadVirtualMemory;
pNtWriteVirtualMemory* NtWriteVirtualMemory;

bool c_memory_manager::initialize() {
	this->process_name = "FiveM.exe";
	this->module_name = "FiveM.exe";
	this->base_module = this->get_base(this->module_name);
	this->base_size = this->get_base_size(this->module_name);
	this->process_id = this->get_process_id(this->process_name);
	this->handle = OpenProcess(PROCESS_ALL_ACCESS, TRUE, (DWORD)this->process_id);

	return true;
}

uintptr_t c_memory_manager::get_base(std::string process_name) {
	uintptr_t addr = NULL;

	if (!this->process_id)
		return NULL;

	HANDLE hsnap = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE | TH32CS_SNAPMODULE32, (DWORD)process_id);
	if (hsnap == INVALID_HANDLE_VALUE)
		return NULL;

	MODULEENTRY32 me = { 0 };

	me.dwSize = sizeof(MODULEENTRY32);
	if (Module32First(hsnap, &me)) {
		this->base_module = (uintptr_t)me.modBaseAddr;
	}
	CloseHandle(hsnap);

	return this->base_module;
}

uintptr_t c_memory_manager::get_base_size(std::string process_name) {
	uintptr_t addr = NULL;

	if (!this->process_id)
		return NULL;

	HANDLE hsnap = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE | TH32CS_SNAPMODULE32, (DWORD)process_id);
	if (hsnap == INVALID_HANDLE_VALUE)
		return NULL;

	MODULEENTRY32 me = { 0 };

	me.dwSize = sizeof(MODULEENTRY32);
	if (Module32First(hsnap, &me)) {
		this->base_size = (uintptr_t)me.modBaseSize;
	}
	CloseHandle(hsnap);

	return this->base_size;
}

uintptr_t c_memory_manager::get_process_id(std::string process_name) {
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
X c_memory_manager::write(uintptr_t address, X* value) {
	X buffer{};

	NtWriteVirtualMemory(this->handle, (PVOID)address, value, sizeof(X), nullptr);
	return buffer;
}

bool c_memory_manager::find_pattern(const char* pattern, const char* mask, uintptr_t& out_address) {
    uintptr_t start_address = this->base_module;
	uintptr_t end_address = this->base_module + this->base_size;
	size_t pattern_length = strlen(mask);

	for (uintptr_t current_address = start_address; current_address < end_address - pattern_length; ++current_address) {
		bool found = true;
		for (size_t i = 0; i < pattern_length; ++i) {
			if (mask[i] != '?' && pattern[i] != *(char*)(current_address + i)) {
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