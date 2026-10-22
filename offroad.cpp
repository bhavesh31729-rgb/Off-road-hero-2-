// OFFROAD HERO - low-spec 3D off-road open world truck game
// Win32 + OpenGL 1.1 (fixed function, no shaders, no external files needed)
#include <windows.h>
#include <GL/gl.h>
#include <GL/glu.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define PI 3.14159265f
static const float WATER_Y = -3.5f;
static const float CS = 4.0f;       // terrain cell size (m)
static const int   N  = 22;         // half grid (draw distance ~ 88 m)
static const int   GW = 2 * N + 1;

static int SW = 640, SH = 480, FULL = 0;
static HWND hwnd; static HDC hdc; static HGLRC hrc;
static bool running = true;
static GLuint fontBase;

// ---------------------------------------------------------------- world
static float H(float x, float z) {
    float h = 7.f * sinf(x * 0.011f) * cosf(z * 0.013f)
            + 3.5f * sinf(x * 0.031f + z * 0.027f)
            + 1.2f * sinf(x * 0.09f) * sinf(z * 0.08f)
            + 0.5f * sinf(x * 0.21f + 1.f) * cosf(z * 0.19f);
    float d = sqrtf(x * x + z * z);
    if (d < 60.f) { float t = d < 30.f ? 1.f : (60.f - d) / 30.f; h *= (1.f - t); }
    return h;
}
static float mudAmt(float x, float z, float h) {
    if (x * x + z * z < 45.f * 45.f) return 0.f;
    if (h < WATER_Y + 0.3f) return 0.f;
    float m = sinf(x * 0.045f + 2.f) * sinf(z * 0.05f + 1.f) + 0.3f * sinf(x * 0.11f + z * 0.09f);
    m = (m - 0.45f) * 4.f;
    return m < 0.f ? 0.f : (m > 1.f ? 1.f : m);
}
static bool treeAt(int ix, int iz, float &tx, float &tz, float &sc) {
    unsigned h = ((unsigned)ix * 73856093u) ^ ((unsigned)iz * 19349663u);
    h ^= h >> 13; h *= 0x5bd1e995u; h ^= h >> 15;
    if ((h & 1023u) >= 170u) return false;
    tx = ix * 10.f + ((h >> 10) & 255u) / 255.f * 8.f + 1.f;
    tz = iz * 10.f + ((h >> 18) & 255u) / 255.f * 8.f + 1.f;
    if (tx * tx + tz * tz < 50.f * 50.f) return false;
    float y = H(tx, tz);
    if (y < WATER_Y + 0.8f) return false;
    if (mudAmt(tx, tz, y) > 0.2f) return false;
    sc = 0.8f + ((h >> 26) & 7u) * 0.08f;
    return true;
}

