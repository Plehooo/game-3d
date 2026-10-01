package com.ditz.adventure;

import android.app.Activity;
import android.graphics.Color;
import android.os.Bundle;
import android.os.Handler;
import android.os.Looper;
import android.view.Gravity;
import android.view.View;
import android.view.ViewGroup;
import android.view.WindowManager;
import android.widget.Button;
import android.widget.FrameLayout;
import android.widget.LinearLayout;
import android.widget.TextView;
import android.widget.Toast;

public class MainActivity extends Activity {
    private GameView gv;
    private TextView hud;
    private final Handler handler = new Handler(Looper.getMainLooper());

    private static final String[] CHARS = {"Ksatria", "Penyihir", "Ninja"};
    private static final String[] SKINS = {"Biru", "Merah", "Hijau", "Ungu", "Emas", "Hitam"};
    private static final String[] PETS = {"Tanpa", "Kucing", "Anjing", "Naga"};
    private static final String[] MAPS = {"Hutan", "Gurun", "Salju"};
    private static final int[] PET_COST = {0, 30, 80, 150};

    private final Runnable hudLoop = new Runnable() {
        @Override public void run() {
            float tod = Native.stat(2);
            String waktu = tod < 0.5f ? "Siang" : "Malam";
            hud.setText("Poin: " + (int) Native.stat(0)
                    + "   HP: " + (int) Native.stat(1)
                    + "   " + waktu
                    + "\n" + CHARS[(int) Native.stat(3)]
                    + " | Skin " + SKINS[(int) Native.stat(4)]
                    + " | Pet " + PETS[(int) Native.stat(5)]
                    + " | " + MAPS[(int) Native.stat(6)]);
            handler.postDelayed(this, 200);
        }
    };

    private int dp(int v) {
        return (int) (v * getResources().getDisplayMetrics().density);
    }

    private Button makeBtn(String t) {
        Button b = new Button(this);
        b.setText(t);
        b.setAllCaps(false);
        b.setTextColor(Color.WHITE);
        b.setBackgroundColor(0x88000000);
        return b;
    }

    @Override protected void onCreate(Bundle s) {
        super.onCreate(s);
        getWindow().addFlags(WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON);

        FrameLayout root = new FrameLayout(this);
        gv = new GameView(this);
        root.addView(gv, new FrameLayout.LayoutParams(-1, -1));

        hud = new TextView(this);
        hud.setTextColor(Color.WHITE);
        hud.setTextSize(14f);
        hud.setBackgroundColor(0x88000000);
        hud.setPadding(dp(8), dp(4), dp(8), dp(4));
        FrameLayout.LayoutParams hp = new FrameLayout.LayoutParams(-2, -2, Gravity.TOP | Gravity.START);
        hp.setMargins(dp(8), dp(8), 0, 0);
        root.addView(hud, hp);

        JoyView joy = new JoyView(this, gv);
        FrameLayout.LayoutParams jp = new FrameLayout.LayoutParams(dp(170), dp(170), Gravity.BOTTOM | Gravity.START);
        jp.setMargins(dp(24), 0, 0, dp(24));
        root.addView(joy, jp);

        Button atk = makeBtn("SERANG");
        atk.setBackgroundColor(0xAAD32F2F);
        atk.setTextSize(16f);
        atk.setOnClickListener(v -> gv.fireAttack());
        FrameLayout.LayoutParams ap = new FrameLayout.LayoutParams(dp(120), dp(120), Gravity.BOTTOM | Gravity.END);
        ap.setMargins(0, 0, dp(30), dp(30));
        root.addView(atk, ap);

        LinearLayout top = new LinearLayout(this);
        top.setOrientation(LinearLayout.HORIZONTAL);
        Button bChar = makeBtn("Karakter");
        Button bSkin = makeBtn("Skin");
        Button bPet = makeBtn("Pet");
        Button bMap = makeBtn("Peta");
        top.addView(bChar); top.addView(bSkin); top.addView(bPet); top.addView(bMap);
        FrameLayout.LayoutParams tp = new FrameLayout.LayoutParams(-2, -2, Gravity.TOP | Gravity.END);
        tp.setMargins(0, dp(8), dp(8), 0);
        root.addView(top, tp);

        bChar.setOnClickListener(v -> {
            final int n = ((int) Native.stat(3) + 1) % CHARS.length;
            gv.queueEvent(() -> Native.setChar(n));
        });
        bSkin.setOnClickListener(v -> {
            final int n = ((int) Native.stat(4) + 1) % SKINS.length;
            int need = n * 50;
            if ((int) Native.stat(0) < need) {
                Toast.makeText(this, "Skin " + SKINS[n] + " terkunci, butuh " + need + " poin", Toast.LENGTH_SHORT).show();
                return;
            }
            gv.queueEvent(() -> Native.setSkin(n));
        });
        bPet.setOnClickListener(v -> {
            final int n = ((int) Native.stat(5) + 1) % PETS.length;
            int need = PET_COST[n];
            if ((int) Native.stat(0) < need) {
                Toast.makeText(this, "Pet " + PETS[n] + " terkunci, butuh " + need + " poin", Toast.LENGTH_SHORT).show();
                return;
            }
            gv.queueEvent(() -> Native.setPet(n));
        });
        bMap.setOnClickListener(v -> {
            final int n = ((int) Native.stat(6) + 1) % MAPS.length;
            gv.queueEvent(() -> Native.setMap(n));
        });

        setContentView(root);
    }

    private void immersive() {
        getWindow().getDecorView().setSystemUiVisibility(
                View.SYSTEM_UI_FLAG_FULLSCREEN | View.SYSTEM_UI_FLAG_HIDE_NAVIGATION
                        | View.SYSTEM_UI_FLAG_IMMERSIVE_STICKY | View.SYSTEM_UI_FLAG_LAYOUT_STABLE
                        | View.SYSTEM_UI_FLAG_LAYOUT_FULLSCREEN | View.SYSTEM_UI_FLAG_LAYOUT_HIDE_NAVIGATION);
    }

    @Override protected void onResume() {
        super.onResume();
        immersive();
        gv.onResume();
        handler.post(hudLoop);
    }

    @Override protected void onPause() {
        super.onPause();
        handler.removeCallbacks(hudLoop);
        gv.onPause();
    }
}
