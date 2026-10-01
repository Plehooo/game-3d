// Ditz Adventure 3D - mesin game native (OpenGL ES 2.0)
#include <jni.h>
#include <GLES2/gl2.h>
#include <math.h>
#include <string.h>
#include <stdlib.h>

// ---------- Matematika ----------
struct M4 { float m[16]; };
struct C { float r, g, b; };
struct V3 { float x, y, z; };

static M4 ident() { M4 r; memset(r.m, 0, sizeof(r.m)); r.m[0] = r.m[5] = r.m[10] = r.m[15] = 1; return r; }
static M4 mul(const M4& a, const M4& b) {
    M4 r;
    for (int c = 0; c < 4; c++)
        for (int row = 0; row < 4; row++) {
            float s = 0;
            for (int k = 0; k < 4; k++) s += a.m[k * 4 + row] * b.m[c * 4 + k];
            r.m[c * 4 + row] = s;
        }
    return r;
}
static M4 trans(float x, float y, float z) { M4 r = ident(); r.m[12] = x; r.m[13] = y; r.m[14] = z; return r; }
static M4 scl(float x, float y, float z) { M4 r = ident(); r.m[0] = x; r.m[5] = y; r.m[10] = z; return r; }
static M4 rotY(float a) { M4 r = ident(); float c = cosf(a), s = sinf(a); r.m[0] = c; r.m[8] = s; r.m[2] = -s; r.m[10] = c; return r; }
static M4 rotX(float a) { M4 r = ident(); float c = cosf(a), s = sinf(a); r.m[5] = c; r.m[9] = -s; r.m[6] = s; r.m[10] = c; return r; }
static M4 rotZ(float a) { M4 r = ident(); float c = cosf(a), s = sinf(a); r.m[0] = c; r.m[4] = -s; r.m[1] = s; r.m[5] = c; return r; }
static M4 persp(float fovy, float asp, float n, float f) {
    M4 r; memset(r.m, 0, sizeof(r.m));
    float t = 1.f / tanf(fovy * 0.5f);
    r.m[0] = t / asp; r.m[5] = t; r.m[10] = (f + n) / (n - f); r.m[11] = -1; r.m[14] = 2 * f * n / (n - f);
    return r;
}
static V3 nrm(V3 v) { float l = sqrtf(v.x * v.x + v.y * v.y + v.z * v.z); if (l < 1e-6f) l = 1; return {v.x / l, v.y / l, v.z / l}; }
static V3 cross(V3 a, V3 b) { return {a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x}; }
static float dot(V3 a, V3 b) { return a.x * b.x + a.y * b.y + a.z * b.z; }
static M4 lookAt(V3 eye, V3 ctr, V3 up) {
    V3 f = nrm({ctr.x - eye.x, ctr.y - eye.y, ctr.z - eye.z});
    V3 s = nrm(cross(f, up));
    V3 u = cross(s, f);
    M4 r = ident();
    r.m[0] = s.x; r.m[4] = s.y; r.m[8] = s.z;
    r.m[1] = u.x; r.m[5] = u.y; r.m[9] = u.z;
    r.m[2] = -f.x; r.m[6] = -f.y; r.m[10] = -f.z;
    r.m[12] = -dot(s, eye); r.m[13] = -dot(u, eye); r.m[14] = dot(f, eye);
    return r;
}
static float frand() { return (float)rand() / (float)RAND_MAX; }
static float lf(float a, float b, float t) { return a + (b - a) * t; }
static float clampf(float v, float a, float b) { return v < a ? a : (v > b ? b : v); }
static C lc(C a, C b, float t) { return {lf(a.r, b.r, t), lf(a.g, b.g, t), lf(a.b, b.b, t)}; }
static C mulc(C a, float k) { return {a.r * k, a.g * k, a.b * k}; }
static float wrapRel(float v, float p, float period) {
    float r = fmodf(v - p, period);
    if (r < 0) r += period;
    return p + r - period * 0.5f;
}
static float lerpAngle(float a, float b, float t) {
    float d = fmodf(b - a, 6.2831853f);
    if (d > 3.1415926f) d -= 6.2831853f;
    if (d < -3.1415926f) d += 6.2831853f;
    return a + d * t;
}

// ---------- Shader & mesh ----------
static GLuint prog = 0, vbo = 0;
static GLint aPos, aNrm, uMVP, uModel, uCol, uSunDir, uSunCol, uAmb, uFogCol, uUnlit;
static M4 gVP;
static float gW = 1, gH = 1;

