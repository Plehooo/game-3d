package com.ditz.adventure;

import android.app.Activity;
import android.app.AlertDialog;
import android.graphics.Color;
import android.graphics.Typeface;
import android.graphics.drawable.GradientDrawable;
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
import android.widget.ScrollView;
import android.widget.TextView;
import android.content.SharedPreferences;
import android.widget.Toast;

public class MainActivity extends Activity {
    private GameView gv;
    private TextView hud, objective, logo;
    private final Handler handler = new Handler(Looper.getMainLooper());
    private SharedPreferences prefs;

    private static final String[] CHARS={"Ksatria","Penyihir","Ninja"};
    private static final String[] SKINS={"Azure Knight","Crimson Rogue","Forest Ranger","Mystic Violet",
            "Golden Hero","Shadow Reaper","Cyber Wave","Neon Rose","Steel Guardian","Inferno"};
    private static final String[] PETS={"Tanpa","Kucing","Anjing","Naga","Phoenix"};
    private static final String[] MAPS={"Ditz City","Emerald Forest","Sahara Ruins","Frost Valley"};
    private static final int[] SKIN_COST={0,60,140,250,400,650,900,1200,1600,2200};
    private static final int[] PET_COST={0,80,180,350,700};

    private int dp(int v){return(int)(v*getResources().getDisplayMetrics().density+.5f);}
    private GradientDrawable bg(int fill,int stroke,int radius){
        GradientDrawable d=new GradientDrawable();d.setColor(fill);d.setCornerRadius(dp(radius));
        if(stroke!=0)d.setStroke(dp(1),stroke);return d;
    }
    private TextView text(String s,float size){
        TextView t=new TextView(this);t.setText(s);t.setTextColor(Color.WHITE);t.setTextSize(size);
        t.setTypeface(Typeface.create("sans",Typeface.BOLD));t.setGravity(Gravity.CENTER_VERTICAL);
        return t;
    }
    private Button button(String s){
        Button b=new Button(this);b.setText(s);b.setAllCaps(false);b.setTextColor(Color.WHITE);
        b.setTextSize(12);b.setTypeface(Typeface.DEFAULT_BOLD);b.setPadding(dp(8),0,dp(8),0);
        b.setBackground(bg(0xB9161D2B,0x6634D7FF,12));return b;
    }
    private void toast(String s){Toast.makeText(this,s,Toast.LENGTH_SHORT).show();}

    private final Runnable hudLoop=new Runnable(){
        @Override public void run(){
            int points=(int)Native.stat(0),hp=(int)Native.stat(1);
            int ch=(int)Native.stat(3),skin=(int)Native.stat(4),pet=(int)Native.stat(5),map=(int)Native.stat(6);
            float tod=Native.stat(2);
            String waktu=tod<.25f?"Pagi":tod<.5f?"Siang":tod<.75f?"Senja":"Malam";
            hud.setText("● "+hp+" HP     ◆ "+points+" POINTS     ◷ "+waktu+"\n"+
                    CHARS[ch]+"  •  "+SKINS[skin]+"  •  "+PETS[pet]+"  •  "+MAPS[map]);
            objective.setText(map==0 ? "CITY QUEST  •  Jelajahi plaza & kalahkan musuh" :
                    "WORLD QUEST  •  Jelajahi "+MAPS[map]+" dan cari musuh");
            if(prefs!=null && handler!=null)prefs.edit().putInt("points",(int)Native.stat(0)).apply();
            handler.postDelayed(this,500);
        }
    };

