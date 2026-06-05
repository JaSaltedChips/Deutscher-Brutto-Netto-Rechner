// =============================================================================
// main.cpp
// --------
// Entry point for the German payroll calculator.
//
// Reads two YAML files:
//   constants_YYYY.yaml  – tax rates, BBG limits, tariff zones for the year
//   config_<name>.yaml   – employee data and per-month gross/bonus amounts
//
// Produces one payslip text file per month and one annual summary, written
// to the --out directory.
//
// Usage:
//   payroll_software_cpp [--config <path>] [--constants <path>]
//                        [--out <dir>] [--monat <1..12>]
//
//   --monat 0  (default) processes all 12 months.
//   --monat N  processes only month N and still accumulates YTD for months
//              before N (so the YTD figures on month N are correct).
// =============================================================================

#include "tax.hpp"
#include "sozialversicherung.hpp"
#include "lohnsteuer.hpp"
#include "payslip.hpp"
#include "annual_summary.hpp"

#include <yaml-cpp/yaml.h>

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

namespace fs = std::filesystem;
using tax::round2;

// ---------------------------------------------------------------------------
// German month names (index 0 = January)
// ---------------------------------------------------------------------------
static const std::string MONTH_NAMES[12] = {
    "Januar", "Februar", "Maerz", "April", "Mai", "Juni",
    "Juli", "August", "September", "Oktober", "November", "Dezember"
};

// ---------------------------------------------------------------------------
// Safe YAML value readers with defaults
// ---------------------------------------------------------------------------
static double yaml_double(const YAML::Node& node, const std::string& key, double def = 0.0) {
    if (node[key] && !node[key].IsNull()) return node[key].as<double>();
    return def;
}
static std::string yaml_str(const YAML::Node& node, const std::string& key,
                            const std::string& def = "") {
    if (node[key] && !node[key].IsNull()) return node[key].as<std::string>();
    return def;
}
static int yaml_int(const YAML::Node& node, const std::string& key, int def = 0) {
    if (node[key] && !node[key].IsNull()) return node[key].as<int>();
    return def;
}
static bool yaml_bool(const YAML::Node& node, const std::string& key, bool def = false) {
    if (node[key] && !node[key].IsNull()) return node[key].as<bool>();
    return def;
}