static const char* VS =
    "attribute vec3 aPos;attribute vec3 aNrm;uniform mat4 uMVP;uniform mat4 uModel;"
    "varying vec3 vN;varying float vD;"
    "void main(){gl_Position=uMVP*vec4(aPos,1.0);vN=(uModel*vec4(aNrm,0.0)).xyz;vD=gl_Position.w;}";
static const char* FS =
    "precision mediump float;varying vec3 vN;varying float vD;"
    "uniform vec3 uCol;uniform vec3 uSunDir;uniform vec3 uSunCol;uniform vec3 uAmb;uniform vec3 uFogCol;uniform float uUnlit;"
    "void main(){vec3 n=normalize(vN);float d=max(dot(n,uSunDir),0.0);"
    "vec3 c=uCol*(uAmb+uSunCol*d);float f=clamp((vD-35.0)/75.0,0.0,1.0);c=mix(c,uFogCol,f);"
    "if(uUnlit>0.5)c=uCol;gl_FragColor=vec4(c,1.0);}";

static GLuint compile(GLenum type, const char* src) {
    GLuint s = glCreateShader(type);
    glShaderSource(s, 1, &src, NULL);
    glCompileShader(s);
    return s;
}

static void initGL() {
    GLuint vs = compile(GL_VERTEX_SHADER, VS), fs = compile(GL_FRAGMENT_SHADER, FS);
    prog = glCreateProgram();
    glAttachShader(prog, vs); glAttachShader(prog, fs);
    glLinkProgram(prog);
    aPos = glGetAttribLocation(prog, "aPos");
    aNrm = glGetAttribLocation(prog, "aNrm");
    uMVP = glGetUniformLocation(prog, "uMVP");
    uModel = glGetUniformLocation(prog, "uModel");
    uCol = glGetUniformLocation(prog, "uCol");
    uSunDir = glGetUniformLocation(prog, "uSunDir");
    uSunCol = glGetUniformLocation(prog, "uSunCol");
    uAmb = glGetUniformLocation(prog, "uAmb");
    uFogCol = glGetUniformLocation(prog, "uFogCol");
    uUnlit = glGetUniformLocation(prog, "uUnlit");

    float n[6][3] = {{1,0,0},{-1,0,0},{0,1,0},{0,-1,0},{0,0,1},{0,0,-1}};
    float t[6][3] = {{0,0,-1},{0,0,1},{1,0,0},{1,0,0},{1,0,0},{-1,0,0}};
    float data[36 * 6];
    int idx[6] = {0, 1, 2, 0, 2, 3};
    float ca[4] = {-1, 1, 1, -1}, cb[4] = {-1, -1, 1, 1};
    int o = 0;
    for (int f = 0; f < 6; f++) {
        V3 N = {n[f][0], n[f][1], n[f][2]};
        V3 U = {t[f][0], t[f][1], t[f][2]};
        V3 Vv = cross(N, U);
        for (int k = 0; k < 6; k++) {
            int c = idx[k];
            data[o++] = 0.5f * (N.x + ca[c] * U.x + cb[c] * Vv.x);
            data[o++] = 0.5f * (N.y + ca[c] * U.y + cb[c] * Vv.y);
            data[o++] = 0.5f * (N.z + ca[c] * U.z + cb[c] * Vv.z);
            data[o++] = N.x; data[o++] = N.y; data[o++] = N.z;
        }
    }
    glGenBuffers(1, &vbo);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(data), data, GL_STATIC_DRAW);
}

static void drawCube(const M4& model, C col, float unlit) {
    M4 mvp = mul(gVP, model);
    glUniformMatrix4fv(uMVP, 1, GL_FALSE, mvp.m);
    glUniformMatrix4fv(uModel, 1, GL_FALSE, model.m);
    glUniform3f(uCol, col.r, col.g, col.b);
    glUniform1f(uUnlit, unlit);
    glDrawArrays(GL_TRIANGLES, 0, 36);
}
// kotak dengan pusat (ox,oy,oz) dan ukuran (sx,sy,sz) relatif terhadap matriks m
static void boxM(const M4& m, float ox, float oy, float oz, float sx, float sy, float sz, C col, float unlit = 0.f) {
    drawCube(mul(m, mul(trans(ox, oy, oz), scl(sx, sy, sz))), col, unlit);
}
static M4 pivotM(const M4& base, float px, float py, float pz, float a) {
    return mul(base, mul(trans(px, py, pz), rotX(a)));
}


