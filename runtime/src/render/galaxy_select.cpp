// SPDX-License-Identifier: GPL-3.0-only
#include "galaxy/render/galaxy_select.h"
#include "galaxy/render/render_config.h"

#include <atomic>
#include <mutex>
#include <string_view>
#include <vector>

namespace galaxy::render {
namespace {

bool is_smg2_active() noexcept {
    const std::string id = texture_pack_game_id();
    return std::string_view(id).starts_with("SB4");
}

std::atomic<std::uint64_t> g_travel{0u};  // stage + 1 in the low 32 bits, star in the high
std::atomic<bool> g_grand_stars{false};
std::atomic<bool> g_names_wanted{false};
std::atomic<bool> g_names_known{false};
std::mutex g_names_mutex;
std::vector<std::string> g_galaxy_names;
std::vector<std::vector<std::string>> g_star_names;

}  // namespace

const GalaxyStage* active_galaxy_stages() noexcept {
    return is_smg2_active() ? kSmg2GalaxyStages : kGalaxyStages;
}

std::size_t active_galaxy_stage_count() noexcept {
    return is_smg2_active() ? kSmg2GalaxyStageCount : kGalaxyStageCount;
}

const char* active_hub_id() noexcept {
    return is_smg2_active() ? kSmg2HubId : kObservatoryId;
}

const char* active_hub_name() noexcept {
    return is_smg2_active() ? kSmg2HubName : kObservatoryName;
}

void request_galaxy_travel(std::size_t stage, std::uint32_t star) noexcept {
    const GalaxyStage* stages = active_galaxy_stages();
    const std::size_t count = active_galaxy_stage_count();
    if (stage >= count || star == 0u || star > stages[stage].stars) {
        return;
    }
    g_travel.store((static_cast<std::uint64_t>(star) << 32u) | (stage + 1u), std::memory_order_release);
}

void request_grand_stars() noexcept {
    g_grand_stars.store(true, std::memory_order_release);
}

bool take_galaxy_travel(std::size_t& stage, std::uint32_t& star) noexcept {
    const std::uint64_t travel = g_travel.exchange(0u, std::memory_order_acq_rel);
    if (travel == 0u) {
        return false;
    }
    stage = static_cast<std::size_t>((travel & 0xFFFFFFFFu) - 1u);
    star = static_cast<std::uint32_t>(travel >> 32u);
    return stage < active_galaxy_stage_count();
}

bool take_grand_stars() noexcept {
    return g_grand_stars.exchange(false, std::memory_order_acq_rel);
}

void want_galaxy_select_names() noexcept {
    if (!g_names_known.load(std::memory_order_acquire)) {
        g_names_wanted.store(true, std::memory_order_release);
    }
}

bool take_galaxy_select_names_wanted() noexcept {
    return g_names_wanted.exchange(false, std::memory_order_acq_rel) && !g_names_known.load(std::memory_order_acquire);
}

void set_galaxy_select_name(std::size_t stage, std::uint32_t star, std::string name) {
    const GalaxyStage* stages = active_galaxy_stages();
    const std::size_t count = active_galaxy_stage_count();
    if (stage >= count || star > stages[stage].stars) {
        return;
    }
    const std::lock_guard<std::mutex> lock(g_names_mutex);
    g_galaxy_names.resize(count);
    g_star_names.resize(count);
    if (star == 0u) {
        g_galaxy_names[stage] = std::move(name);
    } else {
        g_star_names[stage].resize(stages[stage].stars);
        g_star_names[stage][star - 1u] = std::move(name);
    }
    g_names_known.store(true, std::memory_order_release);
}

std::string galaxy_select_name(std::size_t stage) {
    const std::lock_guard<std::mutex> lock(g_names_mutex);
    return stage < g_galaxy_names.size() ? g_galaxy_names[stage] : std::string();
}

std::string galaxy_select_star_name(std::size_t stage, std::uint32_t star) {
    const std::lock_guard<std::mutex> lock(g_names_mutex);
    if (stage >= g_star_names.size() || star == 0u || star > g_star_names[stage].size()) {
        return {};
    }
    return g_star_names[stage][star - 1u];
}

}  // namespace galaxy::render