// ---------------------------------------------------------------- drawing helpers
static void box(float cx, float cy, float cz, float sx, float sy, float sz, float r, float g, float b) {
    float x0 = cx - sx, x1 = cx + sx, y0 = cy - sy, y1 = cy + sy, z0 = cz - sz, z1 = cz + sz;
    glBegin(GL_QUADS);
    glColor3f(r, g, b);
    glVertex3f(x0, y1, z0); glVertex3f(x0, y1, z1); glVertex3f(x1, y1, z1); glVertex3f(x1, y1, z0);
    glColor3f(r * .8f, g * .8f, b * .8f);
    glVertex3f(x0, y0, z1); glVertex3f(x1, y0, z1); glVertex3f(x1, y1, z1); glVertex3f(x0, y1, z1);
    glColor3f(r * .7f, g * .7f, b * .7f);
    glVertex3f(x0, y0, z0); glVertex3f(x0, y1, z0); glVertex3f(x1, y1, z0); glVertex3f(x1, y0, z0);
    glColor3f(r * .6f, g * .6f, b * .6f);
    glVertex3f(x0, y0, z0); glVertex3f(x0, y0, z1); glVertex3f(x0, y1, z1); glVertex3f(x0, y1, z0);
    glVertex3f(x1, y0, z0); glVertex3f(x1, y1, z0); glVertex3f(x1, y1, z1); glVertex3f(x1, y0, z1);
    glEnd();
}
static void wheel(float cx, float cy, float cz, float r, float hw, float rot) {
    const int S = 8; float ys[S], zs[S];
    for (int i = 0; i < S; i++) { float a = rot + i * 2.f * PI / S; ys[i] = cy + sinf(a) * r; zs[i] = cz + cosf(a) * r; }
    glBegin(GL_QUADS);
    for (int i = 0; i < S; i++) {
        int j = (i + 1) % S; float s = (i & 1) ? 0.10f : 0.24f; glColor3f(s, s, s);
        glVertex3f(cx - hw, ys[i], zs[i]); glVertex3f(cx + hw, ys[i], zs[i]);
        glVertex3f(cx + hw, ys[j], zs[j]); glVertex3f(cx - hw, ys[j], zs[j]);
    }
    glEnd();
    for (int k = 0; k < 2; k++) {
        float sx = k ? cx + hw : cx - hw;
        glBegin(GL_TRIANGLE_FAN); glColor3f(0.4f, 0.4f, 0.4f); glVertex3f(sx, cy, cz);
        for (int i = 0; i <= S; i++) glVertex3f(sx, ys[i % S], zs[i % S]);
        glEnd();
    }
}
static void drawTree(float x, float z, float sc) {
    float y = H(x, z);
    box(x, y + 0.9f * sc, z, 0.2f * sc, 0.9f * sc, 0.2f * sc, 0.35f, 0.22f, 0.1f);
    float b = y + 1.4f * sc, t = y + 6.f * sc, r = 1.9f * sc;
    glBegin(GL_TRIANGLES);
    glColor3f(0.10f, 0.38f, 0.12f); glVertex3f(x, t, z); glVertex3f(x - r, b, z - r); glVertex3f(x + r, b, z - r);
    glColor3f(0.08f, 0.30f, 0.10f); glVertex3f(x, t, z); glVertex3f(x + r, b, z - r); glVertex3f(x + r, b, z + r);
    glColor3f(0.13f, 0.45f, 0.15f); glVertex3f(x, t, z); glVertex3f(x + r, b, z + r); glVertex3f(x - r, b, z + r);
    glColor3f(0.09f, 0.33f, 0.11f); glVertex3f(x, t, z); glVertex3f(x - r, b, z + r); glVertex3f(x - r, b, z - r);
    glEnd();
}

static float gh[GW][GW];
static float gc[GW][GW][3];
static float tvert[(2 * N) * (2 * N) * 6 * 3];
static float tcol [(2 * N) * (2 * N) * 6 * 3];

