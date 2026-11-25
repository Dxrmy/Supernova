#include "Game/AudioLib/AudSystemWrapper.hpp"
#include "Game/GameAudio/AudStageBgmTable.hpp"
#include <cstring>
#include <nebula_game_api.h>
#include <nebula_module.h>

extern "C" const NebulaHostServices* nebula_host(void);

namespace {
    constexpr uint32_t kVoiceCount = 64u;
    constexpr uint32_t kEffectLines = 4u;

    NebulaVoice sVoices[kVoiceCount]{};
    NebulaEffectLine sEffectLines[kEffectLines]{};
    AudSystemWrapper* sActiveAudio = nullptr;
}  // namespace

AudSystemWrapper::AudSystemWrapper()
    : mCreated(false), mVoicesRegistered(false), mCurrentBgm(nullptr) {
    sActiveAudio = this;
}

void AudSystemWrapper::createAudioSystem() {
    if (mCreated) {
        return;
    }
    std::memset(sVoices, 0, sizeof(sVoices));
    std::memset(sEffectLines, 0, sizeof(sEffectLines));

    const NebulaHostServices* host = nebula_host();
    if (host != nullptr && host->sound_clock != nullptr) {
        host->sound_clock(host->user, NEBULA_AUDIO_START, 180u);
    }
    mCreated = true;
    nebula_log("[SMG2-AUDIO] AudSystemWrapper::createAudioSystem() started host sound clock (180 Hz)");
}

void AudSystemWrapper::movement() {
}

void AudSystemWrapper::startStageBgm(const char* pStageName) {
    mCurrentBgm = AudStageBgmTable::getBgmName(pStageName);
    nebula_log("[SMG2-AUDIO] AudStageBgmTable('%s') -> %s", pStageName != nullptr ? pStageName : "", mCurrentBgm);
}

void AudSystemWrapper::soundFrameTick() {
    if (!mCreated) {
        return;
    }
    const NebulaHostServices* host = nebula_host();
    if (host == nullptr) {
        return;
    }
    if (!mVoicesRegistered && host->voices_setup != nullptr) {
        host->voices_setup(host->user, sVoices, kVoiceCount, sEffectLines);
        mVoicesRegistered = true;
    }
    if (host->voices_mix != nullptr) {
        host->voices_mix(host->user, 1.0f, 1u);
    }
    if (host->voices_frame != nullptr) {
        host->voices_frame(host->user);
    }
}

extern "C" void smg2_audio_sound_frame_tick() {
    if (sActiveAudio != nullptr) {
        sActiveAudio->soundFrameTick();
    }
}
