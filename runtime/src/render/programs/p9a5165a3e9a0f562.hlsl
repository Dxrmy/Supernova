#include "program.hlsli"
cbuffer MaterialPixel : register(b2) {
    float4 m[4];
};
#define NEBULA_MOTION_CURRENT_INDEX 2
#define NEBULA_MOTION_PREVIOUS_INDEX 3
struct Varyings {
    float4 pos : SV_Position;
    PER_SAMPLE float3 uv0 : TEXCOORD0;
    PER_SAMPLE float2 uv1 : TEXCOORD1;
    VARYINGS_MOTION
};
NativeOut program_ps(Varyings i) {
    float4 prev = float4(1.0, 1.0, 1.0, 1.0);
    float4 r0 = m[0];
    {
        const float4 t = TEX(0).Sample(SAMP(0), (i.uv0.z != 0.0 ? i.uv0.xy / i.uv0.z : i.uv0.xy)).rgba;
        const float3 k = m[1].rgb;
        const float ka = m[2].a;
        const float3 rc = saturate((float3(0.0, 0.0, 0.0) + lerp(r0.rgb, k.rgb, t.rgb)));
        const float ra = (0.0 + lerp(0.0, t.a, ka));
        prev.rgb = rc;
        prev.a = ra;
        NATIVE_STAGE(0, float4(rc, ra))
    }
    {
        const float4 t = TEX(1).Sample(SAMP(1), i.uv1).rgba;
        const float3 k = m[3].rgb;
        const float ka = m[2].a;
        const float3 rc = saturate((prev.rgb + lerp(float3(0.0, 0.0, 0.0), t.rgb, k.rgb)));
        const float ra = (prev.a + lerp(t.a, 0.0, ka));
        prev.rgb = rc;
        prev.a = ra;
        NATIVE_STAGE(1, float4(rc, ra))
    }
    prev = low8(float4(prev.rgb, prev.a));
    PROGRAM_RETURN(saturate(prev));
}
