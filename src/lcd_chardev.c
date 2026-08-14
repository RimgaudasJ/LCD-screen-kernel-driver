#include <linux/delay.h>
#include <linux/errno.h>
#include <linux/fs.h>
#include <linux/jiffies.h>
#include <linux/kernel.h>
#include <linux/string.h>
#include <linux/uaccess.h>

#include "lcd_screen.h"
#include "lithuanian_chars.h"

static u8 lcd_render_lith_char(int lith_idx, const int *slot_map)
{
    int slot;

    if (lith_idx < 0)
        return '?';

    if (!lithuanian_chars[lith_idx].prefer_cgram)
        return lithuanian_chars[lith_idx].fallback_ascii;

    slot = slot_map[lith_idx];
    if (slot >= 0)
        return (u8)slot;

    return lithuanian_chars[lith_idx].fallback_ascii;
}

static int lcd_count_logical_chars(const char *buf, int len)
{
    int i = 0;
    int chars = 0;

    while (i < len) {
        u8 b = (u8)buf[i];

        if (b <= 0x7F) {
            chars++;
            i++;
            continue;
        }

        if ((b == 0xC4 || b == 0xC5) && (i + 1) < len) {
            chars++;
            i += 2;
            continue;
        }

        if (b >= 0x80 && b <= 0xBF) {
            i++;
            continue;
        }

        chars++;
        i++;
    }

    return chars;
}

static int lcd_find_byte_offset_for_char(const struct lcd_screen *lcd, int logical_char)
{
    int i = 0;
    int chars = 0;

    while (i < lcd->msg_len && chars < logical_char) {
        u8 b = (u8)lcd->msg_buffer[i];

        if (b <= 0x7F) {
            i++;
            chars++;
            continue;
        }

        if ((b == 0xC4 || b == 0xC5) && (i + 1) < lcd->msg_len) {
            i += 2;
            chars++;
            continue;
        }

        if (b >= 0x80 && b <= 0xBF) {
            i++;
            continue;
        }

        i++;
        chars++;
    }

    return i;
}

static int lcd_collect_page_chars(const struct lcd_screen *lcd, int start_char,
                                  int *needed, int max_needed)
{
    int byte_idx = lcd_find_byte_offset_for_char(lcd, start_char);
    int rendered = 0;
    int needed_count = 0;

    while (rendered < LCD_PAGE_SIZE_BYTES && byte_idx < lcd->msg_len) {
        u8 b0 = (u8)lcd->msg_buffer[byte_idx];

        if (b0 <= 0x7F) {
            rendered++;
            byte_idx++;
            continue;
        }

        if ((b0 == 0xC4 || b0 == 0xC5) && (byte_idx + 1) < lcd->msg_len) {
            u8 b1 = (u8)lcd->msg_buffer[byte_idx + 1];
            int lith_idx = lith_find_char(b0, b1);

            if (lith_idx >= 0 && lithuanian_chars[lith_idx].prefer_cgram) {
                int i;
                bool exists = false;

                for (i = 0; i < needed_count; i++) {
                    if (needed[i] == lith_idx) {
                        exists = true;
                        break;
                    }
                }

                if (!exists && needed_count < max_needed)
                    needed[needed_count++] = lith_idx;
            }

            rendered++;
            byte_idx += 2;
            continue;
        }

        if (b0 >= 0x80 && b0 <= 0xBF) {
            byte_idx++;
            continue;
        }

        rendered++;
        byte_idx++;
    }

    return needed_count;
}