static void drawTerrain(float cx, float cz) {
    float bx = floorf(cx / CS) * CS, bz = floorf(cz / CS) * CS;
    for (int i = 0; i < GW; i++) for (int j = 0; j < GW; j++) gh[i][j] = H(bx + (i - N) * CS, bz + (j - N) * CS);
    for (int i = 0; i < GW; i++) for (int j = 0; j < GW; j++) {
        float x = bx + (i - N) * CS, z = bz + (j - N) * CS, h = gh[i][j];
        int i0 = i > 0 ? i - 1 : 0, i1 = i < GW - 1 ? i + 1 : GW - 1;
        int j0 = j > 0 ? j - 1 : 0, j1 = j < GW - 1 ? j + 1 : GW - 1;
        float nx = -(gh[i1][j] - gh[i0][j]) / (2.f * CS), nz = -(gh[i][j1] - gh[i][j0]) / (2.f * CS);
        float il = 1.f / sqrtf(nx * nx + 1.f + nz * nz);
        float ny = il; nx *= il; nz *= il;
        float sh = 0.45f + 0.55f * fmaxf(0.f, nx * 0.424f + ny * 0.848f + nz * 0.318f);
        float r = 0.25f, g = 0.5f, b = 0.15f;
        float v = sinf(x * 0.7f) * sinf(z * 0.6f) * 0.04f; r += v; g += v * 1.5f;
        if (h < WATER_Y + 0.9f) { r = 0.7f; g = 0.65f; b = 0.4f; }
        if (h > 9.f || (1.f - ny) > 0.04f) { r = 0.5f; g = 0.48f; b = 0.45f; }
        float m = mudAmt(x, z, h);
        r = r * (1.f - m) + 0.38f * m; g = g * (1.f - m) + 0.25f * m; b = b * (1.f - m) + 0.13f * m;
        gc[i][j][0] = r * sh; gc[i][j][1] = g * sh; gc[i][j][2] = b * sh;
    }
    int n = 0;
    for (int i = 0; i < GW - 1; i++) for (int j = 0; j < GW - 1; j++) {
        int ci[6] = { i, i, i + 1, i + 1, i, i + 1 };
        int cj[6] = { j, j + 1, j, j, j + 1, j + 1 };
        for (int k = 0; k < 6; k++) {
            tvert[n * 3] = bx + (ci[k] - N) * CS; tvert[n * 3 + 1] = gh[ci[k]][cj[k]]; tvert[n * 3 + 2] = bz + (cj[k] - N) * CS;
            tcol[n * 3] = gc[ci[k]][cj[k]][0]; tcol[n * 3 + 1] = gc[ci[k]][cj[k]][1]; tcol[n * 3 + 2] = gc[ci[k]][cj[k]][2];
            n++;
        }
    }
    glEnableClientState(GL_VERTEX_ARRAY); glEnableClientState(GL_COLOR_ARRAY);
    glVertexPointer(3, GL_FLOAT, 0, tvert); glColorPointer(3, GL_FLOAT, 0, tcol);
    glDrawArrays(GL_TRIANGLES, 0, n);
    glDisableClientState(GL_VERTEX_ARRAY); glDisableClientState(GL_COLOR_ARRAY);
    // water
    glBegin(GL_QUADS); glColor3f(0.12f, 0.32f, 0.55f);
    glVertex3f(cx - 220, WATER_Y, cz - 220); glVertex3f(cx - 220, WATER_Y, cz + 220);
    glVertex3f(cx + 220, WATER_Y, cz + 220); glVertex3f(cx + 220, WATER_Y, cz - 220);
    glEnd();
    // trees
    int tx0 = (int)floorf(cx / 10.f), tz0 = (int)floorf(cz / 10.f);
    for (int a = -9; a <= 9; a++) for (int c = -9; c <= 9; c++) {
        float tx, tz, sc; if (treeAt(tx0 + a, tz0 + c, tx, tz, sc)) drawTree(tx, tz, sc);
    }
}

// ---------------------------------------------------------------- game data
struct TruckDef { const char *name; float power, top, grip, mud, price, hl, hw, hh, wr, r, g, b; int bed; };
static TruckDef TD[3] = {
    { "Scout Pickup",    8.f, 24.f, 3.0f, 0.15f,    0.f, 2.4f, 1.0f, 0.5f, 0.45f, 0.80f, 0.15f, 0.10f, 1 },
    { "Rhino Hauler",   10.f, 20.f, 3.5f, 0.45f, 3000.f, 3.0f, 1.2f, 0.7f, 0.60f, 0.20f, 0.40f, 0.70f, 1 },
    { "Goliath Monster",13.f, 23.f, 2.6f, 0.70f, 8000.f, 2.6f, 1.4f, 0.8f, 0.95f, 0.90f, 0.75f, 0.10f, 0 } };

static float px = 0, pz = 16, py = 1, yaw = 0, fwd = 0, lat = 0, pitchS = 0, rollS = 0, wheelRot = 0, camYaw = 0, camY = 5;
static int money = 500, owned = 1, cur = 0, eng[3] = { 0, 0, 0 }, tire[3] = { 0, 0, 0 };
static char msg[128] = ""; static float msgT = 0;
static void say(const char *s, float t = 4.f) { strncpy(msg, s, 127); msg[127] = 0; msgT = t; }

struct Mission { int active, type, stage, reward, cp; float ax, az, bx, bz, time, cx[5], cz[5]; };
static Mission ms = { 0 };

