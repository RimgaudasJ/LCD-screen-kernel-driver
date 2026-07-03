#include <linux/i2c.h>
#include <linux/module.h>
#include <linux/slab.h>

#include "lcd_screen.h"

static int lcd_probe(struct i2c_client *client)
{
    struct lcd_screen *lcd;
    int ret;

    lcd = devm_kzalloc(&client->dev, sizeof(*lcd), GFP_KERNEL);
    if (!lcd)
        return -ENOMEM;

    lcd->client = client;
    mutex_init(&lcd->io_lock);
    i2c_set_clientdata(client, lcd);

    ret = lcd_chardev_register(lcd);
    if (ret)
        goto err_mutex;

    mutex_lock(&lcd->io_lock);
    ret = lcd_hw_init_sequence(client);
    mutex_unlock(&lcd->io_lock);
    if (ret)
        goto err_chardev;

    lcd_paging_start(lcd);

    dev_info(&client->dev, "registered /dev/%s\n", LCD_DEVICE_NAME);
    return 0;

err_chardev:
    lcd_chardev_unregister(lcd);
err_mutex:
    mutex_destroy(&lcd->io_lock);
    return ret;
}

static void lcd_remove(struct i2c_client *client)
{
    struct lcd_screen *lcd = i2c_get_clientdata(client);

    lcd_paging_stop(lcd);

    lcd_chardev_unregister(lcd);

    mutex_lock(&lcd->io_lock);
    lcd_hw_backlight_off(client);
    mutex_unlock(&lcd->io_lock);

    mutex_destroy(&lcd->io_lock);

    dev_info(&client->dev, "removed lcd-screen driver\n");
}

static const struct i2c_device_id lcd_id[] = {
    { "lcd-screen", 0 },
    { }
};
MODULE_DEVICE_TABLE(i2c, lcd_id);

struct i2c_driver lcd_i2c_driver = {
    .driver = {
        .name = LCD_DEVICE_NAME,
    },
    .probe = lcd_probe,
    .remove = lcd_remove,
    .id_table = lcd_id,
};
