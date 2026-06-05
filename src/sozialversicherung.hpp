#pragma once
// =============================================================================
// sozialversicherung.hpp
// ----------------------
// Social-insurance contribution calculation (Sozialversicherungsbeitraege).
//
// Germany has four branches of mandatory social insurance (§§ 1 ff. SGB IV):
//   KV  Krankenversicherung     – statutory health insurance
//   PV  Pflegeversicherung      – long-term care insurance
//   RV  Rentenversicherung      – statutory pension insurance
//   AV  Arbeitslosenversicherung – unemployment insurance
//
// Each branch has a contribution ceiling (Beitragsbemessungsgrenze, BBG).
// Earnings above the BBG are not subject to that branch's contribution.
// KV/PV share one (lower) BBG; RV/AV share a higher BBG.
//
// Key legal rules implemented here:
//   §22 SGB IV  – monthly BBG for regular (laufend) wages
//   §23a SGB IV – pro-rated annual BBG for one-time payments (Einmalzahlungen)
// =============================================================================

#include <yaml-cpp/yaml.h>

namespace sv {

// ---------------------------------------------------------------------------
// Result struct for one month's social-insurance calculation
// ---------------------------------------------------------------------------
struct SVErgebnis {
    // Total SV-liable earnings this month (after BBG caps)
    double sv_brutto_monat;      // KV/PV base
    double sv_brutto_monat_rv;   // RV/AV base (higher BBG)

    // KV – health insurance
    double kv_an;   // employee share
    double kv_ag;   // employer share

    // PV – long-term care
    double pv_an;
    double pv_ag;

    // RV – pension
    double rv_an;
    double rv_ag;

    // AV – unemployment
    double av_an;
    double av_ag;

    // Totals
    double sv_an_gesamt;
    double sv_ag_gesamt;

    // EZ (Einmalzahlung) components: the portion of each contribution that
    // was triggered by one-time payments (used for "davon EZ" lines on the
    // payslip and for YTD EZ tracking).
    double kv_an_ez; double kv_ag_ez;
    double pv_an_ez; double pv_ag_ez;
    double rv_an_ez; double rv_ag_ez;
    double av_an_ez; double av_ag_ez;

    // Liable bases broken out by type — needed by main.cpp to correctly
    // maintain the separate YTD accumulators for laufend vs. Einmalzahlung
    // (required for correct §23a SGB IV checks in subsequent months).
    double pflicht_kvpv_laufend;
    double pflicht_kvpv_einmal;
    double pflicht_rvav_laufend;
    double pflicht_rvav_einmal;
};

// ---------------------------------------------------------------------------
// Main calculation function
// ---------------------------------------------------------------------------

// Computes all four SV branches for one payroll month.
//
// Parameters:
//   brutto_monat         total SV-liable gross (laufend + one-time) — used
//                        only as fallback when laufend/einmal are not split
//   ytd_sv_pflicht_kv    cumulative KV/PV-liable earnings BEFORE this month
//                        (both laufend and Einmalzahlung combined)
//   ytd_sv_pflicht_rv    same for RV/AV
//   constants            loaded constants_YYYY.yaml node
//   has_children         true if the employee has children (affects PV)
//   birth_year           used to determine age for the childless PV surcharge
//   year                 payroll year
//   brutto_laufend       regular monthly wage portion (for monthly BBG check)
//   einmalzahlung        one-time payment portion (for pro-rated annual BBG)
//   month                calendar month 1..12 (for §23a SGB IV pro-ration)
//   ytd_laufend_kv       cumulative KV/PV-liable LAUFEND earnings before this month
//   ytd_laufend_rv       cumulative RV/AV-liable LAUFEND earnings before this month
//   ytd_einmal_kv        cumulative KV/PV-liable EINMAL earnings before this month
//   ytd_einmal_rv        cumulative RV/AV-liable EINMAL earnings before this month
SVErgebnis berechne_sv(double brutto_monat,
                       double ytd_sv_pflicht_kv,
                       double ytd_sv_pflicht_rv,
                       const YAML::Node& constants,
                       bool has_children      = false,
                       int  birth_year        = 1992,
                       int  year              = 2024,
                       double brutto_laufend  = -1.0,
                       double einmalzahlung   = -1.0,
                       int    month           = 1,
                       double ytd_laufend_kv  = 0.0,
                       double ytd_laufend_rv  = 0.0,
                       double ytd_einmal_kv   = 0.0,
                       double ytd_einmal_rv   = 0.0);

} // namespace sv
