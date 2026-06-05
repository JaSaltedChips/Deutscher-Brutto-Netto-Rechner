# German Payroll Calculator — C++ Implementation

A C++17 port of the Python payroll software that computes monthly German
payslips (Gehaltsabrechnungen) and an annual summary for a single employee,
reading the same YAML configuration files as the original Python version.

---

## What Is Built

### Modules

| Source file | Responsibility |
|---|---|
| `src/tax.hpp/.cpp` | Core tax functions: §32a EStG income-tax tariff, solidarity surcharge (§4 SolZG), church tax, Vorsorgepauschale (§39b EStG), rounding helpers |
| `src/sozialversicherung.hpp/.cpp` | Social-insurance contributions (KV, PV, RV, AV) with monthly BBG cap (§22 SGB IV) and pro-rated annual BBG for one-time payments (§23a SGB IV) |
| `src/lohnsteuer.hpp/.cpp` | Monthly wage-tax projection (§39b EStG): 12-month SV simulation for Vorsorgepauschale, PAP-compliant floor-in-cents rounding, Jahresdifferenzmethode for one-time payments |
| `src/payslip.hpp/.cpp` | `MonatsErgebnis` data struct and German text renderer for the monthly payslip |
| `src/annual_summary.hpp/.cpp` | `JahresSumme` data struct and renderer for the annual summary including an estimated income-tax refund/surcharge (Veranlagungsschätzung) |
| `src/main.cpp` | CLI argument parsing, YAML loading, month-by-month processing loop, YTD accumulation, file output |

### Output

Running the program produces one `.txt` file per calendar month and one annual
summary file in the `--out` directory:

```
out/
├── payslip_2024_01_Januar.txt
├── payslip_2024_02_Februar.txt
├── ...
├── payslip_2024_12_Dezember.txt
└── summary_2024.txt
```

Each payslip includes:
- Gross pay breakdown (laufend, Weihnachtsgeld, GWV, 92CC, Inflationsgeld, Nachverrechnung)
- Taxes: Lohnsteuer (laufend + EZ Jahresdifferenzmethode), Solidaritätszuschlag, Kirchensteuer
- Social insurance: KV, PV, RV, AV with AN/AG split and "davon EZ" sub-lines
- Netto calculation
- YTD accumulators (Jan – current month)
- Lohnsteuerbescheinigung key figures (Felder 3–6, 17)
- Tax calculation detail (projected zvE, Vorsorgepauschale, §32a tariff)

The annual summary adds an estimated Einkommensteuer Veranlagung comparison
(withheld Lohnsteuer vs. estimated actual tax) so the employee can anticipate
a refund or surcharge before filing their Steuererklärung.

### Key German Payroll Rules Implemented

| Rule | Where |
|---|---|
| §32a EStG five-zone income-tax tariff | `tax.cpp` |
| §39b EStG Lohnsteuer projection via 12× monthly gross | `lohnsteuer.cpp` |
| §39b Vorsorgepauschale using **allgemeiner** KV-Beitragssatz 14.6 % + Zusatzbeitrag (not the ermäßigter 14.0 %) | `lohnsteuer.cpp` |
| BMF PAP floor-in-cents rounding for monthly LSt: `floor(annual × 100 / 12) / 100` | `lohnsteuer.cpp` |
| §39b Abs. 3 Jahresdifferenzmethode for sonstige Bezüge (Weihnachtsgeld, 92CC) | `lohnsteuer.cpp` |
| §22 SGB IV monthly BBG cap for laufender Arbeitslohn | `sozialversicherung.cpp` |
| §23a SGB IV pro-rated annual BBG for Einmalzahlungen | `sozialversicherung.cpp` |
| §55 SGB XI PV Kinderlosenzuschlag 0.6 % (AN-only, childless + age ≥ 23) | `sozialversicherung.cpp` |
| GWV (92DB §37b EStG): SV-liable as EZ, but AN-steuerfrei | `main.cpp` |
| 92CC (SV/ST-Anteile Nettierung): taxable EZ for both LSt and SV, deducted before Netto | `main.cpp` |
| Inflationsausgleichsprämie (§3 Nr. 11c EStG): entirely tax- and SV-free | `main.cpp` |
| Nachverrechnung: already-taxed back-payment, added directly to Netto | `main.cpp` |

---

## Prerequisites