// ---------- Data game ----------
struct Skin { C body, accent, skin; };
static const Skin SK[10] = {
    {{.18f,.45f,.95f},{1.f,.82f,.18f},{.96f,.78f,.62f}},
    {{.86f,.16f,.16f},{.12f,.12f,.16f},{.92f,.70f,.52f}},
    {{.18f,.72f,.30f},{.55f,.30f,.08f},{.82f,.60f,.42f}},
    {{.58f,.22f,.86f},{1.f,.78f,.20f},{.96f,.80f,.70f}},
    {{.96f,.68f,.08f},{.92f,.94f,1.f},{.92f,.74f,.56f}},
    {{.08f,.09f,.13f},{.95f,.10f,.12f},{.84f,.68f,.52f}},
    {{.08f,.68f,.72f},{.92f,.95f,1.f},{.95f,.78f,.64f}},
    {{.95f,.28f,.60f},{.20f,.85f,.95f},{.98f,.82f,.68f}},
    {{.32f,.34f,.42f},{.72f,.76f,.86f},{.90f,.72f,.55f}},
    {{.75f,.15f,.08f},{1.f,.48f,.08f},{.94f,.68f,.52f}}
};
static const C GROUND[4] = {{.30f,.48f,.34f},{.22f,.58f,.24f},{.86f,.70f,.43f},{.78f,.86f,.92f}};
static const C ENEMYC[4] = {{.28f,.82f,.38f},{.26f,.74f,.32f},{.92f,.48f,.12f},{.50f,.72f,.92f}};
static const C BLACK={.035f,.04f,.055f}, RED={.92f,.10f,.12f}, WHITE={.95f,.97f,1.f};
static const C SILVER={.78f,.82f,.90f}, BROWN={.42f,.25f,.10f};
static const C ROAD={.12f,.13f,.16f}, ROADLINE={.95f,.80f,.26f}, GLASS={.08f,.25f,.42f};
static const C BUILD[8]={{.32f,.38f,.48f},{.55f,.31f,.20f},{.18f,.42f,.52f},{.45f,.27f,.52f},
                         {.68f,.33f,.20f},{.25f,.46f,.34f},{.48f,.48f,.52f},{.72f,.58f,.28f}};

struct Enemy { float x,z,hp,cd,resp,ph; bool alive; };
struct Decor { float x,z,s; int k; };
struct Cloud { float x,z,y,s; };
struct NPC { float x,z,yaw,ph,s; int type; };
struct State {
    float x=0,z=0,yaw=0,camYaw=0,hp=100,points=120,tod=.12f,t=0,walk=0;
    float atkCD=0,atkAnim=0,px=0,pz=-2,pyaw=0;
    int ch=0,skin=0,pet=0,map=0;
};
static State G;
static Enemy E[10];
static Decor D[140];
static Cloud CL[18];
static NPC NPCS[12];
static V3 STARS[120];
static bool gameInit=false;

// ---------- Spawn / world ----------
static void spawnEnemy(Enemy& e){
    float a=frand()*6.2831853f,r=18+frand()*28;
    e.x=G.x+cosf(a)*r; e.z=G.z+sinf(a)*r;
    e.hp=4+G.map,e.cd=0,e.resp=0,e.ph=frand()*6.28f,e.alive=true;
}
static void initGame(){
    srand(24680);
    for(int i=0;i<140;i++){
        D[i].x=frand()*140; D[i].z=frand()*140;
        D[i].s=.7f+frand()*1.25f; D[i].k=i%6;
    }
    for(int i=0;i<18;i++){
        CL[i].x=frand()*240; CL[i].z=frand()*240;
        CL[i].y=34+frand()*18; CL[i].s=5+frand()*9;
    }
    for(int i=0;i<120;i++){
        float a=frand()*6.2831853f,h=.15f+.85f*frand();
        STARS[i]=nrm({cosf(a),h,sinf(a)});
    }
    for(int i=0;i<12;i++){
        NPCS[i].x=(frand()-.5f)*70; NPCS[i].z=(frand()-.5f)*70;
        NPCS[i].yaw=frand()*6.28f; NPCS[i].ph=frand()*6.28f;
        NPCS[i].s=.9f+frand()*.2f; NPCS[i].type=i%4;
    }
    for(int i=0;i<10;i++) spawnEnemy(E[i]);
    gameInit=true;
}

