package com.spinningmomo.capture;

import android.graphics.Bitmap;
import android.graphics.PixelFormat;
import android.hardware.display.VirtualDisplay;
import android.media.Image;
import android.media.ImageReader;
import android.os.Handler;
import android.os.HandlerThread;

import java.io.ByteArrayOutputStream;
import java.nio.ByteBuffer;
import java.util.concurrent.CountDownLatch;
import java.util.concurrent.TimeUnit;
import java.util.concurrent.atomic.AtomicReference;

/**
 * 基于 VirtualDisplay 与 ImageReader 的屏幕采集核心。
 *
 * <p>通过系统显示服务将屏幕直接镜像到目标 Surface 上，
 * 避免了 SurfaceControl.captureDisplay() 的版本兼容脆弱性，
 * 并与未来的硬件编码录制 (MediaCodec InputSurface) 保持一致的采集底层。</p>
 */
final class ScreenCapture {

    private static final int TIMEOUT_SECONDS = 3;

    private ScreenCapture() {}

    static byte[] capture(int format, int quality) throws Exception {
        int normalizedQuality = Math.max(0, Math.min(100, quality));
        DisplayWrapper.DisplaySize size = DisplayWrapper.getPrimaryDisplaySize();

        HandlerThread listenerThread = new HandlerThread("momo-capture-listener");
        listenerThread.start();

        ImageReader reader = null;
        VirtualDisplay virtualDisplay = null;
        Image capturedImage = null;

        try {
            reader = ImageReader.newInstance(size.width, size.height, PixelFormat.RGBA_8888, 2);
            Handler handler = new Handler(listenerThread.getLooper());

            CountDownLatch latch = new CountDownLatch(1);
            AtomicReference<Image> imageRef = new AtomicReference<>();

            reader.setOnImageAvailableListener(r -> {
                Image img = r.acquireLatestImage();
                if (img != null) {
                    if (imageRef.compareAndSet(null, img)) {
                        latch.countDown();
                    } else {
                        img.close();
                    }
                }
            }, handler);

            virtualDisplay = DisplayWrapper.createVirtualDisplay(
                    "momo-screenshot", size.width, size.height, 0, reader.getSurface());

            if (!latch.await(TIMEOUT_SECONDS, TimeUnit.SECONDS)) {
                throw new Exception("Timed out waiting for frame from VirtualDisplay");
            }

            capturedImage = imageRef.get();
            if (capturedImage == null) {
                throw new Exception("VirtualDisplay provided null Image");
            }

            return encodeImage(capturedImage, size.width, size.height, format, normalizedQuality);
        } finally {
            if (capturedImage != null) {
                capturedImage.close();
            }
            if (virtualDisplay != null) {
                virtualDisplay.release();
            }
            if (reader != null) {
                reader.close();
            }
            listenerThread.quitSafely();
        }
    }

    private static byte[] encodeImage(Image image, int width, int height, int format, int quality)
            throws Exception {
        Image.Plane plane = image.getPlanes()[0];
        ByteBuffer buffer = plane.getBuffer();
        int pixelStride = plane.getPixelStride();
        int rowStride = plane.getRowStride();
        int rowPadding = rowStride - pixelStride * width;

        Bitmap bitmap = Bitmap.createBitmap(
                width + rowPadding / pixelStride, height, Bitmap.Config.ARGB_8888);
        bitmap.copyPixelsFromBuffer(buffer);

        Bitmap cleanBitmap = bitmap;
        if (rowPadding > 0) {
            cleanBitmap = Bitmap.createBitmap(bitmap, 0, 0, width, height);
            bitmap.recycle();
        }

        try {
            ByteArrayOutputStream output = new ByteArrayOutputStream(
                    Math.max(32 * 1024, width * height / 2));
            Bitmap.CompressFormat compressFormat = format == 0
                    ? Bitmap.CompressFormat.PNG : Bitmap.CompressFormat.JPEG;
            if (!cleanBitmap.compress(compressFormat, quality, output)) {
                throw new Exception("Bitmap compression failed");
            }
            return output.toByteArray();
        } finally {
            if (!cleanBitmap.isRecycled()) {
                cleanBitmap.recycle();
            }
        }
    }
}
