#pragma once
// =============================================================================
// annual_summary.hpp
// ------------------
// Data container and text renderer for the annual payroll summary
// (Jahreszusammenfassung) including an estimated income-tax
// refund/surcharge calculation (Einkommensteuer-Schaetzung).
// =============================================================================

#include <string>
#include <yaml-cpp/yaml.h>

namespace annual_summary {

struct JahresSumme {
    int         jahr;
    std::string vorname;
    std::string nachname;
    int         geburtsjahr;
    int         steuerklasse;

    // Gross pay components (annual totals)
    double brutto_jahr;               // regular wages
    double sonstige_jahr;             // one-time payments (Weihnachtsgeld)
    double geldwerter_vorteil_jahr;   // 92DB benefit-in-kind (documentary)
    double rkvp_jahr;                 // tax-free travel reimbursement (documentary)
    double sv_st_anteile_jahr;        // 92CC taxable EZ netting amounts
    double brutto_gesamt;             // total taxable gross (Feld 3)
    double inflationsgeld_jahr;       // tax-free inflation bonus (Feld 17)
    double nachverrechnung_jahr;      // net back-payments

    // Taxes withheld (annual)
    double lst_jahr;
    double soli_jahr;
    double kist_jahr;

    // Social insurance (annual totals, both AN and AG shares)
    double kv_an_jahr; double kv_ag_jahr;
    double pv_an_jahr; double pv_ag_jahr;
    double rv_an_jahr; double rv_ag_jahr;
    double av_an_jahr; double av_ag_jahr;
    double sv_an_jahr; double sv_ag_jahr;

    // EZ (Einmalzahlung) sub-totals for each SV branch
    double kv_an_ez_jahr; double kv_ag_ez_jahr;
    double pv_an_ez_jahr; double pv_ag_ez_jahr;
    double rv_an_ez_jahr; double rv_ag_ez_jahr;
    double av_an_ez_jahr; double av_ag_ez_jahr;
    double lst_ez_jahr;

    // Net pay
    double netto_jahr;
    double personalkosten_ag_jahr;   // total employer cost
};

// Renders the annual summary as a UTF-8 German text string.
std::string render_summary(const JahresSumme& j, const YAML::Node& constants);

} // namespace annual_summary
