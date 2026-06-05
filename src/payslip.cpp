#include "payslip.hpp"
#include "tax.hpp"
#include <cmath>
#include <iomanip>
#include <sstream>
#include <string>
#include <vector>

namespace payslip {

using tax::round2;

static const int LINE_WIDTH = 100;
static const std::string HLINE(LINE_WIDTH, '=');
static const std::string SLINE(LINE_WIDTH, '-');

// ---------------------------------------------------------------------------
// Formatting helpers
// ---------------------------------------------------------------------------

// Format a monetary amount in German locale style: 1.234,56
// (period as thousands separator, comma as decimal separator).
static std::string fmt_eur(double value) {
    // Build US-format string first, then swap separators.
    std::ostringstream oss;
    oss << std::fixed << std::setprecision(2) << value;
    std::string s = oss.str();

    // Insert thousands separators (every 3 digits left of the decimal point)
    size_t dot = s.find('.');
    if (dot == std::string::npos) dot = s.size();

    // Work from the left of the integer part, skipping a leading minus
    size_t start = (s[0] == '-') ? 1 : 0;
    int count = 0;
    for (int i = static_cast<int>(dot) - 1; i > static_cast<int>(start); --i) {
        ++count;
        if (count % 3 == 0) s.insert(i, 1, ',');
    }

    // Replace '.' with ','  and all ',' in integer part with '.'
    // After the above, the string has ',' as thousands sep and '.' as decimal.
    // We want '.' as thousands sep and ',' as decimal.
    for (auto& ch : s) {
        if (ch == '.') { ch = ','; }
        else if (ch == ',') { ch = '.'; }
    }
    // Fix: the decimal separator was originally '.', now it is ',' — correct.
    // But we also turned the thousands commas into '.' — also correct.
    return s;
}

// Single-column right-aligned row: label left, value right
static std::string row(const std::string& label, double value) {
    std::string val_str = fmt_eur(value);
    int pad = LINE_WIDTH - static_cast<int>(label.size())
                         - static_cast<int>(val_str.size());
    if (pad < 1) pad = 1;
    return label + std::string(pad, ' ') + val_str;
}

// Two-column row for SV lines: label, AN amount, AG amount.
// AN is right-aligned to column 70; AG is right-aligned to column 100.
static std::string row_two(const std::string& label, double an, double ag) {
    const int col_an = 70;
    const int col_ag = LINE_WIDTH;

    std::string an_str = fmt_eur(an);
    std::string ag_str = fmt_eur(ag);

    std::string line(LINE_WIDTH, ' ');

    // Label (truncated to leave room for the AN column)
    int label_max = col_an - 20;
    for (int i = 0; i < label_max && i < static_cast<int>(label.size()); ++i)
        line[i] = label[i];

    // AN right-aligned at column 70
    int an_start = col_an - static_cast<int>(an_str.size());
    for (int i = 0; i < static_cast<int>(an_str.size()); ++i)
        line[an_start + i] = an_str[i];

    // AG right-aligned at column 100
    int ag_start = col_ag - static_cast<int>(ag_str.size());
    for (int i = 0; i < static_cast<int>(ag_str.size()); ++i)
        line[ag_start + i] = ag_str[i];

    return line;
}

static std::string center(const std::string& text) {
    if (static_cast<int>(text.size()) >= LINE_WIDTH) return text;
    int pad = (LINE_WIDTH - static_cast<int>(text.size())) / 2;
    return std::string(pad, ' ') + text;
}

// Format a rate like 0.146 as "14,6 %"
static std::string pct(double rate) {
    std::ostringstream oss;
    // Use up to 4 significant digits, strip trailing zeros
    double r = rate * 100.0;
    if (r == std::floor(r))
        oss << static_cast<int>(r);
    else {
        oss << std::fixed << std::setprecision(1) << r;
    }
    // Replace '.' with ','
    std::string s = oss.str();
    for (auto& ch : s) if (ch == '.') ch = ',';
    return s + " %";
}

// ---------------------------------------------------------------------------
// Section renderers
// ---------------------------------------------------------------------------

static std::vector<std::string> render_header(const MonatsErgebnis& m) {
    std::vector<std::string> lines;
    lines.push_back(HLINE);
    lines.push_back(center("GEHALTSABRECHNUNG " + m.monatsname + " "
                           + std::to_string(m.jahr)));
    lines.push_back(HLINE);
    lines.push_back("Arbeitnehmer:   " + m.vorname + " " + m.nachname);
    lines.push_back("Geburtsjahr:    " + std::to_string(m.geburtsjahr));
    lines.push_back("Steuerklasse:   " + std::to_string(m.steuerklasse)
                    + "   Konfession: " + m.konfession
                    + "   Bundesland: " + m.bundesland);
    lines.push_back("Krankenkasse:   " + m.krankenkasse + " (gesetzlich)");
    lines.push_back("Abrechnungs-Mo: "
                    + std::string(m.monat < 10 ? "0" : "") + std::to_string(m.monat)
                    + "/" + std::to_string(m.jahr));
    lines.push_back("");
    return lines;
}

static std::vector<std::string> render_bezuege(const MonatsErgebnis& m) {
    std::vector<std::string> lines;
    lines.push_back(SLINE);
    lines.push_back("BEZUEGE");
    lines.push_back(SLINE);
    lines.push_back(row("Laufender Bezug (Brutto)", m.brutto_laufend));
    if (m.sonstige_bezuege > 0)
        lines.push_back(row("Sonstige Bezuege (z. B. anteiliges Weihnachtsgeld)",
                            m.sonstige_bezuege));
    if (m.geldwerter_vorteil > 0)
        lines.push_back(row("Geldwerter Vorteil §37b EStG (92DB, AN-stfrei, SV-pflichtig)",
                            m.geldwerter_vorteil));
    if (m.rkvp > 0)
        lines.push_back(row("RKVP stfrei LSTB (765L, dokumentarisch)", m.rkvp));
    if (m.sv_st_anteile > 0)
        lines.push_back(row("SV/ST-Anteile Nettierung Sachgeschenk (92CC, stpfl. EZ)",
                            m.sv_st_anteile));
    if (m.inflationsgeld > 0)
        lines.push_back(row("Inflationsausgleichspraemie (steuerfrei §3 Nr. 11c EStG)",
                            m.inflationsgeld));
    if (m.nachverrechnung != 0)
        lines.push_back(row("Nachverrechnung / Aufrollungsdifferenz (netto, /552)",
                            m.nachverrechnung));
    lines.push_back(row("Gesamt-Brutto (steuerpflichtig)", m.brutto_gesamt));
    lines.push_back("");
    return lines;
}

static std::vector<std::string> render_steuern(const MonatsErgebnis& m) {
    std::vector<std::string> lines;
    lines.push_back(SLINE);
    lines.push_back("STEUERN (Arbeitnehmer-Anteil)");
    lines.push_back(SLINE);
    lines.push_back(row("Lohnsteuer auf laufenden Bezug", m.lst_laufend));
    lines.push_back(row("Lohnsteuer auf EZ (Einmalzahlung, Jahresdiff.)", m.lst_sonstige));
    lines.push_back(row("Lohnsteuer gesamt", m.lst_gesamt));
    lines.push_back(row("Solidaritaetszuschlag", m.soli));
    lines.push_back(row("Kirchensteuer", m.kist));
    lines.push_back(row("Summe Steuern (AN)", round2(m.lst_gesamt + m.soli + m.kist)));
    lines.push_back("");
    return lines;
}

static std::vector<std::string> render_sv(const MonatsErgebnis& m) {
    std::vector<std::string> lines;
    lines.push_back(SLINE);
    lines.push_back("SOZIALVERSICHERUNG  (Spalten: AN-Anteil          AG-Anteil)");
    lines.push_back(SLINE);

    std::string kv_label = "Krankenversicherung (KV, " + pct(m.rate_kv_allgemein)
                         + " + " + pct(m.rate_kv_zusatz) + " " + m.krankenkasse + ")";
    std::string pv_label = "Pflegeversicherung  (PV, " + pct(m.rate_pv_base)
                         + (m.rate_pv_kinderlos > 0
                            ? " + " + pct(m.rate_pv_kinderlos) + " Kinderl."
                            : "") + ")";
    std::string rv_label = "Rentenversicherung  (RV, " + pct(m.rate_rv) + ")";
    std::string av_label = "Arbeitslosenvers.   (AV, " + pct(m.rate_av) + ")";
    std::string ez_label = "  davon EZ (Einmalzahlung)";

    lines.push_back(row_two(kv_label, m.kv_an, m.kv_ag));
    lines.push_back(row_two(ez_label, m.kv_an_ez, m.kv_ag_ez));
    lines.push_back(row_two(pv_label, m.pv_an, m.pv_ag));
    lines.push_back(row_two(ez_label, m.pv_an_ez, m.pv_ag_ez));
    lines.push_back(row_two(rv_label, m.rv_an, m.rv_ag));
    lines.push_back(row_two(ez_label, m.rv_an_ez, m.rv_ag_ez));
    lines.push_back(row_two(av_label, m.av_an, m.av_ag));
    lines.push_back(row_two(ez_label, m.av_an_ez, m.av_ag_ez));
    lines.push_back(SLINE);
    lines.push_back(row_two("Summe Sozialversicherung", m.sv_an_gesamt, m.sv_ag_gesamt));
    lines.push_back("");
    return lines;
}

static std::vector<std::string> render_netto(const MonatsErgebnis& m) {
    std::vector<std::string> lines;
    lines.push_back(SLINE);
    lines.push_back("NETTO-BERECHNUNG");
    lines.push_back(SLINE);
    double summe_steuern = round2(m.lst_gesamt + m.soli + m.kist);
    lines.push_back(row("Brutto (steuerpflichtig)", m.brutto_gesamt));
    lines.push_back(row("- Steuern (AN)", -summe_steuern));
    lines.push_back(row("- Sozialvers. (AN)", -m.sv_an_gesamt));
    if (m.sv_st_anteile > 0)
        lines.push_back(row("- SV/ST-Anteile Nettierung (92CC, Ausgleich Sachgeschenk)",
                            -m.sv_st_anteile));
    if (m.inflationsgeld > 0)
        lines.push_back(row("+ Inflationsausgleichspraemie (steuerfrei)", m.inflationsgeld));
    if (m.nachverrechnung != 0)
        lines.push_back(row("+ Nachverrechnung / Aufrollungsdifferenz (netto)", m.nachverrechnung));
    lines.push_back(SLINE);
    lines.push_back(row("*** Auszahlungsbetrag (Netto) ***", m.netto));
    lines.push_back("");
    lines.push_back(row("Personalkosten Arbeitgeber gesamt (Brutto + SV-AG)",
                        m.gesamt_personalkosten_ag));
    lines.push_back("");
    return lines;
}

static std::vector<std::string> render_ytd(const MonatsErgebnis& m) {
    std::vector<std::string> lines;
    lines.push_back(SLINE);
    lines.push_back("KUMULIERT YEAR-TO-DATE (Jan - " + m.monatsname + ")");
    lines.push_back(SLINE);
    lines.push_back(row("YTD Brutto", m.ytd_brutto));
    lines.push_back(row("YTD Lohnsteuer", m.ytd_lst));
    lines.push_back(row("  davon EZ (Einmalzahlung)", m.ytd_lst_sonstige));
    lines.push_back(row("YTD Solidaritaetszuschlag", m.ytd_soli));
    lines.push_back(row("YTD Kirchensteuer", m.ytd_kist));
    lines.push_back(row("YTD Sozialversicherung (AN)", m.ytd_sv_an));
    lines.push_back(row("  davon KV (EZ) AN", m.ytd_kv_an_ez));
    lines.push_back(row("  davon PV (EZ) AN", m.ytd_pv_an_ez));
    lines.push_back(row("  davon RV (EZ) AN", m.ytd_rv_an_ez));
    lines.push_back(row("  davon AV (EZ) AN", m.ytd_av_an_ez));
    lines.push_back(row("YTD Sozialversicherung (AG)", m.ytd_sv_ag));
    lines.push_back(row("  davon KV (EZ) AG", m.ytd_kv_ag_ez));
    lines.push_back(row("  davon PV (EZ) AG", m.ytd_pv_ag_ez));
    lines.push_back(row("  davon RV (EZ) AG", m.ytd_rv_ag_ez));
    lines.push_back(row("  davon AV (EZ) AG", m.ytd_av_ag_ez));
    lines.push_back(row("YTD Inflationsausgleichspraemie (steuerfrei)", m.ytd_inflationsgeld));
    if (m.ytd_nachverrechnung != 0)
        lines.push_back(row("YTD Nachverrechnung (netto)", m.ytd_nachverrechnung));
    if (m.ytd_geldwerter_vorteil > 0)
        lines.push_back(row("YTD Geldwerter Vorteil §37b EStG (dokumentarisch)",
                            m.ytd_geldwerter_vorteil));
    if (m.ytd_rkvp > 0)
        lines.push_back(row("YTD RKVP stfrei LSTB (dokumentarisch)", m.ytd_rkvp));
    if (m.ytd_sv_st_anteile > 0)
        lines.push_back(row("YTD SV/ST-Anteile Nettierung Sachgeschenk (92CC, stpfl. EZ)",
                            m.ytd_sv_st_anteile));
    lines.push_back(row("YTD Netto", m.ytd_netto));
    lines.push_back("");
    return lines;
}

static std::vector<std::string> render_kennzahlen(const MonatsErgebnis& m) {
    std::vector<std::string> lines;
    lines.push_back(SLINE);
    lines.push_back("KENNZAHLEN (entsprechen Feldern der Lohnsteuerbescheinigung)");
    lines.push_back(SLINE);
    lines.push_back(row("Bruttoarbeitslohn (YTD, Feld 3)", m.ytd_brutto));
    lines.push_back(row("Einbehaltene Lohnsteuer (YTD, Feld 4)", m.ytd_lst));
    lines.push_back(row("Einbehaltener Solidaritaetszuschlag (YTD, Feld 5)", m.ytd_soli));
    lines.push_back(row("Einbehaltene Kirchensteuer AN (YTD, Feld 6)", m.ytd_kist));
    if (m.ytd_inflationsgeld > 0)
        lines.push_back(row("Steuerfreie AG-Leistungen (YTD, Feld 17)", m.ytd_inflationsgeld));
    lines.push_back("");
    return lines;
}

static std::vector<std::string> render_detail(const MonatsErgebnis& m) {
    std::vector<std::string> lines;
    lines.push_back("");
    lines.push_back(HLINE);
    lines.push_back(center("STEUERBERECHNUNGS-DETAIL  (Erlaeuterung §39b EStG)"));
    lines.push_back(HLINE);
    lines.push_back("");
    lines.push_back("Hochrechnung des Jahresarbeitslohns auf Basis des aktuellen Monatsbruttos:");
    lines.push_back(row("  Projiziertes Jahresbrutto (12 x laufend)", m.projected_annual_gross));
    lines.push_back(row("  - Arbeitnehmer-Pauschbetrag", -tax::ARBEITNEHMER_PAUSCHBETRAG));
    lines.push_back(row("  - Sonderausgaben-Pauschbetrag", -tax::SONDERAUSGABEN_PAUSCHBETRAG));
    lines.push_back(row("  - Vorsorgepauschale (RV+AV+KV+PV AN-Jahr)",
                        -m.annual_vorsorgepauschale));
    lines.push_back(SLINE);
    lines.push_back(row("  = zvE (zu versteuerndes Einkommen, Jahr, laufend)",
                        m.zve_laufend_annual));
    lines.push_back(row("  Jahres-Lohnsteuer (laufend)  §32a EStG", m.annual_lst_laufend));
    lines.push_back(row("  Monats-Lohnsteuer (laufend) = Jahres-LSt / 12", m.lst_laufend));
    lines.push_back("");

    if (m.sonstige_bezuege > 0) {
        lines.push_back("Jahresdifferenzmethode fuer sonstige Bezuege (§39b Abs. 3 EStG):");
        lines.push_back(row("  Jahresbrutto laufend + sonstige Bezuege",
                            m.projected_annual_gross + m.sonstige_bezuege));
        lines.push_back(row("  - Vorsorgepauschale laufend", -m.annual_vorsorgepauschale));
        lines.push_back(row("  - Zusaetzl. Vorsorgepauschale auf sonstige Bezuege (RV+AV)",
                            -m.extra_vorsorge_sonstige));
        lines.push_back(row("  = zvE inkl. sonstige Bezuege", m.zve_incl_sonstige_annual));
        lines.push_back(row("  Jahres-LSt inkl. sonstige Bezuege  §32a EStG",
                            m.annual_lst_incl_sonstige));
        lines.push_back(row("  - Jahres-LSt laufend", -m.annual_lst_laufend));
        lines.push_back(row("  = LSt auf sonstige Bezuege (Differenz)", m.lst_sonstige));
        lines.push_back("");
    }
    return lines;
}

static std::vector<std::string> render_legend(const MonatsErgebnis& m) {
    std::vector<std::string> lines;
    lines.push_back(HLINE);
    lines.push_back("LEGENDE / ABKUERZUNGEN");
    lines.push_back(HLINE);

    auto add = [&](const std::string& abbr, const std::string& desc) {
        std::string s = "  ";
        s += abbr;
        s += std::string(6 - abbr.size(), ' ');
        s += " = ";
        s += desc;
        lines.push_back(s);
    };
    add("AN",    "Arbeitnehmer-Anteil");
    add("AG",    "Arbeitgeber-Anteil");
    add("LSt",   "Lohnsteuer");
    add("Soli",  "Solidaritaetszuschlag");
    add("KiSt",  "Kirchensteuer");
    add("KV",    "Krankenversicherung (gesetzlich)");
    add("PV",    "Pflegeversicherung");
    add("RV",    "Rentenversicherung");
    add("AV",    "Arbeitslosenversicherung");
    add("BBG",   "Beitragsbemessungsgrenze");
    add("zvE",   "zu versteuerndes Einkommen");
    add("YTD",   "Year-to-Date (kumuliert seit Jahresbeginn)");
    add("§32a",  "Einkommensteuertarif EStG");
    add("§39b",  "Lohnsteuerabzug EStG");
    add("GWV",   "Geldwerter Vorteil (92DB, AG zahlt Pausch.Steuer §37b EStG, AN-stfrei, SV-pflichtig)");
    add("RKVP",  "Reisekostenverguetungspauschale (765L stfrei LSTB, dokumentarisch)");
    add("92CC",  "SV/ST-Anteile Nettierung Sachgeschenk (stpfl. EZ, Abzug vor Netto)");
    add("EZ",    "Einmalzahlung");

    lines.push_back("");
    lines.push_back("BEITRAGSBEMESSUNGSGRENZEN (BBG) dieses Jahres:");
    lines.push_back("  KV / PV  = " + fmt_eur(m.bbg_kvpv_monat)
                    + " EUR / Monat  (Beitraege werden auf diesen Brutto-Betrag begrenzt)");
    lines.push_back("  RV / AV  = " + fmt_eur(m.bbg_rvav_monat)
                    + " EUR / Monat  (Beitraege werden auf diesen Brutto-Betrag begrenzt)");
    lines.push_back("");
    lines.push_back(HLINE);
    lines.push_back("Hinweis: Diese Berechnung dient nur zu Informationszwecken. "
                    "Verbindliche Aus-");
    lines.push_back("kuenfte erteilt das Finanzamt oder ein zugelassener Steuerberater.");
    lines.push_back(HLINE);
    return lines;
}

// ---------------------------------------------------------------------------
// Public entry point
// ---------------------------------------------------------------------------

std::string render_payslip(const MonatsErgebnis& m) {
    std::vector<std::string> all;
    auto append = [&](std::vector<std::string> v) {
        for (auto& l : v) all.push_back(std::move(l));
    };
    append(render_header(m));
    append(render_bezuege(m));
    append(render_steuern(m));
    append(render_sv(m));
    append(render_netto(m));
    append(render_ytd(m));
    append(render_kennzahlen(m));
    append(render_detail(m));
    append(render_legend(m));

    std::string out;
    for (const auto& l : all) { out += l; out += '\n'; }
    return out;
}

} // namespace payslip