struct Save { int magic, money, owned, cur, eng[3], tire[3]; };
static void saveGame() { Save s; s.magic = 0x0FF50AD; s.money = money; s.owned = owned; s.cur = cur;
    for (int i = 0; i < 3; i++) { s.eng[i] = eng[i]; s.tire[i] = tire[i]; }
    FILE *f = fopen("save.dat", "wb"); if (f) { fwrite(&s, sizeof s, 1, f); fclose(f); } }
static void loadGame() { Save s; FILE *f = fopen("save.dat", "rb"); if (!f) return;
    if (fread(&s, sizeof s, 1, f) == 1 && s.magic == 0x0FF50AD) { money = s.money; owned = s.owned | 1; cur = s.cur; if (cur < 0 || cur > 2) cur = 0;
        for (int i = 0; i < 3; i++) { eng[i] = s.eng[i]; tire[i] = s.tire[i]; } }
    fclose(f); }

static float frand() { return rand() / (float)RAND_MAX; }
static void pickPoint(float ox, float oz, float dmin, float dmax, float &x, float &z) {
    for (int t = 0; t < 40; t++) {
        float a = frand() * 2.f * PI, d = dmin + frand() * (dmax - dmin);
        x = ox + sinf(a) * d; z = oz + cosf(a) * d;
        float y = H(x, z);
        if (y > WATER_Y + 0.8f && mudAmt(x, z, y) < 0.2f) return;
    }
    x = ox; z = oz + dmin;
}
static float dist2d(float ax, float az, float bx, float bz) { return sqrtf((ax - bx) * (ax - bx) + (az - bz) * (az - bz)); }
static void newMission() {
    memset(&ms, 0, sizeof ms); ms.active = 1; ms.type = rand() % 2;
    if (ms.type == 0) {
        pickPoint(px, pz, 120, 300, ms.ax, ms.az); pickPoint(ms.ax, ms.az, 200, 450, ms.bx, ms.bz);
        float d = dist2d(px, pz, ms.ax, ms.az) + dist2d(ms.ax, ms.az, ms.bx, ms.bz);
        ms.time = d / 7.f + 40.f; ms.reward = (int)(d * 1.5f);
        say("MISSION: pick up cargo, then deliver it! (E = cancel)");
    } else {
        float ox = px, oz = pz, d = 0;
        for (int i = 0; i < 5; i++) { pickPoint(ox, oz, 100, 220, ms.cx[i], ms.cz[i]); d += dist2d(ox, oz, ms.cx[i], ms.cz[i]); ox = ms.cx[i]; oz = ms.cz[i]; }
        ms.time = d / 8.f + 40.f; ms.reward = (int)(d * 1.2f) + 200;
        say("MISSION: checkpoint run! Hit all 5 in time. (E = cancel)");
    }
}
static void objective(float &x, float &z) {
    if (ms.type == 0) { if (ms.stage == 0) { x = ms.ax; z = ms.az; } else { x = ms.bx; z = ms.bz; } }
    else { x = ms.cx[ms.cp]; z = ms.cz[ms.cp]; }
}

// ---------------------------------------------------------------- input
static bool prevK[256];
static bool isDown(int vk) { return GetForegroundWindow() == hwnd && (GetAsyncKeyState(vk) & 0x8000) != 0; }
static bool pressed(int vk) { bool d = isDown(vk); bool r = d && !prevK[vk & 255]; prevK[vk & 255] = d; return r; }

static const float GX = 0.f, GZ = 30.f;
static bool inGarage() { return dist2d(px, pz, GX, GZ) < 20.f && fabsf(fwd) < 4.f; }

static void resetTruck() { px = 0; pz = 16; yaw = 0; fwd = lat = 0; py = H(px, pz) + TD[cur].wr; camYaw = yaw; }

