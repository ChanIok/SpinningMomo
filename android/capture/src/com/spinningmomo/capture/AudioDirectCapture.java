package com.spinningmomo.capture;

import android.annotation.SuppressLint;
import android.media.AudioFormat;
import android.media.AudioRecord;
import android.media.MediaRecorder;
import android.os.Build;

/**
 * 基于 MediaRecorder.AudioSource.REMOTE_SUBMIX 的系统音频采集（适用于 Android < 13 或 AudioPolicy 受限环境）。
 */
final class AudioDirectCapture implements AudioCaptureSource {

    private AudioRecord audioRecord;

    @SuppressLint({"MissingPermission", "WrongConstant"})
    @Override
    public AudioRecord start(int sampleRate, int channels) throws Exception {
        try {
            int channelConfig = (channels == 1) ? AudioFormat.CHANNEL_IN_MONO : AudioFormat.CHANNEL_IN_STEREO;
            AudioFormat audioFormat = new AudioFormat.Builder()
                    .setEncoding(AudioFormat.ENCODING_PCM_16BIT)
                    .setSampleRate(sampleRate)
                    .setChannelMask(channelConfig)
                    .build();

            int minBufferSize = AudioRecord.getMinBufferSize(sampleRate, channelConfig, AudioFormat.ENCODING_PCM_16BIT);
            int bufferSize = (minBufferSize > 0) ? minBufferSize * 4 : 4096 * 4;

            AudioRecord.Builder builder = new AudioRecord.Builder();
            if (Build.VERSION.SDK_INT >= 31) {
                builder.setContext(FakeContext.get());
            }
            builder.setAudioSource(MediaRecorder.AudioSource.REMOTE_SUBMIX);
            builder.setAudioFormat(audioFormat);
            builder.setBufferSizeInBytes(bufferSize);

            audioRecord = builder.build();
            audioRecord.startRecording();
            if (audioRecord.getRecordingState() != AudioRecord.RECORDSTATE_RECORDING) {
                throw new IllegalStateException("AudioRecord failed to enter recording state (REMOTE_SUBMIX)");
            }
            return audioRecord;
        } catch (Throwable t) {
            stop();
            throw (t instanceof Exception) ? (Exception) t : new RuntimeException(t);
        }
    }

    @Override
    public void stop() {
        if (audioRecord != null) {
            try {
                audioRecord.stop();
            } catch (Exception ignored) {
            }
            try {
                audioRecord.release();
            } catch (Exception ignored) {
            }
            audioRecord = null;
        }
    }
}
