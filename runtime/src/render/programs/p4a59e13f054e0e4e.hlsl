#include "program.hlsli"
cbuffer MaterialPixel : register(b2) {
    float4 m[5];
};
#define NEBULA_MOTION_CURRENT_INDEX 2
#define NEBULA_MOTION_PREVIOUS_INDEX 3
struct Varyings {
    float4 pos : SV_Position;
    PER_SAMPLE float4 color : COLOR0;
    PER_SAMPLE float2 uv0 : TEXCOORD0;
    PER_SAMPLE float2 uv1 : TEXCOORD1;
    VARYINGS_MOTION
};
NativeOut program_ps(Varyings i) {
    float4 prev = float4(1.0, 1.0, 1.0, 1.0);
    float4 r0 = m[0];
    float4 r1 = m[1];
    {
        const float4 t = TEX(0).Sample(SAMP(0), i.uv0).rgba;
        const float4 ras = i.color.rgba;
        const float3 rc = saturate((t.rgb + lerp(r0.rgb, r1.rgb, ras.rgb) - 128.0 / 255.0));
        const float ra = saturate((0.0 + lerp(0.0, t.a, ras.a)));
        prev.rgb = rc;
        prev.a = ra;
        NATIVE_STAGE(0, float4(rc, ra))
    }
    {
        const float4 t = ENV_FETCH(TEX(1).Sample(SAMP(1), i.uv1).rgba);
        const float4 ras = i.color.rgba;
        const float3 k = m[2].rgb;
        const float3 rc = saturate((prev.rgb + lerp(float3(0.0, 0.0, 0.0), t.rgb, k.rgb)));
        const float ra = (0.0 + lerp(ras.a, 0.0, 0.0));
        prev.rgb = rc;
        prev.a = ra;
        NATIVE_STAGE(1, float4(rc, ra))
    }
    prev = float4(prev.rgb, prev.a);
    prev.rgb = program_fog_curve(prev.rgb, m[4].rgb, fog_amount(i.pos.w, m[3].x, m[3].y), float4(m[3].x, m[3].y, i.pos.w, 2.0));
    PROGRAM_RETURN(saturate(prev));
}
