#include "driver_ssd1306.h"
#include "driver_ssd1306_font.h"

#define SSD1306_CMD 0
#define SSD1306_DATA 1

#define SSD1306_CMD_LOWER_COLUMN_START_ADDRESS              0x00        /** command lower column start address */
#define SSD1306_CMD_HIGHER_COLUMN_START_ADDRESS             0x10        /** command higher column start address */
#define SSD1306_CMD_MEMORY_ADDRESSING_MODE                  0x20        /** command memory addressing mode */
#define SSD1306_CMD_SET_COLUMN_ADDRESS                      0x21        /** command set column address */
#define SSD1306_CMD_SET_PAGE_ADDRESS                        0x22        /** command set page address */
#define SSD1306_CMD_SET_FADE_OUT_AND_BLINKING               0x23        /** command set fade out and blinking */
#define SSD1306_CMD_RIGHT_HORIZONTAL_SCROLL                 0x26        /** command right horizontal scroll */
#define SSD1306_CMD_LEFT_HORIZONTAL_SCROLL                  0x27        /** command left horizontal scroll */
#define SSD1306_CMD_VERTICAL_RIGHT_HORIZONTAL_SCROLL        0x29        /** command vertical right horizontal scroll */
#define SSD1306_CMD_VERTICAL_LEFT_HORIZONTAL_SCROLL         0x2A        /** command vertical left horizontal scroll */
#define SSD1306_CMD_DEACTIVATE_SCROLL                       0x2E        /** command deactivate scroll */
#define SSD1306_CMD_ACTIVATE_SCROLL                         0x2F        /** command activate scroll */
#define SSD1306_CMD_DISPLAY_START_LINE                      0x40        /** command display start line */
#define SSD1306_CMD_CONTRAST_CONTROL                        0x81        /** command contrast control */
#define SSD1306_CMD_CHARGE_PUMP_SETTING                     0x8D        /** command charge pump setting */
#define SSD1306_CMD_COLUMN_0_MAPPED_TO_SEG0                 0xA0        /** command column 0 mapped to seg 0 */
#define SSD1306_CMD_COLUMN_127_MAPPED_TO_SEG0               0xA1        /** command column 127 mapped to seg 0 */
#define SSD1306_CMD_VERTICAL_SCROLL_AREA                    0xA3        /** command vertical scroll area */
#define SSD1306_CMD_ENTIRE_DISPLAY_OFF                      0xA4        /** command entire display off */
#define SSD1306_CMD_ENTIRE_DISPLAY_ON                       0xA5        /** command entire display on */
#define SSD1306_CMD_NORMAL_DISPLAY                          0xA6        /** command normal display */
#define SSD1306_CMD_INVERSE_DISPLAY                         0xA7        /** command inverse display */
#define SSD1306_CMD_MULTIPLEX_RATIO                         0xA8        /** command multiplex ratio */
#define SSD1306_CMD_DISPLAY_OFF                             0xAE        /** command display off */
#define SSD1306_CMD_DISPLAY_ON                              0xAF        /** command display on */
#define SSD1306_CMD_PAGE_ADDR                               0xB0        /** command page address */
#define SSD1306_CMD_SCAN_DIRECTION_COM0_START               0xC0        /** command scan direction com 0 start */
#define SSD1306_CMD_SCAN_DIRECTION_COMN_1_START             0xC8        /** command scan direction com n-1 start */
#define SSD1306_CMD_DISPLAY_OFFSET                          0xD3        /** command display offset */
#define SSD1306_CMD_DISPLAY_CLOCK_DIVIDE                    0xD5        /** command display clock divide */
#define SSD1306_CMD_SET_ZOOM_IN                             0xD6        /** command set zoom in */
#define SSD1306_CMD_PRE_CHARGE_PERIOD                       0xD9        /** command pre charge period */
#define SSD1306_CMD_COM_PINS_CONF                           0xDA        /** command com pins conf */
#define SSD1306_CMD_COMH_DESLECT_LEVEL                      0xDB        /** command comh deslect level */
#define SSD1306_CMD_NOP                                     0xE3        /** command nop */

