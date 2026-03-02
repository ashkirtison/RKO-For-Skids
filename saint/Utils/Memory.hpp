#pragma once
#include <Windows.h>
#include <cstdint>
#include <memory>
#include "MemoryManager.h"

// Forward declaration - will be initialized in MemoryManager.cpp
extern std::unique_ptr<MemoryManager> MemMgr;

namespace MemoryCompat {
    // Compatibility wrappers that work with ProcessID parameter
    // These functions now delegate to the global MemMgr MemoryManager instance
    
    template <class Ty>
    inline Ty ReadMemory(uintptr_t address, DWORD pid) {
        Ty value{};
        HANDLE Handle = OpenProcess(PROCESS_ALL_ACCESS, FALSE, pid);
        if (Handle) {
            Luck_ReadVirtualMemory(Handle, reinterpret_cast<void*>(address), &value, sizeof(Ty), nullptr);
            CloseHandle(Handle);
        }
        return value;
    }

    template <class Ty>
    inline void WriteMemory(uintptr_t address, Ty value, DWORD pid) {
        HANDLE Handle = OpenProcess(PROCESS_ALL_ACCESS, FALSE, pid);
        if (Handle) {
            Luck_WriteVirtualMemory(Handle, reinterpret_cast<void*>(address), &value, sizeof(Ty), nullptr);
            CloseHandle(Handle);
        }
    }

    inline void ReadRaw(uintptr_t address, void* buffer, size_t size, DWORD pid) {
        HANDLE Handle = OpenProcess(PROCESS_ALL_ACCESS, FALSE, pid);
        if (Handle) {
            Luck_ReadVirtualMemory(Handle, reinterpret_cast<void*>(address), buffer, size, nullptr);
            CloseHandle(Handle);
        }
    }

    inline void WriteRaw(uintptr_t address, const void* buffer, size_t size, DWORD pid) {
        HANDLE Handle = OpenProcess(PROCESS_ALL_ACCESS, FALSE, pid);
        if (Handle) {
            Luck_WriteVirtualMemory(Handle, reinterpret_cast<void*>(address), const_cast<void*>(buffer), size, nullptr);
            CloseHandle(Handle);
        }
    }

    inline std::string ReadString(uintptr_t Address, DWORD pid) {
        std::string result;
        char character;
        int offset = 0;

        int32_t StrLength = ReadMemory<int32_t>(Address + 0x18, pid);

        if (StrLength >= 16) {
            Address = ReadMemory<uintptr_t>(Address, pid);
        }

        while ((character = ReadMemory<char>(Address + offset, pid)) != 0) {
            result.push_back(character);
            offset += sizeof(character);
        }

        return result;
    }
}

namespace Memory {
    // Re-export compatibility functions in Memory namespace for backward compatibility
    using MemoryCompat::ReadMemory;
    using MemoryCompat::WriteMemory;
    using MemoryCompat::ReadString;
    using MemoryCompat::ReadRaw;
    using MemoryCompat::WriteRaw;

    // Alias for old ReadNative calls
    inline void ReadNative(uintptr_t address, void* buffer, size_t size, DWORD pid) {
        ReadRaw(address, buffer, size, pid);
    }

    // Alias for old WriteNative calls
    inline void WriteNative(uintptr_t address, const void* buffer, size_t size, DWORD pid) {
        WriteRaw(address, buffer, size, pid);
    }
}

// Global exports for direct use in existing code
template <class Ty>
inline Ty ReadMemory(uintptr_t address, DWORD pid) {
    return MemoryCompat::ReadMemory<Ty>(address, pid);
}

template <class Ty>
inline void WriteMemory(uintptr_t address, Ty value, DWORD pid) {
    MemoryCompat::WriteMemory<Ty>(address, value, pid);
}

inline std::string ReadString(uintptr_t Address, DWORD pid) {
    return MemoryCompat::ReadString(Address, pid);
}
