#include "nvse_api.h"
#include "load_guard.h"
#include "log.h"
#include "row_appearance.h"

namespace {
void OnNVSEMessage(NVSEMessagingInterface::Message* message) {
    if (!message) return;
    if (message->type == kPostPostLoad && InstallLoadGuard()) InstallSaveAppearance();
}
}

bool NVSEPlugin_Query(const NVSEInterface* nvse, PluginInfo* info) {
    info->infoVersion = 1;
    info->name = "Only Load Latest Save Per Character";
    info->version = 8;
    if (nvse->isEditor || nvse->nvseVersion < 6) return false;
    OpenLog(nvse->GetRuntimeDirectory());
    Log("Only Load Latest Save Per Character v0.5.1 (NVSE %u, runtime %08X)", nvse->nvseVersion, nvse->runtimeVersion);
    if (nvse->runtimeVersion != kRuntime140525 || nvse->isNogore) {
        Log("ERROR: Requires FalloutNV 1.4.0.525, standard runtime.");
        return false;
    }
    return true;
}

bool NVSEPlugin_Load(const NVSEInterface* nvse) {
    const auto messaging = static_cast<NVSEMessagingInterface*>(nvse->QueryInterface(kMessagingInterface));
    if (!messaging || !messaging->RegisterListener(nvse->GetPluginHandle(), "NVSE", OnNVSEMessage)) {
        Log("ERROR: Could not register the NVSE message listener.");
        return false;
    }
    Log("Registered; installing after plugins finish loading. No XML edits; blocked saves use the back/cancel sound.");
    return true;
}
