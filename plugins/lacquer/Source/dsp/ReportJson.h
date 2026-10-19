#pragma once
// Informe JSON de un master (lo escriben lacquerrender y el modo archivo del plugin): análisis de la entrada, cada
// parámetro de la cadena, la medición de la salida y las notas. Con esto un render se puede reproducir exacto.

#include "EngineMaster.h"

#ifndef LACQUER_VERSION
#define LACQUER_VERSION "1.0.0"
#endif

namespace dh::lq {

inline std::string jnum (double v, int dec = 2)
{
    if (! std::isfinite (v)) return "null";
    char b[64]; std::snprintf (b, sizeof b, "%.*f", dec, v); return b;
}
inline std::string jstr (const std::string& s)
{
    std::string o = "\"";
    for (char c : s) { if (c == '"' || c == '\\') o += '\\'; o += c; }
    return o + "\"";
}
inline std::string jarr (const BandCurve& c, int dec = 2) { std::string o = "["; for (int i = 0; i < kBands; ++i) o += (i ? "," : "") + jnum (c[(size_t) i], dec); return o + "]"; }

struct OutMeas { double lufs, tp, lra, sp, distIn = 0, distOut = 0; };
inline OutMeas measure (const std::vector<float>& L, const std::vector<float>& R, double fs)
{
    LoudnessMeter m; m.prepare (fs);
    for (size_t p = 0; p < L.size(); p += 4096) m.process (L.data() + p, R.data() + p, (int) std::min ((size_t) 4096, L.size() - p));
    return { m.integrated(), m.truePeakDb(), m.lra(), m.samplePeakDb() };
}

inline std::string reportJson (const std::string& in, const std::string& out, const Report& r, const Plan& p, const OutMeas& o, long long clipped, double secs)
{
    const StyleDef& S = styleDef (p.opt.style);
    std::string j = "{\n";
    j += "  \"tool\": \"lacquerrender\", \"version\": " + jstr (LACQUER_VERSION) + ",\n";
    j += "  \"input\": " + jstr (in) + ", \"output\": " + jstr (out) + ",\n";
    j += "  \"settings\": { \"style\": " + jstr (S.id) + ", \"intensity\": " + jnum (p.opt.intensity * 100.0, 0) + ", \"target_lufs\": " + jnum (p.targetLufs) + ", \"ceiling_dbtp\": " + jnum (p.ceilingDb) + " },\n";
    j += "  \"analysis\": { \"sample_rate\": " + jnum (r.sampleRate, 0) + ", \"seconds\": " + jnum (r.seconds) + ", \"lufs_i\": " + jnum (r.lufsI) + ", \"lra\": " + jnum (r.lra)
       + ", \"true_peak_dbtp\": " + jnum (r.truePeakDb) + ", \"sample_peak_dbfs\": " + jnum (r.samplePeakDb) + ", \"plr\": " + jnum (r.plr) + ", \"min_psr\": " + jnum (r.minPsr)
       + ",\n    \"correlation\": " + jnum (r.correlation) + ", \"low_correlation\": " + jnum (r.lowCorrelation) + ", \"crest_db\": " + jnum (r.crestDb) + ", \"onsets_per_s\": " + jnum (r.onsetsPerSec)
       + ", \"harsh_hz\": " + jnum (r.harshHz, 0) + ", \"harsh_excess_db\": " + jnum (r.harshExcessDb) + ", \"sibilance_events_per_s\": " + jnum (r.sibilanceEventsPerSec)
       + ", \"band_limit_hz\": " + jnum (r.bandLimitHz, 0) + ", \"clipped_runs\": " + std::to_string (r.clippedRuns) + ", \"already_mastered\": " + (r.alreadyMastered ? "true" : "false")
       + ",\n    \"band_hz\": " + jarr (bandCentres(), 1) + ",\n    \"mid_db\": " + jarr (r.mid) + ",\n    \"side_minus_mid_db\": " + jarr (r.sideMinusMid) + " },\n";
    j += "  \"chain\": {\n";
    j += "    \"hpf_hz\": " + jnum (p.hpfHz, 0) + ",\n";
    j += "    \"eq\": { \"trim_db\": " + jnum (p.eqTrimDb, 3) + ", \"bands\": [";
    for (int i = 0; i < kEqBands; ++i) { const auto& e = p.eq[(size_t) i]; j += std::string (i ? ", " : "") + "{\"type\":" + jstr (e.type == 0 ? "bell" : e.type == 1 ? "low_shelf" : "high_shelf") + ",\"hz\":" + jnum (e.hz, 0) + ",\"q\":" + jnum (e.q, 3) + ",\"gain_db\":" + jnum (e.gainDb) + "}"; }
    j += "],\n      \"wanted_db\": " + jarr (p.eqWant) + ",\n      \"fitted_db\": " + jarr (p.eqFit) + " },\n";
    auto dyn = [&] (const DynBandParams& d) { return "{\"on\":" + std::string (d.on ? "true" : "false") + ",\"hz\":" + jnum (d.hz, 0) + ",\"q\":" + jnum (d.q) + ",\"threshold_rel_db\":" + jnum (d.thresholdDb) + ",\"max_cut_db\":" + jnum (d.maxCutDb) + "}"; };
    j += "    \"dynamic_eq\": { \"harshness\": " + dyn (p.harsh) + ", \"sibilance\": " + dyn (p.sib) + " },\n";
    auto comp = [&] (const CompParams& c) { return "{\"threshold_db\":" + jnum (c.thresholdDb) + ",\"ratio\":" + jnum (c.ratio) + ",\"knee_db\":" + jnum (c.kneeDb, 1) + ",\"attack_ms\":" + jnum (c.attackMs, 1) + ",\"release_ms\":" + jnum (c.releaseMs, 0) + ",\"makeup_db\":" + jnum (c.makeupDb) + "}"; };
    j += "    \"multiband\": { \"on\": " + std::string (p.mbOn ? "true" : "false") + ", \"crossovers_hz\": [" + jnum (p.xo1, 0) + "," + jnum (p.xo2, 0) + "], \"bands\": [" + comp (p.mb[0]) + ", " + comp (p.mb[1]) + ", " + comp (p.mb[2]) + "] },\n";
    j += "    \"glue\": { \"on\": " + std::string (p.glueOn ? "true" : "false") + ", \"params\": " + comp (p.glue) + " },\n";
    j += "    \"stereo\": { \"on\": " + std::string (p.stereo.on ? "true" : "false") + ", \"mono_below_hz\": " + jnum (p.stereo.monoHz, 0) + ", \"side_gain_db\": " + jnum (p.stereo.sideGainDb) + ", \"side_high_shelf_db\": " + jnum (p.stereo.sideHighShelfDb) + " },\n";
    j += "    \"saturation\": { \"on\": " + std::string (p.sat.on ? "true" : "false") + ", \"flavour\": " + jstr (p.sat.flavour == 0 ? "tape" : "tube") + ", \"drive_db\": " + jnum (p.sat.driveDb) + " },\n";
    j += "    \"gain_db\": " + jnum (p.gainDb, 3) + ",\n";
    j += "    \"clipper\": { \"on\": " + std::string (p.clipOn ? "true" : "false") + ", \"threshold_dbfs\": " + jnum (p.clipThresholdDb) + ", \"clipped_subsamples\": " + std::to_string (clipped) + " },\n";
    j += "    \"limiter\": { \"ceiling_dbtp\": " + jnum (p.lim.ceilingDb) + ", \"lookahead_ms\": " + jnum (p.lim.lookaheadMs, 1) + ", \"release_ms\": [" + jnum (p.lim.releaseFastMs, 0) + "," + jnum (p.lim.releaseSlowMs, 0) + "], \"average_gr_db\": " + jnum (p.limiterAvgGrDb) + ", \"max_gr_db\": " + jnum (p.limiterMaxGrDb) + " }\n";
    j += "  },\n";
    j += "  \"result\": { \"lufs_i\": " + jnum (o.lufs) + ", \"true_peak_dbtp\": " + jnum (o.tp) + ", \"sample_peak_dbfs\": " + jnum (o.sp) + ", \"lra\": " + jnum (o.lra) + ", \"plr\": " + jnum (o.tp - o.lufs) + ", \"spectral_distance_in_db\": " + jnum (o.distIn) + ", \"spectral_distance_out_db\": " + jnum (o.distOut) + " },\n";
    j += "  \"notes\": [";
    for (size_t i = 0; i < p.notes.size(); ++i) j += (i ? ", " : "") + jstr (p.notes[i]);
    j += "],\n  \"seconds_to_render\": " + jnum (secs) + "\n}\n";
    return j;
}


} // namespace dh::lq
