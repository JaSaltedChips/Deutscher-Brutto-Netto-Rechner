#pragma once
// =============================================================================
// lohnsteuer.hpp
// --------------
// Monthly wage-tax (Lohnsteuer) calculation per §39b EStG.
//
// The German wage-tax deduction procedure works as follows:
//
// 1. Project the annual income: take the current monthly regular wage and
//    multiply by 12.  (Simplified projection; the official PAP also handles
//    mid-year salary changes via actual YTD amounts but the 12× approach is
//    standard for ongoing contracts.)
//
// 2. Compute the annual Vorsorgepauschale by simulating 12 months of SV
//    contributions at the current wage level, applying BBG caps.
//    The KV component uses the *allgemeiner Beitragssatz* (14.6 %) + the
//    employer-specific Zusatzbeitrag (e.g. 1.2 % for TK), NOT the reduced
//    ermäßigter Beitragssatz (14.0 %).  Using 14.0 % would underestimate
//    the allowance and overcharge the employee — this was the bug in the
//    original Python code, now corrected.
//
// 3. zvE = projected annual gross
//         − Arbeitnehmer-Pauschbetrag (1,230 EUR)
//         − Sonderausgaben-Pauschbetrag (36 EUR)
//         − Vorsorgepauschale
//
// 4. Apply §32a EStG tariff → annual wage tax → divide by 12.
//    The division uses FLOOR-in-cents (truncate toward zero after multiplying
//    by 100), per the BMF Programmablaufplan (PAP).  This differs from
//    commercial rounding and is why the official monthly tax is always ≤ the
//    commercially-rounded equivalent.
//
// 5. For one-time payments (sonstige Bezüge, §39b Abs. 3 EStG):
//    Jahresdifferenzmethode — compute the tax on (annual laufend + EZ),
//    subtract the tax on annual laufend alone.  The difference is the extra
//    tax due on the one-time payment.  A separate, additional Vorsorge­
//    pauschale is computed for the EZ portion (RV, KV, PV components).
// =============================================================================

#include <yaml-cpp/yaml.h>

namespace lohnsteuer {

// Result of one month's wage-tax calculation
struct LStErgebnis {
    double lst_laufend;    // tax on regular monthly wage
    double lst_sonstige;   // extra tax on one-time payment(s) this month
    double lst_gesamt;     // total monthly wage tax (laufend + sonstige)

    // Detailed intermediate values — exposed for the payslip detail section
    double projected_annual_gross;   // 12 × brutto_laufend
    double annual_vorsorgepauschale; // sum of RV + KV + PV allowance (annual)
    double zve_laufend_annual;       // zvE for regular wage projection
    double zve_incl_sonstige_annual; // zvE after adding one-time payment
    double annual_lst_laufend;       // annual tax on laufend
    double annual_lst_incl_sonstige; // annual tax on laufend + sonstige
    double extra_vorsorge_sonstige;  // additional VSP on the EZ portion
};

// Computes one month's wage tax.
//
// Parameters:
//   brutto_laufend    regular monthly gross (laufend Arbeitslohn)
//   sonstige_bezuege  one-time payment this month (e.g. Weihnachtsgeld,
//                     92CC netting amount) — 0 if none
//   constants         loaded constants_YYYY.yaml node
//   has_children      affects PV Kinderlosenzuschlag in VSP simulation
//   birth_year        for age computation (Kinderlosenzuschlag >= 23)
//   year              payroll year
//   steuerklasse      1..6 (only Steuerklasse I is fully implemented)
LStErgebnis berechne_lohnsteuer(double brutto_laufend,
                                double sonstige_bezuege,
                                const YAML::Node& constants,
                                bool has_children  = false,
                                int  birth_year    = 1992,
                                int  year          = 2024,
                                int  steuerklasse  = 1);

} // namespace lohnsteuer
