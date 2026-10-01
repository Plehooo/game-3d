package com.ditz.adventure;

public class Native {
    static { System.loadLibrary("ditzgame"); }

    public static native void init();
    public static native void resize(int w, int h);
    public static native void frame(float dt, float jx, float jy, float camD, boolean attack);
    public static native void setChar(int i);
    public static native void setSkin(int i);
    public static native void setPet(int i);
    public static native void setMap(int i);
    public static native void setPoints(float points);
    public static native boolean spendPoints(float cost);
    // 0 poin, 1 hp, 2 waktu(0..1), 3 karakter, 4 skin, 5 pet, 6 peta
    public static native float stat(int i);
}
