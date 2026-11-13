// AURA Synth Bass - motor de som em C++ puro (porte do AudioWorklet da versão web)
// Copyright (c) 2026 Thiago R. Galvão. Todos os direitos reservados. ID: AURA-SB-TRG-2026
#pragma once
#include <cmath>
#include <vector>
#include <algorithm>
#include <cstdint>

namespace aura {

enum P { oct, wave, pw1, lvl, w2, pw2, l2, d2, o2, sync, sub, sw, nz, cut, res, fenv, kt, drv, pol, glide, pa, pd, lr, lwv, lp, lf, lw,
         fa, fd, fs, fr, a, d, s, r, leg, vol, l3, w3, o3, d3, k3, nc, ovl, lsr, ec, dt, df, dm, NP };

static const double TWO_PI = 6.283185307179586;

struct Engine {
    // ----- parâmetros (mesma ordem de Presets.h) e controles globais
    float p[NP] = {};
    float velSens = 0.75f;    // 0 a 1
    int   velDest = 0;        // 0 volume+brilho, 1 só volume, 2 só brilho
    int   prio = 0;           // 0 última, 1 grave, 2 aguda
    int   atDest = 0;         // 0 off, 1 vibrato, 2 filtro, 3 volume
    double sr = 44100;

    // ----- ruído
    uint32_t rs = 22222u;
    inline double rnd() { rs ^= rs << 13; rs ^= rs >> 17; rs ^= rs << 5; return (rs >> 8) * (1.0 / 8388608.0); }   // [0,1)

    // ----- voz
    double p1 = 0, p2 = 0, ps = 0, p3 = 0, y1 = 0, y2 = 0, y3 = 0, y4 = 0, b0 = 0, b1 = 0, b2 = 0, hx = 0, hy = 0, mv = 0, shT = 0, shV = 0, ha = 0;
    double ml1 = .75, ml2 = 0, mls = 0, mnz = 0, ml3 = 0, movl = 0, mres = 0, mdrv = 0; bool mfirst = true;
    double smRes = 0.35;

    // ----- envelopes
    enum { IDLE, ATT, DEC, REL };
    int ast = IDLE; double aval = 0, astep = 0, atc = 0, atarget = 0, alin = 0;
    int cst = IDLE; double cval = 0, cstep = 0, ctarget = 0, cmul = 1, ccoef = 0; bool cexp = false;
    double cutNow = 1000;
    double freq = 55, freqT = 55, glideC = 0;
    double pitchC = 0, pitchK = 0;
    double velGain = 0.883, velGainT = 0.883, velBright = 1;
    double lfoPh = 0;
    double masterG = 1;
    bool gate = false;
    int lastNote = 36;
    std::vector<int> stack;
    std::vector<int> held; bool sustainOn = false;
    float bend = 0, mod = 0, atVal = 0;

    // ----- compressor
    double lt, kt_, kc_k, yk, kdb, slp, mk, ce = 0, cg = 1, cgt = 1; int cn = 0; double cer, cca, ccr;
    // ----- limitador / delay
    std::vector<float> dline; int dw = 0;
    double dlb0, dlb1, dlb2, dla1, dla2, dz1 = 0, dz2 = 0;
    double dtS = 0.3, dfS = 0.35, dmS = 0, preOut = 0;

    static double dbf(double x) { return 20 * std::log10(x); }
    double kcf(double x, double k) const { return lt + (1 - std::exp(-k * (x - lt))) / k; }
    double slopeAt(double x, double k) const { if (x < lt) return 1; double x2 = x * 1.001; return (dbf(kcf(x2, k)) - dbf(kcf(x, k))) / (dbf(x2) - dbf(x)); }
    double sat(double x) const { return x < lt ? x : (x < kt_ ? kcf(x, kc_k) : std::pow(10, (yk + slp * (dbf(x) - kdb)) / 20)); }