static int a_ssd1306_write_byte(ssd1306_handle_t *handle, uint8_t data, uint8_t cmd)
{
    if (handle->iic_spi == SSD1306_INTERFACE_IIC)
    {
        return handle->iic_write(handle->iic_addr, cmd == 0 ? 0x00 : 0x40, &data, 1);
    }
    else if (handle->iic_spi == SSD1306_INTERFACE_SPI)
    {
        if (handle->spi_cmd_data_gpio_write(cmd) != 0)
        {
            return -1;
        }

        return handle->spi_write_cmd(&data, 1);
    }
    return -1;
}

static int a_ssd1306_multiple_write_byte(ssd1306_handle_t *handle, uint8_t *data, uint8_t len, uint8_t cmd)
{
    if (handle->iic_spi == SSD1306_INTERFACE_IIC)
    {
        return handle->iic_write(handle->iic_addr, cmd == 0 ? 0x00 : 0x40, data, len);
    }
    else if (handle->iic_spi == SSD1306_INTERFACE_SPI)
    {
        if (handle->spi_cmd_data_gpio_write(cmd) != 0)
        {
            return -1;
        }

        return handle->spi_write_cmd(data, len);
    }

    return -1;
}

int ssd1306_draw_point(ssd1306_handle_t *handle, int16_t x, int16_t y, uint8_t data)
{
    uint8_t pos;
    uint8_t bx;
    uint8_t temp = 0;

    if (x > 127 || y > 63)
    {
        return -1;
    }

    pos = y / 8;
    bx = y % 8;
    temp = 1 << bx;
    if (data != 0)
    {
        handle->gram[x][pos] |= temp;
    }
    else
    {
        handle->gram[x][pos] &= ~temp;
    }

    return 0;
}

int ssd1306_draw_stright_line(ssd1306_handle_t *handle, int16_t x1, int16_t y1, int16_t x2, int16_t y2, uint8_t data)
{
    if (x1 == x2)
    {
        for (int i = y1; i <= y2; i++)
        {
            ssd1306_draw_point(handle, x1, i, data);
        }
    }
    else if (y1 == y2)
    {
        for (int i = x1; i <= x2; i++)
        {
            ssd1306_draw_point(handle, i, y1, data);
        }
    }
    else
    {
        return -1;
    }

    return 0;
}

int ssd1306_draw_char(ssd1306_handle_t *handle, int16_t x, int16_t y, uint8_t chr, uint8_t size, uint8_t mode)
{
    uint8_t temp, t, t1;
    uint8_t y0 = y;
    uint8_t csize = (size / 8 + ((size % 8) ? 1 : 0)) * (size / 2);

    chr = chr - ' ';
    for (t = 0; t < csize; t++)
    {
        if (size == 12)
        {
            temp = gsc_ssd1306_ascii_1206[chr][t];
        }
        else if (size == 16)
        {
            temp = gsc_ssd1306_ascii_1608[chr][t];
        }
        else if (size == 24)
        {
            temp = gsc_ssd1306_ascii_2412[chr][t];
        }
        else
        {
            return -1;
        }
        for (t1 = 0; t1 < 8; t1++)
        {
            if ((temp & 0x80) != 0)
            {
                if (ssd1306_draw_point(handle, x, y, mode) != 0)
                {
                    return -1;
                }
            }
            else
            {
                if (ssd1306_draw_point(handle, x, y, !mode) != 0)
                {
                    return -1;
                }
            }
            temp <<= 1;
            y++;
            if ((y - y0) == size)
            {
                y = y0;
                x++;

                break;
            }
        }
    }

    return 0;
}

int ssd1306_draw_string(ssd1306_handle_t *handle, int16_t x, int16_t y, const char *str, uint16_t len, uint8_t color, ssd1306_font_t font)
{
    if (handle == NULL || !handle->inited)
    {
        return -1;
    }

    if ((x > 127) || (y > 63))
    {
        return -1;
    }

    while ((len != 0) && (*str <= '~') && (*str >= ' '))
    {
        if (ssd1306_draw_char(handle, x, y, *str, font, color) != 0)
        {
            return -1;
        }
        x += (uint8_t)(font / 2);
        str++;
        len--;
    }

    return 0;
}

