#define _CRT_SECURE_NO_WARNINGS
#include "MemoryManager.h"
#include <vector>
#include <cstring>

// Global MemoryManager instance
std::unique_ptr<MemoryManager> MemMgr = std::make_unique<MemoryManager>();

int32_t MemoryManager::getProcessId(const std::string& processName) {
	uint32_t processId = 0;
	HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, NULL);

	if (snapshot == INVALID_HANDLE_VALUE) {
		return processId;
	}

	PROCESSENTRY32 processEntry{};
	processEntry.dwSize = sizeof(PROCESSENTRY32);

	if (Process32First(snapshot, &processEntry)) {
		do {
			// Convert wide string to string for comparison
			size_t size = wcslen(processEntry.szExeFile) + 1;
			char exeFile[260];
			wcstombs(exeFile, processEntry.szExeFile, size);
			
			if (!_stricmp(processName.c_str(), exeFile)) {
				processId = processEntry.th32ProcessID;
				break;
			}
		} while (Process32Next(snapshot, &processEntry));
	}

	CloseHandle(snapshot);
	return processId;
}

uintptr_t MemoryManager::getModuleAddress(const std::string& moduleName) {
	uintptr_t moduleAddress = 0;

	if (!processHandle) {
		return moduleAddress;
	}

	DWORD processId = GetProcessId(processHandle);
	HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE | TH32CS_SNAPMODULE32, processId);

	if (snapshot == INVALID_HANDLE_VALUE) {
		return moduleAddress;
	}

	MODULEENTRY32 moduleEntry{};
	moduleEntry.dwSize = sizeof(MODULEENTRY32);

	if (Module32First(snapshot, &moduleEntry)) {
		do {
			// Convert wide string to string for comparison
			size_t size = wcslen(moduleEntry.szModule) + 1;
			char modName[260];
			wcstombs(modName, moduleEntry.szModule, size);
			
			if (!_stricmp(moduleName.c_str(), modName)) {
				moduleAddress = reinterpret_cast<uintptr_t>(moduleEntry.modBaseAddr);
				break;
			}
		} while (Module32Next(snapshot, &moduleEntry));
	}

	CloseHandle(snapshot);
	return moduleAddress;
}

bool MemoryManager::attachToProcess(const std::string& processName)
{
	auto pid = getProcessId(processName);
	HANDLE process = OpenProcess(PROCESS_ALL_ACCESS, false, pid);

	if (process == INVALID_HANDLE_VALUE || !process) {
		return false;
	}

	processHandle = process;
	processId = pid;

	baseAddress = getModuleAddress(processName);

	return true;
}


void MemoryManager::readRaw(uintptr_t address, void* buffer, uintptr_t size) {
	Luck_ReadVirtualMemory(processHandle, reinterpret_cast<void*>(address), &buffer, size, nullptr);
}

std::string MemoryManager::readString(uintptr_t address) {
	std::string result;
	char character;
	int offset = 0;

	int32_t StrLength = read<int32_t>(address + 0x18);

	if (StrLength >= 16) {
		address = read<uintptr_t>(address);
	}

	while ((character = read<char>(address + offset)) != 0)
	{
		result.push_back(character);
		offset += sizeof(character);
	}

	return result;
}

int32_t MemoryManager::getProcessId() {
	return processId;
}

void MemoryManager::setProcessId(int32_t newProcessId) {
	processId = newProcessId;
}

uintptr_t MemoryManager::getBaseAddress() {
	return baseAddress;
}

void MemoryManager::setBaseAddress(uintptr_t newBaseAddress) {
	baseAddress = newBaseAddress;
}
