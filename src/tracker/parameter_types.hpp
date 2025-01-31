#pragma once
#include <array>

// 参数类型枚举
enum class ParameterType {
    Vignette = 0,            // 晕影
    SoftLightIntensity = 1,  // 柔光强度
    SoftLightRange = 2,      // 柔光范围
    Brightness = 3,          // 亮度
    Exposure = 4,            // 曝光
    Contrast = 5,            // 对比度
    Saturation = 6,          // 饱和度
    NaturalSaturation = 7,   // 自然饱和度
    Highlights = 8,          // 高光
    Shadows = 9              // 阴影
};

// 参数值结构体
struct ParameterValue {
    float value = 0.0f;      // 参数值
    float confidence = 0.0f;  // 置信度
    bool is_valid = false;    // 是否有效
};

// 参数存储结构体
struct Parameters {
    std::array<ParameterValue, 10> values;  // 固定大小为10的参数数组

    // 便捷访问方法
    ParameterValue& operator[](ParameterType type) {
        return values[static_cast<size_t>(type)];
    }
    
    const ParameterValue& operator[](ParameterType type) const {
        return values[static_cast<size_t>(type)];
    }
}; 