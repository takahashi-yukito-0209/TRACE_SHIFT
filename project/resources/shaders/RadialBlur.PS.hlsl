#include "Fullscreen.hlsli"

Texture2D<float4> gTexture : register(t0);
SamplerState gSampler : register(s0);

cbuffer RadialBlurSettings : register(b0)
{
    uint gKernelSize;
    uint gDirection;
    float gSigma;
    float gOutlineStrength;
    float2 gBlurCenter;
    float gBlurWidth;
    uint gSampleCount;
    float gScreenScale;
    float gGrayscaleAmount;
};

struct PixelShaderOutput
{
    float4 color : SV_TARGET;
};

/// <summary>
/// 画面外を黒として扱い、中心への収束時に端の色が引き伸ばされるのを防ぐ。
/// </summary>
float3 SampleScreen(float2 texcoord)
{
    if (any(texcoord < 0.0f) || any(texcoord > 1.0f)) {
        return float3(0.0f, 0.0f, 0.0f);
    }
    return gTexture.Sample(gSampler, texcoord).rgb;
}

/// <summary>
/// 中心への収束、放射状ブラー、白黒混合を適用する。
/// </summary>
PixelShaderOutput main(VertexShaderOutput input)
{
    uint sampleCount =
        clamp(gSampleCount, 1u, 32u); // 安全な範囲へ制限したサンプル数
    float2 scaledTexcoord = gBlurCenter +
        (input.texcoord - gBlurCenter) / max(gScreenScale, 0.1f); // 画面倍率を反映した参照UV
    float2 direction =
        scaledTexcoord - gBlurCenter; // 中心から現在のUVへの方向
    float3 accumulatedColor =
        float3(0.0f, 0.0f, 0.0f); // サンプル色の合計

    for (uint sampleIndex = 0; sampleIndex < sampleCount; ++sampleIndex)
    {
        float2 sampleTexcoord =
            scaledTexcoord -
            direction * gBlurWidth * float(sampleIndex); // 中心方向へずらしたUV
        accumulatedColor +=
            SampleScreen(sampleTexcoord);
    }

    float3 blurredColor = accumulatedColor / float(sampleCount); // ブラー適用後の色
    float luminance = dot(blurredColor, float3(0.2126f, 0.7152f, 0.0722f)); // 白黒混合に使う輝度

    PixelShaderOutput output; // 最終的な画面色
    output.color = float4(
        lerp(blurredColor, luminance.xxx, saturate(gGrayscaleAmount)),
        1.0f);
    return output;
}
