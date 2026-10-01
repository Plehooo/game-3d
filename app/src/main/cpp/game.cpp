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
static const Skin SK[6] = {
    {{.2f, .4f, .9f}, {.95f, .85f, .2f}, {.95f, .8f, .65f}},
    {{.85f, .2f, .2f}, {.2f, .2f, .2f}, {.9f, .7f, .55f}},
    {{.2f, .7f, .3f}, {.6f, .4f, .1f}, {.8f, .6f, .45f}},
    {{.6f, .25f, .8f}, {.95f, .85f, .3f}, {.95f, .8f, .7f}},
    {{.95f, .78f, .2f}, {.9f, .9f, .95f}, {.9f, .75f, .6f}},
    {{.12f, .12f, .15f}, {.9f, .1f, .1f}, {.85f, .7f, .55f}},
};
static const C GROUND[3] = {{.28f, .62f, .25f}, {.86f, .74f, .45f}, {.93f, .96f, 1.f}};
static const C ENEMYC[3] = {{.3f, .8f, .3f}, {.9f, .5f, .15f}, {.5f, .8f, 1.f}};
static const C BLACK = {.05f, .05f, .05f};
static const C RED = {.9f, .1f, .1f};
static const C SILVER = {.8f, .82f, .88f};
static const C BROWN = {.4f, .25f, .1f};

struct Enemy { float x, z, hp, cd, resp, ph; bool alive; };
struct Decor { float x, z, s; int k; };
struct Cloud { float x, z, y, s; };
struct State {
    float x = 0, z = 0, yaw = 0, camYaw = 0, hp = 100, points = 0, tod = 0.08f, t = 0, walk = 0;
    float atkCD = 0, atkAnim = 0, px = 0, pz = -2, pyaw = 0;
    int ch = 0, skin = 0, pet = 0, map = 0;
};
static State G;
static Enemy E[8];
static Decor D[90];
static Cloud CL[14];
static V3 STARS[100];
static bool gameInit = false;

static void spawnEnemy(Enemy& e) {
    float a = frand() * 6.2831853f, r = 14 + frand() * 16;
    e.x = G.x + cosf(a) * r; e.z = G.z + sinf(a) * r;
    e.hp = 4; e.cd = 0; e.resp = 0; e.ph = frand() * 6; e.alive = true;
}

static void initGame() {
    srand(12345);
    for (int i = 0; i < 90; i++) {
        D[i].x = frand() * 100; D[i].z = frand() * 100;
        D[i].s = 0.8f + frand() * 0.9f;
        D[i].k = (i % 4 == 3) ? 3 : (i % 2);
    }
    for (int i = 0; i < 14; i++) {
        CL[i].x = frand() * 200; CL[i].z = frand() * 200;
        CL[i].y = 35 + frand() * 15; CL[i].s = 6 + frand() * 8;
    }
    for (int i = 0; i < 100; i++) {
        float a = frand() * 6.2831853f, h = 0.1f + 0.9f * frand();
        STARS[i] = nrm({cosf(a), h, sinf(a)});
    }
    for (int i = 0; i < 8; i++) spawnEnemy(E[i]);
    gameInit = true;
}

// ---------- Logika ----------
static void doAttack() {
    static const float rng[3] = {2.8f, 5.0f, 2.4f};
    static const float dmg[3] = {2.f, 1.f, 1.5f};
    static const float cd[3] = {0.5f, 0.7f, 0.25f};
    G.atkCD = cd[G.ch];
    G.atkAnim = 0.3f;
    // auto-aim ke musuh terdekat
    int best = -1; float bd = 1e9f;
    for (int i = 0; i < 8; i++) {
        if (!E[i].alive) continue;
        float dx = E[i].x - G.x, dz = E[i].z - G.z, d = sqrtf(dx * dx + dz * dz);
        if (d < bd) { bd = d; best = i; }
    }
    if (best >= 0 && bd < rng[G.ch] + 1.f) G.yaw = atan2f(E[best].x - G.x, E[best].z - G.z);
    float fx = sinf(G.yaw), fz = cosf(G.yaw);
    for (int i = 0; i < 8; i++) {
        Enemy& e = E[i];
        if (!e.alive) continue;
        float dx = e.x - G.x, dz = e.z - G.z, d = sqrtf(dx * dx + dz * dz);
        if (d > rng[G.ch]) continue;
        if (d > 1.2f && (dx * fx + dz * fz) / d < 0.2f) continue;
        e.hp -= dmg[G.ch];
        if (d > 0.01f) { e.x += dx / d * 0.8f; e.z += dz / d * 0.8f; }
        if (e.hp <= 0) { e.alive = false; e.resp = 3.f; G.points += 10 + G.pet * 3; }
    }
}

