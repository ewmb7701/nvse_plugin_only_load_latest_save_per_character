#pragma once
#include <cstdint>

// Minimal public ABI, following ewmb7701/ewmb-nvse's PluginAPI.h.
// Messaging declarations follow the xNVSE SDK bundled with Stewie Tweaks 10.00.
struct CommandInfo;
struct NVSEInterface {
    std::uint32_t nvseVersion;
    std::uint32_t runtimeVersion;
    std::uint32_t editorVersion;
    std::uint32_t isEditor;
    bool (*RegisterCommand)(CommandInfo*);
    void (*SetOpcodeBase)(std::uint32_t);
    void* (*QueryInterface)(std::uint32_t);
    std::uint32_t (*GetPluginHandle)();
    bool (*RegisterTypedCommand)(CommandInfo*, std::uint32_t);
    const char* (*GetRuntimeDirectory)();
    std::uint32_t isNogore;
};

struct PluginInfo {
    std::uint32_t infoVersion;
    const char* name;
    std::uint32_t version;
};

struct NVSEMessagingInterface {
    struct Message {
        const char* sender;
        std::uint32_t type;
        std::uint32_t dataLen;
        void* data;
    };
    using Callback = void (*)(Message*);
    std::uint32_t version;
    bool (*RegisterListener)(std::uint32_t, const char*, Callback);
    bool (*Dispatch)(std::uint32_t, std::uint32_t, void*, std::uint32_t, const char*);
};

inline constexpr std::uint32_t kMessagingInterface = 2;
inline constexpr std::uint32_t kPostPostLoad = 9;
inline constexpr std::uint32_t kRuntime140525 = 0x040020D0;
static_assert(sizeof(void*) == 4, "Build the plugin for Win32.");
