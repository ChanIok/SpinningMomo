package com.spinningmomo.capture;

import android.hardware.display.VirtualDisplay;
import android.media.MediaCodec;
import android.media.MediaCodecInfo;
import android.media.MediaCodecList;
import android.media.MediaFormat;
import android.view.Surface;

import java.nio.ByteBuffer;
import java.util.concurrent.TimeoutException;

/**
 * 屏幕视频录制器：通过 VirtualDisplay 镜像主屏内容至 MediaCodec 输入 Surface，直接输出 H.264/H.265 码流。
 */
final class ScreenRecorder {

    private final int fps;
    private final int bitrate;
    private final int codecType;
    private final String mimeType;

    private StreamSink sink;
    private long timeOriginUs;
    private MediaCodec codec;
    private Surface surface;
    private VirtualDisplay virtualDisplay;
    private DisplayWrapper.DisplaySize displaySize;

    private volatile boolean eosRequested = false;
    private volatile boolean eosReceived = false;
    private Thread drainThread;

    ScreenRecorder(int fps, int bitrate, int codecType) {
        this.fps = (fps > 0) ? fps : 60;
        this.bitrate = (bitrate > 0) ? bitrate : 16_000_000;
        this.codecType = codecType;
        this.mimeType = (codecType == 1) ? MediaFormat.MIMETYPE_VIDEO_HEVC : MediaFormat.MIMETYPE_VIDEO_AVC;
    }

    DisplayWrapper.DisplaySize getDisplaySize() {
        return displaySize;
    }

    int getFps() {
        return fps;
    }

    int getCodecType() {
        return codecType;
    }

    void prepare() throws Exception {
        displaySize = DisplayWrapper.getPrimaryDisplaySize();
        int width = displaySize.width & ~1;
        int height = displaySize.height & ~1;

        MediaCodecInfo codecInfo = selectEncoder(mimeType);
        if (codecInfo == null) {
            throw new IllegalStateException("No video encoder found for " + mimeType);
        }

        MediaFormat format = MediaFormat.createVideoFormat(mimeType, width, height);
        format.setInteger(MediaFormat.KEY_COLOR_FORMAT, MediaCodecInfo.CodecCapabilities.COLOR_FormatSurface);
        format.setInteger(MediaFormat.KEY_BIT_RATE, bitrate);
        format.setInteger(MediaFormat.KEY_FRAME_RATE, fps);
        format.setInteger(MediaFormat.KEY_I_FRAME_INTERVAL, 1);
        if (fps > 0) {
            format.setFloat("max-fps-to-encoder", fps);
        }

        codec = MediaCodec.createByCodecName(codecInfo.getName());
        codec.configure(format, null, null, MediaCodec.CONFIGURE_FLAG_ENCODE);
        surface = codec.createInputSurface();
        codec.start();

        virtualDisplay = DisplayWrapper.createVirtualDisplay(
                "momo-record", width, height, 0, surface);
    }

    byte[] fetchConfig(long timeoutMs) throws Exception {
        MediaCodec.BufferInfo bufferInfo = new MediaCodec.BufferInfo();
        long deadline = System.currentTimeMillis() + timeoutMs;
        while (System.currentTimeMillis() < deadline) {
            int outIndex = codec.dequeueOutputBuffer(bufferInfo, 100000);
            if (outIndex >= 0) {
                ByteBuffer outBuffer = codec.getOutputBuffer(outIndex);
                if ((bufferInfo.flags & MediaCodec.BUFFER_FLAG_CODEC_CONFIG) != 0) {
                    byte[] spsPps = new byte[bufferInfo.size];
                    outBuffer.position(bufferInfo.offset);
                    outBuffer.get(spsPps, 0, bufferInfo.size);
                    codec.releaseOutputBuffer(outIndex, false);
                    return spsPps;
                }
                codec.releaseOutputBuffer(outIndex, false);
            }
        }
        throw new TimeoutException("Failed to obtain video SPS/PPS within timeout");
    }

