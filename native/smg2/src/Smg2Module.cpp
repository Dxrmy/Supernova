// SMG2 (Garigari) game module entry points and Nebula host service bindings.
#include <nebula_game_api.h>
#include <nebula_module.h>
#include <nebula_save_import.h>

#include <stdarg.h>
#include <stdlib.h>

extern "C" void nebula_game_main(void);

static const NebulaHostServices* sHost = nullptr;

#if defined(_WIN32) && !defined(NEBULA_MODULE_WITH_CRT)
extern "C" void nebula_run_constructors(void);

extern "C" void* malloc(size_t size) {
    return sHost != nullptr ? sHost->heap_alloc(sHost->user, size) : nullptr;
}

extern "C" void* calloc(size_t count, size_t size) {
    if (size != 0 && count > static_cast<size_t>(-1) / size) {
        return nullptr;
    }
    const size_t bytes = count * size;
    unsigned char* const block = static_cast<unsigned char*>(malloc(bytes));
    if (block != nullptr) {
        for (size_t i = 0; i < bytes; i++) {
            block[i] = 0;
        }
    }
    return block;
}

extern "C" void* realloc(void* data, size_t size) {
    return sHost != nullptr ? sHost->heap_realloc(sHost->user, data, size) : nullptr;
}

extern "C" void free(void* data) {
    if (sHost != nullptr) {
        sHost->heap_free(sHost->user, data);
    }
}
#endif

extern "C" const NebulaHostServices* nebula_host(void) {
    return sHost;
}

extern "C" NEBULA_GAME_API uint32_t nebula_game_attach(const NebulaHostServices* host) {
    if (host == nullptr || host->version < NEBULA_GAME_API_VERSION || host->size < sizeof(NebulaHostServices)) {
        return 0;
    }
    sHost = host;
#if defined(_WIN32) && !defined(NEBULA_MODULE_WITH_CRT)
    nebula_run_constructors();
#endif
    return NEBULA_GAME_API_VERSION;
}

extern "C" NEBULA_GAME_API void nebula_game_run(void) {
    if (sHost == nullptr) {
        nebula_fatal("nebula_game_run before nebula_game_attach");
    }
    nebula_game_main();
}

static char* format_text(char* stack, size_t size, size_t* pLength, const char* format, va_list args) {
    va_list again;
    va_copy(again, args);
    const int n = nebula_vsnprintf(stack, size, format, args);
    char* text = stack;
    size_t length = n < 0 ? 0 : static_cast<size_t>(n);
    if (length >= size) {
        const NebulaHostServices* host = nebula_host();
        text = host != nullptr && host->heap_alloc != nullptr ? static_cast<char*>(host->heap_alloc(host->user, length + 1)) : nullptr;
        if (text != nullptr) {
            nebula_vsnprintf(text, length + 1, format, again);
        } else {
            text = stack;
            length = size - 1;
        }
    }
    va_end(again);
    *pLength = length;
    return text;
}

extern "C" void nebula_vlog(const char* format, va_list args) {
    const NebulaHostServices* host = nebula_host();
    if (host == nullptr || host->log == nullptr) {
        return;
    }
    char stack_text[1024];
    size_t length = 0;
    char* text = format_text(stack_text, sizeof(stack_text), &length, format, args);

    size_t start = 0;
    for (size_t i = 0; i < length; i++) {
        if (text[i] == '\n') {
            text[i] = '\0';
            host->log(host->user, text + start);
            start = i + 1;
        }
    }
    if (start < length) {
        text[length] = '\0';
        host->log(host->user, text + start);
    }
    if (text != stack_text) {
        host->heap_free(host->user, text);
    }
}

extern "C" void nebula_log(const char* format, ...) {
    va_list args;
    va_start(args, format);
    nebula_vlog(format, args);
    va_end(args);
}

extern "C" void nebula_fatal(const char* format, ...) {
    char stack_text[1024];
    size_t length = 0;
    va_list args;
    va_start(args, format);
    char* text = format_text(stack_text, sizeof(stack_text), &length, format, args);
    va_end(args);

    const NebulaHostServices* host = nebula_host();
    if (host != nullptr && host->fatal != nullptr) {
        host->fatal(host->user, text);
    }
    __builtin_trap();
}

extern "C" void nebula_panic(const char* file, int line, const char* format, ...) {
    char detail[512];
    va_list args;
    va_start(args, format);
    nebula_vsnprintf(detail, sizeof(detail), format, args);
    va_end(args);
    nebula_fatal("%s:%d: %s", file != nullptr ? file : "<unknown>", line, detail);
}

extern "C" char nebula_game_region(void) {
    const NebulaHostServices* host = nebula_host();
    if (host != nullptr && host->game_id != nullptr) {
        const char* id = host->game_id(host->user);
        if (id != nullptr && id[0] != '\0' && id[1] != '\0' && id[2] != '\0' && id[3] != '\0') {
            return id[3];
        }
    }
    return 'E';
}

extern "C" NEBULA_GAME_API const struct NebulaModApi* nebula_game_mod_api(void) {
    // TODO(SMG2): Wire LumaCore mod API once LumaHook instrumentation is enabled for Garigari.
    return nullptr;
}

extern "C" void smg2_audio_sound_frame_tick();

extern "C" NEBULA_GAME_API void nebula_game_sound_frame(void) {
    smg2_audio_sound_frame_tick();
}

extern "C" NEBULA_GAME_API uint32_t nebula_game_mii_valid(const NebulaMii*) {
    return 1;
}

extern "C" NEBULA_GAME_API void nebula_game_mii_changed(void) {
}

extern "C" NEBULA_GAME_API void nebula_game_luma_describe(NebulaDescribeWrite write, void* user) {
    if (write != nullptr) {
        static const char kEmpty[] = "{}";
        write(user, kEmpty, sizeof(kEmpty) - 1);
    }
}

extern "C" NEBULA_GAME_API int32_t nebula_save_import_read(const uint8_t* bytes, uint32_t length, NebulaSaveImportDone done, void* context) {
    if (bytes == nullptr || length == 0u || done == nullptr) {
        return NEBULA_SAVE_IMPORT_ARGUMENT;
    }
    // TODO(Garigari): queue async NBSV/GameData.bin inspection on the game thread for Settings -> Save Import.
    return NEBULA_SAVE_IMPORT_DAMAGED;
}