// ---------------------------------------------------------------- update
static void update(float dt) {
    bool up = isDown('W') || isDown(VK_UP), dn = isDown('S') || isDown(VK_DOWN);
    bool lf = isDown('A') || isDown(VK_LEFT), rt = isDown('D') || isDown(VK_RIGHT), hb = isDown(VK_SPACE);
    TruckDef &T = TD[cur];
    float em = 1.f + 0.12f * eng[cur], power = T.power * em, top = T.top * em;
    float gy = H(px, pz), m = mudAmt(px, pz, gy);
    float depth = WATER_Y - gy; if (depth < 0) depth = 0;
    float mr = T.mud + 0.12f * tire[cur]; if (mr > 0.95f) mr = 0.95f;
    float mudPen = m * (1.f - mr);
    float traction = 1.f - 0.6f * mudPen; if (depth > 0.2f) traction *= 0.4f;
    float sf = sinf(yaw), cf = cosf(yaw);
    float sl = (H(px + sf, pz + cf) - H(px - sf, pz - cf)) * 0.5f;

    float acc = 0;
    if (up) acc += power;
    if (dn) acc -= (fwd > 0.5f) ? power * 1.6f : power * 0.6f;
    fwd += acc * traction * dt;
    fwd -= 9.8f * sl * 0.9f * dt;
    float lin = 0.15f + 0.9f * mudPen + (depth > 0.2f ? 0.6f + depth * 0.4f : 0.f);
    fwd -= fwd * lin * dt;
    fwd -= fwd * fabsf(fwd) * (power / (top * top)) * dt;
    if (!up && !dn) { float rr = 1.5f * dt; if (fabsf(fwd) < rr) fwd = 0; else fwd -= fwd > 0 ? rr : -rr; }
    if (hb) fwd -= fwd * 1.5f * dt;
    float grip = (T.grip + 0.2f * tire[cur]) * (1.f - 0.5f * mudPen) * (hb ? 0.3f : 1.f);
    lat *= expf(-grip * dt);

    float st = (lf ? 1.f : 0.f) - (rt ? 1.f : 0.f);
    float sfct = fmaxf(-1.f, fminf(1.f, fwd / 3.f));
    yaw += st * (1.5f / (1.f + fabsf(fwd) * 0.05f)) * sfct * dt;

    float cap = 1e9f; if (depth > 0.2f) cap = 6.f; if (depth > 3.f) cap = 2.5f;
    if (fwd > cap) fwd = cap; if (fwd < -cap) fwd = -cap;
    sf = sinf(yaw); cf = cosf(yaw);
    px += (sf * fwd + cf * lat) * dt; pz += (cf * fwd - sf * lat) * dt;

    // trees
    int tx0 = (int)floorf(px / 10.f), tz0 = (int)floorf(pz / 10.f);
    for (int a = -1; a <= 1; a++) for (int c = -1; c <= 1; c++) {
        float tx, tz, sc;
        if (treeAt(tx0 + a, tz0 + c, tx, tz, sc)) {
            float dx = px - tx, dz = pz - tz, d = sqrtf(dx * dx + dz * dz), rr = 0.6f + T.hw;
            if (d < rr && d > 0.001f) { px = tx + dx / d * rr; pz = tz + dz / d * rr; fwd *= 0.3f; lat *= 0.3f; }
        }
    }
    // body orientation from 4 wheel samples
    float xl = T.hw, zl = T.hl * 0.8f;
#define WH(a, b) H(px + sf * (b) + cf * (a), pz + cf * (b) - sf * (a))
    float hFL = WH(xl, zl), hFR = WH(-xl, zl), hRL = WH(xl, -zl), hRR = WH(-xl, -zl);
    float target = (hFL + hFR + hRL + hRR) * 0.25f + T.wr;
    float minY = WATER_Y - 0.2f + T.wr * 0.5f; if (target < minY) target = minY;
    float k = fminf(1.f, dt * 12.f); py += (target - py) * k;
    float pitch = atan2f((hFL + hFR) * 0.5f - (hRL + hRR) * 0.5f, 2.f * zl);
    float roll  = atan2f((hFL + hRL) * 0.5f - (hFR + hRR) * 0.5f, 2.f * xl);
    k = fminf(1.f, dt * 8.f); pitchS += (pitch - pitchS) * k; rollS += (roll - rollS) * k;
    wheelRot += fwd * dt / T.wr;

    // camera
    float da = yaw - camYaw; while (da > PI) da -= 2 * PI; while (da < -PI) da += 2 * PI;
    camYaw += da * fminf(1.f, dt * 4.f);

    // mission
    if (pressed('E')) { if (ms.active) { ms.active = 0; say("Mission cancelled."); } else newMission(); }
    if (ms.active) {
        ms.time -= dt;
        if (ms.time <= 0) { ms.active = 0; say("Time's up! Mission failed.", 5); }
        else {
            float ox, oz; objective(ox, oz);
            if (dist2d(px, pz, ox, oz) < 10.f) {
                if (ms.type == 0) {
                    if (ms.stage == 0) { ms.stage = 1; say("Cargo loaded! Deliver it now."); }
                    else { money += ms.reward; char b[64]; sprintf(b, "Delivered! +$%d", ms.reward); say(b, 5); ms.active = 0; saveGame(); }
                } else {
                    ms.cp++;
                    if (ms.cp >= 5) { money += ms.reward; char b[64]; sprintf(b, "Run complete! +$%d", ms.reward); say(b, 5); ms.active = 0; saveGame(); }
                    else say("Checkpoint!", 2);
                }
            }
        }
    }
    // garage shop
    if (inGarage()) {
        for (int i = 0; i < 3; i++) if (pressed('1' + i)) {
            if (owned & (1 << i)) { cur = i; say("Truck selected."); }
            else if (money >= (int)TD[i].price) { money -= (int)TD[i].price; owned |= 1 << i; cur = i; say("Truck purchased!"); saveGame(); }
            else say("Not enough money.");
        }
        if (pressed('U')) { int c = 600 * (eng[cur] + 1); if (eng[cur] >= 3) say("Engine maxed."); else if (money >= c) { money -= c; eng[cur]++; say("Engine upgraded!"); saveGame(); } else say("Not enough money."); }
        if (pressed('T')) { int c = 500 * (tire[cur] + 1); if (tire[cur] >= 3) say("Tires maxed."); else if (money >= c) { money -= c; tire[cur]++; say("Tires upgraded!"); saveGame(); } else say("Not enough money."); }
    }
    if (pressed('R')) { resetTruck(); say("Returned to garage."); }
    if (pressed(VK_ESCAPE)) { saveGame(); running = false; }
    if (msgT > 0) msgT -= dt;
}

