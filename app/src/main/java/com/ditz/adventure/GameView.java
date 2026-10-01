package com.ditz.adventure;

import android.content.Context;
import android.opengl.GLSurfaceView;
import android.view.MotionEvent;

import javax.microedition.khronos.egl.EGLConfig;
import javax.microedition.khronos.opengles.GL10;

public class GameView extends GLSurfaceView {
    public volatile float jx = 0f, jy = 0f;
    private final Object lock = new Object();
    private float camAcc = 0f;
    private boolean attack = false;
    private float lastX;
    private int camPtr = -1;

    public GameView(Context c) {
        super(c);
        setEGLContextClientVersion(2);
        setRenderer(new R());
    }

    public void fireAttack() {
        synchronized (lock) { attack = true; }
    }

    private class R implements Renderer {
        long last = 0;

        @Override public void onSurfaceCreated(GL10 g, EGLConfig c) {
            Native.init();
            last = 0;
        }

        @Override public void onSurfaceChanged(GL10 g, int w, int h) {
            Native.resize(w, h);
        }

        @Override public void onDrawFrame(GL10 g) {
            long now = System.nanoTime();
            float dt = last == 0 ? 0.016f : (now - last) / 1e9f;
            last = now;
            if (dt > 0.1f) dt = 0.1f;
            float cd;
            boolean a;
            synchronized (lock) {
                cd = camAcc; camAcc = 0f;
                a = attack; attack = false;
            }
            Native.frame(dt, jx, jy, cd, a);
        }
    }

    @Override public boolean onTouchEvent(MotionEvent e) {
        int act = e.getActionMasked();
        switch (act) {
            case MotionEvent.ACTION_DOWN:
            case MotionEvent.ACTION_POINTER_DOWN:
                if (camPtr == -1) {
                    int i = e.getActionIndex();
                    camPtr = e.getPointerId(i);
                    lastX = e.getX(i);
                }
                break;
            case MotionEvent.ACTION_MOVE:
                if (camPtr != -1) {
                    int i = e.findPointerIndex(camPtr);
                    if (i >= 0) {
                        float x = e.getX(i);
                        synchronized (lock) { camAcc -= (x - lastX) * 0.006f; }
                        lastX = x;
                    }
                }
                break;
            case MotionEvent.ACTION_UP:
            case MotionEvent.ACTION_CANCEL:
                camPtr = -1;
                break;
            case MotionEvent.ACTION_POINTER_UP:
                if (e.getPointerId(e.getActionIndex()) == camPtr) camPtr = -1;
                break;
        }
        return true;
    }
}