// ---------- Gameplay ----------
static void doAttack(){
    static const float rng[3]={3.2f,5.2f,2.6f},dmg[3]={2.f,1.2f,1.8f},cd[3]={.45f,.65f,.24f};
    G.atkCD=cd[G.ch]; G.atkAnim=.30f;
    int best=-1;float bd=1e9f;
    for(int i=0;i<10;i++) if(E[i].alive){
        float dx=E[i].x-G.x,dz=E[i].z-G.z,d=sqrtf(dx*dx+dz*dz);
        if(d<bd){bd=d;best=i;}
    }
    if(best>=0&&bd<rng[G.ch]+1.f) G.yaw=atan2f(E[best].x-G.x,E[best].z-G.z);
    float fx=sinf(G.yaw),fz=cosf(G.yaw);
    for(int i=0;i<10;i++){
        Enemy&e=E[i]; if(!e.alive)continue;
        float dx=e.x-G.x,dz=e.z-G.z,d=sqrtf(dx*dx+dz*dz);
        if(d>rng[G.ch])continue;
        if(d>1.2f&&(dx*fx+dz*fz)/d<.15f)continue;
        e.hp-=dmg[G.ch];
        if(d>.01f){e.x+=dx/d*.9f;e.z+=dz/d*.9f;}
        if(e.hp<=0){e.alive=false;e.resp=3.0f;G.points+=10+G.pet*4+G.map;}
    }
}
static void update(float dt,float jx,float jy,float camD,bool attack){
    G.camYaw+=camD;G.t+=dt;G.tod=fmodf(G.tod+dt/150.f,1.f);
    float fx=sinf(G.camYaw),fz=cosf(G.camYaw),rx=-cosf(G.camYaw),rz=sinf(G.camYaw);
    float len=sqrtf(jx*jx+jy*jy);if(len>1){jx/=len;jy/=len;len=1;}
    if(len>.1f){
        float wx=fx*jy+rx*jx,wz=fz*jy+rz*jx;
        float speed=6.f*(G.ch==2?1.28f:1.f)*len;
        G.x+=wx*speed*dt;G.z+=wz*speed*dt;
        G.yaw=lerpAngle(G.yaw,atan2f(wx,wz),clampf(dt*12,0,1));G.walk+=dt*10;
    }
    if(G.atkCD>0)G.atkCD-=dt;if(G.atkAnim>0)G.atkAnim-=dt;
    if(attack&&G.atkCD<=0)doAttack();
    float espd=2.0f+G.map*.35f;
    for(int i=0;i<10;i++){
        Enemy&e=E[i];
        if(!e.alive){e.resp-=dt;if(e.resp<=0)spawnEnemy(e);continue;}
        float dx=G.x-e.x,dz=G.z-e.z,d=sqrtf(dx*dx+dz*dz);
        if(d>90){spawnEnemy(e);continue;}
        if(d>1.1f){e.x+=dx/d*espd*dt;e.z+=dz/d*espd*dt;}
        if(e.cd>0)e.cd-=dt;
        if(d<1.5f&&e.cd<=0){G.hp-=7+G.map;e.cd=1.0f;}
    }
    if(G.hp<100)G.hp=fminf(100.f,G.hp+dt*.9f);
    if(G.hp<=0){G.hp=100;G.x=0;G.z=0;for(int i=0;i<10;i++)spawnEnemy(E[i]);}
    float tx=G.x-sinf(G.yaw)*1.7f-cosf(G.yaw)*1.1f;
    float tz=G.z-cosf(G.yaw)*1.7f+sinf(G.yaw)*1.1f;
    float dx=tx-G.px,dz=tz-G.pz,k=clampf(dt*4,0,1);
    if(dx*dx+dz*dz>.04f)G.pyaw=lerpAngle(G.pyaw,atan2f(dx,dz),clampf(dt*8,0,1));
    G.px+=dx*k;G.pz+=dz*k;
    for(int i=0;i<12;i++){
        NPC&n=NPCS[i];
        n.x+=sinf(G.t*.25f+n.ph)*dt*.04f;
        n.z+=cosf(G.t*.22f+n.ph)*dt*.04f;
        n.yaw+=sinf(G.t*.9f+n.ph)*dt*.18f;
    }
}

