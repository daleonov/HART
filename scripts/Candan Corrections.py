# Generates Candan's 1st corrections for Jacobsen frequency estimator metric

from math import pi, tan, nan

cpp_code = ''
n = 1
n_max = 100 * 44100  # 10s at 44.1 kHz SR

for i in range(63):
    if n <= 2:
        # That estimator is for N > 2
        candan_correction = nan
    else:
        pi_over_n = pi / n
        candan_correction = tan(pi_over_n) / pi_over_n

    cpp_code += f'    {candan_correction:0.16f},  // N = {n} (2 ** {i})\n'
    n <<= 1

cpp_code = f'constexpr std::array<double, {i}> candan1Corrections =\n{{{{\n' + cpp_code + '}};\n'

print(cpp_code)