static void update(float dt, float jx, float jy, float camD, bool attack) {
    G.camYaw += camD;
    G.t += dt;
    G.tod = fmodf(G.tod + dt / 150.f, 1.f);

    float fx = sinf(G.camYaw), fz = cosf(G.camYaw);
    float rx = -cosf(G.camYaw), rz = sinf(G.camYaw);
    float len = sqrtf(jx * jx + jy * jy);
    if (len > 1.f) { jx /= len; jy /= len; len = 1.f; }
    if (len > 0.1f) {
        float wx = fx * jy + rx * jx, wz = fz * jy + rz * jx;
        float speed = 6.f * (G.ch == 2 ? 1.25f : 1.f) * len;
        G.x += wx * speed * dt; G.z += wz * speed * dt;
        G.yaw = lerpAngle(G.yaw, atan2f(wx, wz), clampf(dt * 12.f, 0.f, 1.f));
        G.walk += dt * 10.f;
    }
    if (G.atkCD > 0) G.atkCD -= dt;
    if (G.atkAnim > 0) G.atkAnim -= dt;
    if (attack && G.atkCD <= 0) doAttack();

    // musuh
    float espd = 2.2f + G.map * 0.3f;
    for (int i = 0; i < 8; i++) {
        Enemy& e = E[i];
        if (!e.alive) {
            e.resp -= dt;
            if (e.resp <= 0) spawnEnemy(e);
            continue;
        }
        float dx = G.x - e.x, dz = G.z - e.z, d = sqrtf(dx * dx + dz * dz);
        if (d > 70.f) { spawnEnemy(e); continue; }
        if (d > 1.1f) { e.x += dx / d * espd * dt; e.z += dz / d * espd * dt; }
        if (e.cd > 0) e.cd -= dt;
        if (d < 1.4f && e.cd <= 0) { G.hp -= 8.f; e.cd = 1.0f; }
    }
    if (G.hp < 100.f) G.hp = fminf(100.f, G.hp + dt);
    if (G.hp <= 0) {
        G.hp = 100; G.x = 0; G.z = 0;
        for (int i = 0; i < 8; i++) spawnEnemy(E[i]);
    }

    // hewan peliharaan mengikuti
    float tx = G.x - sinf(G.yaw) * 1.6f - cosf(G.yaw) * 1.2f;
    float tz = G.z - cosf(G.yaw) * 1.6f + sinf(G.yaw) * 1.2f;
    float dx = tx - G.px, dz = tz - G.pz;
    if (dx * dx + dz * dz > 0.04f) G.pyaw = lerpAngle(G.pyaw, atan2f(dx, dz), clampf(dt * 8.f, 0.f, 1.f));
    float k = clampf(dt * 4.f, 0.f, 1.f);
    G.px += dx * k; G.pz += dz * k;
}