// ---------- Character models ----------
static void drawPlayer(){
    const Skin&s=SK[G.skin];M4 b=mul(trans(G.x,0,G.z),rotY(G.yaw));
    int ch=G.ch;float bw=.7f,bh=.9f,bd=.4f,legH=.72f;
    if(ch==1){bh=1.2f;bw=.75f;}if(ch==2){bw=.55f;bh=.82f;bd=.35f;}
    float sw=sinf(G.walk)*.8f,atk=clampf(G.atkAnim/.3f,0,1);
    C legc=mulc(s.body,.62f);
    boxM(pivotM(b,-.18f,legH,0,sw),0,-legH/2,0,.25f,legH,.25f,legc);
    boxM(pivotM(b,.18f,legH,0,-sw),0,-legH/2,0,.25f,legH,.25f,legc);
    boxM(b,0,legH+bh/2,0,bw,bh,bd,s.body);
    boxM(b,0,legH+.12f,0,bw+.03f,.12f,bd+.03f,s.accent);
    float hy=legH+bh+.28f;
    boxM(b,0,hy,0,.5f,.5f,.5f,s.skin);
    boxM(b,-.12f,hy+.05f,.26f,.08f,.08f,.02f,BLACK,1);
    boxM(b,.12f,hy+.05f,.26f,.08f,.08f,.02f,BLACK,1);
    float ay=legH+bh-.05f,swing=sinf(G.walk)*.7f;
    boxM(pivotM(b,-(bw/2+.12f),ay,0,-swing),0,-.35f,0,.22f,.7f,.22f,s.skin);
    M4 R=pivotM(b,bw/2+.12f,ay,0,swing-atk*2.2f);
    boxM(R,0,-.35f,0,.22f,.7f,.22f,s.skin);
    if(ch==0){
        boxM(b,0,hy+.30f,0,.56f,.18f,.56f,SILVER);boxM(b,0,hy+.50f,0,.1f,.2f,.3f,RED);
        boxM(R,0,-.7f,.6f,.07f,.07f,1.f,SILVER);boxM(R,0,-.7f,.1f,.25f,.08f,.08f,s.accent);
    }else if(ch==1){
        boxM(b,0,hy+.3f,0,.75f,.1f,.75f,s.accent);boxM(b,0,hy+.6f,0,.45f,.5f,.45f,s.body);
        boxM(b,0,hy+.95f,0,.2f,.25f,.2f,s.body);boxM(R,0,-.4f,.2f,.08f,1.8f,.08f,BROWN);
        boxM(R,0,.55f,.2f,.22f,.22f,.22f,s.accent,1);
    }else{
        boxM(b,0,hy+.1f,0,.54f,.12f,.54f,s.accent);boxM(b,0,hy+.1f,-.4f,.12f,.08f,.35f,s.accent);
        boxM(R,0,-.75f,.25f,.06f,.06f,.5f,SILVER);
    }
    if(atk>0){
        float prog=1-atk;
        if(ch==1)boxM(b,0,1.6f,1+4*prog,.5f,.5f,.5f,{.6f,.8f,1.f},1);
        else boxM(b,0,1.3f,1.4f,1.8f*atk+.2f,.12f,.8f,{1.f,1.f,.8f},1);
    }
}
static void drawHumanoid(const M4&b,const C&body,const C&skin,float scale,float phase){
    float sw=sinf(G.t*5+phase)*.55f;
    boxM(pivotM(b,-.16f,.65f,0,sw),0,-.32f,0,.22f,.65f,.22f,mulc(body,.65f));
    boxM(pivotM(b,.16f,.65f,0,-sw),0,-.32f,0,.22f,.65f,.22f,mulc(body,.65f));
    boxM(b,0,.97f,0,.58f,.66f,.38f,body);
    boxM(b,0,1.48f,0,.42f,.42f,.42f,skin);
    boxM(b,-.10f,1.50f,.22f,.06f,.06f,.03f,BLACK,1);
    boxM(b,.10f,1.50f,.22f,.06f,.06f,.03f,BLACK,1);
    boxM(pivotM(b,-.37f,1.2f,0,-sw),0,-.25f,0,.18f,.55f,.18f,skin);
    boxM(pivotM(b,.37f,1.2f,0,sw),0,-.25f,0,.18f,.55f,.18f,skin);
}
static void drawNPCs(){
    for(int i=0;i<12;i++){
        NPC&n=NPCS[i];float wx=wrapRel(n.x,G.x,100),wz=wrapRel(n.z,G.z,100);
        M4 b=mul(trans(wx,0,wz),rotY(n.yaw));C body=SK[(i+1)%10].body,skin=SK[(i+2)%10].skin;
        drawHumanoid(b,body,skin,n.s,n.ph);
        if(G.map==0 && i<4){
            boxM(b,0,1.85f,0,.70f,.12f,.08f,{.08f,.12f,.16f},1);
            boxM(b,0,1.85f,.07f,.45f,.05f,.03f,ROADLINE,1);
        }
    }
}

