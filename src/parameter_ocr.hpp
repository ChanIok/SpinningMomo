#pragma once

#include <string>
#include <vector>
#include <memory>
#include <sstream>
#include <iomanip>
#include <onnxruntime_cxx_api.h>
#include <wincodec.h>

class ParameterOCR {
public:
    // 构造函数
    ParameterOCR(const std::wstring& model_path);
    
    // 预测结果结构
    struct Prediction {
        std::string value;      // 预测值
        std::string type;       // 类型（百分比或小数）
        float confidence;       // 置信度
    };
    
    // 预测函数
    Prediction predict(const std::vector<float>& image_data);
    
    // 图像预处理
    std::vector<float> preprocess_image(IWICBitmapSource* image);
    
private:
    // 后处理
    Prediction postprocess(const std::vector<float>& output);
    
    // 常量
    const int input_height_ = 10;
    const int input_width_ = 24;
    const int num_classes_ = 122;  // 101类百分比 + 21类小数
    
    // ONNX Runtime相关
    Ort::Env env_;
    Ort::Session session_{nullptr};
    Ort::AllocatorWithDefaultOptions allocator_;
    
    // 输入输出信息
    std::vector<const char*> input_names_;
    std::vector<const char*> output_names_;
    std::vector<int64_t> input_shape_;
};