// ---------- Render ----------
static void drawPlayer() {
    const Skin& s = SK[G.skin];
    M4 b = mul(trans(G.x, 0, G.z), rotY(G.yaw));
    int ch = G.ch;
    float bw = 0.7f, bh = 0.9f, bd = 0.4f, legH = 0.7f;
    if (ch == 1) { bh = 1.2f; bw = 0.75f; }
    if (ch == 2) { bw = 0.55f; bh = 0.8f; bd = 0.35f; }
    float sw = sinf(G.walk) * 0.8f;
    float atk = clampf(G.atkAnim / 0.3f, 0.f, 1.f);
    C legc = (ch == 1) ? s.body : mulc(s.body, 0.6f);

    boxM(pivotM(b, -0.18f, legH, 0, sw), 0, -legH / 2, 0, 0.25f, legH, 0.25f, legc);
    boxM(pivotM(b, 0.18f, legH, 0, -sw), 0, -legH / 2, 0, 0.25f, legH, 0.25f, legc);
    boxM(b, 0, legH + bh / 2, 0, bw, bh, bd, s.body);
    boxM(b, 0, legH + 0.12f, 0, bw + 0.02f, 0.12f, bd + 0.02f, s.accent);
    float hy = legH + bh + 0.28f;
    boxM(b, 0, hy, 0, 0.5f, 0.5f, 0.5f, s.skin);
    boxM(b, -0.12f, hy + 0.05f, 0.26f, 0.08f, 0.08f, 0.02f, BLACK);
    boxM(b, 0.12f, hy + 0.05f, 0.26f, 0.08f, 0.08f, 0.02f, BLACK);

    float ay = legH + bh - 0.05f;
    float swing = sinf(G.walk) * 0.7f;
    boxM(pivotM(b, -(bw / 2 + 0.12f), ay, 0, -swing), 0, -0.35f, 0, 0.22f, 0.7f, 0.22f, s.skin);
    M4 R = pivotM(b, bw / 2 + 0.12f, ay, 0, swing - atk * 2.2f);
    boxM(R, 0, -0.35f, 0, 0.22f, 0.7f, 0.22f, s.skin);

    if (ch == 0) {
        boxM(b, 0, hy + 0.3f, 0, 0.56f, 0.18f, 0.56f, SILVER);
        boxM(b, 0, hy + 0.5f, 0, 0.1f, 0.2f, 0.3f, RED);
        boxM(R, 0, -0.7f, 0.6f, 0.07f, 0.07f, 1.0f, SILVER);
        boxM(R, 0, -0.7f, 0.1f, 0.25f, 0.08f, 0.08f, s.accent);
    } else if (ch == 1) {
        boxM(b, 0, hy + 0.3f, 0, 0.75f, 0.1f, 0.75f, s.accent);
        boxM(b, 0, hy + 0.6f, 0, 0.45f, 0.5f, 0.45f, s.body);
        boxM(b, 0, hy + 0.95f, 0, 0.2f, 0.25f, 0.2f, s.body);
        boxM(R, 0, -0.4f, 0.2f, 0.08f, 1.8f, 0.08f, BROWN);
        boxM(R, 0, 0.55f, 0.2f, 0.22f, 0.22f, 0.22f, s.accent, 1.f);
    } else {
        boxM(b, 0, hy + 0.1f, 0, 0.54f, 0.12f, 0.54f, s.accent);
        boxM(b, 0, hy + 0.1f, -0.4f, 0.12f, 0.08f, 0.35f, s.accent);
        boxM(R, 0, -0.75f, 0.25f, 0.06f, 0.06f, 0.5f, SILVER);
    }
    // efek serangan
    if (atk > 0.f) {
        float prog = 1.f - atk;
        if (ch == 1) {
            float dist = 1.f + 4.f * prog;
            boxM(b, 0, 1.6f, dist, 0.5f, 0.5f, 0.5f, {0.6f, 0.8f, 1.f}, 1.f);
        } else {
            boxM(b, 0, 1.3f, 1.4f, 1.8f * atk + 0.2f, 0.12f, 0.8f, {1.f, 1.f, 0.8f}, 1.f);
        }
    }
}