// ---------- Pets ----------
static void drawPet(){
    if(G.pet==0)return;
    float bob=fabsf(sinf(G.t*8))*.08f,lift=(G.pet==3)?1.25f+sinf(G.t*3)*.15f:0;
    M4 b=mul(trans(G.px,lift,G.pz),rotY(G.pyaw));
    if(G.pet==1){
        C c={.95f,.55f,.15f};boxM(b,0,.35f+bob,0,.35f,.3f,.7f,c);boxM(b,0,.5f+bob,.42f,.3f,.28f,.28f,c);
        boxM(b,-.1f,.7f+bob,.42f,.08f,.12f,.06f,c);boxM(b,.1f,.7f+bob,.42f,.08f,.12f,.06f,c);
        boxM(b,0,.55f+bob,-.45f,.07f,.07f,.5f,c);
    }else if(G.pet==2){
        C c={.55f,.35f,.15f};boxM(b,0,.5f+bob,0,.4f,.4f,.9f,c);boxM(b,0,.7f+bob,.55f,.35f,.35f,.35f,c);
        boxM(b,0,.62f+bob,.8f,.18f,.15f,.2f,mulc(c,.7f));boxM(b,0,.7f+bob,-.55f,.08f,.08f,.45f,c);
    }else if(G.pet==3){
        C c={.2f,.8f,.4f},wc={.6f,.95f,.7f};boxM(b,0,.3f,0,.4f,.4f,.9f,c);
        boxM(b,0,.45f,.6f,.3f,.3f,.35f,c);boxM(b,0,.3f,-.7f,.12f,.12f,.6f,mulc(c,.8f));
        float fl=sinf(G.t*10)*.6f;M4 wl=mul(b,mul(trans(-.2f,.5f,0),rotZ(fl))),wr=mul(b,mul(trans(.2f,.5f,0),rotZ(-fl)));
        boxM(wl,-.45f,0,0,.9f,.05f,.5f,wc);boxM(wr,.45f,0,0,.9f,.05f,.5f,wc);
    }else{
        C c={.8f,.2f,.85f};boxM(b,0,.35f+bob,0,.45f,.35f,.55f,c);boxM(b,0,.6f+bob,.38f,.3f,.28f,.3f,c);
    }
}

