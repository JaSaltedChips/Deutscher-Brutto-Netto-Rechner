# Annual income thresholds (2024)
PENSION_INSURANCE_CAP = 7550.00        # Max monthly income for pension contributions (West Germany)
HEALTH_INSURANCE_CAP = 5175.00         # Max monthly income for health insurance contributions
BASIC_ALLOWANCE = 11784.00             # Grundfreibetrag (basic tax-free allowance)

# Social security contribution rates (2024) for 30+ year employee, in percentage
CONTRIBUTION_RATES = {
    "krankenversicherung": {"total": 14.6, "employer": 7.3, "employee": 7.3},
    "krankenversicherung_zusatz": {"total": 1.2, "employer": 0.6, "employee": 0.6},
    "rentenversicherung": {"total": 18.6, "employer": 9.3, "employee": 9.3},
    "arbeitslosversicherung": {"total": 2.6, "employer": 1.3, "employee": 1.3},
    "pflegeversicherung": {"total": 4.0, "employer": 1.7, "employee": 2.3},
}