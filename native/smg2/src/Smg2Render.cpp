// SMG2 Render / J3D & 2D Layout -> NebulaFrameList bridge (M3 + M4).
// Registers SB4E01's 5,539 pipelines from /NebulaData/Pipelines.bin, registers cooked
// NbTextur artwork from TitleLogo / FileSelect in game.pak, and submits valid
// NebulaFrameLists with 2D textured quads & scene passes to Nebula's D3D12 renderer.

#include <nebula_cooked_texture.h>
#include <nebula_frame_list.h>
#include <nebula_game_api.h>
#include <nebula_model.h>
#include <nebula_module.h>

#include <cstring>

extern "C" const NebulaHostServices* nebula_host(void);

namespace Smg2Render {

namespace {
    constexpr uint32_t kFrameSlotCount = 4u;
    constexpr uint32_t kMaxLayoutInputs = 32u;
    constexpr uint32_t kMaxQuads = 16u;

    struct RingVertex2D {
        float x, y, z;
        uint32_t rgba;
        float u, v;
    };

    struct FrameSlot {
        NebulaFrameList list;
        NebulaFramePass passes[5];
        NebulaFrameItem items[kMaxQuads];
        NebulaFrameView view;
        NebulaFrameCamera camera;
        NebulaFrameBlock block;
        float identityJoint[3][4];
        uint32_t scaleFlag;
        NebulaFrameConstants constants[kMaxQuads];
        float constRows[kMaxQuads][5][4];
        RingVertex2D vertices[kMaxQuads * 4u];
        uint32_t indices[kMaxQuads * 6u];
        bool inUse;
    };

    FrameSlot sSlots[kFrameSlotCount]{};
    uint32_t sNextSlot = 0u;
    bool sPipelinesRegistered = false;
    uint32_t sRegisteredCount = 0u;

    uint32_t sUiPipeline = 0u;
    uint32_t sFlatPipeline = 0u;
    uint32_t sTexSpace = 0u;
    uint32_t sTexLogo = 0u;
    uint32_t sTexTico = 0u;
    uint32_t sTexFileFrame = 0u;
    uint32_t sTexFileButton = 0u;

    int sScreenState = 0;      // 0 = Logo/Boot, 1 = Title, 2 = FileSelect, 3 = InStage
    float sScreenAlpha = 1.0f;
    int sFileSlot = 1;

    // Camera/player state for M5
    float sCamYaw = 0.0f;
    float sPlayerX = 0.0f;
    float sPlayerZ = 0.0f;

    uint32_t loadCookedTextureTable(const NebulaHostServices* host, const char* pakPath) {
        if (host == nullptr || host->asset_find == nullptr || host->asset_map == nullptr ||
            host->texture_register == nullptr || host->texture_table == nullptr) {
            return 0u;
        }
        NebulaAssetEntry entry{};
        if (host->asset_find(host->user, pakPath, &entry) == 0 || entry.length < NEBULA_TEXTURE_HEADER) {
            return 0u;
        }
        const auto* mapped = static_cast<const uint8_t*>(host->asset_map(host->user, entry.index));
        if (mapped == nullptr) {
            return 0u;
        }
        const void* texBytes = mapped;
        uint32_t texSize = entry.length;
        const NebulaTexture* rec = nebula_texture_record(texBytes);
        if (rec == nullptr && entry.length >= 24u) {
            uint32_t ver = 0u;
            std::memcpy(&ver, mapped, 4u);
            if (ver == 2142000u) {
                uint32_t count = 0u;
                uint32_t table = 0u;
                std::memcpy(&count, mapped + 4u, 4u);
                std::memcpy(&table, mapped + 8u, 4u);
                if (count > 0u && table + 8u <= entry.length) {
                    uint32_t imgHdr = 0u;
                    std::memcpy(&imgHdr, mapped + table, 4u);
                    if (imgHdr + 12u <= entry.length) {
                        uint32_t dataOff = 0u;
                        std::memcpy(&dataOff, mapped + imgHdr + 8u, 4u);
                        if (dataOff + NEBULA_TEXTURE_HEADER <= entry.length) {
                            texBytes = mapped + dataOff;
                            texSize = entry.length - dataOff;
                            rec = nebula_texture_record(texBytes);
                        }
                    }
                }
            }
        }
        if (rec == nullptr) {
            nebula_log("[SMG2-RENDER] Texture %s is not a valid NbTextur/TPL record", pakPath);
            return 0u;
        }
        NebulaTextureImage img{};
        img.bytes = texBytes;
        img.size = texSize;
        img.width = rec->width;
        img.height = rec->height;
        img.levels = rec->levels;
        img.format = NEBULA_TEXTURE_IMAGE;
        const uint32_t texId = host->texture_register(host->user, &img);
        if (texId == 0u) {
            return 0u;
        }
        NebulaFrameTexture ft{};
        ft.texture = texId;
        ft.min_linear = 1u;
        ft.mag_linear = 1u;
        return host->texture_table(host->user, &ft, 1u);
    }