int ssd1306_draw_rect(ssd1306_handle_t *handle, int16_t x, int16_t y, uint8_t width, uint8_t height, uint8_t color)
{
    if (handle == NULL || !handle->inited)
    {
        return -1;
    }

    ssd1306_draw_stright_line(handle, x, y, x + width - 1, y, color);
    ssd1306_draw_stright_line(handle, x + width -1, y, x + width -1, y + height -1, color);
    ssd1306_draw_stright_line(handle, x, y + height -1, x + width -1, y + height -1, color);
    ssd1306_draw_stright_line(handle, x, y, x, y + height -1, color);

    return 0;
}

int ssd1306_update(ssd1306_handle_t *handle)
{
    if (handle == NULL || !handle->inited)
    {
        return -1;
    }

    for (uint8_t i = 0; i < 8; i++)
    {
        a_ssd1306_write_byte(handle, SSD1306_CMD_PAGE_ADDR + i, SSD1306_CMD);
        a_ssd1306_write_byte(handle, SSD1306_CMD_LOWER_COLUMN_START_ADDRESS, SSD1306_CMD);
        a_ssd1306_write_byte(handle, SSD1306_CMD_HIGHER_COLUMN_START_ADDRESS, SSD1306_CMD);
        uint8_t data[128];
        for (uint8_t n = 0; n < 128; n++)
        {
            data[n] = handle->gram[n][i];
        }
        a_ssd1306_multiple_write_byte(handle, data, 128, SSD1306_DATA);
    }

    return 0;
}

int ssd1306_clear(ssd1306_handle_t *handle, int16_t x, int16_t y, int16_t width, int16_t height)
{
    if (handle == NULL || !handle->inited)
    {
        return -1;
    }

    if (y < x || height < 0 || width < 0)
        return -1;

    for (int i = x; i < x + width; i++)
    {
        for (int j = y; j < y + height; j++)
        {
            ssd1306_draw_point(handle, i, j, 0);
        }
    }

    return 0;
}

int ssd1306_get_point(ssd1306_handle_t *handle, int16_t x, int16_t y)
{
    uint8_t pos;
    uint8_t bx;
    uint8_t temp = 0;

    if (handle == NULL || !handle->inited)
    {
        return 0;
    }

    if ((x > 127) || (y > 63))
    {

        return 0;
    }

    pos = y / 8;
    bx = y % 8;
    temp = 1 << bx;
    return (handle->gram[x][pos] & temp);
}

int ssd1306_init(ssd1306_handle_t *handle)
{
    if (handle == NULL || handle->inited)
    {
        return -1;
    }
    // if (handle->spi_cmd_data_gpio_init() != 0)
    //{
    //     return -1;
    // }
    // if (handle->reset_gpio_init() != 0)
    //{
    //     (void)handle->spi_cmd_data_gpio_deinit();
    //     return -1;
    // }
    // if (handle->reset_gpio_write(0) != 0)
    //{
    //     (void)handle->spi_cmd_data_gpio_deinit();
    //     (void)handle->reset_gpio_deinit();
    //     return -1;
    // }
    // handle->delay_ms(100);
    // if (handle->reset_gpio_write(1) != 0)
    //{
    //     (void)handle->spi_cmd_data_gpio_deinit();
    //     (void)handle->reset_gpio_deinit();
    //     return -1;
    // }
    if (handle->iic_spi == SSD1306_INTERFACE_IIC)
    {
        if (handle->iic_init(handle->iic_addr) != 0)
        {

            //(void)handle->spi_cmd_data_gpio_deinit();
            //(void)handle->reset_gpio_deinit();
            return -1;
        }
    }
    // else if (handle->iic_spi == SSD1306_INTERFACE_SPI)
    //{
    //     if (handle->spi_init() != 0)
    //     {
    //
    //         (void)handle->spi_cmd_data_gpio_deinit();
    //         (void)handle->reset_gpio_deinit();
    //         return -1;
    //     }
    // }
    else
    {
        return -1;
    }
    handle->inited = 1;

    return 0;
}

