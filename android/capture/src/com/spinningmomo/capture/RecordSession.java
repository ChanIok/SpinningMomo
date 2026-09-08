package com.spinningmomo.capture;

import android.net.LocalSocket;

import java.io.ByteArrayOutputStream;
import java.io.DataInputStream;
import java.io.DataOutputStream;
import java.io.IOException;
import java.nio.ByteBuffer;
import java.nio.charset.StandardCharsets;

/**
 * 独立的 ADB 录制会话实体：统管单次录制的完整生命周期、音视频编码器及单条全双工 Socket。
 */
final class RecordSession {

    private final LocalSocket socket;
    private final Object writeLock = new Object();
    private DataOutputStream output;
    private DataInputStream input;

    private ScreenRecorder screenRecorder;
    private AudioEncoder audioEncoder;
    private volatile boolean isRunning = false;

    RecordSession(LocalSocket socket) {
        this.socket = socket;
    }

    static void handle(LocalSocket client) {
        RecordSession session = new RecordSession(client);
        try {
            session.run();
        } catch (Throwable t) {
            System.err.println("RecordSession error: " + t.getMessage());
            try {
                session.sendError("RecordSession failed: " + t.getMessage());
            } catch (Exception ignored) {
            }
        } finally {
            session.release();
            try {
                client.close();
            } catch (Exception ignored) {
            }
        }
    }

    private void run() throws Exception {
        output = new DataOutputStream(socket.getOutputStream());
        input = new DataInputStream(socket.getInputStream());

        // 1. 等待来自 Windows 端的 START_RECORD 协议帧
        CaptureProtocol.Frame startFrame = CaptureProtocol.readFrame(input);
        if (startFrame.type != CaptureProtocol.START_RECORD) {
            sendError("Expected START_RECORD as first message");
            return;
        }

        int fps = 60;
        int bitrate = 16_000_000;
        int codecType = 0; // 0 = H.264, 1 = H.265
        if (startFrame.payload.length >= 8) {
            ByteBuffer bb = ByteBuffer.wrap(startFrame.payload);
            fps = bb.getInt();
            bitrate = bb.getInt();
            if (bb.remaining() >= 1) {
                codecType = bb.get() & 0xff;
            }
        }

        // 2. 初始化视频录制组件并提取初始 SPS/PPS
        screenRecorder = new ScreenRecorder(fps, bitrate, codecType);
        screenRecorder.prepare();
        byte[] videoConfig = screenRecorder.fetchConfig(3000);

        // 3. 初始化音频采集与编码（采用标准 REMOTE_SUBMIX，失败则干净降级为纯视频）
        byte[] audioConfig = null;
        try {
            audioEncoder = new AudioEncoder();
            audioEncoder.prepare();
            audioConfig = audioEncoder.fetchConfig(2000);
        } catch (Throwable t) {
            System.err.println("Audio initialization failed, proceeding with video only: " + t.getMessage());
            if (audioEncoder != null) {
                audioEncoder.stop();
                audioEncoder = null;
            }
        }

        long timeOriginUs = System.nanoTime() / 1000L;
        boolean hasAudio = (audioEncoder != null && audioConfig != null);

        // 4. 构建并发送 RECORD_READY 帧（一次性明确全部流格式与时间基准）
        ByteArrayOutputStream readyBytes = new ByteArrayOutputStream();
        DataOutputStream readyOut = new DataOutputStream(readyBytes);
        readyOut.writeLong(timeOriginUs);
        DisplayWrapper.DisplaySize displaySize = screenRecorder.getDisplaySize();
        readyOut.writeInt(displaySize.width & ~1);
        readyOut.writeInt(displaySize.height & ~1);
        readyOut.writeInt(screenRecorder.getFps());
        readyOut.writeByte(screenRecorder.getCodecType());
        readyOut.writeInt(videoConfig.length);
        readyOut.write(videoConfig);

        readyOut.writeByte(hasAudio ? 1 : 0);
        if (hasAudio) {
            readyOut.writeInt(AudioEncoder.SAMPLE_RATE);
            readyOut.writeInt(AudioEncoder.CHANNELS);
            readyOut.writeInt(audioConfig.length);
            readyOut.write(audioConfig);
        } else {
            readyOut.writeInt(0);
            readyOut.writeInt(0);
            readyOut.writeInt(0);
        }
        readyOut.flush();

        sendFrame(CaptureProtocol.RECORD_READY, 0, 0, timeOriginUs, readyBytes.toByteArray());
        isRunning = true;

        // 5. 启动音视频推流
        StreamSink sink = (type, reqId, flags, timestamp, payload) -> {
            if (isRunning) {
                sendFrame(type, reqId, flags, timestamp, payload);
            }
        };

        screenRecorder.startDraining(sink, timeOriginUs);
        if (hasAudio) {
            audioEncoder.startDraining(sink, timeOriginUs);
        }

        // 6. 监听 Windows 发来的 STOP_RECORD 指令或断开连接
        while (isRunning) {
            try {
                CaptureProtocol.Frame frame = CaptureProtocol.readFrame(input);
                if (frame.type == CaptureProtocol.STOP_RECORD) {
                    break;
                }
            } catch (Exception e) {
                break;
            }
        }

        // 7. 优雅停止采集并排空编码器尾部帧
        isRunning = false;
        if (screenRecorder != null) {
            screenRecorder.stop();
        }
        if (audioEncoder != null) {
            audioEncoder.stop();
        }

        try {
            sendFrame(CaptureProtocol.RECORD_FINISHED, 0, 0, 0, new byte[0]);
        } catch (Exception ignored) {
        }
    }

    void sendFrame(int type, int requestId, int flags, long timestamp, byte[] payload) throws IOException {
        synchronized (writeLock) {
            if (output != null) {
                CaptureProtocol.writeFrame(output, type, requestId, flags, timestamp, payload);
            }
        }
    }

    void sendError(String message) {
        try {
            byte[] payload = message.getBytes(StandardCharsets.UTF_8);
            sendFrame(CaptureProtocol.ERROR, 0, 0, 0, payload);
        } catch (Exception ignored) {
        }
    }

    void release() {
        isRunning = false;
        if (screenRecorder != null) {
            try {
                screenRecorder.stop();
            } catch (Exception ignored) {
            }
            screenRecorder = null;
        }
        if (audioEncoder != null) {
            try {
                audioEncoder.stop();
            } catch (Exception ignored) {
            }
            audioEncoder = null;
        }
    }
}
