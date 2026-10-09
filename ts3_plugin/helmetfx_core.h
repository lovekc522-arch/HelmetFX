#pragma once
// Build:  g++ -std=c++17 -O2 -pthread helmetfx_core.cpp -o helmetfx_test
//   MSVC: cl /std:c++17 /O2 /EHsc helmetfx_core.cpp
//
// STAGES (use these names in the chain string):
//   lp   low-pass          hz, q
//   hp   high-pass         hz, q
//   bp   band-pass         hz, q
//   eq   peaking EQ        hz, q, db
//   ls   low shelf         hz, q, db
//   hs   high shelf        hz, q, db
//   gain volume            db
//   comp compressor        thr, ratio, atk, rel, makeup
//   dist distortion        drive, mode(0 soft,1 hard,2 fold), mix, outdb
//   crush bitcrusher       bits, down, mix
//   ring ring modulator    hz, mix
//   comb comb/flanger      ms, fb, mix
//   reverb room reverb     size, damp, mix
//   pitch pitch shifter    semi, win, mix
//   noise breath/hiss      type(0 white,1 soft), db, breath, depth, gate
//   limit safety limiter   ceil

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <functional>
#include <map>
#include <memory>
#include <mutex>
#include <sstream>
#include <string>
#include <vector>

#if defined(__SSE2__) || defined(_M_X64)
#include <xmmintrin.h>
#define HELMETFX_HAS_SSE 1
#endif

static const float PI = 3.14159265358979f;

// stage setting
using ParamMap = std::map<std::string, float>;

static float getParam(const ParamMap& p, const std::string& key, float def) {
    auto it = p.find(key);
    return it == p.end() ? def : it->second;
}
// read a parameter and force safe range
static float param(const ParamMap& p, const char* key, float def, float lo, float hi) {
    float v = getParam(p, key, def);
    if (!(v == v)) v = def;
    return v < lo ? lo : (v > hi ? hi : v);
}
static float clampf(float v, float lo, float hi) { return v < lo ? lo : (v > hi ? hi : v); }
static float dbToLin(float db) { return std::pow(10.f, db / 20.f); }

// glides a value toward its target so settings changes never click
struct Smooth {
    float cur = 0, target = 0, coef = 0.01f;
    bool  started = false;
    void setRate(float sr, float ms = 10.f) { coef = 1.f - std::exp(-1.f / (sr * ms * 0.001f)); }
    void set(float v) { target = v; if (!started) { cur = v; started = true; } }
    float next() { cur += (target - cur) * coef; return cur; }
};

// stage interface
// each instance has its own memory, so you can put the same effect in the chain several times
class Stage {
public:
    virtual ~Stage() = default;
    virtual void setParams(const ParamMap& p, float sampleRate) = 0;
    virtual void process(float* buf, int n) = 0;
    virtual int latencySamples() const { return 0; }
};

class Biquad : public Stage {
public:
    enum Kind { LowPass, HighPass, BandPass, Peak, LowShelf, HighShelf };
    explicit Biquad(Kind k) : kind_(k) {}

