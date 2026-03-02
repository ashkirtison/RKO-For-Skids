#pragma once
#include <Windows.h>
#include <functional>
#include "Bytecode.hpp"
#include "Memory.hpp"
#include "../Update/offsets.hpp"
#include <Wininet.h>
#include "cppcodec/base64_rfc4648.hpp"
#pragma comment(lib, "Wininet.lib")

class Instance {
private:
	uintptr_t address;
	DWORD ProcessID;
public:
	Instance(uintptr_t addr, DWORD PID) :
		address(addr),
		ProcessID(PID)
	{
	}

    uintptr_t GetAddress() {
        return address;
    }

    std::string Name() {
        std::uintptr_t nameaddr = ReadMemory<uintptr_t>(address + offsets::Name, ProcessID);
        const auto size = ReadMemory<size_t>(nameaddr + 0x10, ProcessID);
        if (size >= 16)
            nameaddr = ReadMemory<uintptr_t>(nameaddr, ProcessID);
        std::string str;
        BYTE code = 0;
        for (std::int32_t i = 0; code = ReadMemory<uint8_t>(nameaddr + i, ProcessID); i++) {
            str.push_back(code);
        }
        return str;
    }

	Instance FindFirstChild(std::string name) {
        std::uintptr_t childrenPtr = ReadMemory<uintptr_t>(address + offsets::Children, ProcessID);
        if (childrenPtr == 0)
            return Instance(0, ProcessID);
        std::uintptr_t childrenStart = ReadMemory<uintptr_t>(childrenPtr, ProcessID);
        std::uintptr_t childrenEnd = ReadMemory<uintptr_t>(childrenPtr + offsets::ChildrenEnd, ProcessID);
        for (std::uintptr_t childAddress = childrenStart; childAddress < childrenEnd; childAddress += 0x10) {
            std::uintptr_t childPtr = ReadMemory<uintptr_t>(childAddress, ProcessID);
            if (childPtr != 0) {
                std::uintptr_t nameaddr = ReadMemory<uintptr_t>(childPtr + offsets::Name, ProcessID);
                const auto size = ReadMemory<size_t>(nameaddr + 0x10, ProcessID);
                if (size >= 16)
                    nameaddr = ReadMemory<uintptr_t>(nameaddr, ProcessID);
                if (size != name.length())
                    continue;
                std::string str;
                BYTE code = 0;
                for (std::int32_t i = 0; code = ReadMemory<uint8_t>(nameaddr + i, ProcessID); i++) {
                    str.push_back(code);
                    if (str != name.substr(0, str.length()))
                        break;
                }
                if (str == name)
                    return Instance(childPtr, ProcessID);
            }
        }
        return Instance(0, ProcessID);
	}

    Instance WaitForChild(std::string name) {
        Instance child = FindFirstChild(name);
        while (child.GetAddress() == 0) {
            Sleep(5);
            child = FindFirstChild(name);
        }
        return child;
    }

    std::string ClassName() {
        std::uintptr_t classaddr = ReadMemory<uintptr_t>(address + offsets::ClassDescriptor, ProcessID);
        std::uintptr_t nameaddr = ReadMemory<uintptr_t>(classaddr + offsets::ClassDescriptorToClassName, ProcessID);
        const auto size = ReadMemory<size_t>(nameaddr + 0x10, ProcessID);
        if (size >= 16)
            nameaddr = ReadMemory<uintptr_t>(nameaddr, ProcessID);
        std::string str;
        BYTE code = 0;
        for (std::int32_t i = 0; code = ReadMemory<uint8_t>(nameaddr + i, ProcessID); i++) {
            str.push_back(code);
        }
        return str;
    }