// ---------- World decoration ----------
static void drawRoads(){
    if(G.map!=0)return;
    // city blocks around the player
    C sidewalk={.42f,.43f,.46f};
    for(int zc=-2;zc<=2;zc++){
        float z=zc*22.0f;
        boxM(trans(G.x,-.46f,G.z+z),0,0,0,110,.08f,8.f,ROAD);
        boxM(trans(G.x,-.40f,G.z+z),0,0,0,.8f,.03f,8.f,sidewalk);
        for(int x=-5;x<=5;x++)boxM(trans(G.x+x*10.0f,-.35f,G.z+z),0,0,0,2.8f,.03f,.12f,ROADLINE,1);
    }
    for(int xc=-2;xc<=2;xc++)boxM(trans(G.x+xc*22.f,-.45f,G.z),0,0,0,8.f,.08f,110.f,ROAD);
}
static void drawBuilding(float x,float z,float s,int idx){
    C bc=BUILD[idx%8];M4 b=trans(x,0,z);
    boxM(b,0,3.2f,0,7.0f*s,6.4f*s,6.5f*s,bc);
    boxM(b,0,.22f,3.28f,7.4f*s,.44f,.22f,{.20f,.22f,.25f});
    // windows front/back
    for(int q=-2;q<=2;q++)boxM(b,q*1.15f*s,3.2f,3.31f,.72f*s,1.05f*s,.10f,GLASS,1);
    for(int q=-2;q<=2;q++)boxM(b,q*1.15f*s,5.0f,-3.31f,.72f*s,1.05f*s,.10f,GLASS,1);
    // roof
    boxM(b,0,6.55f,0,7.4f*s,.35f,6.9f*s,mulc(bc,.72f));
}
static void drawLamp(float x,float z){
    M4 b=trans(x,0,z);boxM(b,0,2.3f,0,.10f,4.6f,.10f,BLACK);
    boxM(b,0,4.55f,0,.65f,.22f,.65f,{.95f,.85f,.45f},1);
}
static void drawCity(){
    drawRoads();
    for(int i=0;i<12;i++){
        float x=wrapRel((i-6)*15.f,G.x,100),z=wrapRel((i%3-1)*32.f,G.z,100);
        drawBuilding(x,z,1.f+(i%3)*.14f,i);
    }
    for(int i=-4;i<=4;i++)drawLamp(wrapRel(i*18.f,G.x,100),wrapRel(11.f,G.z,100));
    // central plaza + fountain
    boxM(trans(G.x,-.37f,G.z),0,0,0,12,.08f,12,{.34f,.34f,.38f});
    boxM(trans(G.x,-.25f,G.z),0,0,0,3.4f,.35f,3.4f,SILVER);
    boxM(trans(G.x,-.05f,G.z),0,0,0,2.2f,.22f,2.2f,{.12f,.34f,.55f});
    boxM(trans(G.x,1.0f,G.z),0,0,0,.35f,2.f,.35f,{.65f,.82f,.92f},1);
    boxM(trans(G.x,2.0f,G.z),0,0,0,1.1f,.20f,1.1f,{.3f,.6f,.9f},1);

    // Two recognizable 3D storefronts beside the plaza.
    M4 shop1=trans(G.x+9.f,0,G.z-9.f);
    boxM(shop1,0,2.0f,0,6.0f,4.0f,5.0f,{.52f,.18f,.30f});
    boxM(shop1,0,4.15f,2.55f,6.4f,.45f,.35f,{.95f,.76f,.18f},1);
    boxM(shop1,0,2.1f,2.56f,3.5f,1.9f,.12f,GLASS,1);
    boxM(shop1,-2.1f,5.0f,0,.55f,.8f,.55f,{.30f,.82f,.95f},1);
    M4 shop2=trans(G.x-10.f,0,G.z+7.f);
    boxM(shop2,0,2.1f,0,6.4f,4.2f,5.3f,{.20f,.36f,.56f});
    boxM(shop2,0,4.35f,2.7f,6.8f,.40f,.35f,{.92f,.22f,.62f},1);
    boxM(shop2,0,2.0f,2.71f,3.8f,2.0f,.12f,GLASS,1);
}
static void drawDecor(){
    if(G.map==0){drawCity();return;}
    C g=GROUND[G.map],patch=mulc(g,.83f);
    for(int i=0;i<140;i++){
        Decor&d=D[i];float wx=wrapRel(d.x,G.x,140),wz=wrapRel(d.z,G.z,140);M4 b=trans(wx,0,wz);float s=d.s;
        if(d.k==5){boxM(b,0,.02f,0,2.5f*s,.04f,2.5f*s,patch);continue;}
        if(G.map==1){
            if(d.k<3){boxM(b,0,s,0,.45f,2*s,.45f,BROWN);boxM(b,0,2*s+.8f*s,0,2.1f*s,1.7f*s,2.1f*s,{.12f,.46f,.17f});}
            else boxM(b,0,.4f*s,0,1.2f*s,.8f*s,1.2f*s,{.11f,.36f,.14f});
        }else if(G.map==2){
            if(d.k%2==0){boxM(b,0,1.1f*s,0,.55f,2.2f*s,.55f,{.24f,.58f,.27f});boxM(b,.48f,1.35f*s,0,.5f,.2f,.2f,{.24f,.58f,.27f});}
            else boxM(b,0,.35f*s,0,1.4f*s,.7f*s,1.1f*s,{.56f,.48f,.38f});
        }else{
            boxM(b,0,.75f*s,0,.5f,1.5f*s,.5f,BROWN);boxM(b,0,2.3f*s,0,2*s,1.5f*s,2*s,{.25f,.52f,.42f});
            boxM(b,0,3.3f*s,0,1.25f*s,1.0f*s,1.25f*s,WHITE);
        }
    }
}

