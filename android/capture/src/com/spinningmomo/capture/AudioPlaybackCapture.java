package com.spinningmomo.capture;

import android.annotation.SuppressLint;
import android.content.Context;
import android.media.AudioAttributes;
import android.media.AudioFormat;
import android.media.AudioManager;
import android.media.AudioRecord;
import android.os.Build;

import java.lang.reflect.Method;

/**
 * 基于 Android 13+ (API 33) 隐藏 AudioPolicy 反射实现的系统内部音频采集。
 * 使用 ROUTE_FLAG_LOOP_BACK_RENDER 标志兼顾系统扬声器正常发声与内录采集，不使用静音的 REMOTE_SUBMIX。
 */
final class AudioPlaybackCapture implements AudioCaptureSource {

    private AudioRecord audioRecord;
    private Object audioPolicy;

    AudioPlaybackCapture() {}

    @SuppressLint("PrivateApi")
    @Override
    public AudioRecord start(int sampleRate, int channels) throws Exception {
        if (Build.VERSION.SDK_INT < 33) {
            throw new IllegalStateException(
                    "Internal audio playback capture requires Android 13 (API 33) or higher, current SDK: " + Build.VERSION.SDK_INT);
        }

        try {
            Class<?> audioMixingRuleClass = Class.forName("android.media.audiopolicy.AudioMixingRule");
            Class<?> audioMixingRuleBuilderClass = Class.forName("android.media.audiopolicy.AudioMixingRule$Builder");
            Object mixingRuleBuilder = audioMixingRuleBuilderClass.getConstructor().newInstance();

            int mixRolePlayers = audioMixingRuleClass.getField("MIX_ROLE_PLAYERS").getInt(null);
            Method setTargetMixRoleMethod = audioMixingRuleBuilderClass.getMethod("setTargetMixRole", int.class);
            setTargetMixRoleMethod.invoke(mixingRuleBuilder, mixRolePlayers);

            AudioAttributes mediaAttributes = new AudioAttributes.Builder()
                    .setUsage(AudioAttributes.USAGE_MEDIA)
                    .build();

            int ruleMatchAttributeUsage = audioMixingRuleClass.getField("RULE_MATCH_ATTRIBUTE_USAGE").getInt(null);
            Method addMixRuleMethod = audioMixingRuleBuilderClass.getMethod("addMixRule", int.class, Object.class);
            addMixRuleMethod.invoke(mixingRuleBuilder, ruleMatchAttributeUsage, mediaAttributes);

            Method voiceCommunicationMethod = audioMixingRuleBuilderClass.getMethod(
                    "voiceCommunicationCaptureAllowed", boolean.class);
            voiceCommunicationMethod.invoke(mixingRuleBuilder, true);

            Object mixingRule = audioMixingRuleBuilderClass.getMethod("build").invoke(mixingRuleBuilder);

            Class<?> audioMixClass = Class.forName("android.media.audiopolicy.AudioMix");
            Class<?> audioMixBuilderClass = Class.forName("android.media.audiopolicy.AudioMix$Builder");
            Object mixBuilder = audioMixBuilderClass.getConstructor(audioMixingRuleClass).newInstance(mixingRule);

            int channelConfig = (channels == 1) ? AudioFormat.CHANNEL_IN_MONO : AudioFormat.CHANNEL_IN_STEREO;
            AudioFormat audioFormat = new AudioFormat.Builder()
                    .setEncoding(AudioFormat.ENCODING_PCM_16BIT)
                    .setSampleRate(sampleRate)
                    .setChannelMask(channelConfig)
                    .build();

            Method setFormatMethod = mixBuilder.getClass().getMethod("setFormat", AudioFormat.class);
            setFormatMethod.invoke(mixBuilder, audioFormat);

            // ROUTE_FLAG_LOOP_BACK_RENDER 确保音频在被内录采集的同时，继续路由到本地扬声器正常发声
            int routeFlags = audioMixClass.getField("ROUTE_FLAG_LOOP_BACK_RENDER").getInt(null);
            Method setRouteFlagsMethod = mixBuilder.getClass().getMethod("setRouteFlags", int.class);
            setRouteFlagsMethod.invoke(mixBuilder, routeFlags);

            Object audioMix = audioMixBuilderClass.getMethod("build").invoke(mixBuilder);

            Class<?> audioPolicyClass = Class.forName("android.media.audiopolicy.AudioPolicy");
            Class<?> audioPolicyBuilderClass = Class.forName("android.media.audiopolicy.AudioPolicy$Builder");
            Object policyBuilder = audioPolicyBuilderClass.getConstructor(Context.class).newInstance(FakeContext.get());

            Method addMixMethod = audioPolicyBuilderClass.getMethod("addMix", audioMixClass);
            addMixMethod.invoke(policyBuilder, audioMix);

            Object policy = audioPolicyBuilderClass.getMethod("build").invoke(policyBuilder);

            Method registerAudioPolicyStaticMethod = AudioManager.class.getDeclaredMethod(
                    "registerAudioPolicyStatic", audioPolicyClass);
            registerAudioPolicyStaticMethod.setAccessible(true);
            int registerResult = (int) registerAudioPolicyStaticMethod.invoke(null, policy);
            if (registerResult != 0) {
                throw new RuntimeException("registerAudioPolicyStatic returned error code: " + registerResult);
            }
            audioPolicy = policy;

            Method createAudioRecordSinkMethod = audioPolicyClass.getMethod("createAudioRecordSink", audioMixClass);
            audioRecord = (AudioRecord) createAudioRecordSinkMethod.invoke(policy, audioMix);
            if (audioRecord == null) {
                throw new RuntimeException("createAudioRecordSink returned null");
            }

            audioRecord.startRecording();
            if (audioRecord.getRecordingState() != AudioRecord.RECORDSTATE_RECORDING) {
                throw new IllegalStateException("AudioRecord failed to enter recording state (AudioPolicy)");
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

        if (audioPolicy != null) {
            try {
                Method unregisterMethod = AudioManager.class.getDeclaredMethod(
                        "unregisterAudioPolicyAsyncStatic", audioPolicy.getClass());
                unregisterMethod.setAccessible(true);
                unregisterMethod.invoke(null, audioPolicy);
            } catch (Exception ignored) {
            }
            audioPolicy = null;
        }
    }
}