    bool sUiInitAttempted = false;
    void ensureUiResources(const NebulaHostServices* host) {
        if (sUiInitAttempted || host == nullptr || host->pipeline_register == nullptr) {
            return;
        }
        sUiInitAttempted = true;
        NebulaLayoutInput inputs[2]{};
        inputs[0].semantic = NEBULA_INPUT_COLOR0;
        inputs[0].stream = 0u;
        inputs[0].format = 2u;  // RGBA8 unorm
        inputs[0].offset = 12u;
        inputs[0].stride = sizeof(RingVertex2D);

        inputs[1].semantic = NEBULA_INPUT_COORD0;
        inputs[1].stream = 0u;
        inputs[1].format = 1u;  // float2
        inputs[1].offset = 16u;
        inputs[1].stride = sizeof(RingVertex2D);

        NebulaPipelineDesc desc{};
        desc.vertex = NEBULA_PROGRAM_SITE | (static_cast<uint64_t>(NEBULA_SHADER_J2D_PICTURE) << 1u);
        desc.pixel = NEBULA_PROGRAM_SITE | (static_cast<uint64_t>(NEBULA_SHADER_J2D_PICTURE) << 1u);
        // Blend alpha (op=1, src=4 SRC_ALPHA, dst=5 INV_SRC_ALPHA), depth test=off (COMPARE_ALWAYS), cull=none, color+alpha write
        desc.state = NEBULA_STATE_BLEND(1u, 4u, 5u) | NEBULA_STATE_DEPTH(0u, NEBULA_COMPARE_ALWAYS, 0u) |
                     NEBULA_STATE_CULL(NEBULA_CULL_NONE) | NEBULA_STATE_COLOR_WRITE | NEBULA_STATE_ALPHA_WRITE;
        desc.input_count = 2u;
        desc.position_stride = sizeof(RingVertex2D);
        sUiPipeline = host->pipeline_register(host->user, &desc, inputs);

        NebulaPipelineDesc flatDesc{};
        flatDesc.vertex = NEBULA_PROGRAM_SITE | (static_cast<uint64_t>(NEBULA_SHADER_FLAT_COLOR) << 1u);
        flatDesc.pixel = NEBULA_PROGRAM_SITE | (static_cast<uint64_t>(NEBULA_SHADER_FLAT_COLOR) << 1u);
        flatDesc.state = NEBULA_STATE_BLEND(0u, 0u, 0u) | NEBULA_STATE_DEPTH(0u, NEBULA_COMPARE_ALWAYS, 0u) |
                         NEBULA_STATE_CULL(NEBULA_CULL_NONE) | NEBULA_STATE_COLOR_WRITE | NEBULA_STATE_ALPHA_WRITE;
        flatDesc.input_count = 0u;
        flatDesc.position_stride = sizeof(RingVertex2D);
        sFlatPipeline = host->pipeline_register(host->user, &flatDesc, nullptr);

        sTexSpace = loadCookedTextureTable(host, "LayoutData/TitleLogo/timg/MyTitleSpace.tpl");
        sTexLogo = loadCookedTextureTable(host, "LayoutData/TitleLogo/timg/MyTitleLogo.tpl");
        sTexTico = loadCookedTextureTable(host, "LayoutData/TitleLogo/timg/MyTitleLogoTico.tpl");
        sTexFileFrame = loadCookedTextureTable(host, "LayoutData/FileSelect/timg/MyFileSelWinFrame.tpl");
        sTexFileButton = loadCookedTextureTable(host, "LayoutData/FileSelect/timg/MyButtonATypeSide.tpl");

        nebula_log("[SMG2-RENDER] UI resources ready: pipe=%u, space=%u, logo=%u, fileFrame=%u",
                   sUiPipeline, sTexSpace, sTexLogo, sTexFileFrame);
    }

