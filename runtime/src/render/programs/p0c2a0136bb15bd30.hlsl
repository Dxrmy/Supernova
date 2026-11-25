#include "program.hlsli"
cbuffer MaterialPixel : register(b2) {
    float4 m[4];
};
#define NEBULA_MOTION_CURRENT_INDEX 2
#define NEBULA_MOTION_PREVIOUS_INDEX 3
struct Varyings {
    float4 pos : SV_Position;
    PER_SAMPLE float4 color : COLOR0;
    PER_SAMPLE float3 uv0 : TEXCOORD0;
    PER_SAMPLE float2 uv1 : TEXCOORD1;
    VARYINGS_MOTION
};
NativeOut program_ps(Varyings i) {
    float4 prev = float4(1.0, 1.0, 1.0, 1.0);
    float4 r0 = m[0];
    float4 r1 = m[1];
    float4 r2 = float4(0.0, 0.0, 0.0, 0.0);
    {
        const float4 t = TEX(0).Sample(SAMP(0), (i.uv0.z != 0.0 ? i.uv0.xy / i.uv0.z : i.uv0.xy)).rgba;
        const float4 ras = i.color.rgba;
        const float3 k = m[2].rgb;
        const float3 rc = saturate((float3(0.0, 0.0, 0.0) + lerp(k.rgb, r0.rgb, t.rgb)));
        const float ra = saturate((0.0 + lerp(0.0, t.a, ras.a)));
        r2.rgb = rc;
        prev.a = ra;
        NATIVE_STAGE(0, float4(rc, ra))
    }
    {
        const float4 t = TEX(1).Sample(SAMP(1), i.uv1).rgba;
        const float4 ras = i.color.rgba;
        const float3 k = m[3].rgb;
        const float3 rc = saturate((ras.rgb + lerp(k.rgb, t.rgb, ras.rgb)));
        const float ra = (0.0 + lerp(0.0, 0.0, ras.a));
        prev.rgb = rc;
        prev.a = ra;
        NATIVE_STAGE(1, float4(rc, ra))
    }
    {
        const float4 ras = i.color.rgba;
        const float3 rc = (float3(0.0, 0.0, 0.0) + lerp(prev.rgb, r2.rgb, low8(r0.aaa)));
        const float ra = saturate((r1.a + lerp(0.0, low8(r1.a), ras.a)));
        prev.rgb = rc;
        prev.a = ra;
        NATIVE_STAGE(2, float4(rc, ra))
    }
    prev = low8(float4(prev.rgb, prev.a));
    PROGRAM_RETURN(saturate(prev));
}
