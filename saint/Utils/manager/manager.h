#pragma once
#include <Windows.h>
#include <cstdint>
#include <memory>
#include "MemoryManager.h"

// Forward declaration
extern std::unique_ptr<MemoryManager> MemMgr;

class manager_class {
public:
    // You'll need to pass the process ID (or get it from MemMgr)
    static uint64_t get_datamodel(uint64_t rbx_base, DWORD pid);
    static uint64_t get_script_context(uint64_t data_model, DWORD pid);
    static uint64_t get_roblox_state(uint64_t script_context, DWORD pid);

    static bool is_game_loaded(uint64_t data_model, DWORD pid);
    static void set_thread_capabilities(uint64_t L, int identity, uintptr_t capabilities, DWORD pid);
};

inline std::unique_ptr<manager_class> manager = std::make_unique<manager_class>();