#include "annual_summary.hpp"
#include "tax.hpp"
#include <algorithm>
#include <iomanip>
#include <sstream>
#include <string>
#include <vector>

namespace annual_summary {

using tax::round2;
using tax::einkommensteuer;
using tax::solidaritaetszuschlag;
using tax::ARBEITNEHMER_PAUSCHBETRAG;
using tax::SONDERAUSGABEN_PAUSCHBETRAG;

static const int LINE_WIDTH = 100;
static const std::string HLINE(LINE_WIDTH, '=');
static const std::string SLINE(LINE_WIDTH, '-');

// ---------------------------------------------------------------------------
// Formatting helpers (same conventions as payslip.cpp)
// ---------------------------------------------------------------------------

static std::string fmt_eur(double value) {
    std::ostringstream oss;
    oss << std::fixed << std::setprecision(2) << value;
    std::string s = oss.str();
    size_t dot = s.find('.');
    if (dot == std::string::npos) dot = s.size();
    size_t start = (s[0] == '-') ? 1 : 0;
    int count = 0;
    for (int i = static_cast<int>(dot) - 1; i > static_cast<int>(start); --i) {
        ++count;
        if (count % 3 == 0) s.insert(i, 1, ',');
    }
    for (auto& ch : s) {
        if (ch == '.') ch = ',';
        else if (ch == ',') ch = '.';
    }
    return s;
}

static std::string row(const std::string& label, double value) {
    std::string val_str = fmt_eur(value);
    int pad = LINE_WIDTH - static_cast<int>(label.size())
                         - static_cast<int>(val_str.size());
    if (pad < 1) pad = 1;
    return label + std::string(pad, ' ') + val_str;
}

static std::string center(const std::string& text) {
    if (static_cast<int>(text.size()) >= LINE_WIDTH) return text;
    int pad = (LINE_WIDTH - static_cast<int>(text.size())) / 2;
    return std::string(pad, ' ') + text;
}

// ---------------------------------------------------------------------------
// Main render function
// ---------------------------------------------------------------------------

std::string render_summary(const JahresSumme& j, const YAML::Node& constants) {
    std::vector<std::string> lines;

    lines.push_back(HLINE);
    lines.push_back(center("JAHRESZUSAMMENFASSUNG " + std::to_string(j.jahr)
                           + "  -  " + j.vorname + " " + j.nachname));
    lines.push_back(HLINE);
    lines.push_back("Geburtsjahr: " + std::to_string(j.geburtsjahr)
                    + "   Steuerklasse: " + std::to_string(j.steuerklasse));
    lines.push_back("");

    // ---- Gross pay section ----
    lines.push_back(SLINE);
    lines.push_back("BEZUEGE (Jahr)");
    lines.push_back(SLINE);
    lines.push_back(row("Laufender Brutto-Arbeitslohn (Jahr)", j.brutto_jahr));
    lines.push_back(row("Sonstige Bezuege / Weihnachtsgeld (Jahr)", j.sonstige_jahr));
    if (j.geldwerter_vorteil_jahr > 0)
        lines.push_back(row("Geldwerter Vorteil §37b EStG (Jahr, AN-stfrei, AG-Pausch.Steuer)",
                            j.geldwerter_vorteil_jahr));
    if (j.rkvp_jahr > 0)
        lines.push_back(row("RKVP stfrei LSTB (Jahr, dokumentarisch)", j.rkvp_jahr));
    if (j.sv_st_anteile_jahr > 0)
        lines.push_back(row("SV/ST-Anteile Nettierung Sachgeschenk 92CC (Jahr, stpfl. EZ)",
                            j.sv_st_anteile_jahr));
    lines.push_back(row("Gesamt-Bruttoarbeitslohn steuerpflichtig (Feld 3)", j.brutto_gesamt));
    if (j.inflationsgeld_jahr > 0)
        lines.push_back(row("Inflationsausgleichspraemie steuerfrei (Feld 17)",
                            j.inflationsgeld_jahr));
    if (j.nachverrechnung_jahr != 0)
        lines.push_back(row("Nachverrechnung / Aufrollungsdifferenzen gesamt (netto)",
                            j.nachverrechnung_jahr));
    lines.push_back("");

    // ---- Taxes withheld ----
    lines.push_back(SLINE);
    lines.push_back("EINBEHALTENE STEUERN (Jahr, AN)");
    lines.push_back(SLINE);
    lines.push_back(row("Lohnsteuer (Feld 4)", j.lst_jahr));
    lines.push_back(row("  davon EZ (Einmalzahlung)", j.lst_ez_jahr));
    lines.push_back(row("Solidaritaetszuschlag (Feld 5)", j.soli_jahr));
    lines.push_back(row("Kirchensteuer (Feld 6)", j.kist_jahr));
    double summe_steuern = round2(j.lst_jahr + j.soli_jahr + j.kist_jahr);
    lines.push_back(row("Summe einbehaltene Steuern", summe_steuern));
    lines.push_back("");

    // ---- Social insurance ----
    lines.push_back(SLINE);
    lines.push_back("SOZIALVERSICHERUNG (Jahr)");
    lines.push_back(SLINE);
    lines.push_back(row("KV  AN", j.kv_an_jahr));
    lines.push_back(row("  davon EZ", j.kv_an_ez_jahr));
    lines.push_back(row("KV  AG", j.kv_ag_jahr));
    lines.push_back(row("  davon EZ", j.kv_ag_ez_jahr));
    lines.push_back(row("PV  AN", j.pv_an_jahr));
    lines.push_back(row("  davon EZ", j.pv_an_ez_jahr));
    lines.push_back(row("PV  AG", j.pv_ag_jahr));
    lines.push_back(row("  davon EZ", j.pv_ag_ez_jahr));
    lines.push_back(row("RV  AN", j.rv_an_jahr));
    lines.push_back(row("  davon EZ", j.rv_an_ez_jahr));
    lines.push_back(row("RV  AG", j.rv_ag_jahr));
    lines.push_back(row("  davon EZ", j.rv_ag_ez_jahr));
    lines.push_back(row("AV  AN", j.av_an_jahr));
    lines.push_back(row("  davon EZ", j.av_an_ez_jahr));
    lines.push_back(row("AV  AG", j.av_ag_jahr));
    lines.push_back(row("  davon EZ", j.av_ag_ez_jahr));
    lines.push_back(row("Summe SV AN", j.sv_an_jahr));
    lines.push_back(row("Summe SV AG", j.sv_ag_jahr));
    lines.push_back("");

    // ---- Net pay ----
    lines.push_back(SLINE);
    lines.push_back("NETTO / PERSONALKOSTEN AG (Jahr)");
    lines.push_back(SLINE);
    lines.push_back(row("Netto-Jahreseinkommen (Auszahlung)", j.netto_jahr));
    if (j.nachverrechnung_jahr != 0)
        lines.push_back(row("  davon Nachverrechnung / Aufrollungsdifferenzen (netto)",
                            j.nachverrechnung_jahr));
    lines.push_back(row("Personalkosten Arbeitgeber (Brutto + SV-AG)", j.personalkosten_ag_jahr));
    lines.push_back("");

    // ---- Income-tax estimate (Veranlagungssicht) ----
    // This section gives a rough estimate of whether the employee can expect
    // a tax refund or surcharge when they file their annual return (Steuererklärung).
    //
    // The payroll tax (Lohnsteuer) withheld monthly is computed using a
    // projected annual income based on the current month's salary.  The actual
    // annual tax assessment (Veranlagung) uses the real annual gross and allows
    // the employee to claim additional deductions (Werbungskosten, Sonderausgaben,
    // haushaltsnahe Dienstleistungen, Homeoffice-Pauschale, etc.).  The estimate
    // here only uses the statutory flat-rate deductions and actual SV contributions,
    // so the actual refund will usually be at least as large.
    lines.push_back(HLINE);
    lines.push_back(center("EINKOMMENSTEUER-SCHAETZUNG (Veranlagungssicht)"));
    lines.push_back(HLINE);

    // Use actual annual SV contributions (not the projected Vorsorgepauschale)
    // as the deductible Vorsorgeaufwendungen in the Veranlagung estimate.
    double vorsorge = j.kv_an_jahr + j.pv_an_jahr + j.rv_an_jahr + j.av_an_jahr;
    double zve = std::max(0.0, j.brutto_gesamt
                               - ARBEITNEHMER_PAUSCHBETRAG
                               - SONDERAUSGABEN_PAUSCHBETRAG
                               - vorsorge);
    double est  = einkommensteuer(zve, constants);
    double soli = solidaritaetszuschlag(est, constants, j.steuerklasse);
    double summe_veranlagung = round2(est + soli + j.kist_jahr);

    lines.push_back(row("Bruttoarbeitslohn", j.brutto_gesamt));
    lines.push_back(row("- Arbeitnehmer-Pauschbetrag", -ARBEITNEHMER_PAUSCHBETRAG));
    lines.push_back(row("- Sonderausgaben-Pauschbetrag", -SONDERAUSGABEN_PAUSCHBETRAG));
    lines.push_back(row("- Vorsorgepauschale (tatsaechlich AN-SV)", -vorsorge));
    lines.push_back(SLINE);
    lines.push_back(row("= zu versteuerndes Einkommen (zvE, Schaetzung)", zve));
    lines.push_back(row("Einkommensteuer (§32a EStG)", est));
    lines.push_back(row("Solidaritaetszuschlag", soli));
    lines.push_back(row("Kirchensteuer", j.kist_jahr));
    lines.push_back(row("Summe Jahres-Steuerlast (Veranlagung)", summe_veranlagung));
    lines.push_back("");

    double diff = round2(summe_steuern - summe_veranlagung);
    if (diff > 0)
        lines.push_back(row(">> Voraussichtliche Steuererstattung", diff));
    else if (diff < 0)
        lines.push_back(row(">> Voraussichtliche Nachzahlung", -diff));
    else
        lines.push_back("   Einbehaltene Steuer = Veranlagungssteuer "
                        "(weder Erstattung noch Nachzahlung).");

    lines.push_back("");
    lines.push_back("Hinweis: Diese Schaetzung beruecksichtigt nur Pauschbetraege "
                    "und tatsaechliche");
    lines.push_back("AN-SV-Beitraege. Werbungskosten ueber 1.230 EUR, "
                    "Sonderausgaben, Spenden, ");
    lines.push_back("haushaltsnahe Dienstleistungen (§35a EStG), Homeoffice-"
                    "Pauschale etc. koennen");
    lines.push_back("die Erstattung weiter erhoehen. Verbindlich nur durch Bescheid"
                    " des Finanzamts.");

    if (j.inflationsgeld_jahr > 0) {
        lines.push_back("");
        lines.push_back("Inflationsausgleichspraemie " + fmt_eur(j.inflationsgeld_jahr)
                        + " EUR ist steuer- und SV-frei (§3 Nr. 11c EStG) und");
        lines.push_back("fliesst nicht in das zu versteuernde Einkommen ein "
                        "(erscheint in Feld 17 der Lohnsteuerbesch.).");
    }

    lines.push_back(HLINE);

    std::string out;
    for (const auto& l : lines) { out += l; out += '\n'; }
    return out;
}

} // namespace annual_summary
