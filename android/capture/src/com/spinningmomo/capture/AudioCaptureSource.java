package com.spinningmomo.capture;

import android.media.AudioRecord;
import android.os.Build;

/**
 * 统一系统音频采集接口：屏蔽底层的 AudioPolicy 与 REMOTE_SUBMIX 实现差异，提供透明降级。
 */
interface AudioCaptureSource {

    AudioRecord start(int sampleRate, int channels) throws Exception;

    void stop();

    static AudioCaptureSource create() {
        return new AudioCaptureSource() {
            private AudioCaptureSource activeSource;

            @Override
            public AudioRecord start(int sampleRate, int channels) throws Exception {
                // Tier 1: Android 13+ (API 33+) 优先尝试支持外放的 AudioPolicy Loopback Render
                if (Build.VERSION.SDK_INT >= 33) {
                    try {
                        AudioPlaybackCapture playback = new AudioPlaybackCapture();
                        AudioRecord record = playback.start(sampleRate, channels);
                        activeSource = playback;
                        System.out.println("AudioCaptureSource: using AudioPlaybackCapture (unmuted speaker, API " + Build.VERSION.SDK_INT + ")");
                        return record;
                    } catch (Throwable t) {
                        System.err.println("AudioPlaybackCapture failed, falling back to REMOTE_SUBMIX: " + t.getMessage());
                    }
                }

                // Tier 2: 全版本保底，使用稳定的 REMOTE_SUBMIX（扬声器静音）
                try {
                    AudioDirectCapture direct = new AudioDirectCapture();
                    AudioRecord record = direct.start(sampleRate, channels);
                    activeSource = direct;
                    System.out.println("AudioCaptureSource: using AudioDirectCapture (REMOTE_SUBMIX, muted speaker)");
                    return record;
                } catch (Throwable t) {
                    System.err.println("AudioDirectCapture failed: " + t.getMessage());
                    throw t;
                }
            }

            @Override
            public void stop() {
                if (activeSource != null) {
                    try {
                        activeSource.stop();
                    } finally {
                        activeSource = null;
                    }
                }
            }
        };
    }
}
