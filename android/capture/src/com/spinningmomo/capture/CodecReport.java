package com.spinningmomo.capture;

import android.media.MediaCodecInfo;
import android.media.MediaCodecList;
import android.media.MediaFormat;
import android.os.Build;
import org.json.JSONArray;
import org.json.JSONException;
import org.json.JSONObject;

/** 只枚举系统声明的能力，不启动编码器，也不据此判断游戏录制性能。 */
final class CodecReport {
    private CodecReport() {}

    static JSONArray collect() throws JSONException {
        JSONArray result = new JSONArray();
        MediaCodecInfo[] codecs = new MediaCodecList(MediaCodecList.ALL_CODECS).getCodecInfos();
        for (MediaCodecInfo codec : codecs) {
            if (!codec.isEncoder()) {
                continue;
            }
            for (String mime : codec.getSupportedTypes()) {
                if (!MediaFormat.MIMETYPE_VIDEO_AVC.equals(mime)
                        && !MediaFormat.MIMETYPE_VIDEO_HEVC.equals(mime)
                        && !MediaFormat.MIMETYPE_AUDIO_AAC.equals(mime)) {
                    continue;
                }
                JSONObject entry = new JSONObject();
                entry.put("name", codec.getName());
                entry.put("mime", mime);
                try {
                    // 旧系统没有这些查询 API；保留未知值，不通过名称猜测硬件能力。
                    entry.put("hardwareAccelerated", Build.VERSION.SDK_INT >= 29
                            ? codec.isHardwareAccelerated() : JSONObject.NULL);
                    entry.put("softwareOnly", Build.VERSION.SDK_INT >= 29
                            ? codec.isSoftwareOnly() : JSONObject.NULL);
                    MediaCodecInfo.CodecCapabilities caps = codec.getCapabilitiesForType(mime);
                    JSONArray profiles = new JSONArray();
                    for (MediaCodecInfo.CodecProfileLevel profile : caps.profileLevels) {
                        JSONObject item = new JSONObject();
                        item.put("profile", profile.profile);
                        item.put("level", profile.level);
                        profiles.put(item);
                    }
                    entry.put("profileLevels", profiles);
                    MediaCodecInfo.VideoCapabilities video = caps.getVideoCapabilities();
                    if (video != null) {
                        boolean surfaceInput = false;
                        for (int format : caps.colorFormats) {
                            surfaceInput |= format == MediaCodecInfo.CodecCapabilities.COLOR_FormatSurface;
                        }
                        entry.put("surfaceInput", surfaceInput);
                        entry.put("widthRange", range(video.getSupportedWidths().getLower(),
                                video.getSupportedWidths().getUpper()));
                        entry.put("heightRange", range(video.getSupportedHeights().getLower(),
                                video.getSupportedHeights().getUpper()));
                        entry.put("widthAlignment", video.getWidthAlignment());
                        entry.put("heightAlignment", video.getHeightAlignment());
                        entry.put("bitrateRange", range(video.getBitrateRange().getLower(),
                                video.getBitrateRange().getUpper()));
                        // 联合尺寸/帧率查询比独立宽高上限更有参考价值，仍不是实测帧率。
                        JSONArray targets = new JSONArray();
                        targets.put(target(video, 1920, 1080, 30));
                        targets.put(target(video, 1920, 1080, 60));
                        targets.put(target(video, 3840, 2160, 30));
                        targets.put(target(video, 3840, 2160, 60));
                        entry.put("reportedVideoTargets", targets);
                    }
                    MediaCodecInfo.AudioCapabilities audio = caps.getAudioCapabilities();
                    if (audio != null) {
                        entry.put("maxInputChannels", audio.getMaxInputChannelCount());
                        JSONArray rates = new JSONArray();
                        for (android.util.Range<Integer> rate : audio.getSupportedSampleRateRanges()) {
                            rates.put(range(rate.getLower(), rate.getUpper()));
                        }
                        entry.put("sampleRateRanges", rates);
                    }
                } catch (RuntimeException error) {
                    // 单个厂商编码器的能力查询失败，不丢失其余编码器的诊断结果。
                    entry.put("error", error.toString());
                }
                result.put(entry);
            }
        }
        return result;
    }

    private static JSONArray range(int lower, int upper) {
        return new JSONArray().put(lower).put(upper);
    }

    private static JSONObject target(MediaCodecInfo.VideoCapabilities video,
            int width, int height, int fps) throws JSONException {
        JSONObject result = new JSONObject();
        result.put("width", width);
        result.put("height", height);
        result.put("fps", fps);
        result.put("supported", video.areSizeAndRateSupported(width, height, fps));
        return result;
    }
}
