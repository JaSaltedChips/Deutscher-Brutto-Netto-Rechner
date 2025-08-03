from helper import output
from helper import export_markdown_payslip_textblock
from social_contribution_calculator import calculate_social_contribution
from income_tax_calculator import calculate_income_tax

def main():
    try:
        raw_input = input("Enter your monthly gross salary in EUR: ").replace(',', '.')
        monthly_salary = float(raw_input)

        # Calculate social contributions
        contributions = calculate_social_contribution(monthly_salary)
        monthly_social_total = sum(c.employee for c in contributions.values())

        # Calculate income tax
        monthly_tax = calculate_income_tax(monthly_salary, monthly_social_total)

        # Output summary
        output(monthly_salary, monthly_tax, contributions)
     
    except ValueError:
        print("Invalid input. Please enter a valid numeric salary.")

if __name__ == "__main__":
    main()