package com.spinningmomo.capture;

import java.io.IOException;

/** 录制音视频数据流的帧接收接口。 */
interface StreamSink {
    void sendFrame(int type, int requestId, int flags, long timestamp, byte[] payload) throws IOException;
}
