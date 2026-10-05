#include <stdio.h>
#include "pico/stdlib.h"
#include "hardware/clocks.h"

static void row(const char *name, uint32_t set_khz, uint32_t measured_khz)
{
    printf("%-8s %9u %12u\n", name, (unsigned)set_khz, (unsigned)measured_khz);
}

void clk_info(void)
{
    // ─── шапка ─────────────────────────────────────────────
    printf("%-8s %9s %12s\n", "clock", "set, kHz", "measured, kHz");
    printf("----------------------------------\n");

    // ─── clk_ref ───────────────────────────────────────────
    row("clk_ref",
        clock_get_hz(clk_ref) / 1000,
        frequency_count_khz(CLOCKS_FC0_SRC_VALUE_CLK_REF));

    // ─── clk_sys ───────────────────────────────────────────
    row("clk_sys",
        clock_get_hz(clk_sys) / 1000,
        frequency_count_khz(CLOCKS_FC0_SRC_VALUE_CLK_SYS));

    // ─── clk_peri ──────────────────────────────────────────
    row("clk_peri",
        clock_get_hz(clk_peri) / 1000,
        frequency_count_khz(CLOCKS_FC0_SRC_VALUE_CLK_PERI));

    // ─── clk_usb ───────────────────────────────────────────
    row("clk_usb",
        clock_get_hz(clk_usb) / 1000,
        frequency_count_khz(CLOCKS_FC0_SRC_VALUE_CLK_USB));

    // ─── clk_adc ───────────────────────────────────────────
    row("clk_adc",
        clock_get_hz(clk_adc) / 1000,
        frequency_count_khz(CLOCKS_FC0_SRC_VALUE_CLK_ADC));

    // ─── rosc: настроенной частоты нет, вместо неё прочерк ─
    printf("%-8s %9s %12u\n",
           "rosc",
           "-",
           (unsigned)frequency_count_khz(CLOCKS_FC0_SRC_VALUE_ROSC_CLKSRC));
}