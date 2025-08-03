from constants import *

# Source: https://www.bmf-steuerrechner.de/javax.faces.resource/2025_1_14_Tarifhistorie_Steuerrechner.pdf.xhtml
def calculate_income_tax(monthly_salary: float, monthly_social_contributions: float) -> float:
    """
    Calculate monthly income tax (Einkommensteuer) for a Tax Class I person (older than 30).
    Applies official 2024 formulas from Bundesfinanzministerium.
    """
    annual_gross = monthly_salary * 12
    annual_social = monthly_social_contributions * 12
    zvE = max(0.0, annual_gross - annual_social)  # taxable amount

    if zvE <= BASIC_ALLOWANCE:
        est = 0.0
    elif zvE <= 17005:
        y = (zvE - BASIC_ALLOWANCE) / 10_000
        est = (954.80 * y + 1400) * y
    elif zvE <= 66760:
        z = (zvE - 17005) / 10_000
        est = (181.19 * z + 2397) * z + 991.21
    elif zvE <= 277825:
        est = 0.42 * zvE - 10636.31
    else:
        est = 0.45 * zvE - 18971.06

    return round(est / 12, 2)  # Monthly tax
