package com.spinningmomo.capture;

import android.hardware.display.VirtualDisplay;
import android.view.Surface;

import java.lang.reflect.Field;
import java.lang.reflect.Method;

/**
 * 封装 Android 内部显示服务 (DisplayManagerGlobal / DisplayManager)，
 * 用于查询屏幕信息与创建镜像虚拟屏幕。
 */
final class DisplayWrapper {

    static final class DisplaySize {
        final int width;
        final int height;
        final int densityDpi;

        DisplaySize(int width, int height, int densityDpi) {
            this.width = width;
            this.height = height;
            this.densityDpi = densityDpi;
        }
    }

    private static Object displayManagerGlobal;
    private static Method getDisplayInfoMethod;
    private static Method createVirtualDisplayMethod;

    private DisplayWrapper() {}

    private static synchronized Object getDisplayManagerGlobal() throws Exception {
        if (displayManagerGlobal == null) {
            Class<?> clazz = Class.forName("android.hardware.display.DisplayManagerGlobal");
            Method getInstanceMethod = clazz.getDeclaredMethod("getInstance");
            displayManagerGlobal = getInstanceMethod.invoke(null);
        }
        return displayManagerGlobal;
    }

    static DisplaySize getPrimaryDisplaySize() throws Exception {
        Object dmg = getDisplayManagerGlobal();
        if (getDisplayInfoMethod == null) {
            getDisplayInfoMethod = dmg.getClass().getMethod("getDisplayInfo", int.class);
        }
        Object displayInfo = getDisplayInfoMethod.invoke(dmg, 0);
        if (displayInfo == null) {
            throw new Exception("DisplayManager returned null DisplayInfo for display 0");
        }

        Class<?> infoClass = displayInfo.getClass();
        Field widthField = infoClass.getDeclaredField("logicalWidth");
        Field heightField = infoClass.getDeclaredField("logicalHeight");
        Field dpiField = infoClass.getDeclaredField("logicalDensityDpi");

        widthField.setAccessible(true);
        heightField.setAccessible(true);
        dpiField.setAccessible(true);

        int width = widthField.getInt(displayInfo);
        int height = heightField.getInt(displayInfo);
        int dpi = dpiField.getInt(displayInfo);

        if (width <= 0 || height <= 0) {
            throw new Exception("Invalid display dimensions: " + width + "x" + height);
        }
        return new DisplaySize(width, height, dpi);
    }

    static VirtualDisplay createVirtualDisplay(String name, int width, int height, int displayIdToMirror,
                                               Surface surface) throws Exception {
        if (createVirtualDisplayMethod == null) {
            // Android 内部隐藏静态方法：DisplayManager.createVirtualDisplay(name, width, height, displayIdToMirror, surface)
            createVirtualDisplayMethod = android.hardware.display.DisplayManager.class
                    .getMethod("createVirtualDisplay", String.class, int.class, int.class, int.class, Surface.class);
        }
        return (VirtualDisplay) createVirtualDisplayMethod.invoke(null, name, width, height, displayIdToMirror, surface);
    }
}