    void prepare(double sampleRate) {
        sr = sampleRate;
        ha = std::exp(-TWO_PI * 25 / sr);
        const double th = -14, kn = 10, ra = 5;
        lt = std::pow(10, th / 20); kt_ = std::pow(10, (th + kn) / 20); slp = 1 / ra;
        double lo = 0.1, hi = 10000, k = 5;
        for (int i = 0; i < 15; i++) { if (slopeAt(kt_, k) < slp) hi = k; else lo = k; k = std::sqrt(lo * hi); }
        kc_k = k; yk = dbf(kcf(kt_, k)); kdb = th + kn;
        mk = std::pow(1 / sat(1), 0.6);
        cer = std::exp(-1 / (0.03 * sr)); cca = 1 - std::exp(-4 / (0.00008 * sr)); ccr = 1 - std::exp(-4 / (0.05 * sr));
        dline.assign((size_t)(sr * 1.6) + 8, 0.f); dw = 0;
        // passa-baixas 4200 Hz Butterworth (igual ao biquad do navegador, Q 0,707 aprox.)
        double w0 = TWO_PI * 4200 / sr, al = std::sin(w0) / (2 * 0.7071), cs = std::cos(w0), a0 = 1 + al;
        dlb0 = (1 - cs) / 2 / a0; dlb1 = (1 - cs) / a0; dlb2 = dlb0; dla1 = -2 * cs / a0; dla2 = (1 - al) / a0;
        reset();
    }
    void reset() {
        p1 = p2 = ps = p3 = y1 = y2 = y3 = y4 = b0 = b1 = b2 = hx = hy = mv = 0; mfirst = true;
        ast = cst = IDLE; aval = 0; gate = false; stack.clear(); held.clear(); sustainOn = false;
        ce = 0; cg = 1; cgt = 1; cn = 0; dz1 = dz2 = 0; std::fill(dline.begin(), dline.end(), 0.f);
        cutNow = baseCut(); freq = freqT = noteFreq(lastNote); pitchC = 0;
    }

    // ----- utilidades
    static double mtof(double n) { return 440 * std::pow(2, (n - 69) / 12); }
    double noteFreq(int n) const { return mtof(n + (int)std::lround(p[oct]) * 12); }
    double cutHz(double v) const { return 30 * std::pow(18000.0 / 30.0, v); }
    double baseCut() const { return std::min(cutHz(p[cut]) * std::pow(2, p[kt] * ((lastNote + std::lround(p[oct]) * 12) - 24) / 12.0), 20000.0); }
    double susF() const { return std::min(baseCut() * velBright * std::pow(2, p[fenv] * p[fs]), 20000.0); }
    double tcCoef(double tc) const { return 1 - std::exp(-1 / (std::max(tc, 1e-5) * sr)); }

    // ----- eventos
    void velApply(int vel) {
        double s = velSens, v = std::max(1, std::min(127, vel)) / 127.0, vr = 100 / 127.0;
        auto curve = [s](double x) { return (1 - s) + s * std::pow(x, 1.7); };
        velBright = velDest == 1 ? 1 : std::pow(2, s * (v - vr) * 2.4);
        velGainT = velDest == 2 ? 0.883 : 0.883 * curve(v) / curve(vr);
    }
    void triggerEnv(int vel) {
        velApply(vel);
        double att = std::max((double)p[a], 0.002), fat = std::max((double)p[fa], 0.002);
        ast = ATT; astep = (1 - aval) / (att * sr); atc = att * sr; alin = 0;     // alin conta amostras do ataque
        aAttLeft = att * sr;
        double base = baseCut();
        cutNow = std::min(base * velBright, 20000.0);
        cst = ATT; cTopLeft = fat * sr; cstep = (std::min(base * velBright * std::pow(2, p[fenv]), 20000.0) - cutNow) / cTopLeft;
        // pitch
        if (p[pa] > 0.05f) { pitchC = p[pa] * 100; pitchK = tcCoef(std::max((double)p[pd], 0.01) / 3); } else { pitchC = 0; pitchK = 0; }
        gate = true;
    }
    double aAttLeft = 0, cTopLeft = 0, aDecLeft = 0, cDecLeft = 0, aRelStep = 0;
    void releaseEnv() {
        ast = REL; cst = REL;
        if (p[ec] > 0.5f) { aRelStep = aval / (std::max((double)p[r], 0.02) * sr) ; aDecLeft = std::max((double)p[r], 0.02) * sr;
            double base = std::max(baseCut(), 5.0); cDecLeft = std::max((double)p[fr], 0.02) * sr; cmul = std::pow(base / std::max(cutNow, 5.0), 1 / cDecLeft); }
        gate = false;
    }
    void setPitch(int n, bool immediate) { freqT = noteFreq(n); glideC = tcCoef(p[glide] / 3); if (immediate || p[glide] < 0.002f) freq = freqT; }
    int pickNote() const {
        if (stack.empty()) return -1;
        if (prio == 1) return *std::min_element(stack.begin(), stack.end());
        if (prio == 2) return *std::max_element(stack.begin(), stack.end());
        return stack.back();
    }
    void trackFilter() { cst = DEC; ctarget = -1; ccoef = tcCoef(0.03); cexp = false; }
    void coreOn(int n, int vel) {
        held.erase(std::remove(held.begin(), held.end(), n), held.end());
        stack.erase(std::remove(stack.begin(), stack.end(), n), stack.end()); stack.push_back(n);
        if (gate && pickNote() != n) return;
        lastNote = n;
        if (!gate) { setPitch(n, true); triggerEnv(vel); }
        else if (p[leg] < 0.5f) { setPitch(n, false); triggerEnv(vel); }
        else { setPitch(n, false); velApply(vel); trackFilter(); }
    }
    void realOff(int n) {
        stack.erase(std::remove(stack.begin(), stack.end(), n), stack.end());
        if (stack.empty()) releaseEnv();
        else { int t = pickNote(); if (t != lastNote) { lastNote = t; setPitch(t, false); trackFilter(); } }
    }
    void noteOn(int n, int vel) { coreOn(n, vel <= 0 ? 100 : vel); }
    void noteOff(int n) { if (sustainOn) { held.push_back(n); return; } realOff(n); }
    void setSustain(bool on) {
        if (on == sustainOn) return; sustainOn = on;
        if (!on) { auto hs = held; held.clear(); for (int n : hs) realOff(n); }
    }
    void allOff() { stack.clear(); held.clear(); sustainOn = false; if (gate) releaseEnv(); }

