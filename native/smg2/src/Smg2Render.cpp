// SMG2 Render / FrameList & LayoutManager bridge.
// Wires Petari's native FrameList, Ui::Layout (NLYT/NLAN/NFNT), and LayoutManager
// directly to Nebula's D3D12 host renderer.

#include <nebula_game_api.h>
#include <nebula_module.h>

class NebulaArena {
public:
    NebulaArena* makeCurrent();
};
namespace Arenas {
    void create();
    NebulaArena* permanent();
}
namespace Frame {
    void registerPipelineList();
    void initDefaults();
}
namespace GameFrame {
    void begin();
    void end();
}

extern "C" const NebulaHostServices* nebula_host(void);

namespace Smg2Render {

namespace {
    bool sInitialized = false;
}  // namespace

uint32_t registerPipelines() {
    if (!sInitialized) {
        sInitialized = true;
        Arenas::create();
        Arenas::permanent()->makeCurrent();
        const NebulaHostServices* host = nebula_host();
        if (host != nullptr && host->submit_frame != nullptr && host->pipeline_register != nullptr) {
            Frame::registerPipelineList();
            Frame::initDefaults();
            nebula_log("[SMG2-RENDER] Arenas + FrameList pipelines & defaults initialized");
        }
    }
    return 1u;
}

void beginFrame() {
    const NebulaHostServices* host = nebula_host();
    if (host == nullptr || host->submit_frame == nullptr) {
        return;
    }
    if (!sInitialized) {
        registerPipelines();
    }
    GameFrame::begin();
}

void endFrame() {
    const NebulaHostServices* host = nebula_host();
    if (host == nullptr || host->submit_frame == nullptr) {
        return;
    }
    GameFrame::end();
}

}  // namespace Smg2Render
