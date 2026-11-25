#include "program.hlsli"
#include "program_lit.hlsli"
cbuffer MaterialPixel : register(b2) {
    float4 m[8];
};
#define NEBULA_MOTION_CURRENT_INDEX 5
#define NEBULA_MOTION_PREVIOUS_INDEX 6
struct Varyings {
    PER_SAMPLE float3 lit_eye : TEXCOORD2;
    PER_SAMPLE float3 lit_nrm : TEXCOORD3;
    nointerpolation uint lit_light : TEXCOORD4;
    float4 pos : SV_Position;
    PER_SAMPLE float4 color : COLOR0;
    PER_SAMPLE float2 uv0 : TEXCOORD0;
    PER_SAMPLE float2 uv1 : TEXCOORD1;
    VARYINGS_MOTION
};
NativeOut program_ps(Varyings i) {
    g_enh = enh_begin(i.lit_eye, i.lit_nrm);
    if (enh_on()) enh_key_highlight(i.lit_light, g_enh, i.lit_eye);
    float4 prev = float4(1.0, 1.0, 1.0, 1.0);
    float4 r0 = m[0];
    float2 tevcoord = float2(0.0, 0.0);
    const float3 ind0 = round(TEX(0).Sample(SAMP(0), i.uv0 / float2(1.0, 1.0)).abg * 255.0);
    {
        const float4 ras = i.color.rgba;
        const float3 k = m[1].rgb;
        const float ka = m[1].a;
        const float3 rc = saturate((float3(0.0, 0.0, 0.0) + lerp(k.rgb, r0.rgb, ras.rgb) - 128.0 / 255.0) * 2.0);
        const float ra = saturate((0.0 + lerp(ka, 0.0, ras.a)) * 2.0);
        prev.rgb = rc;
        prev.a = ra;
        NATIVE_STAGE(0, float4(rc, ra))
    }
    {
        float2 texel = i.uv1 * m[2].xy;
        float3 sv = floor(ind0 / 1.0);
        sv.x += -128.0;
        sv.y += -128.0;
        sv.z += -128.0;
        const float2 off = float2(dot(m[3].xyz, sv), dot(m[4].xyz, sv));
        tevcoord = texel + off;
        const float4 t = HIGHLIGHT_FETCH(TEX(1).Sample(SAMP(1), tevcoord / m[2].zw).rgba);
        const float4 ras = i.color.rgba;
        const float3 k = m[5].rgb;
        const float3 rc = saturate((prev.rgb + lerp(float3(0.0, 0.0, 0.0), k.rgb, t.rgb)));
        const float ra = (ras.a + lerp(0.0, 0.0, 0.0));
        prev.rgb = rc;
        prev.a = ra;
        NATIVE_STAGE(1, float4(rc, ra))
    }
    prev = float4(prev.rgb, prev.a);
    if (enh_on()) prev.rgb = enh_finish(prev.rgb, g_enh);
    prev.rgb = program_fog_curve(prev.rgb, m[7].rgb, fog_amount(i.pos.w, m[6].x, m[6].y), float4(m[6].x, m[6].y, i.pos.w, 2.0));
    PROGRAM_RETURN(float4(max(prev.rgb, 0.0), saturate(prev.a)));
}
