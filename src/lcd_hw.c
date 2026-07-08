#include <linux/delay.h>
#include <linux/errno.h>
#include <linux/i2c.h>

#include "lcd_screen.h"

static int pcf8574_write(struct i2c_client *client, u8 value)
{
    int ret = i2c_master_send(client, &value, 1);

    if (ret < 0) {
        dev_err(&client->dev, "i2c send failed: %d\n", ret);
        return ret;
    }

    if (ret != 1)
        return -EIO;

    return 0;
}

static void lcd_pulse_enable(struct i2c_client *client, uint8_t data) {
    pcf8574_write(client, data | LCD_EN);
    udelay(5);             /* INCREASED: Safe pulse width cushion */
    pcf8574_write(client, data & ~LCD_EN);
    udelay(100);           /* INCREASED: Give controller more time to digest data */
}

static int lcd_send_nibble(struct i2c_client *client, u8 nibble, u8 mode)
{
    u8 value = (nibble & 0xF0) | (mode & LCD_RS) | LCD_BL;
    int ret;

    ret = pcf8574_write(client, value);
    if (ret)
        return ret;

    lcd_pulse_enable(client, value);
    return 0;
}

static int lcd_send_byte(struct i2c_client *client, u8 value, u8 mode)
{
    int ret;

    ret = lcd_send_nibble(client, value & 0xF0, mode);
    if (ret)
        return ret;

    ret = lcd_send_nibble(client, (value << 4) & 0xF0, mode);
    if (ret)
        return ret;

    return 0;
}

int lcd_hw_send_cmd(struct i2c_client *client, u8 cmd)
{
    return lcd_send_byte(client, cmd, 0);
}

int lcd_hw_send_data(struct i2c_client *client, u8 data)
{
    return lcd_send_byte(client, data, LCD_RS);
}

int lcd_hw_load_cgram(struct i2c_client *client, u8 slot, const u8 *bitmap)
{
    int i;
    int ret;

    if (slot >= LCD_CGRAM_SLOTS)
        return -EINVAL;

    ret = lcd_hw_send_cmd(client, 0x40 + (slot * 8));
    if (ret)
        return ret;

    for (i = 0; i < 8; i++) {
        ret = lcd_hw_send_data(client, bitmap[i]);
        if (ret)
            return ret;
    }

    return lcd_hw_send_cmd(client, 0x80);
}

int lcd_hw_backlight_off(struct i2c_client *client)
{
    return pcf8574_write(client, 0x00);
}

int lcd_hw_init_sequence(struct i2c_client *client)
{
    int ret;

    /* Give the display plenty of time to stabilize voltages after boot */
    msleep(100); 

    /* Step 1: Force to 8-bit mode 3 times to sync internal state machine */
    ret = lcd_send_nibble(client, 0x30, 0);
    if (ret) return ret;
    msleep(10); /* Increased from 5ms */

    ret = lcd_send_nibble(client, 0x30, 0);
    if (ret) return ret;
    msleep(5);  /* Failsafe delay - converted from microseconds */

    ret = lcd_send_nibble(client, 0x30, 0);
    if (ret) return ret;
    msleep(5);

    /* Step 2: Switch interface strictly to 4-bit mode execution */
    ret = lcd_send_nibble(client, 0x20, 0);
    if (ret) return ret;
    msleep(5);

    /* Step 3: Device is now operating in 4-bit mode. Configure displays */
    ret = lcd_hw_send_cmd(client, 0x28); /* 2 lines, 5x8 font matrix */
    if (ret) return ret;
    msleep(1);

    ret = lcd_hw_send_cmd(client, 0x0C); /* Display ON, Cursor OFF */
    if (ret) return ret;
    msleep(1);

    ret = lcd_hw_send_cmd(client, 0x06); /* Shift cursor right when writing */
    if (ret) return ret;
    msleep(1);

    ret = lcd_hw_send_cmd(client, 0x01); /* Clear display completely */
    if (ret) return ret;
    
    /* Clear instruction demands a huge execution delay window */
    msleep(5); 
    
    return 0;
}
