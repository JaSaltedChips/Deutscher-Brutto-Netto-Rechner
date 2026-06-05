#include "tax.hpp"
#include <cmath>
#include <stdexcept>

namespace tax {

// ---------------------------------------------------------------------------
// Rounding helpers
// ---------------------------------------------------------------------------

double round2(double value) {
    // Multiply by 100, apply standard round() (which is ROUND_HALF_UP for
    // positive numbers in IEEE 754), then divide back.
    // This matches Python's Decimal ROUND_HALF_UP used throughout the
    // original payroll code.
    return std::round(value * 100.0) / 100.0;
}

double floor_cents(double value) {
    // The BMF Programmablaufplan requires that the monthly wage tax be
    // computed by truncating (not rounding) to the nearest cent:
    //   monthly_tax = floor(annual_tax * 100 / 12) / 100
    // Passing the pre-divided value here: floor_cents(annual / 12).
    return std::floor(value * 100.0) / 100.0;
}

// ---------------------------------------------------------------------------
// §32a EStG income-tax tariff
// ---------------------------------------------------------------------------

double einkommensteuer(double zve, const YAML::Node& constants) {
    if (zve <= 0.0) return 0.0;

    const auto& t  = constants["tarif"];
    double gfb     = t["grundfreibetrag"].as<double>();
    const auto& z2 = t["zone2"];
    const auto& z3 = t["zone3"];
    const auto& z4 = t["zone4"];
    const auto& z5 = t["zone5"];

    // §32a Abs. 1: zvE is rounded DOWN to a whole Euro before applying
    // any zone formula.
    double x = std::floor(zve);

    double est = 0.0;

    if (x <= gfb) {
        // Zone 1: below Grundfreibetrag — fully tax-free
        est = 0.0;

    } else if (x <= z2["upper"].as<double>()) {
        // Zone 2: progressive entry range (Eingangssatz).
        // Formula: y = (x - Grundfreibetrag) / 10,000
        //          ESt = (a·y + b)·y
        double y = (x - gfb) / 10000.0;
        est = (z2["a"].as<double>() * y + z2["b"].as<double>()) * y;

    } else if (x <= z3["upper"].as<double>()) {
        // Zone 3: progressive mid-range.
        // Formula: z = (x - 17,005) / 10,000
        //          ESt = (a·z + b)·z + c
        double z = (x - z2["upper"].as<double>()) / 10000.0;
        est = (z3["a"].as<double>() * z + z3["b"].as<double>()) * z
              + z3["c"].as<double>();

    } else if (x <= z4["upper"].as<double>()) {
        // Zone 4: flat 42 %
        // Formula: ESt = 0.42·x − constant
        est = z4["rate"].as<double>() * x - z4["constant"].as<double>();

    } else {
        // Zone 5: "Reichensteuer" 45 %
        // Formula: ESt = 0.45·x − constant
        est = z5["rate"].as<double>() * x - z5["constant"].as<double>();
    }

    // §32a Abs. 1: result is also floored to a whole Euro.
    return std::floor(est);
}

// ---------------------------------------------------------------------------
// §4 SolZG – solidarity surcharge
// ---------------------------------------------------------------------------

double solidaritaetszuschlag(double annual_wage_tax,
                             const YAML::Node& constants,
                             int steuerklasse) {
    if (annual_wage_tax <= 0.0) return 0.0;

    const auto& s = constants["soli"];
    double freigrenze = (steuerklasse == 3)
        ? s["freigrenze_stkl_III"].as<double>()
        : s["freigrenze_stkl_I"].as<double>();

    // Below the Freigrenze: no Soli at all (applies to most employees
    // since the threshold was raised dramatically in 2021).
    if (annual_wage_tax <= freigrenze) return 0.0;

    // Full Soli: 5.5 % of annual wage tax
    double full = s["rate"].as<double>() * annual_wage_tax;

    // Milderungszone (phase-in zone): if the full Soli would cause a sudden
    // jump, cap it at 11.9 % of the excess above the Freigrenze.
    // The employee pays whichever is smaller.
    double mild = s["milderungs_rate"].as<double>()
                  * (annual_wage_tax - freigrenze);

    return round2(std::min(full, mild));
}

// ---------------------------------------------------------------------------
// Church tax (Kirchensteuer)
// ---------------------------------------------------------------------------

double kirchensteuer(double annual_wage_tax,
                     const YAML::Node& constants,
                     const std::string& konfession,
                     const std::string& bundesland) {
    if (konfession == "none" || konfession.empty() || konfession == "keine")
        return 0.0;

    const auto& k = constants["kirchensteuer"];
    double rate = (bundesland == "BW" || bundesland == "BY")
        ? k["baden_wuerttemberg"].as<double>()  // 8 % in BW and Bavaria
        : k["andere_bundeslaender"].as<double>(); // 9 % elsewhere

    return round2(rate * annual_wage_tax);
}

// ---------------------------------------------------------------------------
// Vorsorgepauschale
// ---------------------------------------------------------------------------

double vorsorgepauschale_jahr(double an_rv, double an_kv, double an_pv) {
    // Simple sum of the three statutory components.
    // Each component is already annual and pre-rounded by the caller
    // (the SV simulation loop in lohnsteuer.cpp).
    return round2(an_rv + an_kv + an_pv);
}

} // namespace tax
