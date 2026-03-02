#pragma once
#include <windows.h>
#include <TlHelp32.h>
#include <string>
#include <memory>

extern "C" int64_t
Luck_ReadVirtualMemory
(
	HANDLE ProcessHandle,
	PVOID BaseAddress,
	PVOID Buffer,
	ULONG NumberOfBytesToRead,
	PULONG NumberOfBytesRead
);

extern "C" int64_t
Luck_WriteVirtualMemory
(
	HANDLE Processhandle,
	PVOID BaseAddress,
	PVOID Buffer,
	ULONG NumberOfBytesToWrite,
	PULONG NumberOfBytesWritten
);

class MemoryManager final {
private:
	HANDLE processHandle;

	int32_t processId;
	uintptr_t baseAddress;
public:
	MemoryManager() = default;
	~MemoryManager() = default;

	int32_t getProcessId(const std::string& processName);
	uintptr_t getModuleAddress(const std::string& moduleName);

	bool attachToProcess(const std::string& processName);

	void readRaw(uintptr_t address, void* buffer, uintptr_t size);
	std::string readString(uintptr_t address);

	template <typename T>
	T read(uintptr_t address);

	template <typename T>
	void write(uintptr_t address, T value);

	int32_t getProcessId();
	void setProcessId(int32_t newProcessId);

	uintptr_t getBaseAddress();
	void setBaseAddress(uintptr_t newBaseAddress);
};

template <typename T>
T MemoryManager::read(uintptr_t address) {
	T buffer{};

	Luck_ReadVirtualMemory(processHandle, reinterpret_cast<void*>(address), &buffer, sizeof(T), nullptr);

	return buffer;
}

template <typename T>
void MemoryManager::write(uintptr_t address, T value) {
	Luck_WriteVirtualMemory(processHandle, reinterpret_cast<void*>(address), &value, sizeof(T), nullptr);
}