// ---------------------------------------------------------------------------
// Main processing loop
// ---------------------------------------------------------------------------
static void run(const std::string& config_path,
                const std::string& constants_path,
                const std::string& out_dir,
                int only_month) {

    YAML::Node constants = YAML::LoadFile(constants_path);
    YAML::Node cfg       = YAML::LoadFile(config_path);

    fs::create_directories(out_dir);

    // ---- Employee metadata ----
    const auto& mit = cfg["mitarbeiter"];
    std::string vorname     = yaml_str(mit, "vorname", "Vorname");
    std::string nachname    = yaml_str(mit, "nachname", "Nachname");
    int  geburtsjahr        = yaml_int(mit, "geburtsjahr", 1992);
    bool hat_kinder         = yaml_bool(mit, "hat_kinder", false);
    int  steuerklasse       = yaml_int(mit, "steuerklasse", 1);
    std::string konfession  = yaml_str(mit, "konfession", "none");
    std::string bundesland  = yaml_str(mit, "bundesland", "BW");
    std::string krankenkasse = yaml_str(mit, "krankenkasse", "TK");
    int  jahr               = yaml_int(cfg, "jahr", 2024);

    // ---- YTD accumulators ----
    // These track cumulative totals as we iterate months in order.
    // They are needed both for:
    //   a) the YTD display lines on each payslip
    //   b) the BBG checks in subsequent months (SV liable base tracking)
    double ytd_brutto      = 0.0;
    double ytd_lst         = 0.0;
    double ytd_soli        = 0.0;
    double ytd_kist        = 0.0;
    double ytd_sv_an       = 0.0;
    double ytd_sv_ag       = 0.0;
    double ytd_netto       = 0.0;
    double ytd_inflation   = 0.0;
    double ytd_nachverr    = 0.0;
    double ytd_gwv         = 0.0;
    double ytd_rkvp        = 0.0;
    double ytd_svst        = 0.0;

    // Cumulative SV-liable bases — needed for the §23a SGB IV pro-rated BBG
    // check on Einmalzahlungen in later months.
    double ytd_sv_pflicht_kv = 0.0;
    double ytd_sv_pflicht_rv = 0.0;
    // Split into laufend vs. einmal components for the §23a check:
    double ytd_laufend_kv    = 0.0;
    double ytd_laufend_rv    = 0.0;
    double ytd_einmal_kv     = 0.0;
    double ytd_einmal_rv     = 0.0;

    // YTD EZ sub-totals
    double ytd_lst_sonstige = 0.0;
    double ytd_kv_an_ez = 0.0; double ytd_kv_ag_ez = 0.0;
    double ytd_pv_an_ez = 0.0; double ytd_pv_ag_ez = 0.0;
    double ytd_rv_an_ez = 0.0; double ytd_rv_ag_ez = 0.0;
    double ytd_av_an_ez = 0.0; double ytd_av_ag_ez = 0.0;

    // ---- Annual summary accumulators ----
    double sum_brutto_laufend = 0.0;
    double sum_sonstige       = 0.0;   // Weihnachtsgeld only (not 92CC)
    double sum_gwv            = 0.0;
    double sum_rkvp_year      = 0.0;
    double sum_svst           = 0.0;
    double sum_inflation      = 0.0;
    double sum_nachverr       = 0.0;
    double sum_kv_an = 0.0; double sum_kv_ag = 0.0;
    double sum_pv_an = 0.0; double sum_pv_ag = 0.0;
    double sum_rv_an = 0.0; double sum_rv_ag = 0.0;
    double sum_av_an = 0.0; double sum_av_ag = 0.0;
    double sum_personalkosten = 0.0;

    // ---- Iterate over months defined in the config ----
    const YAML::Node& monate = cfg["monate"];

    for (const auto& entry : monate) {
        int monat = yaml_int(entry, "monat", 0);
        if (monat < 1 || monat > 12) continue;

        std::string monatsname = yaml_str(entry, "name", MONTH_NAMES[monat - 1]);

        // Gross pay components for this month
        double brutto_laufend  = yaml_double(entry, "brutto_laufend");
        double sonstige_wg     = yaml_double(entry, "sonstige_bezuege");  // Weihnachtsgeld
        double geldwerter_vorteil = yaml_double(entry, "geldwerter_vorteil");
        double rkvp            = yaml_double(entry, "rkvp");
        // 92CC SV/ST-Anteile: taxable EZ that is grossed up for SV/tax purposes
        // then immediately deducted before the net payout (see NETTO section).
        double sv_st_anteile   = yaml_double(entry, "sv_st_anteile");
        double inflationsgeld  = yaml_double(entry, "inflationsgeld");
        double nachverrechnung = yaml_double(entry, "nachverrechnung");

        // Total EZ for SV purposes:
        //   - Weihnachtsgeld and 92CC are taxable EZ → SV via §23a
        //   - GWV (92DB) is SV-liable as EZ but not taxable for the AN
        // All three are passed as einmalzahlung to berechne_sv so that
        // the §23a pro-rated BBG cap is applied correctly.
        double sv_einmal = round2(sonstige_wg + sv_st_anteile + geldwerter_vorteil);

        // Total taxable gross (steuerpflichtig): GWV is excluded because the
        // employer pays the flat tax (§37b EStG); the employee's own LSt is
        // unaffected.
        double brutto_gesamt = round2(brutto_laufend + sonstige_wg + sv_st_anteile);

        // EZ for LSt (Jahresdifferenzmethode): GWV is NOT included because
        // it is AN-steuerfrei.
        double lohnsteuer_sonstige = round2(sonstige_wg + sv_st_anteile);

        // ---- Social insurance ----
        sv::SVErgebnis sv_res = sv::berechne_sv(
            round2(brutto_laufend + sv_einmal),
            ytd_sv_pflicht_kv,
            ytd_sv_pflicht_rv,
            constants,
            hat_kinder,
            geburtsjahr,
            jahr,
            brutto_laufend,
            sv_einmal,
            monat,
            ytd_laufend_kv,
            ytd_laufend_rv,
            ytd_einmal_kv,
            ytd_einmal_rv
        );

        // ---- Wage tax ----
        lohnsteuer::LStErgebnis lst_res = lohnsteuer::berechne_lohnsteuer(
            brutto_laufend,
            lohnsteuer_sonstige,
            constants,
            hat_kinder,
            geburtsjahr,
            jahr,
            steuerklasse
        );

        // ---- Solidarity surcharge and church tax ----
        // Compute on the projected annual LSt; choose the annual figure that
        // includes any EZ if there is one (Soli on sonstige uses the same
        // Jahresdifferenz logic as LSt itself).
        double jahres_lst_for_soli = (lohnsteuer_sonstige > 0)
            ? lst_res.annual_lst_incl_sonstige
            : lst_res.annual_lst_laufend;

        double soli_laufend = tax::solidaritaetszuschlag(
            lst_res.annual_lst_laufend, constants, steuerklasse);
        double kist_laufend = tax::kirchensteuer(
            lst_res.annual_lst_laufend, constants, konfession, bundesland);

        double soli_year_incl = tax::solidaritaetszuschlag(
            jahres_lst_for_soli, constants, steuerklasse);
        double kist_year_incl = tax::kirchensteuer(
            jahres_lst_for_soli, constants, konfession, bundesland);

        double soli_monat = round2(
            lohnsteuer_sonstige > 0
            ? soli_laufend / 12.0 + std::max(0.0, soli_year_incl - soli_laufend)
            : soli_laufend / 12.0);
        double kist_monat = round2(
            lohnsteuer_sonstige > 0
            ? kist_laufend / 12.0 + std::max(0.0, kist_year_incl - kist_laufend)
            : kist_laufend / 12.0);

        // ---- Netto ----
        double summe_steuern = round2(lst_res.lst_gesamt + soli_monat + kist_monat);
        double netto = round2(brutto_gesamt - summe_steuern - sv_res.sv_an_gesamt
                              - sv_st_anteile      // 92CC deducted back before payout
                              + inflationsgeld
                              + nachverrechnung);
        double personalkosten_ag = round2(brutto_gesamt + sv_res.sv_ag_gesamt
                                          + inflationsgeld);

        // ---- Update YTD accumulators ----
        ytd_sv_pflicht_kv = round2(ytd_sv_pflicht_kv + sv_res.sv_brutto_monat);
        ytd_sv_pflicht_rv = round2(ytd_sv_pflicht_rv + sv_res.sv_brutto_monat_rv);
        ytd_laufend_kv    = round2(ytd_laufend_kv + sv_res.pflicht_kvpv_laufend);
        ytd_laufend_rv    = round2(ytd_laufend_rv + sv_res.pflicht_rvav_laufend);
        ytd_einmal_kv     = round2(ytd_einmal_kv  + sv_res.pflicht_kvpv_einmal);
        ytd_einmal_rv     = round2(ytd_einmal_rv  + sv_res.pflicht_rvav_einmal);

        ytd_lst_sonstige = round2(ytd_lst_sonstige + lst_res.lst_sonstige);
        ytd_kv_an_ez = round2(ytd_kv_an_ez + sv_res.kv_an_ez);
        ytd_kv_ag_ez = round2(ytd_kv_ag_ez + sv_res.kv_ag_ez);
        ytd_pv_an_ez = round2(ytd_pv_an_ez + sv_res.pv_an_ez);
        ytd_pv_ag_ez = round2(ytd_pv_ag_ez + sv_res.pv_ag_ez);
        ytd_rv_an_ez = round2(ytd_rv_an_ez + sv_res.rv_an_ez);
        ytd_rv_ag_ez = round2(ytd_rv_ag_ez + sv_res.rv_ag_ez);
        ytd_av_an_ez = round2(ytd_av_an_ez + sv_res.av_an_ez);
        ytd_av_ag_ez = round2(ytd_av_ag_ez + sv_res.av_ag_ez);

        ytd_brutto   = round2(ytd_brutto   + brutto_gesamt);
        ytd_lst      = round2(ytd_lst      + lst_res.lst_gesamt);
        ytd_soli     = round2(ytd_soli     + soli_monat);
        ytd_kist     = round2(ytd_kist     + kist_monat);
        ytd_sv_an    = round2(ytd_sv_an    + sv_res.sv_an_gesamt);
        ytd_sv_ag    = round2(ytd_sv_ag    + sv_res.sv_ag_gesamt);
        ytd_netto    = round2(ytd_netto    + netto);
        ytd_inflation = round2(ytd_inflation + inflationsgeld);
        ytd_nachverr = round2(ytd_nachverr + nachverrechnung);
        ytd_gwv      = round2(ytd_gwv      + geldwerter_vorteil);
        ytd_rkvp     = round2(ytd_rkvp     + rkvp);
        ytd_svst     = round2(ytd_svst     + sv_st_anteile);

        // ---- Annual summary accumulators ----
        sum_brutto_laufend = round2(sum_brutto_laufend + brutto_laufend);
        sum_sonstige       = round2(sum_sonstige       + sonstige_wg);
        sum_gwv            = round2(sum_gwv            + geldwerter_vorteil);
        sum_rkvp_year      = round2(sum_rkvp_year      + rkvp);
        sum_svst           = round2(sum_svst           + sv_st_anteile);
        sum_inflation      = round2(sum_inflation      + inflationsgeld);
        sum_nachverr       = round2(sum_nachverr       + nachverrechnung);
        sum_kv_an = round2(sum_kv_an + sv_res.kv_an);
        sum_kv_ag = round2(sum_kv_ag + sv_res.kv_ag);
        sum_pv_an = round2(sum_pv_an + sv_res.pv_an);
        sum_pv_ag = round2(sum_pv_ag + sv_res.pv_ag);
        sum_rv_an = round2(sum_rv_an + sv_res.rv_an);
        sum_rv_ag = round2(sum_rv_ag + sv_res.rv_ag);
        sum_av_an = round2(sum_av_an + sv_res.av_an);
        sum_av_ag = round2(sum_av_ag + sv_res.av_ag);
        sum_personalkosten = round2(sum_personalkosten + personalkosten_ag);

        // ---- Write payslip only for the requested month(s) ----
        if (only_month != 0 && monat != only_month) continue;

        // Build rate_pv_kinderlos: only non-zero when the employee is childless
        // AND at least 23 years old (§55 SGB XI).
        double rate_pv_kinderlos = 0.0;
        if (!hat_kinder && (jahr - geburtsjahr) >= 23)
            rate_pv_kinderlos = constants["sv"]["pv"]["kinderlosenzuschlag"].as<double>();

        payslip::MonatsErgebnis m{};
        m.monat           = monat;
        m.monatsname      = monatsname;
        m.jahr            = jahr;
        m.vorname         = vorname;
        m.nachname        = nachname;
        m.geburtsjahr     = geburtsjahr;
        m.steuerklasse    = steuerklasse;
        m.konfession      = konfession;
        m.bundesland      = bundesland;
        m.krankenkasse    = krankenkasse;

        m.brutto_laufend      = round2(brutto_laufend);
        m.sonstige_bezuege    = round2(sonstige_wg);
        m.geldwerter_vorteil  = round2(geldwerter_vorteil);
        m.rkvp                = round2(rkvp);
        m.sv_st_anteile       = round2(sv_st_anteile);
        m.inflationsgeld      = round2(inflationsgeld);
        m.nachverrechnung     = round2(nachverrechnung);
        m.brutto_gesamt       = brutto_gesamt;

        m.lst_laufend  = lst_res.lst_laufend;
        m.lst_sonstige = lst_res.lst_sonstige;
        m.lst_gesamt   = lst_res.lst_gesamt;
        m.soli         = soli_monat;
        m.kist         = kist_monat;

        m.kv_an = sv_res.kv_an; m.kv_ag = sv_res.kv_ag;
        m.pv_an = sv_res.pv_an; m.pv_ag = sv_res.pv_ag;
        m.rv_an = sv_res.rv_an; m.rv_ag = sv_res.rv_ag;
        m.av_an = sv_res.av_an; m.av_ag = sv_res.av_ag;
        m.sv_an_gesamt   = sv_res.sv_an_gesamt;
        m.sv_ag_gesamt   = sv_res.sv_ag_gesamt;
        m.sv_brutto_monat = sv_res.sv_brutto_monat;

        m.kv_an_ez = sv_res.kv_an_ez; m.kv_ag_ez = sv_res.kv_ag_ez;
        m.pv_an_ez = sv_res.pv_an_ez; m.pv_ag_ez = sv_res.pv_ag_ez;
        m.rv_an_ez = sv_res.rv_an_ez; m.rv_ag_ez = sv_res.rv_ag_ez;
        m.av_an_ez = sv_res.av_an_ez; m.av_ag_ez = sv_res.av_ag_ez;

        m.netto                    = netto;
        m.gesamt_personalkosten_ag = personalkosten_ag;

        m.ytd_brutto    = ytd_brutto;
        m.ytd_lst       = ytd_lst;
        m.ytd_soli      = ytd_soli;
        m.ytd_kist      = ytd_kist;
        m.ytd_sv_an     = ytd_sv_an;
        m.ytd_sv_ag     = ytd_sv_ag;
        m.ytd_netto     = ytd_netto;
        m.ytd_inflationsgeld     = ytd_inflation;
        m.ytd_nachverrechnung    = ytd_nachverr;
        m.ytd_geldwerter_vorteil = ytd_gwv;
        m.ytd_rkvp               = ytd_rkvp;
        m.ytd_sv_st_anteile      = ytd_svst;

        m.ytd_lst_sonstige = ytd_lst_sonstige;
        m.ytd_kv_an_ez = ytd_kv_an_ez; m.ytd_kv_ag_ez = ytd_kv_ag_ez;
        m.ytd_pv_an_ez = ytd_pv_an_ez; m.ytd_pv_ag_ez = ytd_pv_ag_ez;
        m.ytd_rv_an_ez = ytd_rv_an_ez; m.ytd_rv_ag_ez = ytd_rv_ag_ez;
        m.ytd_av_an_ez = ytd_av_an_ez; m.ytd_av_ag_ez = ytd_av_ag_ez;

        m.projected_annual_gross     = lst_res.projected_annual_gross;
        m.annual_vorsorgepauschale   = lst_res.annual_vorsorgepauschale;
        m.zve_laufend_annual         = lst_res.zve_laufend_annual;
        m.zve_incl_sonstige_annual   = lst_res.zve_incl_sonstige_annual;
        m.annual_lst_laufend         = lst_res.annual_lst_laufend;
        m.annual_lst_incl_sonstige   = lst_res.annual_lst_incl_sonstige;
        m.extra_vorsorge_sonstige    = lst_res.extra_vorsorge_sonstige;

        m.bbg_kvpv_monat    = constants["sv"]["kv"]["bbg_monat"].as<double>();
        m.bbg_rvav_monat    = constants["sv"]["rv"]["bbg_monat"].as<double>();
        m.rate_kv_allgemein = constants["sv"]["kv"]["allgemein"].as<double>();
        m.rate_kv_zusatz    = constants["sv"]["kv"]["tk_zusatz"].as<double>();
        m.rate_pv_base      = constants["sv"]["pv"]["base"].as<double>();
        m.rate_pv_kinderlos = rate_pv_kinderlos;
        m.rate_rv           = constants["sv"]["rv"]["rate"].as<double>();
        m.rate_av           = constants["sv"]["av"]["rate"].as<double>();

        // Build output filename: payslip_YYYY_MM_MonatsName.txt
        std::string month_str = (monat < 10 ? "0" : "") + std::to_string(monat);
        std::string filename  = "payslip_" + std::to_string(jahr) + "_"
                                + month_str + "_" + monatsname + ".txt";
        std::string out_path  = out_dir + "/" + filename;

        std::ofstream ofs(out_path);
        if (!ofs) {
            std::cerr << "ERROR: cannot write " << out_path << "\n";
            continue;
        }
        ofs << payslip::render_payslip(m);
        std::cout << "OK  " << out_path << "\n";
    }

    // ---- Annual summary ----
    annual_summary::JahresSumme js{};
    js.jahr            = jahr;
    js.vorname         = vorname;
    js.nachname        = nachname;
    js.geburtsjahr     = geburtsjahr;
    js.steuerklasse    = steuerklasse;

    js.brutto_jahr             = sum_brutto_laufend;
    js.sonstige_jahr           = sum_sonstige;
    js.geldwerter_vorteil_jahr = sum_gwv;
    js.rkvp_jahr               = sum_rkvp_year;
    js.sv_st_anteile_jahr      = sum_svst;
    js.brutto_gesamt           = round2(sum_brutto_laufend + sum_sonstige + sum_svst);
    js.inflationsgeld_jahr     = sum_inflation;
    js.nachverrechnung_jahr    = sum_nachverr;

    js.lst_jahr  = ytd_lst;
    js.soli_jahr = ytd_soli;
    js.kist_jahr = ytd_kist;

    js.kv_an_jahr = sum_kv_an; js.kv_ag_jahr = sum_kv_ag;
    js.pv_an_jahr = sum_pv_an; js.pv_ag_jahr = sum_pv_ag;
    js.rv_an_jahr = sum_rv_an; js.rv_ag_jahr = sum_rv_ag;
    js.av_an_jahr = sum_av_an; js.av_ag_jahr = sum_av_ag;
    js.sv_an_jahr = round2(sum_kv_an + sum_pv_an + sum_rv_an + sum_av_an);
    js.sv_ag_jahr = round2(sum_kv_ag + sum_pv_ag + sum_rv_ag + sum_av_ag);

    js.kv_an_ez_jahr = ytd_kv_an_ez; js.kv_ag_ez_jahr = ytd_kv_ag_ez;
    js.pv_an_ez_jahr = ytd_pv_an_ez; js.pv_ag_ez_jahr = ytd_pv_ag_ez;
    js.rv_an_ez_jahr = ytd_rv_an_ez; js.rv_ag_ez_jahr = ytd_rv_ag_ez;
    js.av_an_ez_jahr = ytd_av_an_ez; js.av_ag_ez_jahr = ytd_av_ag_ez;
    js.lst_ez_jahr   = ytd_lst_sonstige;

    js.netto_jahr            = ytd_netto;
    js.personalkosten_ag_jahr = sum_personalkosten;

    std::string summary_path = out_dir + "/summary_" + std::to_string(jahr) + ".txt";
    std::ofstream sofs(summary_path);
    if (!sofs) {
        std::cerr << "ERROR: cannot write " << summary_path << "\n";
    } else {
        sofs << annual_summary::render_summary(js, constants);
        std::cout << "OK  " << summary_path << "\n";
    }
}

