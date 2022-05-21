#include <uart.h>
#include <am.h>

void virt_uart_init(void)
{
    //http://byterunner.com/16550.html
    uart_write_reg(IER, 0x00);

    uint8_t lcr = uart_read_reg(LCR);
    uart_write_reg(LCR, lcr | (1 << 7));
    uart_write_reg(DLL, 0x01);
    uart_write_reg(DLM, 0x00);

    lcr = 0;
    uart_write_reg(LCR, lcr | (3 << 0));

    /*
     * enable receive interrupts.
     */
    uint8_t ier = uart_read_reg(IER);
    uart_write_reg(IER, ier | (1 << 0));
}

int drv_uart_putc(char c)
{
    while ((uart_read_reg(LSR) & LSR_TX_IDLE) == 0);
    return uart_write_reg(THR, c);
}