    void setParams(const ParamMap& p, float sr) override {
        float hz = param(p, "hz", 1000.f, 20.f, sr * 0.45f);   // must stay below Nyquist
        float q  = param(p, "q", 0.707f, 0.1f, 10.f);
        float db = param(p, "db", 0.f, -24.f, 24.f);

        float w0 = 2.f * PI * hz / sr, cw = std::cos(w0), sw = std::sin(w0);
        float alpha = sw / (2.f * q);
        float A = std::pow(10.f, db / 40.f);
        float b0, b1, b2, a0, a1, a2;

        switch (kind_) {
        case LowPass:
            b0 = (1 - cw) / 2; b1 = 1 - cw; b2 = b0;
            a0 = 1 + alpha; a1 = -2 * cw; a2 = 1 - alpha; break;
        case HighPass:
            b0 = (1 + cw) / 2; b1 = -(1 + cw); b2 = b0;
            a0 = 1 + alpha; a1 = -2 * cw; a2 = 1 - alpha; break;
        case BandPass:
            b0 = alpha; b1 = 0; b2 = -alpha;
            a0 = 1 + alpha; a1 = -2 * cw; a2 = 1 - alpha; break;
        case Peak:
            b0 = 1 + alpha * A; b1 = -2 * cw; b2 = 1 - alpha * A;
            a0 = 1 + alpha / A; a1 = -2 * cw; a2 = 1 - alpha / A; break;
        case LowShelf: {
            float beta = 2 * std::sqrt(A) * alpha;
            b0 = A * ((A + 1) - (A - 1) * cw + beta);
            b1 = 2 * A * ((A - 1) - (A + 1) * cw);
            b2 = A * ((A + 1) - (A - 1) * cw - beta);
            a0 = (A + 1) + (A - 1) * cw + beta;
            a1 = -2 * ((A - 1) + (A + 1) * cw);
            a2 = (A + 1) + (A - 1) * cw - beta; break; }
        default: { // highshelf
            float beta = 2 * std::sqrt(A) * alpha;
            b0 = A * ((A + 1) + (A - 1) * cw + beta);
            b1 = -2 * A * ((A - 1) + (A + 1) * cw);
            b2 = A * ((A + 1) + (A - 1) * cw - beta);
            a0 = (A + 1) - (A - 1) * cw + beta;
            a1 = 2 * ((A - 1) - (A + 1) * cw);
            a2 = (A + 1) - (A - 1) * cw - beta; break; }
        }
        b0_ = b0 / a0; b1_ = b1 / a0; b2_ = b2 / a0; a1_ = a1 / a0; a2_ = a2 / a0;
    }

    void process(float* buf, int n) override {
        for (int i = 0; i < n; ++i) {
            float x = buf[i];
            float y = b0_ * x + z1_;
            z1_ = b1_ * x - a1_ * y + z2_;
            z2_ = b2_ * x - a2_ * y;
            buf[i] = y;
        }
    }
private:
    Kind  kind_;
    float b0_ = 1, b1_ = 0, b2_ = 0, a1_ = 0, a2_ = 0, z1_ = 0, z2_ = 0;
};

class Gain : public Stage {
public:
    void setParams(const ParamMap& p, float sr) override {
        g_.setRate(sr);
        g_.set(dbToLin(param(p, "db", 0.f, -40.f, 24.f)));
    }
    void process(float* buf, int n) override {
        for (int i = 0; i < n; ++i) buf[i] *= g_.next();
    }
private:
    Smooth g_;
};

class Compressor : public Stage {
public:
    void setParams(const ParamMap& p, float sr) override {
        thrDb_  = param(p, "thr", -18.f, -60.f, 0.f);
        thrLin_ = dbToLin(thrDb_);
        ratio_  = param(p, "ratio", 4.f, 1.f, 20.f);
        float atkMs = param(p, "atk", 5.f, 0.1f, 200.f);
        float relMs = param(p, "rel", 100.f, 5.f, 1000.f);
        atk_ = std::exp(-1.f / (sr * atkMs * 0.001f));
        rel_ = std::exp(-1.f / (sr * relMs * 0.001f));
        makeup_ = dbToLin(param(p, "makeup", 0.f, -12.f, 24.f));
    }
    void process(float* buf, int n) override {
        for (int i = 0; i < n; ++i) {
            float x = buf[i], a = std::fabs(x);
            float c = a > env_ ? atk_ : rel_;
            env_ = a + c * (env_ - a);
            float g = makeup_;
            if (env_ > thrLin_) {
                float overDb = 20.f * std::log10(env_) - thrDb_;
                g *= dbToLin(-overDb * (1.f - 1.f / ratio_));
            }
            buf[i] = x * g;
        }
    }
private:
    float thrDb_ = -18, thrLin_ = 0.125f, ratio_ = 4, atk_ = 0.99f, rel_ = 0.999f,
          makeup_ = 1, env_ = 0;
};