static void drawEnemies() {
    for (int i = 0; i < 8; i++) {
        Enemy& e = E[i];
        if (!e.alive) continue;
        float bob = fabsf(sinf(G.t * 4.f + e.ph)) * 0.3f;
        M4 b = mul(trans(e.x, bob, e.z), rotY(atan2f(G.x - e.x, G.z - e.z)));
        C ec = ENEMYC[G.map];
        boxM(b, 0, 0.5f, 0, 1.0f, 0.9f, 1.0f, ec);
        boxM(b, 0, 0.1f, 0, 1.1f, 0.2f, 1.1f, mulc(ec, 0.7f));
        boxM(b, -0.25f, 0.7f, 0.51f, 0.15f, 0.15f, 0.05f, RED, 1.f);
        boxM(b, 0.25f, 0.7f, 0.51f, 0.15f, 0.15f, 0.05f, RED, 1.f);
        M4 hb = mul(trans(e.x, 1.9f, e.z), rotY(G.camYaw));
        float r = clampf(e.hp / 4.f, 0.f, 1.f);
        boxM(hb, 0, 0, 0, 1.0f, 0.12f, 0.05f, BLACK, 1.f);
        boxM(hb, -(1.f - r) / 2.f, 0, -0.03f, r, 0.12f, 0.05f, RED, 1.f);
    }
}

static void drawPet() {
    if (G.pet == 0) return;
    float bob = fabsf(sinf(G.t * 8.f)) * 0.08f;
    float lift = (G.pet == 3) ? 1.3f + sinf(G.t * 3.f) * 0.15f : 0.f;
    M4 b = mul(trans(G.px, lift, G.pz), rotY(G.pyaw));
    if (G.pet == 1) {
        C c = {.95f, .55f, .15f};
        boxM(b, 0, 0.35f + bob, 0, 0.35f, 0.3f, 0.7f, c);
        boxM(b, 0, 0.5f + bob, 0.42f, 0.3f, 0.28f, 0.28f, c);
        boxM(b, -0.1f, 0.7f + bob, 0.42f, 0.08f, 0.12f, 0.06f, c);
        boxM(b, 0.1f, 0.7f + bob, 0.42f, 0.08f, 0.12f, 0.06f, c);
        boxM(b, 0, 0.55f + bob, -0.45f, 0.07f, 0.07f, 0.5f, c);
        for (int i = 0; i < 4; i++)
            boxM(b, (i % 2 ? 0.12f : -0.12f), 0.1f, (i < 2 ? 0.25f : -0.25f), 0.1f, 0.2f, 0.1f, mulc(c, 0.8f));
    } else if (G.pet == 2) {
        C c = {.55f, .35f, .15f};
        boxM(b, 0, 0.5f + bob, 0, 0.4f, 0.4f, 0.9f, c);
        boxM(b, 0, 0.7f + bob, 0.55f, 0.35f, 0.35f, 0.35f, c);
        boxM(b, 0, 0.62f + bob, 0.8f, 0.18f, 0.15f, 0.2f, mulc(c, 0.7f));
        boxM(b, -0.2f, 0.65f + bob, 0.5f, 0.08f, 0.25f, 0.12f, mulc(c, 0.6f));
        boxM(b, 0.2f, 0.65f + bob, 0.5f, 0.08f, 0.25f, 0.12f, mulc(c, 0.6f));
        boxM(b, 0, 0.7f + bob, -0.55f, 0.08f, 0.08f, 0.45f, c);
        for (int i = 0; i < 4; i++)
            boxM(b, (i % 2 ? 0.15f : -0.15f), 0.15f, (i < 2 ? 0.3f : -0.3f), 0.12f, 0.3f, 0.12f, mulc(c, 0.8f));
    } else {
        C c = {.2f, .8f, .4f}, wc = {.6f, .95f, .7f};
        boxM(b, 0, 0.3f, 0, 0.4f, 0.4f, 0.9f, c);
        boxM(b, 0, 0.45f, 0.6f, 0.3f, 0.3f, 0.35f, c);
        boxM(b, 0, 0.3f, -0.7f, 0.12f, 0.12f, 0.6f, mulc(c, 0.8f));
        boxM(b, -0.08f, 0.6f, 0.65f, 0.05f, 0.05f, 0.05f, RED, 1.f);
        boxM(b, 0.08f, 0.6f, 0.65f, 0.05f, 0.05f, 0.05f, RED, 1.f);
        float fl = sinf(G.t * 10.f) * 0.6f;
        M4 wl = mul(b, mul(trans(-0.2f, 0.5f, 0), rotZ(fl)));
        M4 wr = mul(b, mul(trans(0.2f, 0.5f, 0), rotZ(-fl)));
        boxM(wl, -0.45f, 0, 0, 0.9f, 0.05f, 0.5f, wc);
        boxM(wr, 0.45f, 0, 0, 0.9f, 0.05f, 0.5f, wc);
    }
}

