#include "program.hlsli"
cbuffer MaterialPixel : register(b2) {
    float4 m[7];
};
#define NEBULA_MOTION_CURRENT_INDEX 5
#define NEBULA_MOTION_PREVIOUS_INDEX 6
struct Varyings {
    float4 pos : SV_Position;
    PER_SAMPLE float4 color : COLOR0;
    PER_SAMPLE float2 uv0 : TEXCOORD0;
    PER_SAMPLE float2 uv1 : TEXCOORD1;
    PER_SAMPLE float2 uv2 : TEXCOORD2;
    PER_SAMPLE float2 uv3 : TEXCOORD3;
    PER_SAMPLE float2 uv4 : TEXCOORD4;
    VARYINGS_MOTION
};
NativeOut program_ps(Varyings i) {
    float4 prev = float4(1.0, 1.0, 1.0, 1.0);
    float4 r0 = m[0];
    float4 r1 = m[1];
    float2 tevcoord = float2(0.0, 0.0);
    const float3 ind0 = round(TEX(0).Sample(SAMP(0), i.uv0 / float2(1.0, 1.0)).abg * 255.0);
    {
        const float4 t = TEX(1).Sample(SAMP(1), i.uv1).rgba;
        const float4 ras = i.color.rgba;
        const float3 k = m[2].rgb;
        const float3 rc = saturate((t.rgb + lerp(float3(0.0, 0.0, 0.0), k.rgb, ras.aaa)));
        const float ra = saturate((0.0 + lerp(0.0, ras.a, t.a)));
        prev.rgb = rc;
        prev.a = ra;
        NATIVE_STAGE(0, float4(rc, ra))
    }
    {
        const float4 t = TEX(2).Sample(SAMP(2), i.uv2).rgba;
        const float3 rc = saturate((float3(0.0, 0.0, 0.0) + lerp(float3(0.0, 0.0, 0.0), prev.rgb, t.rgb)) * 2.0);
        const float ra = saturate((0.0 + lerp(0.0, prev.a, t.a)) * 2.0);
        prev.rgb = rc;
        prev.a = ra;
        NATIVE_STAGE(1, float4(rc, ra))
    }
    {
        const float4 t = ENV_FETCH(TEX(3).Sample(SAMP(3), i.uv3).rgba);
        const float3 rc = saturate(prev.rgb + ((round((prev.rgb).r * 255.0)) > (round((r0.aaa).r * 255.0)) ? t.rgb : float3(0.0, 0.0, 0.0)));
        const float ra = saturate((prev.a + lerp(0.0, 0.0, 0.0)) * 2.0);
        prev.rgb = rc;
        prev.a = ra;
        NATIVE_STAGE(2, float4(rc, ra))
    }
    {
        float2 texel = i.uv4 * m[3].xy;
        float3 sv = floor(ind0 / 1.0);
        sv.x += -128.0;
        sv.y += -128.0;
        sv.z += -128.0;
        const float2 off = float2(dot(m[4].xyz, sv), dot(m[5].xyz, sv));
        tevcoord = texel + off;
        const float4 t = TEX(4).Sample(SAMP(4), tevcoord / m[3].zw).rgba;
        const float3 k = m[6].rgb;
        const float ka = m[6].a;
        const float3 rc = saturate((r1.rgb + lerp(t.rgb, k.rgb, prev.rgb)) * 2.0);
        const float ra = saturate((t.a + lerp(0.0, ka, prev.a)));
        prev.rgb = rc;
        prev.a = ra;
        NATIVE_STAGE(3, float4(rc, ra))
    }
    prev = float4(prev.rgb, prev.a);
    PROGRAM_RETURN(saturate(prev));
}
