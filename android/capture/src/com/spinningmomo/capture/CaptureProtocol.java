package com.spinningmomo.capture;

import java.io.DataInputStream;
import java.io.DataOutputStream;
import java.io.EOFException;
import java.io.IOException;

/** Windows 与 Android 捕获服务共用的定长头二进制协议。 */
final class CaptureProtocol {
    static final int MAGIC = 0x4D4F4D4F; // MOMO
    static final int VERSION = 1;
    static final int MAX_PAYLOAD_SIZE = 128 * 1024 * 1024;

    static final int READY = 1;
    static final int SCREENSHOT_REQUEST = 2;
    static final int IMAGE_RESPONSE = 3;
    static final int ERROR = 4;
    static final int SHUTDOWN = 5;
    static final int SHUTDOWN_ACK = 6;
    static final int START_RECORD = 7;
    static final int RECORD_READY = 8;
    static final int VIDEO_SAMPLE = 9;
    static final int STOP_RECORD = 10;
    static final int RECORD_FINISHED = 11;
    static final int AUDIO_SAMPLE = 12;
    static final int AUDIO_ENDED = 13;

    private CaptureProtocol() {}

    static final class Frame {
        final int type;
        final int requestId;
        final int flags;
        final long timestamp;
        final byte[] payload;

        Frame(int type, int requestId, int flags, long timestamp, byte[] payload) {
            this.type = type;
            this.requestId = requestId;
            this.flags = flags;
            this.timestamp = timestamp;
            this.payload = payload;
        }
    }

    static Frame readFrame(DataInputStream input) throws IOException {
        final int magic;
        try {
            magic = input.readInt();
        } catch (EOFException error) {
            throw error;
        }
        if (magic != MAGIC) {
            throw new IOException("Invalid capture protocol magic");
        }
        if (input.readUnsignedShort() != VERSION) {
            throw new IOException("Unsupported capture protocol version");
        }

        int type = input.readUnsignedShort();
        int requestId = input.readInt();
        int flags = input.readInt();
        long timestamp = input.readLong();
        int payloadSize = input.readInt();
        if (payloadSize < 0 || payloadSize > MAX_PAYLOAD_SIZE) {
            throw new IOException("Capture payload exceeds protocol limit");
        }

        byte[] payload = new byte[payloadSize];
        input.readFully(payload);
        return new Frame(type, requestId, flags, timestamp, payload);
    }

    static void writeFrame(DataOutputStream output, int type, int requestId, int flags,
            long timestamp, byte[] payload) throws IOException {
        if (payload == null) {
            payload = new byte[0];
        }
        if (payload.length > MAX_PAYLOAD_SIZE) {
            throw new IOException("Capture payload exceeds protocol limit");
        }

        output.writeInt(MAGIC);
        output.writeShort(VERSION);
        output.writeShort(type);
        output.writeInt(requestId);
        output.writeInt(flags);
        output.writeLong(timestamp);
        output.writeInt(payload.length);
        output.write(payload);
        output.flush();
    }
}
