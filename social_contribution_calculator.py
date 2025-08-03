from dataclasses import dataclass
from typing import Dict
from constants import *

@dataclass
class Contribution:
    base_amount: float
    total: float
    employer: float
    employee: float

def calculate_contribution(name: str, base: float) -> Contribution:
    """
    Calculate the social contribution amounts for a given insurance type and base amount.
    """
    rate = CONTRIBUTION_RATES[name]
    total = base * rate["total"] / 100
    employer = base * rate["employer"] / 100
    employee = base * rate["employee"] / 100
    return Contribution(base, total, employer, employee)

def calculate_social_contribution(monthly_salary: float) -> Dict[str, Contribution]:
    """
    Calculate all statutory social security contributions for a given gross monthly salary.
    Caps are applied where necessary (e.g., health and pension insurance).
    """
    contributions = {}

    # Cap for health and nursing care insurance
    capped_health_base = min(monthly_salary, HEALTH_INSURANCE_CAP)

    # Health insurance
    contributions["krankenversicherung"] = calculate_contribution("krankenversicherung", capped_health_base)

    # Additional health insurance
    contributions["krankenversicherung_zusatz"] = calculate_contribution("krankenversicherung_zusatz", capped_health_base)

    # Nursing care insurance (Pflegeversicherung) – also capped at health threshold
    contributions["pflegeversicherung"] = calculate_contribution("pflegeversicherung", capped_health_base)

    # Unemployment insurance
    contributions["arbeitslosversicherung"] = calculate_contribution("arbeitslosversicherung", monthly_salary)

    # Pension insurance – capped at pension threshold
    capped_pension_base = min(monthly_salary, PENSION_INSURANCE_CAP)
    contributions["rentenversicherung"] = calculate_contribution("rentenversicherung", capped_pension_base)

    return contributions