static void drawDecor() {
    C g = GROUND[G.map];
    C patch = mulc(g, 0.85f);
    for (int i = 0; i < 90; i++) {
        Decor& d = D[i];
        float wx = wrapRel(d.x, G.x, 100.f), wz = wrapRel(d.z, G.z, 100.f);
        M4 b = trans(wx, 0, wz);
        float s = d.s;
        if (d.k == 3) { boxM(b, 0, 0.02f, 0, 2.5f * s, 0.04f, 2.5f * s, patch); continue; }
        if (G.map == 0) {
            if (d.k == 0) {
                boxM(b, 0, s, 0, 0.5f, 2 * s, 0.5f, BROWN);
                boxM(b, 0, 2 * s + 0.9f * s, 0, 2.2f * s, 1.8f * s, 2.2f * s, {.15f, .5f, .18f});
            } else boxM(b, 0, 0.4f, 0, 1.2f, 0.8f, 1.2f, {.12f, .4f, .15f});
        } else if (G.map == 1) {
            if (d.k == 0) {
                boxM(b, 0, 1.1f * s, 0, 0.6f, 2.2f * s, 0.6f, {.2f, .6f, .25f});
                boxM(b, 0.5f, 1.3f * s, 0, 0.5f, 0.25f, 0.25f, {.2f, .6f, .25f});
            } else boxM(b, 0, 0.4f * s, 0, 1.4f * s, 0.8f * s, 1.1f * s, {.55f, .5f, .45f});
        } else {
            if (d.k == 0) {
                boxM(b, 0, s * 0.7f, 0, 0.5f, 1.4f * s, 0.5f, BROWN);
                boxM(b, 0, 2.2f * s, 0, 2.0f * s, 1.4f * s, 2.0f * s, {.15f, .45f, .3f});
                boxM(b, 0, 3.2f * s, 0, 1.2f * s, 1.0f * s, 1.2f * s, {.95f, .97f, 1.f});
            } else boxM(b, 0, 0.4f * s, 0, 1.4f * s, 0.8f * s, 1.1f * s, {.7f, .85f, .95f});
        }
    }
}

