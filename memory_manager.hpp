#pragma once
#include <Windows.h>
#include <iostream>
#include <vector>
#include <string>
#include <TlHelp32.h>
#include <memory>

class c_memory_manager {
public:
	uintptr_t base_module;
	uintptr_t base_size;
	uintptr_t process_id;
	HANDLE handle;
	std::string process_name;
	std::string module_name;

	bool initialize();
	uintptr_t get_base(std::string process_name);
	uintptr_t get_base_size(std::string process_name);
	uintptr_t get_process_id(std::string process_name);

	template<typename X>
	X read(uintptr_t address);

	template<typename X>
	X write(uintptr_t address, X* value);

	bool find_pattern(const char* pattern, const char* mask, uintptr_t& out_address);
};
inline std::unique_ptr<c_memory_manager> g_memory = std::make_unique<c_memory_manager>();