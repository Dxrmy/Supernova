#include "program.hlsli"
#include "program_lit.hlsli"
cbuffer MaterialPixel : register(b2) {
    float4 m[8];
};
#define NEBULA_MOTION_CURRENT_INDEX 6
#define NEBULA_MOTION_PREVIOUS_INDEX 7
struct Varyings {
    PER_SAMPLE float3 lit_eye : TEXCOORD3;
    PER_SAMPLE float3 lit_nrm : TEXCOORD4;
    nointerpolation uint lit_light : TEXCOORD5;
    float4 pos : SV_Position;
    PER_SAMPLE float4 color : COLOR0;
    PER_SAMPLE float2 uv0 : TEXCOORD0;
    PER_SAMPLE float3 uv1 : TEXCOORD1;
    PER_SAMPLE float2 uv2 : TEXCOORD2;
    VARYINGS_MOTION
};
NativeOut program_ps(Varyings i) {
    g_enh = enh_begin(i.lit_eye, i.lit_nrm);
    float4 lit0 = float4(0.0, 0.0, 0.0, 0.0);
    float4 lit1 = float4(0.0, 0.0, 0.0, 0.0);
    if (enh_on()) {
        {
            const float3 enh_mat = i.color.rgb;
            const float3 enh_amb = m[7].rgb;
            const float3 enh_amb_lin = enh_lin(enh_amb);
            float3 enh_acc = AMBIENT_TERM(enh_amb_lin * enh_sky(g_enh));
            enh_add_light(i.lit_light, 0u, true, 2u, true, true, i.lit_eye, enh_amb, enh_amb_lin, enh_acc);
            enh_add_light(i.lit_light, 1u, true, 2u, false, true, i.lit_eye, enh_amb, enh_amb_lin, enh_acc);
            enh_acc *= enh_sky_reflection(g_enh, enh_amb_lin);
            lit0.rgb = enh_mat * enh_srgb(enh_shoulder(max(enh_acc, 0.0)));
        }
        lit0.a = i.color.a * saturate((m[7].a * 255.0 + light_term(i.lit_light, 2u, 3u, 2u, i.lit_eye, g_enh.N).a) / 255.0);
    } else {
        lit0.rgb = i.color.rgb * saturate((AMBIENT_TERM(m[7].rgb) * 255.0 + light_term(i.lit_light, 0u, 3u, 2u, i.lit_eye, g_enh.N).rgb + light_term(i.lit_light, 1u, 3u, 2u, i.lit_eye, g_enh.N).rgb) / 255.0);
        lit0.a = i.color.a * saturate((AMBIENT_TERM(m[7].a) * 255.0 + light_term(i.lit_light, 2u, 3u, 2u, i.lit_eye, g_enh.N).a) / 255.0);
    }
    i.color = lit0;
    float4 prev = float4(1.0, 1.0, 1.0, 1.0);
    float4 r0 = m[0];
    float4 r1 = m[1];
    float2 tevcoord = float2(0.0, 0.0);
    const float3 ind0 = round(TEX(0).Sample(SAMP(0), i.uv0 / float2(1.0, 1.0)).abg * 255.0);
    {
        const float4 t = TEX(1).Sample(SAMP(1), (i.uv1.z != 0.0 ? i.uv1.xy / i.uv1.z : i.uv1.xy)).rgba;
        const float4 ras = i.color.rgba;
        const float3 k = m[2].rgb;
        const float3 rc = saturate((float3(0.0, 0.0, 0.0) + lerp(k.rgb, r0.rgb, ras.rgb)));
        const float ra = saturate((r0.a + lerp(t.a, r1.a, ras.a)) * 4.0);
        prev.rgb = rc;
        prev.a = ra;
        NATIVE_STAGE(0, float4(rc, ra))
    }
    {
        float2 texel = i.uv2 * m[3].xy;
        float3 sv = floor(ind0 / 1.0);
        sv.x += -128.0;
        sv.y += -128.0;
        sv.z += -128.0;
        const float2 off = float2(dot(m[4].xyz, sv), dot(m[5].xyz, sv));
        tevcoord = texel + off;
        const float4 t = ENV_FETCH(enh_env(TEX(2).Sample(SAMP(2), tevcoord / m[3].zw).rgba));
        const float ka = 1.0;
        const float3 rc = saturate((prev.aaa + lerp(r1.rgb, t.rgb, prev.rgb)));
        const float ra = saturate((0.0 + lerp(0.0, ka, ka)));
        prev.rgb = rc;
        prev.a = ra;
        NATIVE_STAGE(1, float4(rc, ra))
    }
    {
        const float4 t = TEX(1).Sample(SAMP(1), (i.uv1.z != 0.0 ? i.uv1.xy / i.uv1.z : i.uv1.xy)).rgba;
        const float3 rc = saturate((prev.rgb + lerp(float3(0.0, 0.0, 0.0), float3(0.0, 0.0, 0.0), float3(0.0, 0.0, 0.0))));
        const float ra = (0.0 + lerp(t.a, 0.0, 0.0));
        prev.rgb = rc;
        prev.a = ra;
        NATIVE_STAGE(2, float4(rc, ra))
    }
    prev = float4(prev.rgb, prev.a);
    const float a8 = round(prev.a * 255.0);
    if (!((a8 >= round(m[6].x * 255.0)) && (a8 <= round(m[6].y * 255.0)))) {
        discard;
    }
    if (enh_on()) prev.rgb = enh_finish(prev.rgb, g_enh);
    PROGRAM_RETURN(float4(max(prev.rgb, 0.0), saturate(prev.a)));
}