static int lcd_display_page_locked(struct lcd_screen *lcd)
{
    int start_char = lcd->current_page * LCD_PAGE_SIZE_BYTES;
    int needed[ARRAY_SIZE(lithuanian_chars)];
    int slot_map[ARRAY_SIZE(lithuanian_chars)];
    u8 render[LCD_PAGE_SIZE_BYTES];
    int needed_count;
    int loaded_count;
    int byte_idx;
    int rendered = 0;
    int i;
    int ret;

    memset(slot_map, -1, sizeof(slot_map));
    memset(render, ' ', sizeof(render));

    needed_count = lcd_collect_page_chars(lcd, start_char, needed,
                                          ARRAY_SIZE(needed));
    loaded_count = min(needed_count, LCD_CGRAM_SLOTS);

    /* HD44780 exposes only 8 CGRAM slots; approximated letters do not consume them. */

    for (i = 0; i < loaded_count; i++) {
        int lith_idx = needed[i];

        slot_map[lith_idx] = i;
        ret = lcd_hw_load_cgram(lcd->client, i, lithuanian_chars[lith_idx].bitmap);
        if (ret)
            return ret;
    }

    byte_idx = lcd_find_byte_offset_for_char(lcd, start_char);
    while (rendered < LCD_PAGE_SIZE_BYTES && byte_idx < lcd->msg_len) {
        u8 b0 = (u8)lcd->msg_buffer[byte_idx];

        if (b0 <= 0x7F) {
            render[rendered++] = b0;
            byte_idx++;
            continue;
        }

        if ((b0 == 0xC4 || b0 == 0xC5) && (byte_idx + 1) < lcd->msg_len) {
            u8 b1 = (u8)lcd->msg_buffer[byte_idx + 1];
            int lith_idx = lith_find_char(b0, b1);

            render[rendered++] = lcd_render_lith_char(lith_idx, slot_map);

            byte_idx += 2;
            continue;
        }

        if (b0 >= 0x80 && b0 <= 0xBF) {
            byte_idx++;
            continue;
        }

        render[rendered++] = '?';
        byte_idx++;
    }

    ret = lcd_hw_send_cmd(lcd->client, 0x01);
    if (ret)
        return ret;
    usleep_range(2000, 3000);

    ret = lcd_hw_send_cmd(lcd->client, 0x80);
    if (ret)
        return ret;

    for (i = 0; i < LCD_COLS; i++) {
        ret = lcd_hw_send_data(lcd->client, render[i]);
        if (ret)
            return ret;
    }

    ret = lcd_hw_send_cmd(lcd->client, 0xC0);
    if (ret)
        return ret;

    for (i = 0; i < LCD_COLS; i++) {
        ret = lcd_hw_send_data(lcd->client, render[LCD_COLS + i]);
        if (ret)
            return ret;
    }

    return 0;
}

static int lcd_total_pages(const struct lcd_screen *lcd)
{
    if (lcd->msg_chars <= 0)
        return 1;

    return DIV_ROUND_UP(lcd->msg_chars, LCD_PAGE_SIZE_BYTES);
}

static void lcd_page_work_handler(struct work_struct *work)
{
    struct delayed_work *dwork = to_delayed_work(work);
    struct lcd_screen *lcd = container_of(dwork, struct lcd_screen, page_work);
    int total_pages;

    mutex_lock(&lcd->io_lock);

    total_pages = lcd_total_pages(lcd);
    if (total_pages > 1) {
        lcd->current_page = (lcd->current_page + 1) % total_pages;
        lcd_display_page_locked(lcd);
    }

    mutex_unlock(&lcd->io_lock);

    schedule_delayed_work(&lcd->page_work,
                          msecs_to_jiffies(LCD_PAGE_INTERVAL_MS));
}

void lcd_paging_start(struct lcd_screen *lcd)
{
    INIT_DELAYED_WORK(&lcd->page_work, lcd_page_work_handler);
    schedule_delayed_work(&lcd->page_work,
                          msecs_to_jiffies(LCD_PAGE_INTERVAL_MS));
}

void lcd_paging_stop(struct lcd_screen *lcd)
{
    cancel_delayed_work_sync(&lcd->page_work);
}

static int lcd_open(struct inode *inode, struct file *file)
{
    struct lcd_screen *lcd = container_of(inode->i_cdev, struct lcd_screen, cdev);

    file->private_data = lcd;
    return 0;
}

static int lcd_release(struct inode *inode, struct file *file)
{
    (void)inode;
    (void)file;
    return 0;
}

