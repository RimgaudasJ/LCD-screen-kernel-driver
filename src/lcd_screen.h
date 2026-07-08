#ifndef LCD_SCREEN_H
#define LCD_SCREEN_H

#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/fs.h>
#include <linux/i2c.h>
#include <linux/mutex.h>
#include <linux/types.h>
#include <linux/workqueue.h>

#define LCD_DEVICE_NAME "lcd-screen"
#define LCD_CLASS_NAME  "lcd"

#define LCD_COLS 16
#define LCD_ROWS 2
#define LCD_PAGE_SIZE_BYTES (LCD_COLS * LCD_ROWS)
#define LCD_CGRAM_SLOTS 8
#define LCD_MAX_MSG_LEN 256
#define LCD_PAGE_INTERVAL_MS 3000

/* PCF8574 -> HD44780 backpack mapping */
#define LCD_RS (1 << 0) /* P0 */
#define LCD_RW (1 << 1) /* P1 */
#define LCD_EN (1 << 2) /* P2 */
#define LCD_BL (1 << 3) /* P3 */

struct lcd_screen {
    struct i2c_client *client;
    struct cdev cdev;
    dev_t devt;
    struct class *class;
    struct device *device;
    struct mutex io_lock;
    char msg_buffer[LCD_MAX_MSG_LEN];
    int msg_len;
    int msg_chars;
    int current_page;
    struct delayed_work page_work;
};

int lcd_hw_init_sequence(struct i2c_client *client);
int lcd_hw_send_cmd(struct i2c_client *client, u8 cmd);
int lcd_hw_send_data(struct i2c_client *client, u8 data);
int lcd_hw_load_cgram(struct i2c_client *client, u8 slot, const u8 *bitmap);
int lcd_hw_backlight_off(struct i2c_client *client);

int lcd_chardev_register(struct lcd_screen *lcd);
void lcd_chardev_unregister(struct lcd_screen *lcd);
void lcd_paging_start(struct lcd_screen *lcd);
void lcd_paging_stop(struct lcd_screen *lcd);

extern struct i2c_driver lcd_i2c_driver;

#endif