// ---------------------------------------------------------------------------
// CLI argument parsing
// ---------------------------------------------------------------------------
int main(int argc, char* argv[]) {
    // Defaults: read the existing Python project's YAML files directly,
    // write output into ./out relative to the C++ binary.
    std::string config_path    = "../payroll_software/config_harsha.yaml";
    std::string constants_path = "../payroll_software/constants_2024.yaml";
    std::string out_dir        = "./out";
    int only_month = 0;  // 0 = all months

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if ((arg == "--config" || arg == "-c") && i + 1 < argc)
            config_path = argv[++i];
        else if ((arg == "--constants" || arg == "-k") && i + 1 < argc)
            constants_path = argv[++i];
        else if ((arg == "--out" || arg == "-o") && i + 1 < argc)
            out_dir = argv[++i];
        else if ((arg == "--monat" || arg == "-m") && i + 1 < argc)
            only_month = std::stoi(argv[++i]);
        else if (arg == "--help" || arg == "-h") {
            std::cout <<
                "Usage: payroll_software_cpp [options]\n"
                "  --config    <path>   Employee YAML config (default: ../payroll_software/config_harsha.yaml)\n"
                "  --constants <path>   Tax constants YAML  (default: ../payroll_software/constants_2024.yaml)\n"
                "  --out       <dir>    Output directory     (default: ./out)\n"
                "  --monat     <1..12>  Single month only    (0 = all, default: 0)\n";
            return 0;
        } else {
            std::cerr << "Unknown argument: " << arg << "\n";
            return 1;
        }
    }

    try {
        run(config_path, constants_path, out_dir, only_month);
    } catch (const std::exception& ex) {
        std::cerr << "ERROR: " << ex.what() << "\n";
        return 1;
    }
    return 0;
}