static ssize_t lcd_write(struct file *file, const char __user *buf, size_t count,
                         loff_t *ppos)
{
    struct lcd_screen *lcd = file->private_data;
    char input[LCD_MAX_MSG_LEN];
    size_t i;
    size_t out = 0;
    int ret;

    if (!count)
        return 0;

    if (count > (LCD_MAX_MSG_LEN - 1))
        count = LCD_MAX_MSG_LEN - 1;

    if (copy_from_user(input, buf, count))
        return -EFAULT;

    input[count] = '\0';

    lcd_paging_stop(lcd);

    if (mutex_lock_interruptible(&lcd->io_lock))
        return -ERESTARTSYS;

    memset(lcd->msg_buffer, 0, sizeof(lcd->msg_buffer));
    for (i = 0; i < count; i++) {
        if (input[i] == '\n' || input[i] == '\r')
            continue;

        lcd->msg_buffer[out++] = input[i];
    }

    lcd->msg_len = (int)out;
    lcd->msg_chars = lcd_count_logical_chars(lcd->msg_buffer, lcd->msg_len);
    lcd->current_page = 0;

    ret = lcd_display_page_locked(lcd);
    if (!ret)
        mod_delayed_work(system_wq, &lcd->page_work,
                         msecs_to_jiffies(LCD_PAGE_INTERVAL_MS));

    mutex_unlock(&lcd->io_lock);

    if (ret)
        return ret;

    ret = (int)count;
    *ppos += count;
    return ret;
}

static const struct file_operations lcd_fops = {
    .owner = THIS_MODULE,
    .open = lcd_open,
    .release = lcd_release,
    .write = lcd_write,
};

static void lcd_chardev_cleanup(struct lcd_screen *lcd, bool chrdev_registered,
                                bool cdev_added, bool class_created,
                                bool device_created)
{
    if (device_created)
        device_destroy(lcd->class, lcd->devt);

    if (class_created)
        class_destroy(lcd->class);

    if (cdev_added)
        cdev_del(&lcd->cdev);

    if (chrdev_registered)
        unregister_chrdev_region(lcd->devt, 1);

    cancel_delayed_work_sync(&lcd->page_work);
}

int lcd_chardev_register(struct lcd_screen *lcd)
{
    int ret;
    bool chrdev_registered = false;
    bool cdev_added = false;
    bool class_created = false;
    bool device_created = false;

    INIT_DELAYED_WORK(&lcd->page_work, lcd_page_work_handler);

    ret = alloc_chrdev_region(&lcd->devt, 0, 1, LCD_DEVICE_NAME);
    if (ret) {
        lcd_chardev_cleanup(lcd, chrdev_registered, cdev_added,
                            class_created, device_created);
        return ret;
    }

    chrdev_registered = true;

    cdev_init(&lcd->cdev, &lcd_fops);
    lcd->cdev.owner = THIS_MODULE;

    ret = cdev_add(&lcd->cdev, lcd->devt, 1);
    if (ret) {
        lcd_chardev_cleanup(lcd, chrdev_registered, cdev_added,
                            class_created, device_created);
        return ret;
    }

    cdev_added = true;

    lcd->class = class_create(LCD_CLASS_NAME);
    if (IS_ERR(lcd->class)) {
        ret = PTR_ERR(lcd->class);
        lcd_chardev_cleanup(lcd, chrdev_registered, cdev_added,
                            class_created, device_created);
        return ret;
    }

    class_created = true;

    lcd->device = device_create(lcd->class, &lcd->client->dev, lcd->devt, NULL,
                                LCD_DEVICE_NAME);
    if (IS_ERR(lcd->device)) {
        ret = PTR_ERR(lcd->device);
        lcd_chardev_cleanup(lcd, chrdev_registered, cdev_added,
                            class_created, device_created);
        return ret;
    }

    device_created = true;

    return 0;
}

void lcd_chardev_unregister(struct lcd_screen *lcd)
{
    lcd_chardev_cleanup(lcd, true, true, true, true);
}