// mode 0 = soft/tanh, 1 = hard clip, 2 = wave fold
class Distortion : public Stage {
public:
    void setParams(const ParamMap& p, float sr) override {
        drive_ = param(p, "drive", 4.f, 1.f, 50.f);
        mode_  = (int)param(p, "mode", 0.f, 0.f, 2.f);
        out_   = dbToLin(param(p, "outdb", 0.f, -24.f, 12.f));
        norm_  = (mode_ == 0) ? 1.f / std::tanh(drive_) : 1.f;   // keep level sane
        mix_.setRate(sr);
        mix_.set(param(p, "mix", 1.f, 0.f, 1.f));
    }
    void process(float* buf, int n) override {
        for (int i = 0; i < n; ++i) {
            float x = buf[i], v = x * drive_, y;
            if (mode_ == 0)      y = std::tanh(v) * norm_;
            else if (mode_ == 1) y = clampf(v, -1.f, 1.f);
            else                 y = std::fabs(std::fabs(std::fmod(v - 1.f, 4.f)) - 2.f) - 1.f;
            float m = mix_.next();
            buf[i] = (x * (1.f - m) + y * m) * out_;
        }
    }
private:
    float drive_ = 4, norm_ = 1, out_ = 1; int mode_ = 0; Smooth mix_;
};

class Bitcrush : public Stage {
public:
    void setParams(const ParamMap& p, float sr) override {
        levels_ = std::pow(2.f, param(p, "bits", 8.f, 1.f, 16.f) - 1.f);
        down_   = (int)param(p, "down", 1.f, 1.f, 64.f);
        mix_.setRate(sr);
        mix_.set(param(p, "mix", 1.f, 0.f, 1.f));
    }
    void process(float* buf, int n) override {
        for (int i = 0; i < n; ++i) {
            if (count_ <= 0) { held_ = std::round(buf[i] * levels_) / levels_; count_ = down_; }
            --count_;
            float m = mix_.next();
            buf[i] = buf[i] * (1.f - m) + held_ * m;
        }
    }
private:
    float levels_ = 128; int down_ = 1, count_ = 0; float held_ = 0; Smooth mix_;
};

class RingMod : public Stage {
public:
    void setParams(const ParamMap& p, float sr) override {
        inc_ = 2.f * PI * param(p, "hz", 80.f, 1.f, 5000.f) / sr;
        mix_.setRate(sr);
        mix_.set(param(p, "mix", 0.5f, 0.f, 1.f));
    }
    void process(float* buf, int n) override {
        for (int i = 0; i < n; ++i) {
            float y = buf[i] * std::sin(phase_);
            phase_ += inc_; if (phase_ > 2.f * PI) phase_ -= 2.f * PI;
            float m = mix_.next();
            buf[i] = buf[i] * (1.f - m) + y * m;
        }
    }
private:
    float phase_ = 0, inc_ = 0.01f; Smooth mix_;
};

class Comb : public Stage {
public:
    void setParams(const ParamMap& p, float sr) override {
        if (sr != sr_) { sr_ = sr; buf_.assign((size_t)(0.06f * sr) + 4, 0.f); w_ = 0; }
        delay_ = param(p, "ms", 5.f, 0.5f, 50.f) * 0.001f * sr;
        fb_ = param(p, "fb", 0.5f, -0.95f, 0.95f);
        mix_.setRate(sr);
        mix_.set(param(p, "mix", 0.5f, 0.f, 1.f));
    }
    void process(float* buf, int n) override {
        int N = (int)buf_.size();
        for (int i = 0; i < n; ++i) {
            float pos = (float)w_ - delay_; if (pos < 0) pos += N;
            int i0 = (int)pos, i1 = (i0 + 1 >= N) ? 0 : i0 + 1;
            float fr = pos - i0;
            float d = buf_[i0] * (1 - fr) + buf_[i1] * fr;
            float y = buf[i] + fb_ * d;
            buf_[w_] = y; if (++w_ >= N) w_ = 0;
            float m = mix_.next();
            buf[i] = buf[i] * (1.f - m) + y * m;
        }
    }
private:
    std::vector<float> buf_; int w_ = 0; float sr_ = 0, delay_ = 100, fb_ = 0.5f; Smooth mix_;
};

