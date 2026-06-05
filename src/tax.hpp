#pragma once
// =============================================================================
// tax.hpp
// -------
// Core German income-tax functions for the payroll calculator.
//
// German tax law references used throughout this file:
//   §32a EStG  – income-tax tariff (Einkommensteuertarif)
//   §4  SolZG  – solidarity surcharge (Solidaritaetszuschlag)
//   §39b EStG  – payroll-tax deduction procedure (Lohnsteuerabzug)
//   §10c EStG  – standard deduction for special expenses (Sonderausgaben-PB)
//   §9a EStG   – employee flat-rate expense deduction (AN-Pauschbetrag)
// =============================================================================

#include <yaml-cpp/yaml.h>

namespace tax {

// ---------------------------------------------------------------------------
// Monetary rounding
// ---------------------------------------------------------------------------

// Commercial rounding to 2 decimal places (ROUND_HALF_UP).
// Used for all SV contributions and most intermediate amounts.
// Matches Python's Decimal(str(x)).quantize("0.01", ROUND_HALF_UP).
double round2(double value);

// Floor-in-cents division (truncate toward zero after multiplying by 100).
// The BMF Programmablaufplan (PAP) specifies that the monthly wage tax is
// computed as  floor(annual_tax * 100 / 12) / 100  — i.e., always round
// DOWN to the nearest cent.  This differs from commercial rounding and is
// why a naive round2(annual / 12) produces a result that is a few cents too
// high compared to the official payroll software.
double floor_cents(double value);

// ---------------------------------------------------------------------------
// §32a EStG – Income-tax tariff
// ---------------------------------------------------------------------------

// Computes the tariff income tax (tarifliche Einkommensteuer) for a given
// annual taxable income (zu versteuerndes Einkommen, zvE).
//
// The tariff has five zones:
//   Zone 1  zvE <= Grundfreibetrag          -> 0 (tax-free allowance)
//   Zone 2  Grundfreibetrag < zvE <= 17,005 -> progressive entry range (y-formula)
//   Zone 3  17,005 < zvE <= 66,760          -> progressive mid-range  (z-formula)
//   Zone 4  66,760 < zvE <= 277,825         -> flat 42 %
//   Zone 5  zvE > 277,825                   -> "Reichensteuer" 45 %
//
// §32a Abs. 1 EStG requires zvE to be rounded DOWN to a whole Euro before
// applying the formula (hence std::floor inside the implementation).
// The result is also floored to a whole Euro.
//
// Rate constants (a, b, c, limits) are loaded from constants_YYYY.yaml so
// that the program works unchanged across tax years.
double einkommensteuer(double zve, const YAML::Node& constants);

// ---------------------------------------------------------------------------
// §4 SolZG – Solidarity surcharge (Solidaritaetszuschlag)
// ---------------------------------------------------------------------------

// The solidarity surcharge is computed on the annual wage tax.
// Rules (Steuerklasse I, 2024):
//   - Below the Freigrenze (18,130 EUR annual LSt for StKl I):  Soli = 0
//   - Milderungszone just above the Freigrenze: Soli = min(5.5% * LSt,
//     11.9% * (LSt – Freigrenze)) — phased in gradually to avoid a cliff
//   - Above the Milderungszone: full 5.5% of annual LSt
//
// The Freigrenze was raised significantly in 2021, so most employees with
// moderate incomes pay zero Soli.
double solidaritaetszuschlag(double annual_wage_tax,
                             const YAML::Node& constants,
                             int steuerklasse = 1);

// ---------------------------------------------------------------------------
// Church tax (Kirchensteuer) — stub
// ---------------------------------------------------------------------------

// Church tax is collected by the state on behalf of religious communities.
// Rate: 8 % of income tax in Baden-Württemberg and Bavaria, 9 % elsewhere.
// If the employee has no registered church membership (konfession == "none"),
// the function returns 0.
double kirchensteuer(double annual_wage_tax,
                     const YAML::Node& constants,
                     const std::string& konfession,
                     const std::string& bundesland);

// ---------------------------------------------------------------------------
// §39b Abs. 4 EStG – Vorsorge­pauschale (pension/health insurance allowance)
// ---------------------------------------------------------------------------

// The Vorsorgepauschale reduces the taxable income used for wage-tax
// projection.  It consists of three components (§39b Abs. 2 S. 5 Nr. 3):
//   a) Employee's statutory pension contribution (RV-AN, 9.3 %)
//   b) Employee's health-insurance contribution at the *allgemeiner*
//      Beitragssatz 14.6 % + Zusatzbeitrag (not the ermäßigter 14.0 %)
//      divided by two (AN half), using the KV/PV BBG as a cap.
//      NOTE: The law specifies the allgemeiner Beitragssatz here; using
//      the ermäßigter 14.0 % underestimates the allowance and produces a
//      slightly higher (incorrect) monthly tax — this is the bug fixed in
//      the Python codebase.
//   c) Employee's long-term care insurance (PV-AN), including the childless
//      surcharge (Kinderlosenzuschlag) where applicable.
//   AV (unemployment insurance) is explicitly excluded from the Vorsorge­
//   pauschale by §39b.
//
// All three annual totals are passed in pre-computed from the SV simulation
// loop in lohnsteuer.cpp.
double vorsorgepauschale_jahr(double an_rv, double an_kv, double an_pv);

// ---------------------------------------------------------------------------
// Constants (deductions that are fixed by law and do not vary by year)
// ---------------------------------------------------------------------------

// Arbeitnehmer-Pauschbetrag: §9a Nr. 1a EStG, flat 1,230 EUR/year (since 2022)
constexpr double ARBEITNEHMER_PAUSCHBETRAG   = 1230.0;

// Sonderausgaben-Pauschbetrag: §10c EStG, flat 36 EUR/year (singles)
constexpr double SONDERAUSGABEN_PAUSCHBETRAG =   36.0;

} // namespace tax
