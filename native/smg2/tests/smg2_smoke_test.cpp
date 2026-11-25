// Smoke test: opens an imported SB4E01 game.pak, loads smg_game.dll, attaches NebulaHostServices,
// and executes nebula_game_run() in deterministic single-step mode.
#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <nebula_game_api.h>
#include "galaxy/asset_store.h"

#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <string>

namespace {

struct SmokeHost {
    galaxy::assets::AssetStore store;
    std::string game_id;
    std::chrono::steady_clock::time_point start = std::chrono::steady_clock::now();
    int stepped = 0;
};

void svc_log(void*, const char* text) {
    std::printf("%s\n", text != nullptr ? text : "");
}

void svc_fatal(void*, const char* text) {
    std::fprintf(stderr, "FATAL: %s\n", text != nullptr ? text : "");
    std::exit(2);
}

void* svc_heap_alloc(void*, size_t size) { return std::malloc(size == 0 ? 1 : size); }
void* svc_heap_realloc(void*, void* p, size_t size) { return std::realloc(p, size == 0 ? 1 : size); }
void svc_heap_free(void*, void* p) { std::free(p); }

void to_entry(const galaxy::assets::Entry& src, NebulaAssetEntry* dst) {
    dst->index = src.index;
    dst->length = static_cast<uint32_t>(src.length);
    dst->position = src.position;
    dst->children = src.children;
    dst->folder = src.folder ? 1u : 0u;
    dst->kind = src.kind;
    dst->name = src.name;
}

int svc_asset_find(void* user, const char* path, NebulaAssetEntry* out) {
    auto* h = static_cast<SmokeHost*>(user);
    auto e = h->store.find(path != nullptr ? path : "");
    if (!e) return 0;
    if (out != nullptr) to_entry(*e, out);
    return 1;
}

int svc_asset_entry(void* user, int32_t index, NebulaAssetEntry* out) {
    auto* h = static_cast<SmokeHost*>(user);
    auto e = h->store.entry(index);
    if (!e) return 0;
    if (out != nullptr) to_entry(*e, out);
    return 1;
}

int svc_asset_child(void* user, int32_t folder, uint32_t child, NebulaAssetEntry* out) {
    auto* h = static_cast<SmokeHost*>(user);
    auto e = h->store.child(folder, child);
    if (!e) return 0;
    if (out != nullptr) to_entry(*e, out);
    return 1;
}

int32_t svc_asset_read(void* user, int32_t file, uint64_t offset, uint32_t size, void* dst) {
    auto* h = static_cast<SmokeHost*>(user);
    auto e = h->store.entry(file);
    if (!e || e->folder || offset + size > e->length) return -1;
    return h->store.read(e->position + offset, size, dst) ? static_cast<int32_t>(size) : -1;
}

const void* svc_asset_map(void* user, int32_t file) {
    auto* h = static_cast<SmokeHost*>(user);
    auto e = h->store.entry(file);
    if (!e || e->folder) return nullptr;
    return h->store.map(e->position, static_cast<size_t>(e->length));
}

const char* svc_game_id(void* user) {
    return static_cast<SmokeHost*>(user)->game_id.c_str();
}

uint64_t svc_clock_ns(void* user) {
    auto* h = static_cast<SmokeHost*>(user);
    return static_cast<uint64_t>(
        std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now() - h->start).count() + 1);
}

void svc_frame_wait(void*) {}
void svc_frame_begun(void*, int, uint32_t) {}
void svc_test_frame_step(void* user) {
    static_cast<SmokeHost*>(user)->stepped++;
}

}  // namespace

int main(int argc, char** argv) {
    std::setvbuf(stdout, nullptr, _IONBF, 0);
    std::setvbuf(stderr, nullptr, _IONBF, 0);
    if (argc < 3) {
        std::fprintf(stderr, "Usage: smg2_smoke_test <smg_game.dll> <game.pak>\n");
        return 1;
    }

    SmokeHost host;
    if (!host.store.open(std::filesystem::path(argv[2]))) {
        std::fprintf(stderr, "Failed to open game.pak: %s\n", argv[2]);
        return 1;
    }
    host.game_id = std::string(host.store.game_id());
    std::printf("Opened game.pak: game_id=%s\n", host.game_id.c_str());

    HMODULE dll = LoadLibraryA(argv[1]);
    if (dll == nullptr) {
        std::fprintf(stderr, "Failed to load DLL %s (err=%lu)\n", argv[1], GetLastError());
        return 1;
    }

    using AttachFn = uint32_t (*)(const NebulaHostServices*);
    using RunFn = void (*)(void);
    auto attach = reinterpret_cast<AttachFn>(GetProcAddress(dll, "nebula_game_attach"));
    auto run = reinterpret_cast<RunFn>(GetProcAddress(dll, "nebula_game_run"));
    if (attach == nullptr || run == nullptr) {
        std::fprintf(stderr, "Missing nebula_game_attach or nebula_game_run exports\n");
        return 1;
    }

    NebulaHostServices svc{};
    svc.version = NEBULA_GAME_API_VERSION;
    svc.size = sizeof(NebulaHostServices);
    svc.user = &host;
    svc.log = svc_log;
    svc.fatal = svc_fatal;
    svc.heap_alloc = svc_heap_alloc;
    svc.heap_realloc = svc_heap_realloc;
    svc.heap_free = svc_heap_free;
    svc.asset_find = svc_asset_find;
    svc.asset_find_game = svc_asset_find;
    svc.asset_entry = svc_asset_entry;
    svc.asset_child = svc_asset_child;
    svc.asset_read = svc_asset_read;
    svc.asset_map = svc_asset_map;
    svc.game_id = svc_game_id;
    svc.frame_wait = svc_frame_wait;
    svc.frame_begun = svc_frame_begun;
    svc.clock_ns = svc_clock_ns;
    svc.test_frame_step = svc_test_frame_step;

    const uint32_t ver = attach(&svc);
    std::printf("nebula_game_attach returned version=%u\n", ver);
    if (ver != NEBULA_GAME_API_VERSION) {
        std::fprintf(stderr, "API version mismatch: expected %u, got %u\n", NEBULA_GAME_API_VERSION, ver);
        return 1;
    }

    run();
    std::printf("smg2_smoke_test completed cleanly (steps=%d)\n", host.stepped);
    FreeLibrary(dll);
    return host.stepped > 0 ? 0 : 1;
}
