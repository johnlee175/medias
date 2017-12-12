package com.johnsoft.testyuv;

/**
 * @author John Kenrinus Lee
 * @version 2017-11-16
 */
public class YuvConsumer {
    static {
//        System.loadLibrary("rtsp-stream");
        System.loadLibrary("rtmp-stream");
    }

    public static void initAsync(final int w, final int h) {
        new Thread("YuvConsumer") {
            @Override
            public void run() {
                nativeInit(w, h);
            }
        }.start();
    }

    private static native void nativeInit(int w, int h);

    public static native void nativeFillFrame(byte[] array);
}