    // ----- osciladores
    static inline double blep(double t, double dt) {
        if (t < dt) { t /= dt; return t + t - t * t - 1; }
        if (t > 1 - dt) { t = (t - 1) / dt; return t * t + t + t + 1; }
        return 0;
    }
    static inline double osc(int w, double ph, double dt, double pwv) {
        if (w == 0) return 1 - 4 * std::fabs(ph - 0.5);
        if (w == 1) return 2 * ph - 1 - blep(ph, dt);
        double q = ph + 1 - pwv; if (q >= 1) q -= 1;
        return (ph < pwv ? 1 : -1) + blep(ph, dt) - blep(q, dt) + (1 - 2 * pwv);
    }

    // ----- processamento (mono). Escreve n amostras em out.
    void process(float* out, int n) {
        const int lsr_ = (int)std::lround(p[lsr]);
        const bool ext = lsr_ > 0;
        const double lvl1 = p[lvl], lvl2 = p[l2], lvlS = p[sub], lvlN = p[nz], lvl3 = p[l3];
        const int w1 = (int)std::lround(p[wave]), w2_ = (int)std::lround(p[aura::w2]), w3_ = (int)std::lround(p[aura::w3]);
        const double ratio = std::pow(2, std::lround(p[o2]) + p[d2] / 12.0), ratio3 = std::pow(2, std::lround(p[o3]) + p[d3] / 12.0);
        const double fx3 = p[lr] * std::pow(2, p[d3] / 12.0);
        const bool k3on = p[k3] > 0.5f, sync_ = p[sync] > 0.5f, sm = p[sw] > 0.5f, nc_ = p[nc] > 0.5f, ecb = p[ec] > 0.5f;
        const int pol_ = (int)std::lround(p[pol]);
        if (mfirst) { mfirst = false; ml1 = lvl1; ml2 = lvl2; mls = lvlS; mnz = lvlN; ml3 = lvl3; movl = p[ovl]; mres = p[res]; mdrv = p[drv]; }
        const double bs = 1 - std::pow(0.65, 1.0 / 128.0);
        const double lsm = 0.004 * 44100 / sr * 1.0;   // suavização dos níveis (no navegador: 0,004 por amostra)
        const double lsm2 = 0.004;
        (void)lsm;
        const double bendC = bend >= 0 ? bend * 200.0 : bend * 1200.0;
        const double lfoGp = (ext ? 0 : p[lp] * 100.0) + mod * 60.0 + (atDest == 1 ? atVal * 80.0 : 0.0);
        const double lfoGc = ext ? 0 : (double)p[lf];
        const double lfoGw = ext ? 0 : p[lw] * 0.4;
        const double atCut = atDest == 2 ? atVal * 1.5 : 0.0;
        const double masterT = p[vol] * (1 + (atDest == 3 ? atVal * 0.6 : 0.0));
        const double mc = tcCoef(0.012), vc = tcCoef(0.004);
        const double lfoInc = p[lr] / sr;
        const bool lfoSq = p[lwv] > 0.5f;
        const double dtT = p[dt], dfT = p[df], dmT = p[dm] * 0.9;
        const double ec_k = tcCoef(0.05), ef_k = tcCoef(0.03);
        const double sl = 1.0 / sr;
        (void)sl;
        const double ampTc = tcCoef(std::max((double)p[d], 0.02) / 3), fTc = tcCoef(std::max((double)p[fd], 0.02) / 3);
        const double arTc = tcCoef(std::max((double)p[r], 0.02) / 3), frTc = tcCoef(std::max((double)p[fr], 0.02) / 3);
        const double sus = p[s];
        const double base = baseCut();

        for (int i = 0; i < n; i++) {
            // --- envelope de volume
            if (ast == ATT) {
                aval += astep; aAttLeft -= 1;
                if (aAttLeft <= 0) { aval = 1; ast = DEC; if (ecb) { double tg = std::max(sus, 0.0001); aDecLeft = std::max((double)p[d], 0.02) * sr; astep = (tg - 1) / aDecLeft; atarget = tg; } else atarget = sus; }
            } else if (ast == DEC) {
                if (ecb) { if (aDecLeft > 0) { aval += astep; aDecLeft -= 1; } else aval = atarget; }
                else aval += (sus - aval) * ampTc;
            } else if (ast == REL) {
                if (ecb) { if (aDecLeft > 0) { aval -= aRelStep; aDecLeft -= 1; if (aval < 0) aval = 0; } else aval = 0; }
                else aval += (0 - aval) * arTc;
            }
            // --- envelope de filtro
            if (cst == ATT) {
                cutNow += cstep; cTopLeft -= 1;
                if (cTopLeft <= 0) {
                    cst = DEC; double sf = susF();
                    if (ecb) { cDecLeft = std::max((double)p[fd], 0.02) * sr; cmul = std::pow(std::max(sf, 5.0) / std::max(cutNow, 5.0), 1 / cDecLeft); cexp = true; }
                    else { cexp = false; ctarget = -1; ccoef = fTc; }
                }
            } else if (cst == DEC) {
                if (cexp) { if (cDecLeft > 0) { cutNow *= cmul; cDecLeft -= 1; } }
                else { double tg = susF(); cutNow += (tg - cutNow) * ccoef; }
            } else if (cst == REL) {
                if (ecb) { if (cDecLeft > 0) { cutNow *= cmul; cDecLeft -= 1; } }
                else cutNow += (base - cutNow) * frTc;
            } else cutNow = base;
            if (ast == REL && aval < 1e-6 && !gate) { aval = 0; }

            // --- pitch / glide
            freq += (freqT - freq) * (p[glide] < 0.002f ? 1.0 : glideC);
            double pc = pitchC; if (pitchK > 0) { pitchC += (0 - pitchC) * pitchK; if (std::fabs(pitchC) < 1e-6) pitchC = 0; }
            // --- LFO nativo
            lfoPh += lfoInc; if (lfoPh >= 1) lfoPh -= 1;
            double lv = lfoSq ? (lfoPh < 0.5 ? 1.0 : -1.0) : (lfoPh < 0.5 ? 4 * lfoPh - 1 : 3 - 4 * lfoPh);

            // --- voz
            ml1 += lsm2 * (lvl1 - ml1); ml2 += lsm2 * (lvl2 - ml2); mls += lsm2 * (lvlS - mls); mnz += lsm2 * (lvlN - mnz); ml3 += lsm2 * (lvl3 - ml3);
            if ((i & 127) == 0) { mres += bs * 0 + 0.35 * (p[res] - mres); mdrv += 0.35 * (p[drv] - mdrv); movl += 0.35 * (p[ovl] - movl); }
            const double ovg = movl * 0.6, k = mres * 4.25, dg = 1 + mdrv * 7, comp = 1 + k * 0.55, og = 1 / (1 + 1.5 * mdrv);
            double cents = bendC + lv * lfoGp + pc; if (ext) cents += mv * p[lp] * 100;
            double f = freq; if (cents != 0) f *= std::pow(2, cents / 1200);
            const double dt1 = std::min(f / sr, 0.45), dt2 = std::min(f * ratio / sr, 0.45);
            const double pm = lv * lfoGw + (ext ? mv * p[lw] * 0.4 : 0);
            p1 += dt1; bool wrap = false; if (p1 >= 1) { p1 -= 1; wrap = true; }
            p2 += dt2; if (p2 >= 1) p2 -= 1;
            if (wrap && sync_) { p2 = p1 * dt2 / dt1; if (p2 >= 1) p2 -= std::floor(p2); }
            double s3 = 0;
            if (ml3 > 1e-4 || lsr_ == 1) { double f3 = k3on ? f * ratio3 : fx3, d3_ = std::min(f3 / sr, 0.45); p3 += d3_; if (p3 >= 1) p3 -= 1; s3 = osc(w3_, p3, d3_, 0.5); }
            if (lsr_ == 1) mv = s3; else if (lsr_ == 2) { shT -= p[lr] / sr; if (shT <= 0) { shT += 1; shV = rnd() * 2 - 1; } mv = shV; }
            double sg = 0;
            if (ml1 > 1e-4) sg += ml1 * osc(w1, p1, dt1, std::min(0.95, std::max(0.05, p[pw1] + pm)));
            if (ml2 > 1e-4) sg += ml2 * osc(w2_, p2, dt2, std::min(0.95, std::max(0.05, p[pw2] + pm)));
            if (mls > 1e-4) {
                if (sm) { ps += dt1 * 0.5; if (ps >= 1) ps -= 1; sg += mls * 0.75 * ((ps < 0.5 ? 1 : -1) + blep(ps, dt1 * 0.5) - blep(ps < 0.5 ? ps + 0.5 : ps - 0.5, dt1 * 0.5)); }
                else { ps += dt1; if (ps >= 1) ps -= 1; sg += mls * std::sin(TWO_PI * ps); }
            }
            if (ml3 > 1e-4) sg += ml3 * s3;
            if (mnz > 1e-4) { double w = rnd() * 2 - 1, nv = w; if (nc_) { b0 = 0.99765 * b0 + w * 0.0990460; b1 = 0.96300 * b1 + w * 0.2965164; b2 = 0.57000 * b2 + w * 1.0526913; nv = (b0 + b1 + b2 + w * 0.1848) * 0.3; } sg += mnz * 0.6 * nv; }
            sg *= 0.6;
            if (ovg > 1e-4) sg += ovg * hy;
            double cm = lv * lfoGc + atCut + (ext ? mv * p[lf] : 0.0);
            double fc = cutNow; if (cm != 0) fc *= std::pow(2, cm);
            if (fc > sr * 0.3) fc = sr * 0.3; else if (fc < 5) fc = 5;
            const double g = 1 - std::exp(-TWO_PI * fc / sr);
            const double x = 1.4286 * std::tanh(0.7 * sg * dg) * comp - k * std::tanh(y4) + 1e-9 * (rnd() - 0.5);
            y1 += g * (x - y1); y2 += g * (y1 - y2); y3 += g * (y2 - y3); y4 += g * (y3 - y4);
            double ov = (pol_ == 1 ? y1 : (pol_ == 2 ? y2 : (pol_ == 3 ? y3 : y4))) * og;
            hy = ha * (hy + ov - hx); hx = ov;
            if (std::fabs(y4) < 1e-18) y1 = y2 = y3 = y4 = 0;
            if (std::fabs(hy) < 1e-18) hy = 0;

            // --- amp, compressor, velocidade, master
            double sig = ov * aval;
            double ab = std::fabs(sig); ce = ab > ce ? ab : ce * cer;
            if ((cn++ & 3) == 0) cgt = ce > 1e-5 ? sat(ce) / ce : 1;
            cg += (cgt - cg) * (cgt < cg ? cca : ccr);
            sig *= cg * mk;
            velGain += (velGainT - velGain) * vc; sig *= velGain;
            masterG += (masterT - masterG) * mc; sig *= masterG;
            // delay em paralelo
            double dtSm = dtT; (void)dtSm; dtS += (dtT - dtS) * tcCoef(0.05); dfS += (dfT - dfS) * tcCoef(0.03); dmS += (dmT - dmS) * tcCoef(0.03);
            const int L = (int)dline.size();
            double rp = dw - dtS * sr; while (rp < 0) rp += L;
            int i0 = (int)rp; double fr_ = rp - i0; int i1 = i0 + 1 >= L ? 0 : i0 + 1;
            double dv = dline[i0] * (1 - fr_) + dline[i1] * fr_;
            double lp_ = dlb0 * dv + dz1; dz1 = dlb1 * dv - dla1 * lp_ + dz2; dz2 = dlb2 * dv - dla2 * lp_;
            dline[dw] = (float)(sig + dfS * lp_); if (++dw >= L) dw = 0;
            double mix = sig + dmS * lp_;
            // limitador soft (mesma curva do navegador: entrada x/3 sobre uma curva definida em [-3,3])
            preOut = mix;
            const double xp = std::fabs(mix);
            const double outv = xp < 0.6 ? xp : 0.6 + 0.4 * std::tanh((xp - 0.6) / 0.4);
            out[i] = (float)(mix < 0 ? -outv : outv);
        }
    }
};

} // namespace aura