    std::function<void()> SetScriptBytecode(const std::vector<char>& bytes, size_t size) {
        uintptr_t offset = (ClassName() == "LocalScript")
            ? offsets::LocalScriptByteCode
            : offsets::ModuleScriptByteCode;

        uintptr_t embedded = ReadMemory<uintptr_t>(address + offset, ProcessID);

        uintptr_t original_bytecode_ptr = ReadMemory<uintptr_t>(embedded + 0x10, ProcessID);
        uint64_t original_size = ReadMemory<uint64_t>(embedded + 0x20, ProcessID);

        HANDLE hProcess = OpenProcess(PROCESS_ALL_ACCESS, FALSE, ProcessID);
        void* newMem = VirtualAllocEx(hProcess, nullptr, size, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
        if (!newMem) {
            CloseHandle(hProcess);
            return []() {};
        }

        Memory::WriteNative((uintptr_t)newMem, bytes.data(), bytes.size(), ProcessID);
        WriteMemory<uintptr_t>(embedded + 0x10, reinterpret_cast<uintptr_t>(newMem), ProcessID);
        WriteMemory<uint64_t>(embedded + 0x20, static_cast<uint64_t>(size), ProcessID);

        CloseHandle(hProcess);

        return [=]() {
            WriteMemory<uintptr_t>(embedded + 0x10, original_bytecode_ptr, ProcessID);
            WriteMemory<uint64_t>(embedded + 0x20, original_size, ProcessID);
            HANDLE hProcess = OpenProcess(PROCESS_ALL_ACCESS, FALSE, ProcessID);
            VirtualFreeEx(hProcess, newMem, 0, MEM_RELEASE);
            CloseHandle(hProcess);
            };
    }

    void a() {

    }
    inline std::string GetBytecode(uintptr_t scriptAddress) {
        if (!scriptAddress)
            return "";

        uintptr_t classDescriptor = ReadMemory<uintptr_t>(scriptAddress + 0x18, ProcessID);
        if (!classDescriptor)
            return "";

        std::string className = ReadString(classDescriptor + 0x8, ProcessID);
        if (className.empty())
            return "";

        if (className != "AuroraScript" &&
            className != "ModuleScript" &&
            className != "Script" &&
            className != "LocalScript") {
            return "";
        }

        uintptr_t protectedString = 0;
        if (className == "ModuleScript") {
            protectedString = ReadMemory<uintptr_t>(scriptAddress + offsets::ModuleScriptByteCode, ProcessID);
        }
        else {
            protectedString = ReadMemory<uintptr_t>(scriptAddress + offsets::LocalScriptByteCode, ProcessID);
        }

        if (!protectedString || protectedString < 0x10000 || protectedString >= 0x7FFFFFFFFFFF)
            return "";

        uintptr_t dataPtr = ReadMemory<uintptr_t>(protectedString + 0x10, ProcessID);
        uint64_t size = ReadMemory<uint64_t>(protectedString + 0x20, ProcessID);

        if (!dataPtr || size == 0 || size > 10 * 1024 * 1024)
            return "";

        std::vector<char> buffer(size + 1, 0);
        Memory::ReadNative(dataPtr, buffer.data(), size, ProcessID);

        return Bytecode::Decompress(std::string(buffer.data(), size));
    }
        inline std::string SendHttpPost(const std::string& data) {
        static HINTERNET hInternet = InternetOpenA("HTTP Client", INTERNET_OPEN_TYPE_DIRECT, NULL, NULL, 0);
        if (!hInternet) return "";
        static HINTERNET hConnect = InternetConnectA(hInternet, "127.0.0.1", 9002, NULL, NULL, INTERNET_SERVICE_HTTP, 0, 0);
        if (!hConnect) return "";
        const char* headers = "Content-Type: text/plain\r\n";
        HINTERNET hRequest = HttpOpenRequestA(hConnect, "POST", NULL, NULL, NULL, NULL, INTERNET_FLAG_RELOAD, 0);
        if (!hRequest) return "";
        std::string encoded = cppcodec::base64_rfc4648::encode(data);
        if (!HttpSendRequestA(hRequest, headers, (DWORD)strlen(headers), (LPVOID)encoded.c_str(), (DWORD)encoded.size())) {
            InternetCloseHandle(hRequest);
            return "";
        }
        std::vector<char> response;
        char buffer[8192];
        DWORD bytesRead;
        while (InternetReadFile(hRequest, buffer, sizeof(buffer), &bytesRead) && bytesRead > 0)
            response.insert(response.end(), buffer, buffer + bytesRead);
        InternetCloseHandle(hRequest);
        return std::string(response.begin(), response.end());
    }
    inline std::string DecompileExternal(uintptr_t scriptAddress) {
        if (!scriptAddress) return "";
        std::string bytecode = GetBytecode(scriptAddress);
        if (bytecode.empty()) return "";
        std::string response = SendHttpPost(bytecode);
        return response;
    }
};

inline Instance FetchDatamodel(uintptr_t BaseModule, DWORD ProcessID) {
	uintptr_t Fake = ReadMemory<uintptr_t>(BaseModule + offsets::FakeDataModelPointer, ProcessID);
	uintptr_t Real = ReadMemory<uintptr_t>(Fake + offsets::FakeDataModelToDataModel, ProcessID);
	return Instance(Real, ProcessID);
}
#include <windows.h>
#include <psapi.h>
#include <regex>
#include <filesystem>
#include <fstream>
inline std::string GetRobloxFolderFromPID(DWORD pid) {
    char* localAppData = nullptr;
    size_t len = 0;
    _dupenv_s(&localAppData, &len, "LOCALAPPDATA");
    if (!localAppData) {
        return "";
    }

    std::string robloxPath = std::string(localAppData) + "\\Roblox\\Versions\\";
    free(localAppData);

    // Debug: Check if path exists
    if (!std::filesystem::exists(robloxPath)) {
        return "";
    }

    // Find the most recent version folder
    std::string latestVersion;
    try {
        for (const auto& entry : std::filesystem::directory_iterator(robloxPath)) {
            if (entry.is_directory()) {
                std::string dirName = entry.path().filename().string();

                // Check for Roblox executables
                if (std::filesystem::exists(entry.path() / "RobloxPlayerBeta.exe") ||
                    std::filesystem::exists(entry.path() / "RobloxPlayerLauncher.exe") ||
                    dirName.find("version-") == 0) {

                    latestVersion = entry.path().string();
                    // For debugging, you can add a log here
                }
            }
        }
    }
    catch (const std::exception& e) {
        return "";
    }

    return latestVersion;
}

std::string GetFileNameFromPath(const std::string& fullPath)
{
    size_t pos = fullPath.find_last_of("\\/");
    if (pos == std::string::npos)
        return fullPath;

    return fullPath.substr(pos + 1);
}

static bool withinDirectory(const std::filesystem::path& base, const std::filesystem::path& path) {
    std::filesystem::path baseAbs = std::filesystem::absolute(base).lexically_normal();
    std::filesystem::path resolvedPath = baseAbs;

    for (const std::filesystem::path& part : path) {
        if (part == "..") {
            if (resolvedPath.has_parent_path()) {
                resolvedPath = resolvedPath.parent_path();
            }
        }
        else if (part != ".") {
            resolvedPath /= part;
        }
    }

    resolvedPath = resolvedPath.lexically_normal();

    return std::mismatch(baseAbs.begin(), baseAbs.end(), resolvedPath.begin()).first == baseAbs.end();
}