// ---------- Rendering ----------
static void render(){
    float a=G.tod*6.2831853f,h=sinf(a);
    V3 sunDir=nrm({cosf(a)*.9f,h,.3f});
    float dayf=clampf((h+.15f)/.4f,0,1);
    C day={.35f,.68f,1.f},dusk={1.f,.48f,.25f},night={.015f,.025f,.085f},sky;
    if(h>.25f)sky=day;else if(h>0)sky=lc(dusk,day,h/.25f);else if(h>-.2f)sky=lc(dusk,night,-h/.2f);else sky=night;
    C sunC=lc({1.f,.50f,.25f},{1.f,.97f,.90f},clampf(h*2.5f,0,1));
    C amb=lc({.10f,.12f,.22f},{.52f,.54f,.60f},dayf);
    V3 ld=(h>-.05f)?sunDir:nrm({-sunDir.x,-sunDir.y,-sunDir.z});
    C lightC=(h>-.05f)?mulc(sunC,dayf):C{.18f,.20f,.36f};
    if(h>-.05f&&h<.05f)lightC=mulc(sunC,.4f);
    glViewport(0,0,(GLsizei)gW,(GLsizei)gH);glClearColor(sky.r,sky.g,sky.b,1);
    glEnable(GL_DEPTH_TEST);glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);
    glUseProgram(prog);glBindBuffer(GL_ARRAY_BUFFER,vbo);
    glEnableVertexAttribArray(aPos);glEnableVertexAttribArray(aNrm);
    glVertexAttribPointer(aPos,3,GL_FLOAT,GL_FALSE,24,(void*)0);
    glVertexAttribPointer(aNrm,3,GL_FLOAT,GL_FALSE,24,(void*)12);
    glUniform3f(uSunDir,ld.x,ld.y,ld.z);glUniform3f(uSunCol,lightC.r,lightC.g,lightC.b);
    glUniform3f(uAmb,amb.r,amb.g,amb.b);glUniform3f(uFogCol,sky.r,sky.g,sky.b);
    float cy=G.camYaw;V3 eye={G.x-sinf(cy)*8.f,5.0f,G.z-cosf(cy)*8.f},ctr={G.x,1.25f,G.z};
    gVP=mul(persp(1.02f,gW/gH,.25f,500),lookAt(eye,ctr,{0,1,0}));
    boxM(trans(G.x+sunDir.x*90,sunDir.y*90,G.z+sunDir.z*90),0,0,0,9,9,9,{1.f,.88f,.35f},1);
    boxM(trans(G.x-sunDir.x*90,-sunDir.y*90,G.z-sunDir.z*90),0,0,0,6,6,6,{.88f,.92f,1.f},1);
    if(h<-.05f){float br=clampf((-h-.05f)/.2f,0,1);for(int i=0;i<120;i++)boxM(trans(G.x+STARS[i].x*150,STARS[i].y*150,G.z+STARS[i].z*150),0,0,0,.65f,.65f,.65f,{br,br,br},1);}
    float cb=.3f+.7f*dayf;for(int i=0;i<18;i++){float wx=wrapRel(CL[i].x+G.t*2,G.x,240),wz=wrapRel(CL[i].z,G.z,240);boxM(trans(wx,CL[i].y,wz),0,0,0,CL[i].s*1.6f,2,CL[i].s,{cb,cb,cb},1);}
    boxM(trans(G.x,-.5f,G.z),0,0,0,650,1,650,GROUND[G.map]);
    drawDecor();drawNPCs();drawPet();drawEnemies();drawPlayer();
}

// ---------- JNI ----------
extern "C" {
JNIEXPORT void JNICALL Java_com_ditz_adventure_Native_init(JNIEnv*,jclass){initGL();if(!gameInit)initGame();}
JNIEXPORT void JNICALL Java_com_ditz_adventure_Native_resize(JNIEnv*,jclass,jint w,jint h){gW=(float)w;gH=(float)(h>0?h:1);}
JNIEXPORT void JNICALL Java_com_ditz_adventure_Native_frame(JNIEnv*,jclass,jfloat dt,jfloat jx,jfloat jy,jfloat camD,jboolean atk){update(dt,jx,jy,camD,atk==JNI_TRUE);render();}
JNIEXPORT void JNICALL Java_com_ditz_adventure_Native_setChar(JNIEnv*,jclass,jint i){G.ch=((i%3)+3)%3;}
JNIEXPORT void JNICALL Java_com_ditz_adventure_Native_setSkin(JNIEnv*,jclass,jint i){G.skin=((i%10)+10)%10;}
JNIEXPORT void JNICALL Java_com_ditz_adventure_Native_setPet(JNIEnv*,jclass,jint i){G.pet=((i%5)+5)%5;}
JNIEXPORT void JNICALL Java_com_ditz_adventure_Native_setMap(JNIEnv*,jclass,jint i){G.map=((i%4)+4)%4;for(int k=0;k<10;k++)spawnEnemy(E[k]);}
JNIEXPORT void JNICALL Java_com_ditz_adventure_Native_setPoints(JNIEnv*,jclass,jfloat p){G.points=fmaxf(0,p);}
JNIEXPORT jboolean JNICALL Java_com_ditz_adventure_Native_spendPoints(JNIEnv*,jclass,jfloat cost){
    if(G.points+0.001f<cost)return JNI_FALSE;G.points-=cost;return JNI_TRUE;
}
JNIEXPORT jfloat JNICALL Java_com_ditz_adventure_Native_stat(JNIEnv*,jclass,jint i){
    switch(i){case 0:return G.points;case 1:return G.hp;case 2:return G.tod;case 3:return(float)G.ch;case 4:return(float)G.skin;case 5:return(float)G.pet;case 6:return(float)G.map;}
    return 0;
}
}
