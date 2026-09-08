package com.spinningmomo.capture;

import android.media.AudioFormat;
import android.media.AudioRecord;
import android.media.MediaCodec;
import android.media.MediaCodecInfo;
import android.media.MediaFormat;

import java.nio.ByteBuffer;

/**
 * 音频编码器：从 AudioCaptureSource 读取 PCM 数据并送入 MediaCodec 编码为 AAC-LC。
 */
final class AudioEncoder {

    static final int SAMPLE_RATE = 48000;
    static final int CHANNELS = 2;
    static final int BITRATE = 128000;

    private final AudioCaptureSource capture;
    private StreamSink sink;
    private long timeOriginUs;
    private MediaCodec codec;
    private AudioRecord audioRecord;

    private volatile boolean eosRequested = false;
    private volatile boolean eosReceived = false;
    private Thread inputThread;
    private Thread outputThread;

    AudioEncoder() {
        this.capture = AudioCaptureSource.create();
    }

    void prepare() throws Exception {
        audioRecord = capture.start(SAMPLE_RATE, CHANNELS);

        MediaFormat format = MediaFormat.createAudioFormat(MediaFormat.MIMETYPE_AUDIO_AAC, SAMPLE_RATE, CHANNELS);
        format.setInteger(MediaFormat.KEY_AAC_PROFILE, MediaCodecInfo.CodecProfileLevel.AACObjectLC);
        format.setInteger(MediaFormat.KEY_BIT_RATE, BITRATE);
        format.setInteger(MediaFormat.KEY_MAX_INPUT_SIZE, 16384);

        codec = MediaCodec.createEncoderByType(MediaFormat.MIMETYPE_AUDIO_AAC);
        codec.configure(format, null, null, MediaCodec.CONFIGURE_FLAG_ENCODE);
        codec.start();
    }

    byte[] fetchConfig(long timeoutMs) throws Exception {
        int bufferSize = AudioRecord.getMinBufferSize(SAMPLE_RATE, AudioFormat.CHANNEL_IN_STEREO,
                AudioFormat.ENCODING_PCM_16BIT);
        if (bufferSize <= 0) {
            bufferSize = 4096;
        }
        byte[] readBuffer = new byte[bufferSize];
        int readBytes = audioRecord.read(readBuffer, 0, readBuffer.length);
        if (readBytes > 0) {
            int inputIndex = codec.dequeueInputBuffer(100000);
            if (inputIndex >= 0) {
                ByteBuffer inputBuffer = codec.getInputBuffer(inputIndex);
                inputBuffer.clear();
                inputBuffer.put(readBuffer, 0, readBytes);
                codec.queueInputBuffer(inputIndex, 0, readBytes, 0, 0);
            }
        }

        MediaCodec.BufferInfo bufferInfo = new MediaCodec.BufferInfo();
        long deadline = System.currentTimeMillis() + timeoutMs;
        while (System.currentTimeMillis() < deadline) {
            int outIndex = codec.dequeueOutputBuffer(bufferInfo, 100000);
            if (outIndex >= 0) {
                ByteBuffer outBuffer = codec.getOutputBuffer(outIndex);
                if ((bufferInfo.flags & MediaCodec.BUFFER_FLAG_CODEC_CONFIG) != 0) {
                    byte[] config = new byte[bufferInfo.size];
                    outBuffer.position(bufferInfo.offset);
                    outBuffer.get(config, 0, bufferInfo.size);
                    codec.releaseOutputBuffer(outIndex, false);
                    return config;
                }
                codec.releaseOutputBuffer(outIndex, false);
            }
        }
        // AAC-LC 48000Hz 立体声标准 2 字节配置：0x11, 0x90
        return new byte[]{(byte) 0x11, (byte) 0x90};
    }

