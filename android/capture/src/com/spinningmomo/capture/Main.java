package com.spinningmomo.capture;

import android.os.Build;
import android.os.Process;
import org.json.JSONArray;
import org.json.JSONObject;

/** 由 app_process 启动的诊断入口；不需要安装 APK 或创建 Activity。 */
public final class Main {
    private Main() {}

    public static void main(String[] args) {
        try {
            Workarounds.apply();

            String command = args.length == 0 ? "info" : args[0];
            if ("info".equals(command)) {
                if (args.length > 1) {
                    usageAndExit();
                    return;
                }
                printInfo();
                exit(0);
                return;
            }

            if ("screenshot".equals(command)) {
                captureScreenshot(args);
                exit(0);
                return;
            }

            if ("server".equals(command)) {
                runServer(args);
                exit(0);
                return;
            }

            usageAndExit();
        } catch (Exception | LinkageError error) {
            System.err.println("Momo capture failed: " + error);
            error.printStackTrace(System.err);
            exit(1);
        }
    }

    private static void printInfo() throws Exception {
        JSONObject device = new JSONObject();
        device.put("manufacturer", Build.MANUFACTURER);
        device.put("model", Build.MODEL);
        device.put("androidRelease", Build.VERSION.RELEASE);
        device.put("sdkInt", Build.VERSION.SDK_INT);
        device.put("supportedAbis", new JSONArray(Build.SUPPORTED_ABIS));
        device.put("uid", Process.myUid());
        device.put("pid", Process.myPid());

        JSONObject result = new JSONObject();
        result.put("protocolVersion", 1);
        result.put("command", "info");
        result.put("device", device);
        result.put("encoders", CodecReport.collect());
        // info 命令的 stdout 只承载一份 JSON，日志和运行错误走 stderr。
        System.out.println(result.toString(2));
        System.out.flush();
    }

    private static void captureScreenshot(String[] args) throws Exception {
        String format = "jpeg";
        int quality = 100;

        for (int i = 1; i < args.length; i++) {
            String argument = args[i];
            if ("--format".equals(argument) && i + 1 < args.length) {
                format = args[++i];
            } else if ("--quality".equals(argument) && i + 1 < args.length) {
                quality = Integer.parseInt(args[++i]);
            } else {
                usageAndExit();
                return;
            }
        }

        int formatCode = 1;
        if ("png".equalsIgnoreCase(format)) {
            formatCode = 0;
        } else if (!"jpeg".equalsIgnoreCase(format) && !"jpg".equalsIgnoreCase(format)) {
            throw new IllegalArgumentException("Unsupported screenshot format: " + format);
        }

        byte[] image = ScreenCapture.capture(formatCode, quality);
        // exec-out 将 stdout 原样传回 Windows；绝不能通过 println 或 JSON 包装图片。
        System.out.write(image, 0, image.length);
        System.out.flush();
    }

    private static void runServer(String[] args) throws Exception {
        if (args.length != 3 || !"--socket".equals(args[1]) || args[2].isEmpty()) {
            usageAndExit();
            return;
        }
        CaptureServer.run(args[2]);
    }

    private static void usageAndExit() {
        System.err.println("Usage: com.spinningmomo.capture.Main info");
        System.err.println("   or: com.spinningmomo.capture.Main screenshot"
                + " --format jpeg [--quality 0..100]");
        System.err.println("   or: com.spinningmomo.capture.Main server --socket <name>");
        exit(2);
    }

    private static void exit(int status) {
        // 独立 app_process 入口主动结束进程；直接返回会进入 VM 销毁后的原生清理路径，
        // 已在 MuMu Android 15 的 __cxa_finalize 中观察到崩溃。
        System.exit(status);
    }
}
