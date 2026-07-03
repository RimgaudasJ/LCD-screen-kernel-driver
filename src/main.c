#include <linux/init.h>
#include <linux/module.h>

#include "lcd_screen.h"

MODULE_LICENSE("GPL");
MODULE_AUTHOR("AI Collaborator");
MODULE_DESCRIPTION("RPi3B I2C LCD Driver (HD44780 via PCF8574)");
MODULE_VERSION("1.0");

static int __init lcd_screen_init(void)
{
    return i2c_add_driver(&lcd_i2c_driver);
}

static void __exit lcd_screen_exit(void)
{
    i2c_del_driver(&lcd_i2c_driver);
}

module_init(lcd_screen_init);
module_exit(lcd_screen_exit);