// ---------------------------------------------------------------- render
static void text(float x, float y, const char *s, float r = 1, float g = 1, float b = 1) {
    glColor3f(r, g, b); glRasterPos2f(x, y); glListBase(fontBase - 32); glCallLists((GLsizei)strlen(s), GL_UNSIGNED_BYTE, s);
}
static void drawTruck() {
    TruckDef &T = TD[cur];
    glPushMatrix();
    glTranslatef(px, py, pz); glRotatef(yaw * 180.f / PI, 0, 1, 0);
    glRotatef(-pitchS * 180.f / PI, 1, 0, 0); glRotatef(rollS * 180.f / PI, 0, 0, 1);
    float cy = T.wr * 0.5f + T.hh * 0.5f, ch = T.hh * 0.5f, top = cy + ch;
    box(0, cy, 0, T.hw, ch, T.hl, T.r, T.g, T.b);
    box(0, top + T.hh * 0.45f, T.hl * 0.2f, T.hw * 0.9f, T.hh * 0.45f, T.hl * 0.3f, fminf(1.f, T.r * 1.1f), fminf(1.f, T.g * 1.1f), fminf(1.f, T.b * 1.1f));
    box(0, top + T.hh * 0.55f, T.hl * 0.5f + 0.02f, T.hw * 0.8f, T.hh * 0.25f, 0.02f, 0.1f, 0.15f, 0.25f);
    if (ms.active && ms.type == 0 && ms.stage == 1 && T.bed) box(0, top + 0.4f, -T.hl * 0.5f, T.hw * 0.7f, 0.4f, T.hl * 0.3f, 0.6f, 0.45f, 0.2f);
    for (int a = -1; a <= 1; a += 2) for (int b = -1; b <= 1; b += 2)
        wheel(a * (T.hw + 0.05f), 0, b * 0.65f * T.hl, T.wr, 0.25f + T.wr * 0.15f, wheelRot);
    glPopMatrix();
}
static void beacon(float x, float z, float r, float g, float b) { box(x, H(x, z) + 25.f, z, 0.4f, 25.f, 0.4f, r, g, b); }