int ssd1306_deinit(ssd1306_handle_t *handle)
{
    uint8_t buf[2];

    if (handle == NULL || !handle->inited)
    {
        return -1;
    }

    buf[0] = SSD1306_CMD_CHARGE_PUMP_SETTING;
    buf[1] = 0x10 | (0 << 2);
    a_ssd1306_multiple_write_byte(handle, (uint8_t *)buf, 2, SSD1306_CMD);

    a_ssd1306_write_byte(handle, SSD1306_CMD_DISPLAY_OFF, SSD1306_CMD);

    // handle->reset_gpio_deinit();
    // handle->spi_cmd_data_gpio_deinit();
    if (handle->iic_spi == SSD1306_INTERFACE_IIC)
    {
        return handle->iic_deinit(handle->iic_addr);
    }
    // else if (handle->iic_spi == SSD1306_INTERFACE_SPI)
    //{
    //     return handle->spi_deinit();
    // }
    else
    {
        return -1;
    }
    handle->inited = 0;

    return 0;
}

int ssd1306_set_interface(ssd1306_handle_t *handle, ssd1306_interface_t interface)
{
    if (handle == NULL)
    {
        return -1;
    }
    handle->iic_spi = (uint8_t)interface;
    return 0;
}

int ssd1306_get_interface(ssd1306_handle_t *handle, ssd1306_interface_t *interface)
{
    if (handle == NULL)
    {
        return -1;
    }
    *interface = (ssd1306_interface_t)(handle->iic_spi);
    return 0;
}

int ssd1306_set_addr(ssd1306_handle_t *handle, uint16_t addr)
{
    if (handle == NULL)
    {
        return -1;
    }
    handle->iic_addr = addr;
    return 0;
}

int ssd1306_set_low_column_start_address(ssd1306_handle_t *handle, uint8_t addr)
{
    if (handle == NULL)
    {
        return -1;
    }
    if (handle->inited != 1)
    {
        return -1;
    }
    if (addr > 0x0F)
    {

        return -1;
    }

    return a_ssd1306_write_byte(handle, SSD1306_CMD_LOWER_COLUMN_START_ADDRESS | (addr & 0x0F), SSD1306_CMD);
}

int ssd1306_set_high_column_start_address(ssd1306_handle_t *handle, uint8_t addr)
{
    if (handle == NULL || !handle->inited)
    {
        return -1;
    }
    if (addr > 0x0F)
    {

        return -1;
    }

    return a_ssd1306_write_byte(handle, SSD1306_CMD_HIGHER_COLUMN_START_ADDRESS | (addr & 0x0F), SSD1306_CMD);
}

int ssd1306_set_memory_addressing_mode(ssd1306_handle_t *handle, ssd1306_memory_addressing_mode_t mode)
{
    uint8_t buf[2];

    if (handle == NULL)
    {
        return -1;
    }
    if (handle->inited != 1)
    {
        return -1;
    }

    buf[0] = SSD1306_CMD_MEMORY_ADDRESSING_MODE;
    buf[1] = mode;

    return a_ssd1306_multiple_write_byte(handle, (uint8_t *)buf, 2, SSD1306_CMD);
}

int ssd1306_set_column_address_range(ssd1306_handle_t *handle, uint8_t start_addr, uint8_t end_addr)
{
    uint8_t buf[3];

    if (handle == NULL)
    {
        return -1;
    }
    if (handle->inited != 1)
    {
        return -1;
    }
    if (start_addr > 0x7F)
    {

        return -1;
    }
    if (end_addr > 0x7F)
    {

        return -1;
    }

    buf[0] = SSD1306_CMD_SET_COLUMN_ADDRESS;
    buf[1] = start_addr & 0x7F;
    buf[2] = end_addr & 0x7F;

    return a_ssd1306_multiple_write_byte(handle, (uint8_t *)buf, 3, SSD1306_CMD);
}

