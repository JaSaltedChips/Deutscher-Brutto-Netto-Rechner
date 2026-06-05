#pragma once
// =============================================================================
// payslip.hpp
// -----------
// Data container and text renderer for a single monthly payslip
// (Gehaltsabrechnung).  Output is German text formatted to 100 characters
// width (suitable for A4 printing in a monospace font).
// =============================================================================

#include <string>

namespace payslip {

// ---------------------------------------------------------------------------
// All data needed to render one month's payslip
// ---------------------------------------------------------------------------
struct MonatsErgebnis {
    // ---- Period and employee identity ----
    int         monat;
    std::string monatsname;   // e.g. "Februar"
    int         jahr;
    std::string vorname;
    std::string nachname;
    int         geburtsjahr;
    int         steuerklasse;
    std::string konfession;
    std::string bundesland;
    std::string krankenkasse;

    // ---- Gross pay components ----
    double brutto_laufend;    // regular monthly wage
    double sonstige_bezuege;  // one-time payment (Weihnachtsgeld etc.)
    // GWV (92DB §37b EStG): employer pays flat tax on a benefit-in-kind;
    // the employee's own income tax is NOT affected — it is SV-liable (as EZ)
    // but not steuer-pflichtig for the AN.  Shown on payslip for transparency.
    double geldwerter_vorteil;
    // RKVP (765L): tax-free travel-cost reimbursement — purely documentary,
    // no SV or tax effect, not included in any deduction calculation.
    double rkvp;
    // 92CC (SV/ST-Anteile Nettierung Sachgeschenk): taxable EZ that represents
    // the SV/tax portion the employer recovered for an in-kind gift; treated
    // as a sonstige Bezüge for both LSt (Jahresdifferenzmethode) and SV
    // (§23a SGB IV EZ rules).  Deducted again before Netto to neutralise the
    // gross-up effect on the cash payout.
    double sv_st_anteile;
    double inflationsgeld;    // tax- and SV-free inflation bonus (§3 Nr. 11c EStG)
    double nachverrechnung;   // net back-payment (already taxed in prior months)
    double brutto_gesamt;     // total taxable gross = laufend + sonstige + sv_st_anteile

    // ---- Taxes (employee share) ----
    double lst_laufend;
    double lst_sonstige;
    double lst_gesamt;
    double soli;
    double kist;

    // ---- Social insurance (monthly) ----
    double kv_an; double kv_ag;
    double pv_an; double pv_ag;
    double rv_an; double rv_ag;
    double av_an; double av_ag;
    double sv_an_gesamt; double sv_ag_gesamt;
    double sv_brutto_monat;   // KV/PV base (for information)

    // EZ (Einmalzahlung) sub-components — the "davon EZ" lines on the payslip
    double kv_an_ez; double kv_ag_ez;
    double pv_an_ez; double pv_ag_ez;
    double rv_an_ez; double rv_ag_ez;
    double av_an_ez; double av_ag_ez;

    // ---- Net pay ----
    double netto;
    double gesamt_personalkosten_ag; // total employer cost = gross + SV-AG

    // ---- YTD accumulators (cumulative after this month) ----
    double ytd_brutto;
    double ytd_lst;
    double ytd_soli;
    double ytd_kist;
    double ytd_sv_an;
    double ytd_sv_ag;
    double ytd_netto;
    double ytd_inflationsgeld;
    double ytd_nachverrechnung;
    double ytd_geldwerter_vorteil;
    double ytd_rkvp;
    double ytd_sv_st_anteile;

    // YTD EZ sub-totals
    double ytd_lst_sonstige;
    double ytd_kv_an_ez; double ytd_kv_ag_ez;
    double ytd_pv_an_ez; double ytd_pv_ag_ez;
    double ytd_rv_an_ez; double ytd_rv_ag_ez;
    double ytd_av_an_ez; double ytd_av_ag_ez;

    // ---- Tax calculation detail (for the explanation section) ----
    double projected_annual_gross;
    double annual_vorsorgepauschale;
    double zve_laufend_annual;
    double zve_incl_sonstige_annual;
    double annual_lst_laufend;
    double annual_lst_incl_sonstige;
    double extra_vorsorge_sonstige;

    // ---- BBG and rate display values (for legend / header) ----
    double bbg_kvpv_monat;
    double bbg_rvav_monat;
    double rate_kv_allgemein;
    double rate_kv_zusatz;
    double rate_pv_base;
    double rate_pv_kinderlos;  // 0 if employee has children
    double rate_rv;
    double rate_av;
};

// Renders a complete payslip as a UTF-8 German text string.
std::string render_payslip(const MonatsErgebnis& m);

} // namespace payslip