static void render() {
    glClearColor(0.62f, 0.78f, 0.92f, 1); glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glMatrixMode(GL_PROJECTION); glLoadIdentity(); gluPerspective(60.0, (double)SW / SH, 0.5, 230.0);
    glMatrixMode(GL_MODELVIEW); glLoadIdentity();
    float cx = px - sinf(camYaw) * 9.f, cz = pz - cosf(camYaw) * 9.f, gy = H(cx, cz) + 1.5f;
    float wantY = py + 4.f; if (wantY < gy) wantY = gy; camY += (wantY - camY) * 0.2f;
    gluLookAt(cx, camY, cz, px + sinf(yaw) * 3.f, py + 1.2f, pz + cosf(yaw) * 3.f, 0, 1, 0);

    glEnable(GL_DEPTH_TEST); glEnable(GL_FOG);
    glDisable(GL_CULL_FACE); glDisable(GL_LIGHTING);
    glFogi(GL_FOG_MODE, GL_LINEAR); glFogf(GL_FOG_START, 70.f); glFogf(GL_FOG_END, 200.f);
    float fc[4] = { 0.62f, 0.78f, 0.92f, 1 }; glFogfv(GL_FOG_COLOR, fc); glHint(GL_FOG_HINT, GL_FASTEST);
    drawTerrain(px, pz);
    // garage
    box(GX, H(GX, GZ) + 2.5f, GZ, 6, 2.5f, 4, 0.6f, 0.6f, 0.65f);
    box(GX, H(GX, GZ) + 5.2f, GZ, 6.5f, 0.3f, 4.5f, 0.9f, 0.45f, 0.1f);
    box(GX, H(GX, GZ) + 1.4f, GZ - 4.05f, 3, 1.4f, 0.05f, 0.15f, 0.15f, 0.2f);
    drawTruck();
    glDisable(GL_FOG);
    if (ms.active) {
        float ox, oz; objective(ox, oz);
        if (ms.type == 0) beacon(ox, oz, ms.stage == 0 ? 1.f : 0.2f, 1.f, 0.2f); else beacon(ox, oz, 1.f, 0.9f, 0.1f);
    }
    beacon(GX, GZ, 1.f, 0.5f, 0.1f);

    // HUD
    glDisable(GL_DEPTH_TEST);
    glMatrixMode(GL_PROJECTION); glLoadIdentity(); glOrtho(0, SW, 0, SH, -1, 1);
    glMatrixMode(GL_MODELVIEW); glLoadIdentity();
    char b[160];
    sprintf(b, "$%d   %s   %d km/h", money, TD[cur].name, (int)(fabsf(fwd) * 3.6f)); text(10, SH - 20, b);
    float gyy = H(px, pz);
    if (WATER_Y - gyy > 0.2f) text(10, SH - 40, "WATER - slow!", 0.4f, 0.7f, 1);
    else if (mudAmt(px, pz, gyy) > 0.3f) text(10, SH - 40, "MUD", 0.8f, 0.55f, 0.3f);
    if (ms.active) {
        float ox, oz; objective(ox, oz); float d = dist2d(px, pz, ox, oz);
        if (ms.type == 0) sprintf(b, "%s  %dm  Time %ds  Reward $%d", ms.stage == 0 ? "PICK UP" : "DELIVER", (int)d, (int)ms.time, ms.reward);
        else sprintf(b, "CHECKPOINT %d/5  %dm  Time %ds  Reward $%d", ms.cp + 1, (int)d, (int)ms.time, ms.reward);
        text(SW / 2 - 150, SH - 20, b, 1, 1, 0.4f);
        float rel = atan2f(ox - px, oz - pz) - yaw, dx = -sinf(rel), dy = cosf(rel);
        float ax = SW / 2.f, ay = SH - 60.f;
        glBegin(GL_TRIANGLES); glColor3f(1, 0.9f, 0.1f);
        glVertex3f(ax + dx * 22, ay + dy * 22, 0);
        glV
