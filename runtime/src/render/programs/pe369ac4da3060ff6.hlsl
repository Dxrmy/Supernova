#include "program.hlsli"
#include "program_lit.hlsli"
cbuffer MaterialPixel : register(b2) {
    float4 m[10];
};
#define NEBULA_MOTION_CURRENT_INDEX 7
#define NEBULA_MOTION_PREVIOUS_INDEX 8
struct Varyings {
    PER_SAMPLE float3 lit_eye : TEXCOORD4;
    PER_SAMPLE float3 lit_nrm : TEXCOORD5;
    nointerpolation uint lit_light : TEXCOORD6;
    float4 pos : SV_Position;
    PER_SAMPLE float4 color : COLOR0;
    PER_SAMPLE float2 uv0 : TEXCOORD0;
    PER_SAMPLE float3 uv1 : TEXCOORD1;
    PER_SAMPLE float2 uv2 : TEXCOORD2;
    PER_SAMPLE float2 uv3 : TEXCOORD3;
    VARYINGS_MOTION
};
NativeOut program_ps(Varyings i) {
    g_enh = enh_begin(i.lit_eye, i.lit_nrm);
    if (enh_on()) enh_key_highlight(i.lit_light, g_enh, i.lit_eye);
    float4 prev = float4(1.0, 1.0, 1.0, 1.0);
    float4 r0 = m[0];
    float4 r1 = m[1];
    float4 r2 = m[2];
    float2 tevcoord = float2(0.0, 0.0);
    const float3 ind0 = round(TEX(0).Sample(SAMP(0), i.uv0 / float2(1.0, 1.0)).abg * 255.0);
    {
        float2 texel = (i.uv1.z != 0.0 ? i.uv1.xy / i.uv1.z : i.uv1.xy) * m[3].xy;
        float3 sv = floor(ind0 / 1.0);
        sv.x += -128.0;
        sv.y += -128.0;
        sv.z += -128.0;
        const float2 off = float2(dot(m[4].xyz, sv), dot(m[5].xyz, sv));
        tevcoord = texel + off;
        const float4 t = TEX(1).Sample(SAMP(1), tevcoord / m[3].zw).rgba;
        const float4 ras = i.color.rgba;
        const float3 k = m[6].rgb;
        const float3 rc = saturate((ras.rgb + lerp(float3(0.0, 0.0, 0.0), k.rgb, t.rgb)));
        const float ra = saturate((0.0 + lerp(0.0, t.a, ras.a)));
        prev.rgb = rc;
        prev.a = ra;
        NATIVE_STAGE(0, float4(rc, ra))
    }
    {
        float2 texel = i.uv2 * m[7].xy;
        float3 sv = floor(ind0 / 1.0);
        sv.x += -128.0;
        sv.y += -128.0;
        sv.z += -128.0;
        const float2 off = float2(dot(m[4].xyz, sv), dot(m[5].xyz, sv));
        tevcoord = texel + off;
        const float4 t = ENV_FETCH(enh_env(TEX(2).Sample(SAMP(2), tevcoord / m[7].zw).rgba));
        const float4 ras = i.color.rgba;
        const float3 k = m[8].rgb;
        const float3 rc = saturate((prev.rgb + lerp(low8(r2.rgb), k.rgb, t.rgb)));
        const float ra = (0.0 + lerp(ras.a, 0.0, 0.0));
        prev.rgb = rc;
        prev.a = ra;
        NATIVE_STAGE(1, float4(rc, ra))
    }
    {
        const float4 t = TEX(3).Sample(SAMP(3), i.uv3).rgba;
        const float3 rc = saturate(prev.rgb + ((round((low8(r0.aaa)).r * 255.0)) > (round((t.rgb).r * 255.0)) ? r0.rgb : float3(0.0, 0.0, 0.0)));
        const float ra = saturate((0.0 + lerp(0.0, low8(r1.a), t.a)));
        prev.rgb = rc;
        prev.a = ra;
        NATIVE_STAGE(2, float4(rc, ra))
    }
    prev = float4(prev.rgb, prev.a);
    const float a8 = round(prev.a * 255.0);
    if (!((a8 >= round(m[9].x * 255.0)) && (a8 <= round(m[9].y * 255.0)))) {
        discard;
    }
    if (enh_on()) prev.rgb = enh_finish(prev.rgb, g_enh);
    PROGRAM_RETURN(float4(max(prev.rgb, 0.0), saturate(prev.a)));
}
