#include "lohnsteuer.hpp"
#include "tax.hpp"
#include "sozialversicherung.hpp"
#include <algorithm>
#include <cmath>

namespace lohnsteuer {

using tax::round2;
using tax::floor_cents;
using tax::einkommensteuer;
using tax::vorsorgepauschale_jahr;
using tax::ARBEITNEHMER_PAUSCHBETRAG;
using tax::SONDERAUSGABEN_PAUSCHBETRAG;

// ---------------------------------------------------------------------------
// Internal: zvE from annual gross and annual Vorsorgepauschale
// ---------------------------------------------------------------------------
static double compute_zve(double annual_gross, double annual_vorsorge) {
    double zve = annual_gross
               - ARBEITNEHMER_PAUSCHBETRAG
               - SONDERAUSGABEN_PAUSCHBETRAG
               - annual_vorsorge;
    return std::max(0.0, zve);
}

// ---------------------------------------------------------------------------
// Main function
// ---------------------------------------------------------------------------

LStErgebnis berechne_lohnsteuer(double brutto_laufend,
                                double sonstige_bezuege,
                                const YAML::Node& constants,
                                bool has_children,
                                int  birth_year,
                                int  year,
                                int  steuerklasse) {

    // -----------------------------------------------------------------------
    // Step 1: project annual gross from the current monthly regular wage
    // -----------------------------------------------------------------------
    double annual_gross_laufend = 12.0 * brutto_laufend;

    // -----------------------------------------------------------------------
    // Step 2: simulate 12 months of SV contributions to compute the annual
    // Vorsorgepauschale (§39b Abs. 2 S. 5 Nr. 3 EStG).
    //
    // Why simulate instead of just multiplying by 12?
    // Because the BBG caps are non-linear: once cumulative earnings exceed
    // the annual BBG, contributions stop.  For high earners who hit the BBG
    // mid-year, a simple 12× would overestimate the Vorsorgepauschale.
    // The simulation correctly handles this by tracking the cumulative
    // KV/PV and RV/AV liable bases across the 12 virtual months.
    //
    // KV component: uses allgemeiner Beitragssatz (14.6%) + Zusatzbeitrag,
    // NOT the ermäßigter Beitragssatz (14.0%).  §39b requires the allgemein
    // rate here.  Using 14.0% was the bug in the original Python code.
    // -----------------------------------------------------------------------
    const auto& sv_cfg = constants["sv"];
    double kv_vorsorge_rate = sv_cfg["kv"]["allgemein"].as<double>()
                            + sv_cfg["kv"]["tk_zusatz"].as<double>();

    double sum_rv_an        = 0.0;
    double sum_kv_vorsorge  = 0.0;
    double sum_pv_an        = 0.0;
    double ytd_sim_kv       = 0.0;
    double ytd_sim_rv       = 0.0;

    for (int m = 0; m < 12; ++m) {
        sv::SVErgebnis sim = sv::berechne_sv(
            brutto_laufend,  // brutto_monat (all laufend, no EZ in simulation)
            ytd_sim_kv,
            ytd_sim_rv,
            constants,
            has_children,
            birth_year,
            year,
            brutto_laufend,  // brutto_laufend
            0.0,             // einmalzahlung = 0 in the projection
            m + 1            // month (1..12)
        );

        sum_rv_an += sim.rv_an;

        // KV Vorsorgepauschale uses the reduced-rate formula with the
        // allgemeiner Beitragssatz on the KV/PV-liable base.
        sum_kv_vorsorge += round2(sim.sv_brutto_monat * kv_vorsorge_rate / 2.0);

        sum_pv_an += sim.pv_an;

        ytd_sim_kv += sim.sv_brutto_monat;
        ytd_sim_rv += sim.sv_brutto_monat_rv;
    }

    double annual_vorsorge = vorsorgepauschale_jahr(sum_rv_an, sum_kv_vorsorge, sum_pv_an);

    // -----------------------------------------------------------------------
    // Step 3: compute annual taxable income (zvE) and annual wage tax
    // -----------------------------------------------------------------------
    double zve_laufend       = compute_zve(annual_gross_laufend, annual_vorsorge);
    double annual_lst_laufend = einkommensteuer(zve_laufend, constants);

    // -----------------------------------------------------------------------
    // Step 4: monthly tax = annual tax / 12, truncated (floor) to the cent.
    // The BMF PAP explicitly specifies floor-division here, not rounding.
    // This is why the monthly tax is always ≤ the commercially-rounded value
    // and matches the official ELSTER / payroll-software output.
    // -----------------------------------------------------------------------
    double lst_laufend_month = floor_cents(annual_lst_laufend / 12.0);

    // -----------------------------------------------------------------------
    // Step 5: Jahresdifferenzmethode for one-time payments (§39b Abs. 3)
    //
    // The one-time payment is NOT spread over 12 months; instead, the full
    // amount is added to the projected annual income and the tax difference
    // (annual tax WITH EZ minus annual tax WITHOUT EZ) is the extra monthly
    // deduction.
    //
    // An additional Vorsorgepauschale is computed for the EZ portion:
    //   - RV: liable up to the remaining annual RV BBG room
    //   - KV: liable up to the remaining annual KV BBG room (and monthly cap)
    //   - PV: follows KV base, includes Kinderlosenzuschlag if applicable
    //   - AV is excluded from the Vorsorgepauschale by §39b
    // -----------------------------------------------------------------------
    double lst_sonstige_month  = 0.0;
    double zve_incl            = zve_laufend;
    double annual_lst_incl     = annual_lst_laufend;
    double extra_vorsorge      = 0.0;

    if (sonstige_bezuege > 0.0) {
        // How much of the annual RV/AV BBG room remains after the laufend
        // projection?
        double rv_remaining = std::max(
            0.0, sv_cfg["rv"]["bbg_jahr"].as<double>() - annual_gross_laufend);
        double rv_liable = std::min(sonstige_bezuege, rv_remaining);

        // KV/PV: same logic, additionally capped at the monthly BBG because
        // the EZ is paid in a single month.
        double kv_remaining_year = std::max(
            0.0, sv_cfg["kv"]["bbg_jahr"].as<double>() - annual_gross_laufend);
        double kv_liable = std::min({sonstige_bezuege,
                                     kv_remaining_year,
                                     sv_cfg["kv"]["bbg_monat"].as<double>()});

        int age = year - birth_year;
        double pv_kinderlos = (!has_children && age >= 23)
            ? sv_cfg["pv"]["kinderlosenzuschlag"].as<double>()
            : 0.0;

        // Extra Vorsorgepauschale attributable to the EZ (same component
        // rules as Step 2, applied only to the EZ-liable base amounts).
        extra_vorsorge = round2(
            rv_liable * sv_cfg["rv"]["rate"].as<double>() / 2.0
            + kv_liable * kv_vorsorge_rate / 2.0
            + kv_liable * (sv_cfg["pv"]["base"].as<double>() / 2.0 + pv_kinderlos)
        );

        double annual_gross_incl = annual_gross_laufend + sonstige_bezuege;
        zve_incl    = compute_zve(annual_gross_incl, annual_vorsorge + extra_vorsorge);
        annual_lst_incl = einkommensteuer(zve_incl, constants);

        lst_sonstige_month = round2(
            std::max(0.0, annual_lst_incl - annual_lst_laufend));
    }

    double lst_gesamt = round2(lst_laufend_month + lst_sonstige_month);

    return LStErgebnis{
        lst_laufend_month,
        lst_sonstige_month,
        lst_gesamt,
        round2(annual_gross_laufend),
        round2(annual_vorsorge),
        round2(zve_laufend),
        round2(zve_incl),
        round2(annual_lst_laufend),
        round2(annual_lst_incl),
        sonstige_bezuege > 0.0 ? round2(extra_vorsorge) : 0.0,
    };
}

} // namespace lohnsteuer