    void startDraining(StreamSink sink, long timeOriginUs) {
        this.sink = sink;
        this.timeOriginUs = timeOriginUs;
        inputThread = new Thread(this::feedInput, "momo-audio-input");
        outputThread = new Thread(this::drainOutput, "momo-audio-output");

        inputThread.start();
        outputThread.start();
    }

    private synchronized void terminateAudio(String reason, Throwable t) {
        if (t != null) {
            System.err.println("Audio encoding terminated due to " + reason + ": " + t.getMessage());
        } else {
            System.err.println("Audio encoding terminated: " + reason);
        }
        eosRequested = true;
        if (sink != null) {
            try {
                sink.sendFrame(CaptureProtocol.AUDIO_ENDED, 0, 0, 0, new byte[0]);
            } catch (Exception ignored) {
            }
        }
        if (capture != null) {
            try {
                capture.stop();
            } catch (Exception ignored) {
            }
        }
    }

    private void feedInput() {
        long totalFramesRead = 0;
        final int bytesPerFrame = CHANNELS * 2;

        int bufferSize = AudioRecord.getMinBufferSize(SAMPLE_RATE, AudioFormat.CHANNEL_IN_STEREO,
                AudioFormat.ENCODING_PCM_16BIT);
        if (bufferSize <= 0) {
            bufferSize = 4096;
        }
        byte[] readBuffer = new byte[bufferSize];

        while (!eosRequested) {
            int readBytes = audioRecord.read(readBuffer, 0, readBuffer.length);
            if (readBytes <= 0) {
                if (readBytes < 0) {
                    terminateAudio("AudioRecord read error " + readBytes, null);
                    break;
                }
                if (eosRequested) {
                    break;
                }
                continue;
            }

            long ptsUs = totalFramesRead * 1000000L / SAMPLE_RATE;
            int framesInChunk = readBytes / bytesPerFrame;
            totalFramesRead += framesInChunk;

            try {
                int inputIndex = -1;
                while (!eosRequested && inputIndex < 0) {
                    inputIndex = codec.dequeueInputBuffer(10000);
                }
                if (inputIndex >= 0) {
                    ByteBuffer inputBuffer = codec.getInputBuffer(inputIndex);
                    inputBuffer.clear();
                    inputBuffer.put(readBuffer, 0, readBytes);
                    codec.queueInputBuffer(inputIndex, 0, readBytes, ptsUs, 0);
                }
            } catch (Exception e) {
                terminateAudio("queueInputBuffer failed", e);
                break;
            }
        }

        try {
            for (int i = 0; i < 20; ++i) {
                int inputIndex = codec.dequeueInputBuffer(50000);
                if (inputIndex >= 0) {
                    codec.queueInputBuffer(inputIndex, 0, 0, 0, MediaCodec.BUFFER_FLAG_END_OF_STREAM);
                    break;
                }
            }
        } catch (Exception ignored) {
        }
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
                        if (sink != null) {
                            sink.sendFrame(CaptureProtocol.AUDIO_SAMPLE, 0, bufferInfo.flags,
                                    bufferInfo.presentationTimeUs, sample);
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
                terminateAudio("drainOutput failed", e);
                break;
            }
        }
    }

    boolean stop() {
        eosRequested = true;
        if (capture != null) {
            try {
                capture.stop();
            } catch (Exception ignored) {
            }
        }

        if (inputThread != null) {
            try {
                inputThread.join(1000);
            } catch (InterruptedException ignored) {
            }
            if (inputThread.isAlive()) {
                inputThread.interrupt();
                try {
                    inputThread.join(500);
                } catch (InterruptedException ignored) {
                }
            }
            inputThread = null;
        }

        if (outputThread != null) {
            try {
                outputThread.join(1000);
            } catch (InterruptedException ignored) {
            }
            if (outputThread.isAlive()) {
                outputThread.interrupt();
                try {
                    outputThread.join(500);
                } catch (InterruptedException ignored) {
                }
            }
            outputThread = null;
        }

        if (codec != null) {
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

        return eosReceived;
    }
}