class Reverb : public Stage {
public:
    void setParams(const ParamMap& p, float sr) override {
        if (sr != sr_) {
            sr_ = sr;
            static const int ct[8] = {1116, 1188, 1277, 1356, 1422, 1491, 1557, 1617};
            static const int at[4] = {556, 441, 341, 225};
            for (int i = 0; i < 8; ++i) { c_[i].b.assign((size_t)(ct[i] * sr / 44100.f) + 1, 0.f); c_[i].i = 0; c_[i].store = 0; }
            for (int i = 0; i < 4; ++i) { a_[i].b.assign((size_t)(at[i] * sr / 44100.f) + 1, 0.f); a_[i].i = 0; }
        }
        fb_ = 0.7f + 0.28f * param(p, "size", 0.3f, 0.f, 1.f);
        d1_ = 0.4f * param(p, "damp", 0.5f, 0.f, 1.f);
        d2_ = 1.f - d1_;
        mix_.setRate(sr);
        mix_.set(param(p, "mix", 0.25f, 0.f, 1.f));
    }
    void process(float* buf, int n) override {
        for (int i = 0; i < n; ++i) {
            float in = buf[i] * 0.015f, wet = 0;
            for (auto& c : c_) wet += c.run(in, fb_, d1_, d2_);
            for (auto& a : a_) wet = a.run(wet);
            float m = mix_.next();
            buf[i] = buf[i] * (1.f - m) + wet * m * WET_GAIN;
        }
    }
private:
    static constexpr float WET_GAIN = 3.f;
    struct CombF {
        std::vector<float> b; int i = 0; float store = 0;
        float run(float in, float fb, float d1, float d2) {
            float o = b[i];
            store = o * d2 + store * d1;
            b[i] = in + store * fb;
            if (++i >= (int)b.size()) i = 0;
            return o;
        }
    };
    struct AllP {
        std::vector<float> b; int i = 0;
        float run(float in) {
            float bo = b[i];
            b[i] = in + bo * 0.5f;
            if (++i >= (int)b.size()) i = 0;
            return bo - in;
        }
    };
    CombF c_[8]; AllP a_[4];
    float sr_ = 0, fb_ = 0.78f, d1_ = 0.2f, d2_ = 0.8f; Smooth mix_;
};

class PitchShift : public Stage {
public:
    void setParams(const ParamMap& p, float sr) override {
        if (sr != sr_) { sr_ = sr; buf_.assign((size_t)(0.25f * sr) + 8, 0.f); w_ = 0; d_ = 0; }
        float semi = param(p, "semi", 0.f, -12.f, 12.f);
        ratio_ = std::pow(2.f, semi / 12.f);
        W_ = param(p, "win", 40.f, 15.f, 100.f) * 0.001f * sr;
        if (d_ >= W_) d_ = std::fmod(d_, W_);
        latency_ = (int)(W_ * 0.5f);
        mix_.setRate(sr);
        mix_.set(param(p, "mix", 1.f, 0.f, 1.f));
    }
    int latencySamples() const override { return latency_; }
    void process(float* buf, int n) override {
        const int N = (int)buf_.size();
        const float rate = 1.f - ratio_;
        for (int i = 0; i < n; ++i) {
            buf_[w_] = buf[i];
            float d1 = d_, d2 = d_ + W_ * 0.5f; if (d2 >= W_) d2 -= W_;
            float w1 = 1.f - std::fabs(2.f * d1 / W_ - 1.f);
            float w2 = 1.f - std::fabs(2.f * d2 / W_ - 1.f);
            float y = read(w_, d1, N) * w1 + read(w_, d2, N) * w2;
            d_ += rate;
            if (d_ >= W_) d_ -= W_; else if (d_ < 0) d_ += W_;
            if (++w_ >= N) w_ = 0;
            float m = mix_.next();
            buf[i] = buf[i] * (1.f - m) + y * m;
        }
    }
private:
    float read(int w, float d, int N) const {
        float pos = (float)w - d; if (pos < 0) pos += N;
        int i0 = (int)pos, i1 = (i0 + 1 >= N) ? 0 : i0 + 1;
        float fr = pos - i0;
        return buf_[i0] * (1 - fr) + buf_[i1] * fr;
    }
    std::vector<float> buf_; int w_ = 0, latency_ = 0;
    float sr_ = 0, d_ = 0, W_ = 1920, ratio_ = 1; Smooth mix_;
};