int ssd1306_set_page_address_range(ssd1306_handle_t *handle, uint8_t start_addr, uint8_t end_addr)
{
    uint8_t buf[3];

    if (handle == NULL)
    {
        return -1;
    }
    if (handle->inited != 1)
    {
        return -1;
    }
    if (start_addr > 0x07)
    {

        return -1;
    }
    if (end_addr > 0x07)
    {

        return -1;
    }

    buf[0] = SSD1306_CMD_SET_PAGE_ADDRESS;
    buf[1] = start_addr & 0x07;
    buf[2] = end_addr & 0x07;

    return a_ssd1306_multiple_write_byte(handle, (uint8_t *)buf, 3, SSD1306_CMD);
}

int ssd1306_set_fade_blinking_mode(ssd1306_handle_t *handle, ssd1306_fade_blinking_mode_t mode, uint8_t frames)
{
    uint8_t buf[2];

    if (handle == NULL)
    {
        return -1;
    }
    if (handle->inited != 1)
    {
        return -1;
    }
    if (frames > 0x0F)
    {

        return -1;
    }

    buf[0] = SSD1306_CMD_SET_FADE_OUT_AND_BLINKING;
    buf[1] = (uint8_t)((mode << 4) | (frames & 0x0F));

    return a_ssd1306_multiple_write_byte(handle, (uint8_t *)buf, 2, SSD1306_CMD);
}

int ssd1306_set_right_horizontal_scroll(ssd1306_handle_t *handle, uint8_t start_page_addr, uint8_t end_page_addr, ssd1306_scroll_frame_t frames)
{
    uint8_t buf[7];

    if (handle == NULL)
    {
        return -1;
    }
    if (handle->inited != 1)
    {
        return -1;
    }
    if (start_page_addr > 0x07)
    {

        return -1;
    }
    if (end_page_addr > 0x07)
    {

        return -1;
    }

    buf[0] = SSD1306_CMD_RIGHT_HORIZONTAL_SCROLL;
    buf[1] = 0x00;
    buf[2] = start_page_addr & 0x07;
    buf[3] = frames & 0x07;
    buf[4] = end_page_addr & 0x07;
    buf[5] = 0x00;
    buf[6] = 0xFF;

    return a_ssd1306_multiple_write_byte(handle, (uint8_t *)buf, 7, SSD1306_CMD);
}

int ssd1306_set_left_horizontal_scroll(ssd1306_handle_t *handle, uint8_t start_page_addr, uint8_t end_page_addr,
                                       ssd1306_scroll_frame_t frames)
{
    uint8_t buf[7];

    if (handle == NULL)
    {
        return -1;
    }
    if (handle->inited != 1)
    {
        return -1;
    }
    if (start_page_addr > 0x07)
    {

        return -1;
    }
    if (end_page_addr > 0x07)
    {

        return -1;
    }

    buf[0] = SSD1306_CMD_LEFT_HORIZONTAL_SCROLL;
    buf[1] = 0x00;
    buf[2] = start_page_addr & 0x07;
    buf[3] = frames & 0x07;
    buf[4] = end_page_addr & 0x07;
    buf[5] = 0x00;
    buf[6] = 0xFF;

    return a_ssd1306_multiple_write_byte(handle, (uint8_t *)buf, 7, SSD1306_CMD);
}

int ssd1306_set_vertical_right_horizontal_scroll(ssd1306_handle_t *handle, uint8_t start_page_addr, uint8_t end_page_addr,
                                                 uint8_t rows, ssd1306_scroll_frame_t frames)
{
    uint8_t buf[6];

    if (handle == NULL)
    {
        return -1;
    }
    if (handle->inited != 1)
    {
        return -1;
    }
    if (start_page_addr > 0x07)
    {

        return -1;
    }
    if (end_page_addr > 0x07)
    {

        return -1;
    }
    if (rows > 0x3F)
    {

        return -1;
    }

    buf[0] = SSD1306_CMD_VERTICAL_RIGHT_HORIZONTAL_SCROLL;
    buf[1] = 0x00;
    buf[2] = start_page_addr & 0x07;
    buf[3] = frames & 0x07;
    buf[4] = end_page_addr & 0x07;
    buf[5] = rows & 0x3F;

    return a_ssd1306_multiple_write_byte(handle, (uint8_t *)buf, 6, SSD1306_CMD);
}

