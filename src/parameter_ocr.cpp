#include "parameter_ocr.hpp"
#include "window_utils.hpp"
#include <algorithm>
#include <numeric>
#include <cmath>

ParameterOCR::ParameterOCR(const std::wstring& model_path) 
    : env_(ORT_LOGGING_LEVEL_WARNING, "parameter_ocr"),
      input_names_{"input"},
      output_names_{"output"}
{

    // 初始化输入形状
    input_shape_ = {1, 1, input_height_, input_width_};  // [batch_size, channels, height, width]
    
    // 会话选项
    Ort::SessionOptions session_options;
    session_options.SetIntraOpNumThreads(1);
    session_options.SetGraphOptimizationLevel(GraphOptimizationLevel::ORT_ENABLE_ALL);
    
    // 创建会话
    session_ = Ort::Session(env_, model_path.c_str(), session_options);
}

ParameterOCR::Prediction ParameterOCR::predict(const std::vector<float>& image_data) {
    if (image_data.size() != input_height_ * input_width_) {
        throw std::runtime_error("输入数据大小不匹配");
    }
    
    // 创建输入tensor
    auto memory_info = Ort::MemoryInfo::CreateCpu(OrtArenaAllocator, OrtMemTypeDefault);
    std::vector<float> input_tensor_values(1 * 1 * input_height_ * input_width_);
    
    // 复制数据到正确的位置
    std::copy(image_data.begin(), image_data.end(), input_tensor_values.begin());
    
    auto input_tensor = Ort::Value::CreateTensor<float>(
        memory_info,
        input_tensor_values.data(),
        input_tensor_values.size(),
        input_shape_.data(),
        input_shape_.size()
    );
    
    // 运行推理
    auto output_tensors = session_.Run(
        Ort::RunOptions{nullptr},
        input_names_.data(),
        &input_tensor,
        1,
        output_names_.data(),
        1
    );
    
    // 获取输出
    float* output_data = output_tensors[0].GetTensorMutableData<float>();
    std::vector<float> output_vector(output_data, output_data + num_classes_);
    
    // 后处理
    return postprocess(output_vector);
}

ParameterOCR::Prediction ParameterOCR::postprocess(const std::vector<float>& output) {
    // 找到最大值及其索引
    auto max_it = std::max_element(output.begin(), output.end());
    int pred_idx = std::distance(output.begin(), max_it);
    
    // 计算softmax
    std::vector<float> softmax_output(output.size());
    float max_val = *max_it;
    float sum = 0.0f;
    
    for (size_t i = 0; i < output.size(); i++) {
        float exp_val = std::exp(output[i] - max_val);
        softmax_output[i] = exp_val;
        sum += exp_val;
    }
    
    for (float& val : softmax_output) {
        val /= sum;
    }
    
    // 获取预测结果
    Prediction pred;
    pred.confidence = softmax_output[pred_idx];
    
    if (pred_idx <= 100) {
        // 百分比值
        pred.value = std::to_string(pred_idx) + "%";
        pred.type = "percentage";
    } else {
        // 小数值
        float decimal_value = (pred_idx - 101) / 10.0f - 1.0f;
        std::stringstream ss;
        ss << std::fixed << std::setprecision(1) << decimal_value;
        pred.value = ss.str();
        pred.type = "decimal";
    }
    
    return pred;
}

std::vector<float> ParameterOCR::preprocess_image(IWICBitmapSource* image) {
    // 获取图像尺寸
    UINT width = 0, height = 0;
    image->GetSize(&width, &height);
    
    // 读取图像数据
    std::vector<BYTE> buffer(width * height);
    HRESULT hr = image->CopyPixels(nullptr, width, width * height, buffer.data());
    if (FAILED(hr)) {
        throw std::runtime_error("无法读取图像数据");
    }
    
    // 转换为float并归一化
    std::vector<float> float_data;
    float_data.reserve(buffer.size());
    for (BYTE val : buffer) {
        float_data.push_back(static_cast<float>(val) / 255.0f);
    }
    
    return float_data;
}
