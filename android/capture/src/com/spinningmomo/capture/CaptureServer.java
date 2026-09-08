package com.spinningmomo.capture;

import android.net.LocalServerSocket;
import android.net.LocalSocket;

import java.io.DataInputStream;
import java.io.DataOutputStream;
import java.io.IOException;
import java.nio.charset.StandardCharsets;

/** ADB 连接期间常驻的 Android 捕获服务（处理截图指令，并托管独立录制监听）。 */
final class CaptureServer {

    private CaptureServer() {}

    static void run(String socketName) throws Exception {
        LocalServerSocket serverSocket = new LocalServerSocket(socketName);
        LocalServerSocket recordServerSocket = new LocalServerSocket(socketName + "-record");

        Thread recordListenerThread = new Thread(() -> {
            while (!Thread.currentThread().isInterrupted()) {
                try {
                    LocalSocket recordClient = recordServerSocket.accept();
                    RecordSession.handle(recordClient);
                } catch (IOException e) {
                    break;
                }
            }
        }, "momo-record-listener");
        recordListenerThread.start();

        try {
            while (true) {
                LocalSocket client = serverSocket.accept();
                try (LocalSocket activeClient = client;
                        DataInputStream input = new DataInputStream(activeClient.getInputStream());
                        DataOutputStream output = new DataOutputStream(activeClient.getOutputStream())) {
                    CaptureProtocol.writeFrame(output, CaptureProtocol.READY, 0, 0, 0,
                            new byte[0]);
                    serveClient(input, output);
                    return;
                } catch (IOException clientError) {
                }
            }
        } finally {
            try {
                serverSocket.close();
            } catch (IOException ignored) {
            }
            try {
                recordServerSocket.close();
            } catch (IOException ignored) {
            }
            recordListenerThread.interrupt();
        }
    }

    private static void serveClient(DataInputStream input, DataOutputStream output)
            throws IOException {
        while (true) {
            CaptureProtocol.Frame frame = CaptureProtocol.readFrame(input);
            if (frame.type == CaptureProtocol.SCREENSHOT_REQUEST) {
                handleScreenshot(frame, output);
            } else if (frame.type == CaptureProtocol.SHUTDOWN) {
                CaptureProtocol.writeFrame(output, CaptureProtocol.SHUTDOWN_ACK,
                        frame.requestId, 0, 0, new byte[0]);
                return;
            } else {
                sendError(output, frame.requestId,
                        "Unsupported capture command: " + frame.type);
            }
        }
    }

    private static void handleScreenshot(CaptureProtocol.Frame frame, DataOutputStream output)
            throws IOException {
        if (frame.payload.length != 2) {
            sendError(output, frame.requestId, "Screenshot request payload must be 2 bytes");
            return;
        }

        int format = frame.payload[0] & 0xff;
        int quality = frame.payload[1] & 0xff;
        if (format != 0 && format != 1) {
            sendError(output, frame.requestId, "Unsupported screenshot format");
            return;
        }

        try {
            byte[] image = ScreenCapture.capture(format, quality);
            CaptureProtocol.writeFrame(output, CaptureProtocol.IMAGE_RESPONSE,
                    frame.requestId, format, 0, image);
        } catch (Exception error) {
            sendError(output, frame.requestId, error.toString());
        }
    }

    private static void sendError(DataOutputStream output, int requestId, String message)
            throws IOException {
        CaptureProtocol.writeFrame(output, CaptureProtocol.ERROR, requestId, 0, 0,
                message.getBytes(StandardCharsets.UTF_8));
    }
}
