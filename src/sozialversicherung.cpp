#include "sozialversicherung.hpp"
#include "tax.hpp"
#include <algorithm>
#include <cmath>

namespace sv {

using tax::round2;

// ---------------------------------------------------------------------------
// Internal helpers: BBG-capped liable bases
// ---------------------------------------------------------------------------

// Determines how much of the REGULAR monthly wage is SV-liable, subject to
// the monthly BBG (§22 SGB IV).
//
// Rule: for laufender Arbeitslohn, only the portion up to the monthly BBG
// is contribution-liable.  An additional annual cap guards against the edge
// case where many months slightly above the BBG would exceed the annual limit.
//
//   liable = min(brutto_laufend, bbg_month)
//          , capped further at (bbg_year - ytd_laufend_before)
static double pflichtig_laufend(double brutto_laufend,
                                double ytd_laufend_before,
                                double bbg_month,
                                double bbg_year) {
    if (brutto_laufend <= 0.0) return 0.0;

    double liable = std::min(brutto_laufend, bbg_month);

    // Annual guard: remaining room in the yearly BBG
    double remaining_year = std::max(0.0, bbg_year - ytd_laufend_before);
    liable = std::min(liable, remaining_year);

    return std::max(0.0, liable);
}

// Determines how much of a ONE-TIME payment (Einmalzahlung / sonstige Bezüge)
// is SV-liable, per §23a SGB IV.
//
// Background: Einmalzahlungen (e.g. Christmas bonus, the 92CC netting amount,
// GWV treated as EZ for SV) are not simply capped at the monthly BBG.
// Instead, the law computes a "pro-rated annual BBG" up to and including the
// payment month:
//
//   bbg_anteilig = bbg_month * month_number   (e.g. July = 7 * 7,550 = 52,850)
//
// All SV-liable earnings so far this year (laufend YTD incl. current month +
// all previous Einmalzahlungen) count against this pro-rated ceiling.
// The one-time payment may use whatever room remains up to that ceiling.
//
// Crucially, the MONTHLY BBG is NOT applied to Einmalzahlungen — a single
// bonus can consume the entire remaining annual room even if it far exceeds
// the monthly limit.
static double pflichtig_einmal(double einmalzahlung,
                               double ytd_laufend_incl_current,
                               double ytd_einmal_before,
                               int    month,
                               double bbg_month,
                               double bbg_year) {
    if (einmalzahlung <= 0.0) return 0.0;

    // Pro-rated annual BBG up to this month
    double bbg_anteilig = std::min(bbg_month * static_cast<double>(month), bbg_year);

    // How much of the annual room has already been consumed?
    double already_used = ytd_laufend_incl_current + ytd_einmal_before;

    double remaining = std::max(0.0, bbg_anteilig - already_used);
    double liable    = std::min(einmalzahlung, remaining);

    return std::max(0.0, liable);
}

// ---------------------------------------------------------------------------
// Main function
// ---------------------------------------------------------------------------

SVErgebnis berechne_sv(double brutto_monat,
                       double ytd_sv_pflicht_kv,
                       double ytd_sv_pflicht_rv,
                       const YAML::Node& constants,
                       bool   has_children,
                       int    birth_year,
                       int    year,
                       double brutto_laufend,
                       double einmalzahlung,
                       int    month,
                       double ytd_laufend_kv,
                       double ytd_laufend_rv,
                       double ytd_einmal_kv,
                       double ytd_einmal_rv) {

    // Backwards-compatibility: if the laufend/einmal split was not provided,
    // treat everything as laufend (no Einmalzahlung).
    if (brutto_laufend < 0.0 && einmalzahlung < 0.0) {
        brutto_laufend = brutto_monat;
        einmalzahlung  = 0.0;
    } else if (brutto_laufend < 0.0) {
        brutto_laufend = std::max(0.0, brutto_monat - einmalzahlung);
    } else if (einmalzahlung < 0.0) {
        einmalzahlung  = std::max(0.0, brutto_monat - brutto_laufend);
    }

    const auto& sv_cfg = constants["sv"];

    // ---- KV / PV BBG -------------------------------------------------------
    double bbg_kv_month = sv_cfg["kv"]["bbg_monat"].as<double>();
    double bbg_kv_year  = sv_cfg["kv"]["bbg_jahr"].as<double>();

    double pflicht_kv_laufend = pflichtig_laufend(
        brutto_laufend, ytd_laufend_kv, bbg_kv_month, bbg_kv_year);

    double pflicht_kv_einmal = pflichtig_einmal(
        einmalzahlung,
        ytd_laufend_kv + pflicht_kv_laufend,
        ytd_einmal_kv,
        month, bbg_kv_month, bbg_kv_year);

    double pflicht_kv = pflicht_kv_laufend + pflicht_kv_einmal;

    // ---- RV / AV BBG (higher limit) ----------------------------------------
    double bbg_rv_month = sv_cfg["rv"]["bbg_monat"].as<double>();
    double bbg_rv_year  = sv_cfg["rv"]["bbg_jahr"].as<double>();

    double pflicht_rv_laufend = pflichtig_laufend(
        brutto_laufend, ytd_laufend_rv, bbg_rv_month, bbg_rv_year);

    double pflicht_rv_einmal = pflichtig_einmal(
        einmalzahlung,
        ytd_laufend_rv + pflicht_rv_laufend,
        ytd_einmal_rv,
        month, bbg_rv_month, bbg_rv_year);

    double pflicht_rv = pflicht_rv_laufend + pflicht_rv_einmal;

    // ---- KV contributions (allgemeiner Beitragssatz + Zusatzbeitrag) -------
    // Total rate = 14.6% (general) + TK Zusatzbeitrag 1.2% = 15.8%
    // Split 50/50 between employee (AN) and employer (AG).
    double kv_rate = sv_cfg["kv"]["allgemein"].as<double>()
                   + sv_cfg["kv"]["tk_zusatz"].as<double>();
    double kv_an = round2(pflicht_kv * kv_rate / 2.0);
    double kv_ag = round2(pflicht_kv * kv_rate / 2.0);

    // ---- PV contributions --------------------------------------------------
    // Base rate: 3.4 % split 50/50.
    // Childless surcharge: 0.6 % paid by the employee ONLY (AN-only),
    // applicable when the employee is childless and at least 23 years old
    // (§55 SGB XI).  Employers are explicitly excluded from this surcharge.
    double pv_base    = sv_cfg["pv"]["base"].as<double>();         // 3.4 %
    double pv_kinderlos = sv_cfg["pv"]["kinderlosenzuschlag"].as<double>(); // 0.6 %

    double pv_an = round2(pflicht_kv * pv_base / 2.0);
    double pv_ag = round2(pflicht_kv * pv_base / 2.0);

    int age = year - birth_year;
    if (!has_children && age >= 23) {
        // Kinderlosenzuschlag: only the employee pays this extra 0.6 %
        pv_an = round2(pv_an + pflicht_kv * pv_kinderlos);
    }

    // ---- RV contributions --------------------------------------------------
    // 18.6 % total, split 50/50 (9.3 % each).
    double rv_rate = sv_cfg["rv"]["rate"].as<double>();
    double rv_an = round2(pflicht_rv * rv_rate / 2.0);
    double rv_ag = round2(pflicht_rv * rv_rate / 2.0);

    // ---- AV contributions --------------------------------------------------
    // 2.6 % total, split 50/50 (1.3 % each).
    double av_rate = sv_cfg["av"]["rate"].as<double>();
    double av_an = round2(pflicht_rv * av_rate / 2.0);
    double av_ag = round2(pflicht_rv * av_rate / 2.0);

    // ---- EZ (Einmalzahlung) sub-components ---------------------------------
    // These are purely informational lines on the payslip ("davon EZ").
    // They represent the fraction of each contribution attributable to
    // one-time payments rather than regular wages.
    double kv_an_ez = round2(pflicht_kv_einmal * kv_rate / 2.0);
    double kv_ag_ez = round2(pflicht_kv_einmal * kv_rate / 2.0);

    double pv_an_ez_base = pflicht_kv_einmal * pv_base / 2.0;
    double pv_ag_ez_base = pflicht_kv_einmal * pv_base / 2.0;
    if (!has_children && age >= 23) {
        pv_an_ez_base += pflicht_kv_einmal * pv_kinderlos;
    }
    double pv_an_ez = round2(pv_an_ez_base);
    double pv_ag_ez = round2(pv_ag_ez_base);

    double rv_an_ez = round2(pflicht_rv_einmal * rv_rate / 2.0);
    double rv_ag_ez = round2(pflicht_rv_einmal * rv_rate / 2.0);

    double av_an_ez = round2(pflicht_rv_einmal * av_rate / 2.0);
    double av_ag_ez = round2(pflicht_rv_einmal * av_rate / 2.0);

    // ---- Totals ------------------------------------------------------------
    double sv_an = round2(kv_an + pv_an + rv_an + av_an);
    double sv_ag = round2(kv_ag + pv_ag + rv_ag + av_ag);

    SVErgebnis result{};
    result.sv_brutto_monat    = round2(pflicht_kv);
    result.sv_brutto_monat_rv = round2(pflicht_rv);

    result.kv_an = kv_an; result.kv_ag = kv_ag;
    result.pv_an = pv_an; result.pv_ag = pv_ag;
    result.rv_an = rv_an; result.rv_ag = rv_ag;
    result.av_an = av_an; result.av_ag = av_ag;

    result.sv_an_gesamt = sv_an;
    result.sv_ag_gesamt = sv_ag;

    result.kv_an_ez = kv_an_ez; result.kv_ag_ez = kv_ag_ez;
    result.pv_an_ez = pv_an_ez; result.pv_ag_ez = pv_ag_ez;
    result.rv_an_ez = rv_an_ez; result.rv_ag_ez = rv_ag_ez;
    result.av_an_ez = av_an_ez; result.av_ag_ez = av_ag_ez;

    result.pflicht_kvpv_laufend = round2(pflicht_kv_laufend);
    result.pflicht_kvpv_einmal  = round2(pflicht_kv_einmal);
    result.pflicht_rvav_laufend = round2(pflicht_rv_laufend);
    result.pflicht_rvav_einmal  = round2(pflicht_rv_einmal);

    return result;
}

} // namespace sv
