from constants import *

def calculate_income_tax(monthly_salary: float, monthly_social_contributions: float) -> float:
    """
    Calculate monthly income tax (Einkommensteuer) for a Tax Class I person (older than 30).
    Applies official 2024 formulas from Bundesfinanzministerium.
    """
    annual_gross = monthly_salary * 12
    annual_social = monthly_social_contributions * 12
    taxable_income = max(0.0, annual_gross - annual_social)  # zvE

    if taxable_income <= BASIC_ALLOWANCE:
        est = 0.0
    elif taxable_income <= 17005:
        y = (taxable_income - BASIC_ALLOWANCE) / 10_000
        est = (954.80 * y + 1400) * y
    elif taxable_income <= 66760:
        z = (taxable_income - 17005) / 10_000
        est = (181.19 * z + 2397) * z + 991.21
    elif taxable_income <= 277825:
        est = 0.42 * taxable_income - 10636.31
    else:
        est = 0.45 * taxable_income - 18971.06

    return round(est / 12, 2)  # Monthly tax
