module;

export module Utils.Graphics.CaptureRegion;

import std;
import <d3d11.h>;
import <wil/com.h>;
import <windows.h>;

namespace Utils::Graphics::CaptureRegion {

// 捕获区域（以窗口纹理左上角为原点）
export struct CropRegion {
  UINT left = 0;
  UINT top = 0;
  UINT width = 0;
  UINT height = 0;
};

// 基于当前捕获纹理尺寸，计算目标窗口客户区在窗口纹理中的区域。
// 客户区宽高按偶数向下对齐（与录制模块 calculate_capture_dimensions
// 一致），便于与视频编码尺寸一致。
export auto calculate_client_crop_region(HWND target_window, UINT texture_width,
                                         UINT texture_height)
    -> std::expected<CropRegion, std::string>;

// 将源纹理按指定区域裁剪到输出纹理（输出纹理可复用）
export auto crop_texture_to_region(ID3D11Device* device, ID3D11DeviceContext* context,
                                   ID3D11Texture2D* source_texture, const CropRegion& region,
                                   wil::com_ptr<ID3D11Texture2D>& output_texture)
    -> std::expected<ID3D11Texture2D*, std::string>;

}  // namespace Utils::Graphics::CaptureRegion
