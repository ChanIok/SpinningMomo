package com.spinningmomo.capture;

import android.app.Application;
import android.app.Instrumentation;
import android.content.pm.ApplicationInfo;
import android.os.Build;

import java.lang.reflect.Constructor;
import java.lang.reflect.Field;
import java.lang.reflect.Method;

/**
 * 为 app_process (shell UID) 运行环境注入必要的上下文和全局对象，
 * 避免调用 DisplayManager 等系统服务时抛出空指针或权限异常。
 */
public final class Workarounds {

    private static final Class<?> ACTIVITY_THREAD_CLASS;
    private static final Object ACTIVITY_THREAD;

    static {
        try {
            if (android.os.Looper.myLooper() == null) {
                android.os.Looper.prepare();
                try {
                    java.lang.reflect.Field sMainLooper = android.os.Looper.class.getDeclaredField("sMainLooper");
                    sMainLooper.setAccessible(true);
                    sMainLooper.set(null, android.os.Looper.myLooper());
                } catch (Throwable ignored) {
                }
            }

            ACTIVITY_THREAD_CLASS = Class.forName("android.app.ActivityThread");
            Constructor<?> constructor = ACTIVITY_THREAD_CLASS.getDeclaredConstructor();
            constructor.setAccessible(true);
            ACTIVITY_THREAD = constructor.newInstance();

            Field sCurrentActivityThread = ACTIVITY_THREAD_CLASS.getDeclaredField("sCurrentActivityThread");
            sCurrentActivityThread.setAccessible(true);
            sCurrentActivityThread.set(null, ACTIVITY_THREAD);

            Field mSystemThread = ACTIVITY_THREAD_CLASS.getDeclaredField("mSystemThread");
            mSystemThread.setAccessible(true);
            mSystemThread.setBoolean(ACTIVITY_THREAD, true);
        } catch (Throwable t) {
            throw new AssertionError("Failed to initialize ActivityThread workaround", t);
        }
    }

    private Workarounds() {}

    public static void apply() {
        if (Build.VERSION.SDK_INT >= 31) {
            fillConfigurationController();
        }
        fillAppInfo();
        fillAppContext();
    }

    private static void fillAppInfo() {
        try {
            Class<?> appBindDataClass = Class.forName("android.app.ActivityThread$AppBindData");
            Constructor<?> constructor = appBindDataClass.getDeclaredConstructor();
            constructor.setAccessible(true);
            Object appBindData = constructor.newInstance();

            ApplicationInfo appInfo = new ApplicationInfo();
            appInfo.packageName = FakeContext.PACKAGE_NAME;

            Field appInfoField = appBindDataClass.getDeclaredField("appInfo");
            appInfoField.setAccessible(true);
            appInfoField.set(appBindData, appInfo);

            Field mBoundApplication = ACTIVITY_THREAD_CLASS.getDeclaredField("mBoundApplication");
            mBoundApplication.setAccessible(true);
            mBoundApplication.set(ACTIVITY_THREAD, appBindData);
        } catch (Throwable ignored) {
        }
    }

    private static void fillAppContext() {
        try {
            Application app = Instrumentation.newApplication(Application.class, FakeContext.get());
            Field mInitialApplication = ACTIVITY_THREAD_CLASS.getDeclaredField("mInitialApplication");
            mInitialApplication.setAccessible(true);
            mInitialApplication.set(ACTIVITY_THREAD, app);
        } catch (Throwable ignored) {
        }
    }

    private static void fillConfigurationController() {
        try {
            Class<?> configClass = Class.forName("android.app.ConfigurationController");
            Class<?> internalClass = Class.forName("android.app.ActivityThreadInternal");
            Constructor<?> constructor = configClass.getDeclaredConstructor(internalClass);
            constructor.setAccessible(true);
            Object controller = constructor.newInstance(ACTIVITY_THREAD);

            Field mConfigurationController = ACTIVITY_THREAD_CLASS.getDeclaredField("mConfigurationController");
            mConfigurationController.setAccessible(true);
            mConfigurationController.set(ACTIVITY_THREAD, controller);
        } catch (Throwable ignored) {
        }
    }

    static android.content.Context getSystemContext() {
        try {
            Method getSystemContext = ACTIVITY_THREAD_CLASS.getDeclaredMethod("getSystemContext");
            return (android.content.Context) getSystemContext.invoke(ACTIVITY_THREAD);
        } catch (Throwable t) {
            return null;
        }
    }
}
