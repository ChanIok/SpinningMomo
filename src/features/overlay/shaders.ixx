module;

export module Features.Overlay.Shaders;

import std;

export namespace Features::Overlay::Shaders {

// 基本渲染着色器（用于显示捕获的游戏画面）
const std::string BASIC_VERTEX_SHADER = R"(
struct VS_INPUT {
    float2 pos : POSITION;
    float2 tex : TEXCOORD;
};
struct PS_INPUT {
    float4 pos : SV_POSITION;
    float2 tex : TEXCOORD;
};
PS_INPUT main(VS_INPUT input) {
    PS_INPUT output;
    output.pos = float4(input.pos, 0.0f, 1.0f);
    output.tex = input.tex;
    return output;
}
)";

const std::string BASIC_PIXEL_SHADER = R"(
Texture2D tex : register(t0);
SamplerState samp : register(s0);
float4 main(float4 pos : SV_POSITION, float2 texCoord : TEXCOORD) : SV_Target {
    return tex.Sample(samp, texCoord);
}
)";

}  // namespace Features::Overlay::Shaders