// gate: voice level (dB) above which noise is allowed. If silence turned noise on, you would transmit constantly
// breath: speed of a slow volume wobble in Hz (0 = steady), depth 0..1.
class Noise : public Stage {
public:
    Noise() { static uint32_t seedCounter = 0x9E3779B9u; seedCounter = seedCounter * 1664525u + 1013904223u; rng_ = seedCounter | 1u; }
    void setParams(const ParamMap& p, float sr) override {
        type_   = (int)param(p, "type", 1.f, 0.f, 1.f);
        level_.setRate(sr);
        level_.set(dbToLin(param(p, "db", -40.f, -80.f, 0.f)));
        breathInc_ = 2.f * PI * param(p, "breath", 0.f, 0.f, 2.f) / sr;
        depth_  = param(p, "depth", 0.5f, 0.f, 1.f);
        float g = param(p, "gate", -50.f, -80.f, 0.f);
        gateLin_ = (g <= -80.f) ? 0.f : dbToLin(g);
        relCoef_ = std::exp(-1.f / (sr * 0.15f));
        openCoef_ = 1.f - std::exp(-1.f / (sr * 0.01f));
    }
    void process(float* buf, int n) override {
        for (int i = 0; i < n; ++i) {
            float x = buf[i], a = std::fabs(x);
            env_ = a > env_ ? a : a + relCoef_ * (env_ - a);
            open_ += ((env_ >= gateLin_ ? 1.f : 0.f) - open_) * openCoef_;

            rng_ ^= rng_ << 13; rng_ ^= rng_ >> 17; rng_ ^= rng_ << 5;
            float r = (float)(rng_ >> 8) / 8388608.f - 1.f;
            if (type_ == 1) { lp_ += 0.25f * (r - lp_); r = lp_ * 2.f; }

            float lfo = 1.f;
            if (breathInc_ > 0) {
                lfo = 1.f - depth_ * (0.5f + 0.5f * std::sin(phase_));
                phase_ += breathInc_; if (phase_ > 2.f * PI) phase_ -= 2.f * PI;
            }
            buf[i] = x + r * level_.next() * open_ * lfo;
        }
    }
private:
    uint32_t rng_; int type_ = 1; Smooth level_;
    float breathInc_ = 0, depth_ = 0.5f, phase_ = 0, gateLin_ = 0.003f,
          env_ = 0, open_ = 0, lp_ = 0, relCoef_ = 0.999f, openCoef_ = 0.002f;
};

// nothing may exceed "ceil" dB. buildChain adds one at the end automatically so a bad chain can never blast anyone's ears.
class Limiter : public Stage {
public:
    void setParams(const ParamMap& p, float sr) override {
        ceil_ = dbToLin(param(p, "ceil", -1.f, -24.f, 0.f));
        rel_ = std::exp(-1.f / (sr * 0.05f));
    }
    void process(float* buf, int n) override {
        for (int i = 0; i < n; ++i) {
            float a = std::fabs(buf[i]);
            float need = a > ceil_ ? ceil_ / a : 1.f;
            g_ = need < g_ ? need : 1.f + rel_ * (g_ - 1.f);
            buf[i] *= g_;
        }
    }
private:
    float ceil_ = 0.89f, rel_ = 0.999f, g_ = 1.f;
};

// full chain and staging
struct Chain {
    std::vector<std::unique_ptr<Stage>> stages;
    void process(float* buf, int n) { for (auto& s : stages) s->process(buf, n); }
    int latencySamples() const {
        int t = 0; for (auto& s : stages) t += s->latencySamples(); return t;
    }
};

using Factory = std::function<std::unique_ptr<Stage>()>;

template <class T, class... Args>
static Factory maker(Args... args) {
    return [=]() -> std::unique_ptr<Stage> { return std::unique_ptr<Stage>(new T(args...)); };
}

