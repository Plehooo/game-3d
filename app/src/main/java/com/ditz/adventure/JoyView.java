package com.ditz.adventure;

import android.content.Context;
import android.graphics.Canvas;
import android.graphics.Paint;
import android.view.MotionEvent;
import android.view.View;

public class JoyView extends View {
    private final GameView gv;
    private final Paint p = new Paint(Paint.ANTI_ALIAS_FLAG);
    private float kx = 0f, ky = 0f;

    public JoyView(Context c, GameView gv) {
        super(c);
        this.gv = gv;
    }

    @Override protected void onDraw(Canvas c) {
        float cx = getWidth() / 2f, cy = getHeight() / 2f;
        float R = Math.min(cx, cy) * 0.95f;
        p.setStyle(Paint.Style.FILL);
        p.setColor(0x44FFFFFF);
        c.drawCircle(cx, cy, R, p);
        p.setColor(0x99FFFFFF);
        c.drawCircle(cx + kx * R * 0.6f, cy + ky * R * 0.6f, R * 0.35f, p);
    }

    @Override public boolean onTouchEvent(MotionEvent e) {
        float cx = getWidth() / 2f, cy = getHeight() / 2f;
        float R = Math.min(cx, cy) * 0.95f * 0.6f;
        switch (e.getActionMasked()) {
            case MotionEvent.ACTION_DOWN:
            case MotionEvent.ACTION_MOVE: {
                float dx = (e.getX() - cx) / R, dy = (e.getY() - cy) / R;
                float len = (float) Math.sqrt(dx * dx + dy * dy);
                if (len > 1f) { dx /= len; dy /= len; }
                kx = dx; ky = dy;
                gv.jx = dx;
                gv.jy = -dy;
                break;
            }
            case MotionEvent.ACTION_UP:
            case MotionEvent.ACTION_CANCEL:
                kx = 0f; ky = 0f;
                gv.jx = 0f; gv.jy = 0f;
                break;
        }
        invalidate();
        return true;
    }
}