int ssd1306_set_vertical_left_horizontal_scroll(ssd1306_handle_t *handle, uint8_t start_page_addr, uint8_t end_page_addr,
                                                uint8_t rows, ssd1306_scroll_frame_t frames)
{
    uint8_t buf[6];

    if (handle == NULL)
    {
        return -1;
    }
    if (handle->inited != 1)
    {
        return -1;
    }
    if (start_page_addr > 0x07)
    {

        return -1;
    }
    if (end_page_addr > 0x07)
    {

        return -1;
    }
    if (rows > 0x3F)
    {

        return -1;
    }

    buf[0] = SSD1306_CMD_VERTICAL_LEFT_HORIZONTAL_SCROLL;
    buf[1] = 0x00;
    buf[2] = start_page_addr & 0x07;
    buf[3] = frames & 0x07;
    buf[4] = end_page_addr & 0x07;
    buf[5] = rows & 0x3F;

    return a_ssd1306_multiple_write_byte(handle, (uint8_t *)buf, 6, SSD1306_CMD);
}

int ssd1306_deactivate_scroll(ssd1306_handle_t *handle)
{
    if (handle == NULL)
    {
        return -1;
    }
    if (handle->inited != 1)
    {
        return -1;
    }

    return a_ssd1306_write_byte(handle, SSD1306_CMD_DEACTIVATE_SCROLL, SSD1306_CMD);
}

int ssd1306_activate_scroll(ssd1306_handle_t *handle)
{
    if (handle == NULL)
    {
        return -1;
    }
    if (handle->inited != 1)
    {
        return -1;
    }

    return a_ssd1306_write_byte(handle, SSD1306_CMD_ACTIVATE_SCROLL, SSD1306_CMD);
}

int ssd1306_set_display_start_line(ssd1306_handle_t *handle, uint8_t l)
{
    if (handle == NULL)
    {
        return -1;
    }
    if (handle->inited != 1)
    {
        return -1;
    }
    if (l > 0x3F)
    {

        return -1;
    }

    return a_ssd1306_write_byte(handle, SSD1306_CMD_DISPLAY_START_LINE | (l & 0x3F), SSD1306_CMD);
}

int ssd1306_set_contrast(ssd1306_handle_t *handle, uint8_t contrast)
{
    uint8_t buf[2];

    if (handle == NULL)
    {
        return -1;
    }
    if (handle->inited != 1)
    {
        return -1;
    }

    buf[0] = SSD1306_CMD_CONTRAST_CONTROL;
    buf[1] = contrast;

    return a_ssd1306_multiple_write_byte(handle, (uint8_t *)buf, 2, SSD1306_CMD);
}

int ssd1306_set_charge_pump(ssd1306_handle_t *handle, ssd1306_charge_pump_t enable)
{
    uint8_t buf[2];

    if (handle == NULL)
    {
        return -1;
    }
    if (handle->inited != 1)
    {
        return -1;
    }

    buf[0] = SSD1306_CMD_CHARGE_PUMP_SETTING;
    buf[1] = (uint8_t)(0x10 | (enable << 2));

    return a_ssd1306_multiple_write_byte(handle, (uint8_t *)buf, 2, SSD1306_CMD);
}

int ssd1306_set_segment_remap(ssd1306_handle_t *handle, ssd1306_segment_column_remap_t remap)
{
    if (handle == NULL)
    {
        return -1;
    }
    if (handle->inited != 1)
    {
        return -1;
    }

    if (remap != 0)
    {
        return a_ssd1306_write_byte(handle, SSD1306_CMD_COLUMN_127_MAPPED_TO_SEG0, SSD1306_CMD);
    }
    else
    {
        return a_ssd1306_write_byte(handle, SSD1306_CMD_COLUMN_0_MAPPED_TO_SEG0, SSD1306_CMD);
    }
}