    @Override protected void onCreate(Bundle state){
        super.onCreate(state);
        getWindow().addFlags(WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON);
        prefs=getSharedPreferences("ditz_adventure",MODE_PRIVATE);

        FrameLayout root=new FrameLayout(this);
        gv=new GameView(this);
        root.addView(gv,new FrameLayout.LayoutParams(-1,-1));

        // Logo / branding
        logo=text("DITZ  ADVENTURE",18);
        logo.setGravity(Gravity.CENTER);
        logo.setLetterSpacing(.12f);
        logo.setBackground(bg(0xC9111725,0xAA34D7FF,18));
        FrameLayout.LayoutParams lp=new FrameLayout.LayoutParams(dp(245),dp(48),Gravity.TOP|Gravity.CENTER_HORIZONTAL);
        lp.setMargins(0,dp(10),0,0);root.addView(logo,lp);

        hud=text("",12);hud.setPadding(dp(12),dp(8),dp(12),dp(8));
        hud.setBackground(bg(0xB5101724,0x6634D7FF,16));
        FrameLayout.LayoutParams hp=new FrameLayout.LayoutParams(dp(330),dp(58),Gravity.TOP|Gravity.START);
        hp.setMargins(dp(12),dp(12),0,0);root.addView(hud,hp);

        objective=text("",11);objective.setPadding(dp(10),0,dp(10),0);objective.setBackground(bg(0x99101724,0,14));
        FrameLayout.LayoutParams op=new FrameLayout.LayoutParams(dp(390),dp(34),Gravity.TOP|Gravity.START);
        op.setMargins(dp(12),dp(78),0,0);root.addView(objective,op);

        JoyView joy=new JoyView(this,gv);
        FrameLayout.LayoutParams jp=new FrameLayout.LayoutParams(dp(180),dp(180),Gravity.BOTTOM|Gravity.START);
        jp.setMargins(dp(22),0,0,dp(22));root.addView(joy,jp);

        Button atk=button("⚔  SERANG");
        atk.setTextSize(17);atk.setBackground(bg(0xD9C62839,0xFFFF8891,60));
        atk.setOnClickListener(v->gv.fireAttack());
        FrameLayout.LayoutParams ap=new FrameLayout.LayoutParams(dp(132),dp(132),Gravity.BOTTOM|Gravity.END);
        ap.setMargins(0,0,dp(30),dp(30));root.addView(atk,ap);

        LinearLayout menu=new LinearLayout(this);menu.setOrientation(LinearLayout.HORIZONTAL);menu.setGravity(Gravity.CENTER);
        String[] labels={"SHOP","MAP","HERO","PET","MORE"};
        Button[] mb=new Button[labels.length];
        for(int i=0;i<labels.length;i++){mb[i]=button(labels[i]);menu.addView(mb[i],new LinearLayout.LayoutParams(dp(68),dp(44)));}
        FrameLayout.LayoutParams mp=new FrameLayout.LayoutParams(-2,dp(50),Gravity.TOP|Gravity.END);
        mp.setMargins(0,dp(8),dp(10),0);root.addView(menu,mp);

        mb[0].setOnClickListener(v->showShop());
        mb[1].setOnClickListener(v->showMap());
        mb[2].setOnClickListener(v->nextCharacter());
        mb[3].setOnClickListener(v->nextPet());
        mb[4].setOnClickListener(v->showMore());

        TextView hint=text("MOVE  •  DRAG CAMERA  •  ATTACK  •  EXPLORE",10);
        hint.setGravity(Gravity.CENTER);hint.setBackground(bg(0x76101724,0,14));
        FrameLayout.LayoutParams hpp=new FrameLayout.LayoutParams(dp(330),dp(30),Gravity.BOTTOM|Gravity.CENTER_HORIZONTAL);
        hpp.setMargins(0,0,0,dp(10));root.addView(hint,hpp);

        setContentView(root);
        gv.queueEvent(()->{
            Native.setPoints(prefs.getInt("points",120));
            Native.setChar(prefs.getInt("char",0));
            Native.setSkin(ownedSkin());
            Native.setPet(ownedPet());
            Native.setMap(prefs.getInt("map",0));
        });
    }

    private int ownedSkin(){
        int s=prefs.getInt("skin",0);
        return prefs.getBoolean("skin_"+s,true)?s:0;
    }
    private int ownedPet(){
        int p=prefs.getInt("pet",0);
        return prefs.getBoolean("pet_"+p,true)?p:0;
    }
    private void nextCharacter(){
        int n=((int)Native.stat(3)+1)%CHARS.length; prefs.edit().putInt("char",n).apply();
        gv.queueEvent(()->Native.setChar(n)); toast("Hero: "+CHARS[n]);
    }
    private void nextPet(){ showPetShop(); }

    private LinearLayout dialogBox(String title,String subtitle){
        LinearLayout box=new LinearLayout(this);box.setOrientation(LinearLayout.VERTICAL);
        box.setPadding(dp(18),dp(14),dp(18),dp(18));box.setBackground(bg(0xFF0C1320,0xFF2D4058,22));
        TextView t=text(title,23);t.setTextColor(0xFF64E5FF);box.addView(t,new LinearLayout.LayoutParams(-1,dp(44)));
        TextView st=text(subtitle,11);st.setTextColor(0xFFB9C7D8);box.addView(st,new LinearLayout.LayoutParams(-1,dp(34)));
        return box;
    }