    void startDraining(StreamSink sink, long timeOriginUs) {
        this.sink = sink;
        this.timeOriginUs = timeOriginUs;
        drainThread = new Thread(this::drainOutput, "momo-video-drain");
        drainThread.start();
    }

    private void drainOutput() {
        MediaCodec.BufferInfo bufferInfo = new MediaCodec.BufferInfo();
        long drainTimeoutDeadline = -1;

        while (true) {
            try {
                int outIndex = codec.dequeueOutputBuffer(bufferInfo, 100000);
                if (outIndex >= 0) {
                    ByteBuffer outBuffer = codec.getOutputBuffer(outIndex);
                    if ((bufferInfo.flags & MediaCodec.BUFFER_FLAG_CODEC_CONFIG) != 0) {
                        codec.releaseOutputBuffer(outIndex, false);
                        continue;
                    }
                    if (bufferInfo.size > 0) {
                        byte[] sample = new byte[bufferInfo.size];
                        outBuffer.position(bufferInfo.offset);
                        outBuffer.get(sample, 0, bufferInfo.size);

                        long ptsUs = Math.max(0, bufferInfo.presentationTimeUs - timeOriginUs);
                        int flags = ((bufferInfo.flags & MediaCodec.BUFFER_FLAG_KEY_FRAME) != 0) ? 1 : 0;
                        if (sink != null) {
                            sink.sendFrame(CaptureProtocol.VIDEO_SAMPLE, 0, flags, ptsUs, sample);
                        }
                    }
                    codec.releaseOutputBuffer(outIndex, false);
                    if ((bufferInfo.flags & MediaCodec.BUFFER_FLAG_END_OF_STREAM) != 0) {
                        eosReceived = true;
                        break;
                    }
                } else if (outIndex == MediaCodec.INFO_TRY_AGAIN_LATER) {
                    if (eosRequested) {
                        if (drainTimeoutDeadline < 0) {
                            drainTimeoutDeadline = System.currentTimeMillis() + 1500;
                        } else if (System.currentTimeMillis() > drainTimeoutDeadline) {
                            break;
                        }
                    }
                }
            } catch (Exception e) {
                break;
            }
        }
    }

    static MediaCodecInfo selectEncoder(String mimeType) {
        MediaCodecList codecList = new MediaCodecList(MediaCodecList.REGULAR_CODECS);
        for (MediaCodecInfo info : codecList.getCodecInfos()) {
            if (!info.isEncoder()) {
                continue;
            }
            for (String type : info.getSupportedTypes()) {
                if (mimeType.equalsIgnoreCase(type)) {
                    return info;
                }
            }
        }
        return null;
    }

    boolean stop() {
        eosRequested = true;
        if (virtualDisplay != null) {
            try {
                virtualDisplay.release();
            } catch (Exception ignored) {
            }
            virtualDisplay = null;
        }
        if (codec != null) {
            try {
                codec.signalEndOfInputStream();
            } catch (Exception ignored) {
            }
        }
        boolean threadTerminated = false;
        if (drainThread != null) {
            try {
                drainThread.join(2000);
            } catch (InterruptedException ignored) {
            }
            if (drainThread.isAlive()) {
                drainThread.interrupt();
                try {
                    drainThread.join(500);
                } catch (InterruptedException ignored) {
                }
            }
            threadTerminated = !drainThread.isAlive();
            drainThread = null;
        } else {
            threadTerminated = true;
        }
        if (surface != null) {
            try {
                surface.release();
            } catch (Exception ignored) {
            }
            surface = null;
        }
        if (codec != null) {
            if (threadTerminated) {
                try {
                    codec.stop();
                } catch (Exception ignored) {
                }
                try {
                    codec.release();
                } catch (Exception ignored) {
                }
                codec = null;
            }
        }
        return eosReceived && threadTerminated;
    }
}
