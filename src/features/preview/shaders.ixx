module;

export module Features.Preview.Shaders;

import std;

export namespace Features::Preview::Shaders {

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

// 视口框渲染着色器（用于显示当前可视区域框架）
const std::string VIEWPORT_VERTEX_SHADER = R"(
struct VS_INPUT {
    float2 pos : POSITION;
    float4 color : COLOR;
};

struct PS_INPUT {
    float4 pos : SV_POSITION;
    float4 color : COLOR;
};

PS_INPUT main(VS_INPUT input) {
    PS_INPUT output;
    output.pos = float4(input.pos.x * 2 - 1, -(input.pos.y * 2 - 1), 0, 1);
    output.color = input.color;
    return output;
}
)";

const std::string VIEWPORT_PIXEL_SHADER = R"(
struct PS_INPUT {
    float4 pos : SV_POSITION;
    float4 color : COLOR;
};

float4 main(PS_INPUT input) : SV_Target {
    return input.color;
}
)";

}  // namespace Features::Preview::Shaders