static const std::map<std::string, Factory>& registry() {
    static const std::map<std::string, Factory> r = {
        {"lp",     maker<Biquad>(Biquad::LowPass)},
        {"hp",     maker<Biquad>(Biquad::HighPass)},
        {"bp",     maker<Biquad>(Biquad::BandPass)},
        {"eq",     maker<Biquad>(Biquad::Peak)},
        {"ls",     maker<Biquad>(Biquad::LowShelf)},
        {"hs",     maker<Biquad>(Biquad::HighShelf)},
        {"gain",   maker<Gain>()},
        {"comp",   maker<Compressor>()},
        {"dist",   maker<Distortion>()},
        {"crush",  maker<Bitcrush>()},
        {"ring",   maker<RingMod>()},
        {"comb",   maker<Comb>()},
        {"reverb", maker<Reverb>()},
        {"pitch",  maker<PitchShift>()},
        {"noise",  maker<Noise>()},
        {"limit",  maker<Limiter>()},
    };
    return r;
}

static std::vector<std::string> split(const std::string& s, char sep) {
    std::vector<std::string> out; std::stringstream ss(s); std::string part;
    while (std::getline(ss, part, sep)) out.push_back(part);
    return out;
}

std::shared_ptr<Chain> buildChain(const std::string& text, float sampleRate,
                                  std::vector<std::string>* warnings = nullptr) {
    auto chain = std::make_shared<Chain>();
    const size_t MAX_STAGES = 16;
    bool endsWithLimit = false;

    for (const auto& stageText : split(text, '|')) {
        if (stageText.empty()) continue;
        if (chain->stages.size() >= MAX_STAGES) { if (warnings) warnings->push_back("too many stages"); break; }

        auto colon = stageText.find(':');
        std::string type = stageText.substr(0, colon);
        ParamMap params;
        if (colon != std::string::npos) {
            for (const auto& kv : split(stageText.substr(colon + 1), ',')) {
                auto eq = kv.find('=');
                if (eq == std::string::npos) continue;
                try { params[kv.substr(0, eq)] = std::stof(kv.substr(eq + 1)); }
                catch (...) { if (warnings) warnings->push_back("bad number: " + kv); }
            }
        }
        auto it = registry().find(type);
        if (it == registry().end()) { if (warnings) warnings->push_back("unknown stage: " + type); continue; }

        auto stage = it->second();
        stage->setParams(params, sampleRate);
        chain->stages.push_back(std::move(stage));
        endsWithLimit = (type == "limit");
    }
    if (!chain->stages.empty() && !endsWithLimit) {
        auto lim = registry().at("limit")();
        lim->setParams({}, sampleRate);
        chain->stages.push_back(std::move(lim));
    }
    return chain;
}

class ChainHolder {
public:
    void publish(std::shared_ptr<Chain> next) {
        std::shared_ptr<Chain> old = std::atomic_load(&current_);
        std::atomic_store(&current_, std::move(next));
        std::lock_guard<std::mutex> lock(shelfMutex_);
        if (old) retired_.push_back(std::move(old));
    }
    void emptyShelf() {
        std::lock_guard<std::mutex> lock(shelfMutex_);
        retired_.clear();
    }
    std::shared_ptr<Chain> grab() const { return std::atomic_load(&current_); }
private:
    std::shared_ptr<Chain> current_;
    std::vector<std::shared_ptr<Chain>> retired_;
    std::mutex shelfMutex_;
};

static ChainHolder g_holder;

// audio thread
void onMicChunk(int16_t* samples, int count) {
#ifdef HELMETFX_HAS_SSE
    _mm_setcsr(_mm_getcsr() | 0x8040); // treat tiny "denormal" numbers as zero (avoids CPU spikes)
#endif
    auto chain = g_holder.grab();
    if (!chain) return;

    static thread_local std::vector<float> scratch;
    if ((int)scratch.size() < count) scratch.resize(count);

    for (int i = 0; i < count; ++i) scratch[i] = samples[i] / 32768.f;
    chain->process(scratch.data(), count);
    for (int i = 0; i < count; ++i)
        samples[i] = (int16_t)(clampf(scratch[i], -1.f, 1.f) * 32767.f);
}