| Tool | Minimum version | Notes |
|---|---|---|
| CMake | 3.14 | [cmake.org](https://cmake.org/download/) |
| C++ compiler | MSVC 2017 / GCC 8 / Clang 7 | Must support C++17 |
| Internet access | — | CMake fetches yaml-cpp automatically on first configure |

> **yaml-cpp** is downloaded automatically via CMake `FetchContent` — no manual
> installation is needed.

---

## Build

```bash
# 1. Enter the project directory
cd payroll_software_cpp

# 2. Create and enter a build directory
mkdir build
cd build

# 3. Configure (downloads yaml-cpp on first run — needs internet)
cmake .. -DCMAKE_BUILD_TYPE=Release

# 4. Compile
cmake --build . --config Release
```

The compiled binary is placed in `payroll_software_cpp/Release/` (Windows/MSVC)
or `payroll_software_cpp/build/` (Linux/macOS with a single-config generator).

---

## Run

From the `payroll_software_cpp/` directory:

```bash
# All 12 months + annual summary (default output: ./out/)
./Release/payroll_software_cpp.exe

# Single month (e.g. July)
./Release/payroll_software_cpp.exe --monat 7

# Custom paths
./Release/payroll_software_cpp.exe \
    --config    ../payroll_software/config_harsha.yaml \
    --constants ../payroll_software/constants_2024.yaml \
    --out       ./out \
    --monat     0

# Help
./Release/payroll_software_cpp.exe --help
```

### CLI Options

| Option | Short | Default | Description |
|---|---|---|---|
| `--config <path>` | `-c` | `../payroll_software/config_harsha.yaml` | Employee YAML config |
| `--constants <path>` | `-k` | `../payroll_software/constants_2024.yaml` | Tax/SV constants for the year |
| `--out <dir>` | `-o` | `./out` | Output directory (created if absent) |
| `--monat <1..12>` | `-m` | `0` (all) | Process a single month; `0` = all 12 |

> When `--monat N` is used, the program still processes all preceding months
> internally so that the YTD figures on month N are correct.

---

## Project Structure

```
payroll_software_cpp/
├── CMakeLists.txt          # Build definition; fetches yaml-cpp automatically
├── README.md               # This file
├── build/                  # CMake build artefacts (not committed)
├── Release/                # Compiled binary (Windows/MSVC output)
│   └── payroll_software_cpp.exe
├── out/                    # Generated payslips and summary
│   ├── payslip_2024_01_Januar.txt
│   ├── ...
│   └── summary_2024.txt
└── src/
    ├── main.cpp
    ├── tax.hpp / tax.cpp
    ├── sozialversicherung.hpp / sozialversicherung.cpp
    ├── lohnsteuer.hpp / lohnsteuer.cpp
    ├── payslip.hpp / payslip.cpp
    └── annual_summary.hpp / annual_summary.cpp
```

---

## Configuration Files

Both files are shared with the Python project and read directly from
`../payroll_software/` by default.

### `constants_2024.yaml`

Holds all year-specific tax and SV parameters:

- `tarif` — §32a EStG zone boundaries and formula coefficients
- `soli` — Freigrenze and Milderungszone rates
- `kirchensteuer` — rates by Bundesland
- `sv.kv` — KV allgemeiner Beitragssatz, TK Zusatzbeitrag, BBG month/year
- `sv.pv` — PV base rate, Kinderlosenzuschlag, BBG
- `sv.rv` — RV rate, BBG month/year
- `sv.av` — AV rate, BBG month/year

To use a different tax year, create a `constants_202X.yaml` with the updated
values and pass it via `--constants`.

### `config_harsha.yaml`

Holds employee data and per-month gross figures:

- `mitarbeiter` — name, Geburtsjahr, Steuerklasse, Konfession, Bundesland, Krankenkasse, hat_kinder
- `arbeitgeber` — name, Ort, Bundesland
- `jahr` — payroll year
- `monate` — list of 12 entries, each with:
  - `brutto_laufend` — regular monthly wage
  - `sonstige_bezuege` — one-time payment (e.g. Weihnachtsgeld)
  - `geldwerter_vorteil` — 92DB benefit-in-kind (SV-EZ, AN-steuerfrei)
  - `sv_st_anteile` — 92CC taxable EZ netting amount
  - `inflationsgeld` — tax- and SV-free inflation bonus
  - `nachverrechnung` — net back-payment (no further tax/SV deduction)
  - `rkvp` — tax-free travel reimbursement (documentary only)

---

## Relationship to the Python Version

This C++ implementation is a direct port of
`../payroll_software/` and produces numerically identical results, with two
corrections that were also applied to the Python source:

1. **KV Vorsorgepauschale rate**: uses the *allgemeiner* Beitragssatz 14.6 %
   (from `constants["sv"]["kv"]["allgemein"]`) instead of the hardcoded
   ermäßigter 14.0 %. This increases the Vorsorgepauschale and reduces the
   monthly Lohnsteuer by a few euros.

2. **Monthly LSt rounding**: uses `floor(annual_tax × 100 / 12) / 100`
   (BMF PAP floor-in-cents) instead of commercial rounding, matching official
   payroll-software output.
