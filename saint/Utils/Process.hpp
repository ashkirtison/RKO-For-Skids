#pragma once
#include <vector>
#include <Windows.h>
#include <TlHelp32.h>

namespace Process {
	std::vector<DWORD> GetProcessID() {
		std::vector<DWORD> processIDs;
		HANDLE snapShot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, NULL);
		if (snapShot == INVALID_HANDLE_VALUE) {
			return processIDs;
		}
		PROCESSENTRY32W entry = { sizeof(PROCESSENTRY32W) };
		if (Process32FirstW(snapShot, &entry)) {
			if (_wcsicmp(L"RobloxPlayerBeta.exe", entry.szExeFile) == 0) {
				processIDs.push_back(entry.th32ProcessID);
			}
			while (Process32NextW(snapShot, &entry)) {
				if (_wcsicmp(L"RobloxPlayerBeta.exe", entry.szExeFile) == 0) {
					processIDs.push_back(entry.th32ProcessID);
				}
			}
		}
		CloseHandle(snapShot);
		return processIDs;
	}

	uintptr_t GetModuleBase(DWORD pid) {
		MODULEENTRY32W entry = { sizeof(entry) };
		HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE, pid);
		if (snapshot == INVALID_HANDLE_VALUE) return 0;
		if (Module32FirstW(snapshot, &entry)) {
			do {
				if (_wcsicmp(entry.szModule, L"RobloxPlayerBeta.exe") == 0) {
					CloseHandle(snapshot);
					return reinterpret_cast<uintptr_t>(entry.modBaseAddr);
				}
			} while (Module32NextW(snapshot, &entry));
		}
		CloseHandle(snapshot);
		return 0;
	}

	HWND GetWindowsProcess(DWORD ProcessID) {
		struct EnumData {
			DWORD targetPID;
			HWND hwnd;
		} data{ ProcessID, nullptr };

		EnumWindows([](HWND hwnd, LPARAM lParam) -> BOOL {
			EnumData* pData = reinterpret_cast<EnumData*>(lParam);
			DWORD pid = 0;
			GetWindowThreadProcessId(hwnd, &pid);
			if (pid == pData->targetPID) {
				pData->hwnd = hwnd;
				return FALSE;
			}
			return TRUE;
			}, reinterpret_cast<LPARAM>(&data));

		return data.hwnd;
	}
}