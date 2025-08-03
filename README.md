# Tax Calculator for Tax Class 1

This program calculates net monthly income for individuals over 30 year old, working in West Germany falling under **Tax Class 1**. It includes only **social security contributions** and **income tax** in its calculations — **church tax** and **solidarity surcharge** are **not** considered.

The calculator is designed specifically for the **2024 tax year**, using the official contribution caps and percentages in effect for that year. These values are defined in the `constants.py` file. Using this program for any other tax year may lead to inaccurate results.

### References:
1. https://www.tk.de/firmenkunden/service/fachthemen/fachthema-beitraege/sv-rechengroessen-2024-2158154 – Source for contribution caps (2024)
2. https://www.lohn-info.de/sozialversicherungsbeitraege2024.html - Source for percentage rates (2024)
3. https://www.bmf-steuerrechner.de/javax.faces.resource/2025_1_14_Tarifhistorie_Steuerrechner.pdf.xhtml – Source for the official income tax calculation formula (2024)
