# German Payroll Calculator — C++ Implementation

A C++17 program that computes monthly German payslips (Gehaltsabrechnungen) and
an annual summary for a single employee, driven by YAML configuration files
stored under `input/<year>/`.

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
# 1. Create and enter a build directory
mkdir build
cd build

# 2. Configure (downloads yaml-cpp on first run — needs internet)
cmake .. -DCMAKE_BUILD_TYPE=Release

# 3. Compile
cmake --build . --config Release
```

The compiled binary is placed in the project root (`Release/` on Windows/MSVC,
or the project root on Linux/macOS with a single-config generator).

---

## Run

From the project root directory:

```bash
# All 12 months + annual summary (default output: ./out/)
./Release/payroll_software_cpp.exe

# Single month (e.g. July)
./Release/payroll_software_cpp.exe --monat 7

# Custom paths
./Release/payroll_software_cpp.exe \
    --config    ./input/2024/config_harsha.yaml \
    --constants ./input/2024/constants.yaml \
    --out       ./out \
    --monat     0

# Help
./Release/payroll_software_cpp.exe --help
```

### CLI Options

| Option | Short | Default | Description |
|---|---|---|---|
| `--config <path>` | `-c` | `./input/2024/config_harsha.yaml` | Employee YAML config |
| `--constants <path>` | `-k` | `./input/2024/constants.yaml` | Tax/SV constants for the year |
| `--out <dir>` | `-o` | `./out` | Output directory (created if absent) |
| `--monat <1..12>` | `-m` | `0` (all) | Process a single month; `0` = all 12 |

> When `--monat N` is used, the program still processes all preceding months
> internally so that the YTD figures on month N are correct.

---

## Getting Started

1. Copy the templates into a local `input/<year>/` directory (gitignored — your
   personal data never leaves your machine):

   ```bash
   mkdir -p input/2024
   cp template_input/2024/config.yaml    input/2024/config.yaml
   cp template_input/2024/constants.yaml input/2024/constants.yaml
   ```

2. Fill in `input/2024/config.yaml` with your personal details and per-month
   gross figures (see [Configuration Files](#configuration-files) below).

3. Verify the rates in `input/2024/constants.yaml` against the official sources
   for your tax year — the 2024 values are pre-filled.

4. Build and run:

   ```bash
   mkdir build && cd build
   cmake .. -DCMAKE_BUILD_TYPE=Release
   cmake --build . --config Release
   cd ..
   ./Release/payroll_software_cpp.exe
   ```

   Output lands in `out/`.

---

## Project Structure

```
Deutscher-Brutto-Netto-Rechner/
├── CMakeLists.txt              # Build definition; fetches yaml-cpp automatically
├── README.md                   # This file
├── template_input/             # Committed templates — no personal data
│   └── 2024/
│       ├── config.yaml         # Employee + monthly gross template (all zeros)
│       └── constants.yaml      # 2024 tax/SV constants with source references
├── input/                      # Gitignored — your personal YAML files go here
│   └── 2024/
│       ├── config.yaml
│       └── constants.yaml
├── build/                      # CMake build artefacts (not committed)
├── Release/                    # Compiled binary (Windows/MSVC output)
│   └── payroll_software_cpp.exe
├── out/                        # Generated payslips and summary (not committed)
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

YAML files live under `input/<year>/` (gitignored). Use the templates in
`template_input/<year>/` as your starting point. To run a different tax year,
create a new `input/<year>/` directory, copy the templates, and update the
values accordingly.

### `constants.yaml`

Holds all year-specific tax and SV parameters:

- `tarif` — §32a EStG zone boundaries and formula coefficients
- `soli` — Freigrenze and Milderungszone rates
- `kirchensteuer` — rates by Bundesland
- `sv.kv` — KV allgemeiner Beitragssatz, Zusatzbeitrag of your Krankenkasse, BBG month/year
- `sv.pv` — PV base rate, Kinderlosenzuschlag, BBG
- `sv.rv` — RV rate, BBG month/year
- `sv.av` — AV rate, BBG month/year

### `config.yaml`

Holds employee data and per-month gross figures:

- `mitarbeiter` — name, Geburtsjahr, Steuerklasse, Konfession, Bundesland, Krankenkasse, hat_kinder
- `arbeitgeber` — name, Ort, Bundesland
- `jahr` — payroll year
- `monate` — list of 12 entries, each with:
  - `brutto_laufend` — regular monthly wage
  - `sonstige_bezuege` — one-time payment (e.g. Weihnachtsgeld), taxed via Jahresdifferenzmethode
  - `geldwerter_vorteil` — 92DB benefit-in-kind §37b EStG (SV-EZ, AN-steuerfrei)
  - `sv_st_anteile` — 92CC taxable EZ netting amount (deducted before Netto)
  - `inflationsgeld` — tax- and SV-free inflation bonus §3 Nr. 11c EStG
  - `nachverrechnung` — already-taxed net back-payment (added directly to Netto)
  - `rkvp` — tax-free travel reimbursement 765L (documentary only, no tax/SV effect)
