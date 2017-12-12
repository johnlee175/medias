package com.johnsoft.testyuv;

import java.io.IOException;

import android.content.Context;
import android.graphics.ImageFormat;
import android.hardware.Camera;
import android.util.AttributeSet;
import android.view.SurfaceHolder;
import android.view.SurfaceView;

public class MyView extends SurfaceView implements SurfaceHolder.Callback {
        Camera camera;

        public MyView(Context context, AttributeSet attrs) {
            super(context, attrs);
            getHolder().addCallback(this);
            YuvConsumer.initAsync(640, 480);
        }

        @Override
        public void surfaceCreated(SurfaceHolder holder) {
            if (camera == null) {
                try {
                    camera = Camera.open();
                    Camera.Parameters params = camera.getParameters();
                    params.setFocusMode(Camera.Parameters.FOCUS_MODE_CONTINUOUS_VIDEO);
                    params.setPreviewSize(640, 480);
                    params.setPreviewFormat(ImageFormat.YV12);
                    camera.setParameters(params);
                    camera.setPreviewCallback(new Camera.PreviewCallback() {
                        @Override
                        public void onPreviewFrame(byte[] data, Camera camera) {
                            YuvConsumer.nativeFillFrame(data);
                        }
                    });
                    camera.setPreviewDisplay(holder);
                    camera.startPreview();
                } catch (IOException e) {
                    e.printStackTrace();
                }
            }
        }

        @Override
        public void surfaceChanged(SurfaceHolder holder, int format, int width, int height) {
        }

        @Override
        public void surfaceDestroyed(SurfaceHolder holder) {
            if (camera != null) {
                camera.stopPreview();
                camera.release();
                camera = null;
            }
        }
    }