    void pushQuad(FrameSlot& slot, uint32_t& quadIdx, float x, float y, float w, float h, uint32_t texTable, float alpha) {
        if (quadIdx >= kMaxQuads || sUiPipeline == 0u || texTable == 0u) {
            return;
        }
        const uint32_t q = quadIdx++;
        const uint8_t a8 = static_cast<uint8_t>(alpha <= 0.0f ? 0.0f : (alpha >= 1.0f ? 255.0f : alpha * 255.0f));
        const uint32_t rgba = 0x00FFFFFFu | (static_cast<uint32_t>(a8) << 24u);

        RingVertex2D* v = &slot.vertices[q * 4u];
        v[0] = {x, y, 0.0f, rgba, 0.0f, 0.0f};
        v[1] = {x + w, y, 0.0f, rgba, 1.0f, 0.0f};
        v[2] = {x + w, y + h, 0.0f, rgba, 1.0f, 1.0f};
        v[3] = {x, y + h, 0.0f, rgba, 0.0f, 1.0f};

        uint32_t* idx = &slot.indices[q * 6u];
        const uint32_t base = q * 4u;
        idx[0] = base + 0u;
        idx[1] = base + 1u;
        idx[2] = base + 2u;
        idx[3] = base + 0u;
        idx[4] = base + 2u;
        idx[5] = base + 3u;

        // MaterialConstants for j2d_picture.hlsl: konst_color, konst_alpha, black, white, params(1, 1, 0, 0)
        std::memset(slot.constRows[q], 0, sizeof(slot.constRows[q]));
        slot.constRows[q][3][0] = 1.0f;
        slot.constRows[q][3][1] = 1.0f;
        slot.constRows[q][3][2] = 1.0f;
        slot.constRows[q][3][3] = 1.0f;
        slot.constRows[q][4][0] = 1.0f;
        slot.constRows[q][4][1] = 1.0f;

        slot.constants[q].slot = NEBULA_NONE32;
        slot.constants[q].vertex_rows = 0u;
        slot.constants[q].pixel_rows = 5u;
        slot.constants[q].rows = slot.constRows[q];

        NebulaFrameItem& item = slot.items[q];
        std::memset(&item, 0, sizeof(item));
        item.mesh = sizeof(RingVertex2D);
        item.pipeline = sUiPipeline;
        item.constants = q;
        item.textures = texTable;
        item.block = 0u;
        item.ring_at = 0u;
        item.ring_count = 0u;
        item.first = q * 6u;
        item.count = 6u;
        item.slot = 100u + q;
        item.generation = 1u;
        item.part = q;
        item.matrix = 0u;
        item.view = 0u;
        item.light_set = NEBULA_NONE32;
        item.flags = NEBULA_ITEM_RING | NEBULA_ITEM_INDEXED | NEBULA_ITEM_NO_CULL | (NEBULA_CATEGORY_DRAW << NEBULA_ITEM_CATEGORY_SHIFT);
    }