int ssd1306_set_vertical_scroll_area(ssd1306_handle_t *handle, uint8_t start_row, uint8_t end_row)
{
    uint8_t buf[3];

    if (handle == NULL)
    {
        return -1;
    }
    if (handle->inited != 1)
    {
        return -1;
    }
    if (start_row > 0x3F)
    {

        return -1;
    }
    if (end_row > 0x7F)
    {

        return -1;
    }
    if (end_row > start_row)
    {

        return -1;
    }

    buf[0] = SSD1306_CMD_VERTICAL_SCROLL_AREA;
    buf[1] = start_row;
    buf[2] = end_row;

    return a_ssd1306_multiple_write_byte(handle, (uint8_t *)buf, 3, SSD1306_CMD);
}

int ssd1306_set_entire_display(ssd1306_handle_t *handle, ssd1306_entire_display_t enable)
{
    if (handle == NULL)
    {
        return -1;
    }
    if (handle->inited != 1)
    {
        return -1;
    }

    if (enable != 0)
    {
        return a_ssd1306_write_byte(handle, SSD1306_CMD_ENTIRE_DISPLAY_ON, SSD1306_CMD);
    }
    else
    {
        return a_ssd1306_write_byte(handle, SSD1306_CMD_ENTIRE_DISPLAY_OFF, SSD1306_CMD);
    }
}

int ssd1306_set_display_mode(ssd1306_handle_t *handle, ssd1306_display_mode_t mode)
{
    if (handle == NULL)
    {
        return -1;
    }
    if (handle->inited != 1)
    {
        return -1;
    }

    if (mode != 0)
    {
        return a_ssd1306_write_byte(handle, SSD1306_CMD_INVERSE_DISPLAY, SSD1306_CMD);
    }
    else
    {
        return a_ssd1306_write_byte(handle, SSD1306_CMD_NORMAL_DISPLAY, SSD1306_CMD);
    }
}

int ssd1306_set_multiplex_ratio(ssd1306_handle_t *handle, uint8_t multiplex)
{
    uint8_t buf[2];

    if (handle == NULL)
    {
        return -1;
    }
    if (handle->inited != 1)
    {
        return -1;
    }
    if (multiplex < 0x0F)
    {

        return -1;
    }
    if (multiplex > 0x3F)
    {

        return -1;
    }

    buf[0] = SSD1306_CMD_MULTIPLEX_RATIO;
    buf[1] = multiplex;

    return a_ssd1306_multiple_write_byte(handle, (uint8_t *)buf, 2, SSD1306_CMD);
}

int ssd1306_set_display(ssd1306_handle_t *handle, ssd1306_display_t on_off)
{
    if (handle == NULL)
    {
        return -1;
    }
    if (handle->inited != 1)
    {
        return -1;
    }

    if (on_off != 0)
    {
        return a_ssd1306_write_byte(handle, SSD1306_CMD_DISPLAY_ON, SSD1306_CMD);
    }
    else
    {
        return a_ssd1306_write_byte(handle, SSD1306_CMD_DISPLAY_OFF, SSD1306_CMD);
    }
}

int ssd1306_set_page_address(ssd1306_handle_t *handle, uint8_t addr)
{
    if (handle == NULL)
    {
        return -1;
    }
    if (handle->inited != 1)
    {
        return -1;
    }
    if (addr > 0x07)
    {

        return -1;
    }

    return a_ssd1306_write_byte(handle, SSD1306_CMD_PAGE_ADDR | (addr & 0x07), SSD1306_CMD);
}

int ssd1306_set_scan_direction(ssd1306_handle_t *handle, ssd1306_scan_direction_t dir)
{
    if (handle == NULL)
    {
        return -1;
    }
    if (handle->inited != 1)
    {
        return -1;
    }

    if (dir != 0)
    {
        return a_ssd1306_write_byte(handle, SSD1306_CMD_SCAN_DIRECTION_COMN_1_START, SSD1306_CMD);
    }
    else
    {
        return a_ssd1306_write_byte(handle, SSD1306_CMD_SCAN_DIRECTION_COM0_START, SSD1306_CMD);
    }
}

