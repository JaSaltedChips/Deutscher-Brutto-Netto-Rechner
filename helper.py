from typing import Dict
from social_contribution_calculator import Contribution

def output(monthly_salary, monthly_tax, contributions):
        annual_gross = monthly_salary * 12
        annual_tax = monthly_tax * 12

        print("\n")
        print("=" * 58)
        print(f"{'Ergebnis':<30}{'Monat':>12} {'Jahr':>12}")
        print("-" * 58)

        # Brutto
        print(f"{'Brutto:':<30}{monthly_salary:>12,.2f} €{annual_gross:>12,.2f} €")
        print(f"{'Geldwerter Vorteil:':<30}{0:>12,.2f} €{0:>12,.2f} €")
        print("")

        # Steuern
        print(f"{'Steuern':<30}")
        print("-" * 58)
        print(f"{'Solidaritätszuschlag:':<30}{0:>12,.2f} €{0:>12,.2f} €")
        print(f"{'Kirchensteuer:':<30}{0:>12,.2f} €{0:>12,.2f} €")
        print(f"{'Lohnsteuer:':<30}{monthly_tax:>12,.2f} €{annual_tax:>12,.2f} €")
        print("-" * 58)
        print(f"{'Steuern:':<30}{monthly_tax:>12,.2f} €{annual_tax:>12,.2f} €")
        print("-" * 58)
        print("")

        # Sozialabgaben
        print(f"{'Sozialabgaben':<30}")
        print("-" * 58)
        renten = contributions['rentenversicherung'].employee
        arbeitslos = contributions['arbeitslosversicherung'].employee
        kranken = contributions['krankenversicherung'].employee + contributions['krankenversicherung_zusatz'].employee
        pflege = contributions['pflegeversicherung'].employee

        print(f"{'Rentenversicherung:':<30}{renten:>12,.2f} €{renten * 12:>12,.2f} €")
        print(f"{'Arbeitslosenversicherung:':<30}{arbeitslos:>12,.2f} €{arbeitslos * 12:>12,.2f} €")
        print(f"{'Krankenversicherung:':<30}{kranken:>12,.2f} €{kranken * 12:>12,.2f} €")
        print(f"{'Pflegeversicherung:':<30}{pflege:>12,.2f} €{pflege * 12:>12,.2f} €")

        monthly_social_total = renten + arbeitslos + kranken + pflege
        annual_social_total = monthly_social_total * 12
        print("-" * 58)
        print(f"{'Sozialabgaben:':<30}{monthly_social_total:>12,.2f} €{annual_social_total:>12,.2f} €")
        print("-" * 58)
        print("")

        # Netto
        net_monthly = monthly_salary - monthly_tax - monthly_social_total
        net_annual = annual_gross - annual_tax - annual_social_total
        print(f"{'Netto:':<30}{net_monthly:>12,.2f} €{net_annual:>12,.2f} €")
        print("=" * 58)

def export_markdown_payslip_textblock(
    monthly_salary: float,
    monthly_tax: float,
    contributions: Dict[str, Contribution],
    output_path: str = "payslip.md"
):
    renten = contributions['rentenversicherung'].employee
    arbeitslos = contributions['arbeitslosversicherung'].employee
    kranken = contributions['krankenversicherung'].employee + contributions['krankenversicherung_zusatz'].employee
    pflege = contributions['pflegeversicherung'].employee

    monthly_social_total = renten + arbeitslos + kranken + pflege
    annual_social_total = monthly_social_total * 12
    annual_tax = monthly_tax * 12
    annual_gross = monthly_salary * 12

    net_monthly = monthly_salary - monthly_tax - monthly_social_total
    net_annual = annual_gross - annual_tax - annual_social_total

    def line(label: str, monthly: float, yearly: float) -> str:
        return f"{label:<35}{monthly:>10,.2f} €{yearly:>13,.2f} €"

    def sep(char="-", width=60) -> str:
        return char * width

    # Build block
    lines = [
        sep("="),
        f"{'Ergebnis':<35}{'Monat':>10} {'Jahr':>13}",
        sep("-"),
        line("Brutto:", monthly_salary, annual_gross),
        line("Geldwerter Vorteil:", 0, 0),
        "",
        "Steuern",
        sep("-"),
        line("Solidaritätszuschlag:", 0, 0),
        line("Kirchensteuer:", 0, 0),
        line("Lohnsteuer:", monthly_tax, annual_tax),
        sep("-"),
        line("Steuern:", monthly_tax, annual_tax),
        sep("-"),
        "",
        "Sozialabgaben",
        sep("-"),
        line("Rentenversicherung:", renten, renten * 12),
        line("Arbeitslosenversicherung:", arbeitslos, arbeitslos * 12),
        line("Krankenversicherung:", kranken, kranken * 12),
        line("Pflegeversicherung:", pflege, pflege * 12),
        sep("-"),
        line("Sozialabgaben:", monthly_social_total, annual_social_total),
        sep("-"),
        "",
        line("Netto:", net_monthly, net_annual),
        sep("="),
    ]

    with open(output_path, 'w', encoding='utf-8') as f:
        f.write("```\n")  # Markdown code block
        f.write("\n".join(lines))
        f.write("\n```")

    print(f"\nFixed-layout payslip exported to `{output_path}`")