    void pushSolidQuad(FrameSlot& slot, uint32_t& quadIdx, float x, float y, float w, float h, float r, float g, float b, float a) {
        if (quadIdx >= kMaxQuads || sFlatPipeline == 0u) {
            return;
        }
        const uint32_t q = quadIdx++;
        RingVertex2D* v = &slot.vertices[q * 4u];
        v[0] = {x, y, 0.0f, 0xFFFFFFFFu, 0.0f, 0.0f};
        v[1] = {x + w, y, 0.0f, 0xFFFFFFFFu, 1.0f, 0.0f};
        v[2] = {x + w, y + h, 0.0f, 0xFFFFFFFFu, 1.0f, 1.0f};
        v[3] = {x, y + h, 0.0f, 0xFFFFFFFFu, 0.0f, 1.0f};

        uint32_t* idx = &slot.indices[q * 6u];
        const uint32_t base = q * 4u;
        idx[0] = base + 0u;
        idx[1] = base + 1u;
        idx[2] = base + 2u;
        idx[3] = base + 0u;
        idx[4] = base + 2u;
        idx[5] = base + 3u;

        std::memset(slot.constRows[q], 0, sizeof(slot.constRows[q]));
        slot.constRows[q][0][0] = r;
        slot.constRows[q][0][1] = g;
        slot.constRows[q][0][2] = b;
        slot.constRows[q][0][3] = a;

        slot.constants[q].slot = NEBULA_NONE32;
        slot.constants[q].vertex_rows = 1u;
        slot.constants[q].pixel_rows = 0u;
        slot.constants[q].rows = slot.constRows[q];

        NebulaFrameItem& item = slot.items[q];
        std::memset(&item, 0, sizeof(item));
        item.mesh = sizeof(RingVertex2D);
        item.pipeline = sFlatPipeline;
        item.constants = q;
        item.textures = 0u;
        item.block = 0u;
        item.ring_at = 0u;
        item.ring_count = 0u;
        item.first = q * 6u;
        item.count = 6u;
        item.slot = 100u + q;
        item.generation = 1u;
        item.part = q;
        item.matrix = 0u;
        item.view = 0u;
        item.light_set = NEBULA_NONE32;
        item.flags = NEBULA_ITEM_RING | NEBULA_ITEM_INDEXED | NEBULA_ITEM_NO_CULL | (NEBULA_CATEGORY_DRAW << NEBULA_ITEM_CATEGORY_SHIFT);
    }
}  // namespace

void setVisualScreenState(int state, float alpha, int fileSlot, const char*) {
    sScreenState = state;
    sScreenAlpha = alpha;
    sFileSlot = fileSlot;
}

void setCameraAndPlayerPreview(float camYaw, float playerX, float playerZ) {
    sCamYaw = camYaw;
    sPlayerX = playerX;
    sPlayerZ = playerZ;
}

uint32_t registerPipelines() {
    if (sPipelinesRegistered) {
        return sRegisteredCount;
    }
    sPipelinesRegistered = true;

    const NebulaHostServices* host = nebula_host();
    if (host == nullptr || host->asset_find == nullptr || host->asset_map == nullptr || host->pipeline_register == nullptr) {
        return 0u;
    }

    NebulaAssetEntry entry{};
    if (host->asset_find(host->user, "/NebulaData/Pipelines.bin", &entry) == 0 &&
        host->asset_find(host->user, "NebulaData/Pipelines.bin", &entry) == 0) {
        nebula_log("[SMG2-RENDER] TODO(Garigari): /NebulaData/Pipelines.bin not found in game.pak");
        return 0u;
    }

    const void* mapped = host->asset_map(host->user, entry.index);
    if (mapped == nullptr || entry.length < NEBULA_PIPELINE_FILE_HEADER_BYTES) {
        return 0u;
    }

    const auto* bytes = static_cast<const uint8_t*>(mapped);
    if (std::memcmp(bytes, NEBULA_PIPELINE_FILE_MAGIC, 8u) != 0) {
        return 0u;
    }

    uint32_t count = 0u;
    std::memcpy(&count, bytes + 8u, sizeof(count));

    uint32_t at = NEBULA_PIPELINE_FILE_HEADER_BYTES;
    NebulaLayoutInput inputs[kMaxLayoutInputs]{};
    uint32_t registered = 0u;

    for (uint32_t i = 0u; i < count; ++i) {
        if (entry.length - at < NEBULA_PIPELINE_RECORD_BYTES) {
            break;
        }
        NebulaPipelineDesc desc{};
        std::memcpy(&desc.vertex, bytes + at, 8u);
        std::memcpy(&desc.pixel, bytes + at + 8u, 8u);
        std::memcpy(&desc.state, bytes + at + 16u, 4u);
        std::memcpy(&desc.input_count, bytes + at + 20u, 4u);
        std::memcpy(&desc.position_stride, bytes + at + 24u, 4u);
        at += NEBULA_PIPELINE_RECORD_BYTES;

        if ((entry.length - at) / NEBULA_LAYOUT_INPUT_BYTES < desc.input_count) {
            break;
        }
        const uint32_t copyCount = desc.input_count <= kMaxLayoutInputs ? desc.input_count : kMaxLayoutInputs;
        if (copyCount > 0u) {
            std::memcpy(inputs, bytes + at, copyCount * sizeof(NebulaLayoutInput));
        }
        at += desc.input_count * NEBULA_LAYOUT_INPUT_BYTES;

        if (desc.input_count <= kMaxLayoutInputs) {
            host->pipeline_register(host->user, &desc, inputs);
            registered++;
        }
    }

    sRegisteredCount = registered;
    nebula_log("[SMG2-RENDER] Registered %u / %u pipelines from /NebulaData/Pipelines.bin", registered, count);
    ensureUiResources(host);
    return registered;
}

void submitFrame(uint32_t frame) {
    const NebulaHostServices* host = nebula_host();
    if (host == nullptr || host->submit_frame == nullptr) {
        return;
    }

    ensureUiResources(host);

    FrameSlot& slot = sSlots[sNextSlot];
    sNextSlot = (sNextSlot + 1u) % kFrameSlotCount;

    if (slot.inUse) {
        while (!nebula_frame_list_released(&slot.list)) {
            if (host->frame_wait != nullptr) {
                host->frame_wait(host->user);
            } else {
                break;
            }
        }
    }

    std::memset(&slot.list, 0, sizeof(slot.list));
    std::memset(slot.passes, 0, sizeof(slot.passes));
    std::memset(&slot.view, 0, sizeof(slot.view));
    std::memset(&slot.camera, 0, sizeof(slot.camera));
    std::memset(&slot.block, 0, sizeof(slot.block));
    std::memset(slot.identityJoint, 0, sizeof(slot.identityJoint));

    slot.identityJoint[0][0] = 1.0f;
    slot.identityJoint[1][1] = 1.0f;
    slot.identityJoint[2][2] = 1.0f;

    slot.camera.view[0][0] = 1.0f;
    slot.camera.view[1][1] = 1.0f;
    slot.camera.view[2][2] = 1.0f;
    slot.camera.role = NEBULA_CAMERA_SCENE;

    slot.scaleFlag = 1u;
    slot.block.joints = slot.identityJoint;
    slot.block.scale_flags = &slot.scaleFlag;
    slot.block.joint_count = 1u;
    slot.block.slot = 1u;
    slot.block.generation = 1u;
    slot.block.flags = NEBULA_BLOCK_EYE_SPACE | NEBULA_BLOCK_NO_MOTION;
    slot.block.camera = NEBULA_NONE32;
    slot.block.calc_mode = 1u;
    slot.block.normals_from = NEBULA_NONE32;

    slot.view.orthographic = 1u;
    slot.view.terms[0] = 2.0f / 640.0f;
    slot.view.terms[1] = -1.0f;
    slot.view.terms[2] = -2.0f / 456.0f;
    slot.view.terms[3] = 1.0f;
    slot.view.viewport[2] = 65536.0f;
    slot.view.viewport[3] = 65536.0f;
    slot.view.viewport[5] = 1.0f;
    slot.view.scissor[2] = 65536.0f;
    slot.view.scissor[3] = 65536.0f;

    uint32_t quadCount = 0u;
    if (sScreenState == 0) {
        // Logo / Boot screen: fade SMG2 space backdrop + logo emblem
        pushQuad(slot, quadCount, 0.0f, 0.0f, 640.0f, 456.0f, sTexSpace, 0.35f * sScreenAlpha);
        pushQuad(slot, quadCount, 120.0f, 110.0f, 400.0f, 180.0f, sTexLogo, sScreenAlpha);
    } else if (sScreenState == 1) {
        // Title Screen: Space backdrop + SMG2 Logo + Tico emblem
        pushQuad(slot, quadCount, 0.0f, 0.0f, 640.0f, 456.0f, sTexSpace, 1.0f);
        pushQuad(slot, quadCount, 90.0f, 70.0f, 460.0f, 210.0f, sTexLogo, 1.0f);
        pushQuad(slot, quadCount, 288.0f, 305.0f, 64.0f, 64.0f, sTexTico, 1.0f);
        pushQuad(slot, quadCount, 220.0f, 376.0f, 200.0f, 42.0f, sTexFileButton, 1.0f);
    } else if (sScreenState == 2) {
        // File Select: Space backdrop + 3 save file frames with active slot highlight
        pushQuad(slot, quadCount, 0.0f, 0.0f, 640.0f, 456.0f, sTexSpace, 0.85f);
        for (int s = 1; s <= 3; ++s) {
            const float y = 70.0f + static_cast<float>(s - 1) * 112.0f;
            const float a = (s == sFileSlot) ? 1.0f : 0.45f;
            pushQuad(slot, quadCount, 140.0f, y, 360.0f, 92.0f, sTexFileFrame, a);
        }
    } else {
        // Sky Station Galaxy bring-up view (responds to M5 player/camera movement)
        pushQuad(slot, quadCount, 0.0f, 0.0f, 640.0f, 456.0f, sTexSpace, 0.9f);
        const float px = 288.0f + sPlayerX * 0.05f - sCamYaw * 20.0f;
        const float py = 196.0f + sPlayerZ * 0.05f;
        pushQuad(slot, quadCount, px, py, 64.0f, 64.0f, sTexTico, 1.0f);
    }

    // Fallback diagnostic bright yellow solid quad in center of screen
    pushSolidQuad(slot, quadCount, 270.0f, 390.0f, 100.0f, 24.0f, 1.0f, 0.9f, 0.1f, 1.0f);

    for (uint32_t q = 0u; q < quadCount; ++q) {
        slot.items[q].ring_count = quadCount * 4u;
    }

    // Cornflower Blue (r=0.2, g=0.4, b=0.8, a=1.0) swapchain sanity clear
    const uint32_t clearRgba = NEBULA_RGBA(51, 102, 204, 255);

    slot.passes[0].kind = NEBULA_PASS_CLEAR;
    slot.passes[0].layer = NEBULA_LAYER_SCENE;
    slot.passes[0].phase = 0xFFu;
    slot.passes[0].clear_channels = 1u | 2u | 4u;
    slot.passes[0].clear_rgba = clearRgba;
    slot.passes[0].scene_camera = NEBULA_NONE32;
    slot.passes[0].scene_view = NEBULA_NONE32;

    slot.passes[1].kind = NEBULA_PASS_SCENE_BEGIN;
    slot.passes[1].scene_camera = 0u;
    slot.passes[1].scene_view = 0u;

    slot.passes[2].kind = NEBULA_PASS_SCENE_END;
    slot.passes[2].scene_camera = 0u;
    slot.passes[2].scene_view = 0u;

    slot.passes[3].kind = NEBULA_PASS_DRAW;
    slot.passes[3].layer = NEBULA_LAYER_SCREEN;
    slot.passes[3].phase = 0xFFu;
    slot.passes[3].load = 0u;
    slot.passes[3].first_item = 0u;
    slot.passes[3].item_count = quadCount;
    slot.passes[3].scene_camera = 0u;
    slot.passes[3].scene_view = 0u;

    slot.passes[4].kind = NEBULA_PASS_SHOW;
    slot.passes[4].layer = NEBULA_LAYER_SCREEN;

    slot.list.passes = slot.passes;
    slot.list.pass_count = 5u;
    slot.list.items = quadCount > 0u ? slot.items : nullptr;
    slot.list.item_count = quadCount;
    slot.list.views = &slot.view;
    slot.list.view_count = 1u;
    slot.list.cameras = &slot.camera;
    slot.list.camera_count = 1u;
    slot.list.blocks = &slot.block;
    slot.list.block_count = 1u;
    slot.list.constants = quadCount > 0u ? slot.constants : nullptr;
    slot.list.constants_count = quadCount;
    slot.list.ring_vertices = quadCount > 0u ? slot.vertices : nullptr;
    slot.list.ring_vertex_bytes = quadCount * 4u * sizeof(RingVertex2D);
    slot.list.ring_indices = quadCount > 0u ? slot.indices : nullptr;
    slot.list.ring_index_count = quadCount * 6u;
    slot.list.flags = NEBULA_FRAME_HAS_SCENE | (frame == 0u ? NEBULA_FRAME_CAMERA_CUT : 0u);
    slot.list.camera_epoch = 1u;
    slot.list.sky_up[1] = 1.0f;
    slot.list.sky_up_known = 1u;
    slot.list.released = 0u;

    slot.inUse = true;
    host->submit_frame(host->user, &slot.list);
}

}  // namespace Smg2Render