static void render() {
    // waktu
    float a = G.tod * 6.2831853f;
    float h = sinf(a);
    V3 sunDir = nrm({cosf(a) * 0.9f, h, 0.3f});
    float dayf = clampf((h + 0.15f) / 0.4f, 0.f, 1.f);
    C day = {.45f, .72f, 1.f}, dusk = {1.f, .55f, .3f}, night = {.02f, .03f, .1f}, sky;
    if (h > 0.25f) sky = day;
    else if (h > 0.f) sky = lc(dusk, day, h / 0.25f);
    else if (h > -0.2f) sky = lc(dusk, night, -h / 0.2f);
    else sky = night;

    C sunC = lc({1.f, .55f, .3f}, {1.f, .97f, .9f}, clampf(h * 2.5f, 0.f, 1.f));
    C amb = lc({.12f, .14f, .25f}, {.5f, .52f, .58f}, dayf);
    V3 ld = (h > -0.05f) ? sunDir : nrm({-sunDir.x, -sunDir.y, -sunDir.z});
    C lightC = (h > -0.05f) ? mulc(sunC, dayf) : C{.18f, .2f, .35f};
    if (h > -0.05f && h < 0.05f) lightC = mulc(sunC, 0.4f);

    glViewport(0, 0, (GLsizei)gW, (GLsizei)gH);
    glClearColor(sky.r, sky.g, sky.b, 1.f);
    glEnable(GL_DEPTH_TEST);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    glUseProgram(prog);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glEnableVertexAttribArray(aPos);
    glEnableVertexAttribArray(aNrm);
    glVertexAttribPointer(aPos, 3, GL_FLOAT, GL_FALSE, 24, (void*)0);
    glVertexAttribPointer(aNrm, 3, GL_FLOAT, GL_FALSE, 24, (void*)12);
    glUniform3f(uSunDir, ld.x, ld.y, ld.z);
    glUniform3f(uSunCol, lightC.r, lightC.g, lightC.b);
    glUniform3f(uAmb, amb.r, amb.g, amb.b);
    glUniform3f(uFogCol, sky.r, sky.g, sky.b);

    // kamera
    float cy = G.camYaw;
    V3 eye = {G.x - sinf(cy) * 7.f, 4.5f, G.z - cosf(cy) * 7.f};
    V3 ctr = {G.x, 1.2f, G.z};
    M4 proj = persp(1.05f, gW / gH, 0.3f, 500.f);
    gVP = mul(proj, lookAt(eye, ctr, {0, 1, 0}));

    // langit: matahari, bulan, bintang, awan
    boxM(trans(G.x + sunDir.x * 85, sunDir.y * 85, G.z + sunDir.z * 85), 0, 0, 0, 9, 9, 9, {1.f, .9f, .45f}, 1.f);
    boxM(trans(G.x - sunDir.x * 85, -sunDir.y * 85, G.z - sunDir.z * 85), 0, 0, 0, 6, 6, 6, {.9f, .92f, 1.f}, 1.f);
    if (h < -0.05f) {
        float br = clampf((-h - 0.05f) / 0.2f, 0.f, 1.f);
        for (int i = 0; i < 100; i++)
            boxM(trans(G.x + STARS[i].x * 150, STARS[i].y * 150, G.z + STARS[i].z * 150), 0, 0, 0, 0.8f, 0.8f, 0.8f, {br, br, br}, 1.f);
    }
    float cb = 0.3f + 0.7f * dayf;
    for (int i = 0; i < 14; i++) {
        float wx = wrapRel(CL[i].x + G.t * 2.f, G.x, 200.f), wz = wrapRel(CL[i].z, G.z, 200.f);
        boxM(trans(wx, CL[i].y, wz), 0, 0, 0, CL[i].s * 1.6f, 2.f, CL[i].s, {cb, cb, cb}, 1.f);
    }

    // tanah
    boxM(trans(G.x, -0.5f, G.z), 0, 0, 0, 600, 1, 600, GROUND[G.map]);
    drawDecor();
    drawPet();
    drawEnemies();
    drawPlayer();
}

// ---------- JNI ----------
extern "C" {
JNIEXPORT void JNICALL Java_com_ditz_adventure_Native_init(JNIEnv*, jclass) {
    initGL();
    if (!gameInit) initGame();
}
JNIEXPORT void JNICALL Java_com_ditz_adventure_Native_resize(JNIEnv*, jclass, jint w, jint h) {
    gW = (float)w; gH = (float)(h > 0 ? h : 1);
}
JNIEXPORT void JNICALL Java_com_ditz_adventure_Native_frame(JNIEnv*, jclass, jfloat dt, jfloat jx, jfloat jy, jfloat camD, jboolean atk) {
    update(dt, jx, jy, camD, atk == JNI_TRUE);
    render();
}
JNIEXPORT void JNICALL Java_com_ditz_adventure_Native_setChar(JNIEnv*, jclass, jint i) { G.ch = i % 3; }
JNIEXPORT void JNICALL Java_com_ditz_adventure_Native_setSkin(JNIEnv*, jclass, jint i) { G.skin = i % 6; }
JNIEXPORT void JNICALL Java_com_ditz_adventure_Native_setPet(JNIEnv*, jclass, jint i) { G.pet = i % 4; }
JNIEXPORT void JNICALL Java_com_ditz_adventure_Native_setMap(JNIEnv*, jclass, jint i) {
    G.map = i % 3;
    for (int k = 0; k < 8; k++) spawnEnemy(E[k]);
}
JNIEXPORT jfloat JNICALL Java_com_ditz_adventure_Native_stat(JNIEnv*, jclass, jint i) {
    switch (i) {
        case 0: return G.points;
        case 1: return G.hp;
        case 2: return G.tod;
        case 3: return (float)G.ch;
        case 4: return (float)G.skin;
        case 5: return (float)G.pet;
        case 6: return (float)G.map;
    }
    return 0.f;
}
}
