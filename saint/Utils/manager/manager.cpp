#include "manager.h"
#include "../Update/Offsets.hpp"

// You can either use your MemoryCompat functions or directly use MemoryManager
// Option 1: Using MemoryCompat (easier, matches your existing pattern)

#include <Memory.hpp>

// Option 2: Direct MemoryManager usage (more efficient)
// This assumes MemMgr is properly initialized elsewhere

uint64_t manager_class::get_datamodel(uint64_t rbx_base, DWORD pid) {
    // Using MemoryCompat (with PID parameter)
    uint64_t fake_datamodel = MemoryCompat::ReadMemory<uint64_t>(rbx_base + Offsets::FakeDataModelPointer, pid);
    uint64_t valid_datamodel = MemoryCompat::ReadMemory<uint64_t>(fake_datamodel + Offsets::FakeDataModelToDataModel, pid);
    return valid_datamodel;

    // Alternative using direct MemoryManager:
    // uint64_t fake_datamodel = MemMgr->ReadMemory<uint64_t>(rbx_base + Offsets::FakeDataModelPointer);
    // uint64_t valid_datamodel = MemMgr->ReadMemory<uint64_t>(fake_datamodel + Offsets::FakeDataModelToDataModel);
    // return valid_datamodel;
}

uint64_t manager_class::get_script_context(uint64_t data_model, DWORD pid) {
    uint64_t datamodel_children = MemoryCompat::ReadMemory<uint64_t>(data_model + Offsets::Children, pid);
    uint64_t child_value = MemoryCompat::ReadMemory<uint64_t>(datamodel_children, pid);
    uint64_t script_context = MemoryCompat::ReadMemory<uint64_t>(child_value + Offsets::ScriptContext, pid);

    return script_context;
}

uint64_t manager_class::get_roblox_state(uint64_t script_context, DWORD pid) {
    MemoryCompat::WriteMemory<BOOLEAN>(script_context + 0x920, TRUE, pid);

    uint64_t decrypted_state_address = (script_context + 0x3E0);
    uint64_t decrypted_state = MemoryCompat::ReadMemory<uint64_t>(decrypted_state_address, pid);

    uint64_t roblox_state;
    uint32_t* p = reinterpret_cast<uint32_t*>(&roblox_state);

    p[0] = uint32_t(decrypted_state) - uint32_t(decrypted_state_address);
    p[1] = uint32_t(decrypted_state >> 32) - uint32_t(decrypted_state_address);

    // Check if address is valid - you'll need to implement this check
    bool is_valid = true; // You should replace this with actual validation
    // For example: is_valid = MemMgr->IsAddressValid(roblox_state);

    uint64_t verified_roblox_state = (!roblox_state || (roblox_state & 0x7) || !is_valid) ? 0x0 : roblox_state;
    return verified_roblox_state;
}

bool manager_class::is_game_loaded(uint64_t data_model, DWORD pid) {
    BYTE game_loaded = MemoryCompat::ReadMemory<BYTE>(data_model + Offsets::GameLoaded, pid);
    bool is_loaded = (game_loaded == 31);
    return is_loaded;
}

void manager_class::set_thread_capabilities(uint64_t L, int identity, uintptr_t capabilities, DWORD pid) {
    uint64_t userdata = MemoryCompat::ReadMemory<uint64_t>(L + Offsets::lua_State::userdata, pid);
    if (!userdata) return;

    MemoryCompat::WriteMemory<uintptr_t>(userdata + Offsets::userdata::capabilities, capabilities, pid);
    MemoryCompat::WriteMemory<int>(userdata + Offsets::userdata::identity, identity, pid);
}