    private void showShop(){showShop(false);}
    private void showShop(boolean pets){
        LinearLayout box=dialogBox(pets?"PET SHOP":"SKIN SHOP",pets?"Companion cosmetic collection":"Customize your hero • purchases are permanent");
        ScrollView scroll=new ScrollView(this);LinearLayout list=new LinearLayout(this);list.setOrientation(LinearLayout.VERTICAL);
        int count=pets?PETS.length:SKINS.length;
        for(int i=0;i<count;i++){
            final int idx=i;int cost=pets?PET_COST[i]:SKIN_COST[i];
            String name=pets?PETS[i]:SKINS[i];
            boolean owned=prefs.getBoolean((pets?"pet_":"skin_")+i,i==0);
            LinearLayout row=new LinearLayout(this);row.setGravity(Gravity.CENTER_VERTICAL);row.setPadding(dp(10),dp(8),dp(10),dp(8));
            row.setBackground(bg(0xFF121C2A,0x332D4058,16));
            TextView n=text(name,14);n.setText((i==0?"★ ":"✦ ")+name+(owned?"  ✓":""));row.addView(n,new LinearLayout.LayoutParams(0,dp(56),1));
            Button b=button(owned?"EQUIP":(cost+" PTS"));
            row.addView(b,new LinearLayout.LayoutParams(dp(105),dp(44)));
            if(owned)b.setOnClickListener(v->{
                if(pets){prefs.edit().putInt("pet",idx).apply();gv.queueEvent(()->Native.setPet(idx));toast("Pet equipped: "+name);}
                else {prefs.edit().putInt("skin",idx).apply();gv.queueEvent(()->Native.setSkin(idx));toast("Skin equipped: "+name);}
            });
            else b.setOnClickListener(v->{
                if(!Native.spendPoints(cost)){toast("Point belum cukup.");return;}
                prefs.edit().putBoolean((pets?"pet_":"skin_")+idx,true)
                        .putInt(pets?"pet":"skin",idx).apply();
                if(pets)gv.queueEvent(()->Native.setPet(idx));else gv.queueEvent(()->Native.setSkin(idx));
                toast("Unlocked: "+name+" ✨"); showShop(pets);
            });
            list.addView(row,new LinearLayout.LayoutParams(-1,dp(66)));
            LinearLayout.LayoutParams sp=new LinearLayout.LayoutParams(-1,dp(8));list.addView(new View(this),sp);
        }
        scroll.addView(list);box.addView(scroll,new LinearLayout.LayoutParams(-1,dp(360)));
        LinearLayout actions=new LinearLayout(this);actions.setGravity(Gravity.CENTER);
        Button skin=button("SKINS"),pet=button("PETS"),close=button("CLOSE");
        skin.setOnClickListener(v->showShop(false));
        pet.setOnClickListener(v->showShop(true));close.setOnClickListener(v->{});actions.addView(skin);actions.addView(pet);actions.addView(close);
        box.addView(actions,new LinearLayout.LayoutParams(-1,dp(52)));
        AlertDialog d=new AlertDialog.Builder(this).setView(box).create();
        close.setOnClickListener(v->d.dismiss());
        d.setOnShowListener(x->{WindowManager.LayoutParams w=d.getWindow().getAttributes();w.dimAmount=.65f;d.getWindow().setAttributes(w);d.getWindow().addFlags(WindowManager.LayoutParams.FLAG_DIM_BEHIND);});
        d.show();
    }

    private void showPetShop(){showShop(true);}

    private void showMap(){
        LinearLayout box=dialogBox("WORLD MAP","4 regions • fast travel • each world changes the atmosphere");
        LinearLayout list=new LinearLayout(this);list.setOrientation(LinearLayout.VERTICAL);
        for(int i=0;i<MAPS.length;i++){
            final int idx=i;Button b=button((i==(int)Native.stat(6)?"● ":"○ ")+MAPS[i]+(i==0?"  • CITY":""));
            b.setTextSize(15);b.setGravity(Gravity.START|Gravity.CENTER_VERTICAL);b.setPadding(dp(18),0,0,0);
            b.setOnClickListener(v->{prefs.edit().putInt("map",idx).apply();gv.queueEvent(()->Native.setMap(idx));toast("Travelled to "+MAPS[idx]+" 🌍");});
            list.addView(b,new LinearLayout.LayoutParams(-1,dp(58)));LinearLayout.LayoutParams sp=new LinearLayout.LayoutParams(-1,dp(9));list.addView(new View(this),sp);
        }
        TextView info=text("CITY: roads, plaza, shops, buildings & NPCs\nFOREST: dense green exploration\nDESERT: ruins & sand\nSNOW: icy world",11);
        info.setPadding(dp(8),dp(10),dp(8),dp(10));box.addView(list);box.addView(info);
        new AlertDialog.Builder(this).setView(box).setPositiveButton("DONE",null).show();
    }

    private void showMore(){
        LinearLayout box=dialogBox("DITZ ADVENTURE","Built with Java + C++ NDK • OpenGL ES 2.0");
        TextView info=text("⭐ Explore a living world\n⚔ Defeat enemies to earn points\n🛍 Collect skins & companions\n🏙 Visit Ditz City and meet NPCs\n🌙 Dynamic day / night cycle\n☁ Clouds + stars + weather-style lighting",14);
        info.setPadding(dp(8),dp(12),dp(8),dp(12));box.addView(info);
        new AlertDialog.Builder(this).setView(box).setPositiveButton("PLAY",null).show();
    }

    private void immersive(){
        getWindow().getDecorView().setSystemUiVisibility(
                View.SYSTEM_UI_FLAG_FULLSCREEN|View.SYSTEM_UI_FLAG_HIDE_NAVIGATION|
                View.SYSTEM_UI_FLAG_IMMERSIVE_STICKY|View.SYSTEM_UI_FLAG_LAYOUT_STABLE|
                View.SYSTEM_UI_FLAG_LAYOUT_FULLSCREEN|View.SYSTEM_UI_FLAG_LAYOUT_HIDE_NAVIGATION);
    }
    @Override public void onWindowFocusChanged(boolean has){super.onWindowFocusChanged(has);if(has)immersive();}
    @Override protected void onResume(){super.onResume();immersive();gv.onResume();handler.post(hudLoop);}
    @Override protected void onPause(){super.onPause();handler.removeCallbacks(hudLoop);gv.onPause();}
}