int ssd1306_set_display_offset(ssd1306_handle_t *handle, uint8_t offset)
{
    uint8_t buf[2];

    if (handle == NULL)
    {
        return -1;
    }
    if (handle->inited != 1)
    {
        return -1;
    }
    if (offset > 0x3F)
    {

        return -1;
    }

    buf[0] = SSD1306_CMD_DISPLAY_OFFSET;
    buf[1] = offset;

    return a_ssd1306_multiple_write_byte(handle, (uint8_t *)buf, 2, SSD1306_CMD);
}

int ssd1306_set_display_clock(ssd1306_handle_t *handle, uint8_t oscillator_frequency, uint8_t clock_divide)
{
    uint8_t buf[2];

    if (handle == NULL)
    {
        return -1;
    }
    if (handle->inited != 1)
    {
        return -1;
    }
    if (oscillator_frequency > 0x0F)
    {

        return -1;
    }
    if (clock_divide > 0x0F)
    {

        return -1;
    }

    buf[0] = SSD1306_CMD_DISPLAY_CLOCK_DIVIDE;
    buf[1] = (oscillator_frequency << 4) | clock_divide;

    return a_ssd1306_multiple_write_byte(handle, (uint8_t *)buf, 2, SSD1306_CMD);
}

int ssd1306_set_zoom_in(ssd1306_handle_t *handle, ssd1306_zoom_in_t zoom)
{
    uint8_t buf[2];

    if (handle == NULL)
    {
        return -1;
    }
    if (handle->inited != 1)
    {
        return -1;
    }

    buf[0] = SSD1306_CMD_SET_ZOOM_IN;
    buf[1] = zoom;

    return a_ssd1306_multiple_write_byte(handle, (uint8_t *)buf, 2, SSD1306_CMD);
}

int ssd1306_set_precharge_period(ssd1306_handle_t *handle, uint8_t phase1_period, uint8_t phase2_period)
{
    uint8_t buf[2];

    if (handle == NULL)
    {
        return -1;
    }
    if (handle->inited != 1)
    {
        return -1;
    }
    if (phase1_period > 0x0F)
    {

        return -1;
    }
    if (phase2_period > 0x0F)
    {

        return -1;
    }

    buf[0] = SSD1306_CMD_PRE_CHARGE_PERIOD;
    buf[1] = (phase2_period << 4) | phase1_period;

    return a_ssd1306_multiple_write_byte(handle, (uint8_t *)buf, 2, SSD1306_CMD);
}

int ssd1306_set_com_pins_hardware_conf(ssd1306_handle_t *handle, ssd1306_pin_conf_t conf, ssd1306_left_right_remap_t remap)
{
    uint8_t buf[2];

    if (handle == NULL)
    {
        return -1;
    }
    if (handle->inited != 1)
    {
        return -1;
    }

    buf[0] = SSD1306_CMD_COM_PINS_CONF;
    buf[1] = (uint8_t)((conf << 4) | (remap << 5) | 0x02);

    return a_ssd1306_multiple_write_byte(handle, (uint8_t *)buf, 2, SSD1306_CMD);
}

int ssd1306_set_deselect_level(ssd1306_handle_t *handle, ssd1306_deselect_level_t level)
{
    uint8_t buf[2];

    if (handle == NULL)
    {
        return -1;
    }
    if (handle->inited != 1)
    {
        return -1;
    }

    buf[0] = SSD1306_CMD_COMH_DESLECT_LEVEL;
    buf[1] = (uint8_t)(level << 4);

    return a_ssd1306_multiple_write_byte(handle, (uint8_t *)buf, 2, SSD1306_CMD);
}

int ssd1306_write_cmd(ssd1306_handle_t *handle, uint8_t *buf, uint8_t len)
{
    if (handle == NULL)
    {
        return -1;
    }
    if (handle->inited != 1)
    {
        return -1;
    }

    return a_ssd1306_multiple_write_byte(handle, (uint8_t *)buf, len, SSD1306_CMD);
}

int ssd1306_write_data(ssd1306_handle_t *handle, uint8_t *buf, uint8_t len)
{
    if (handle == NULL)
    {
        return -1;
    }
    if (handle->inited != 1)
    {
        return -1;
    }

    return a_ssd1306_multiple_write_byte(handle, (uint8_t *)buf, len, SSD1306_DATA);
}