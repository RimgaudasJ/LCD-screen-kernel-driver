#include <linux/errno.h>
#include <linux/fs.h>
#include <linux/kernel.h>
#include <linux/delay.h>
#include <linux/jiffies.h>
#include <linux/string.h>
#include <linux/uaccess.h>

#include "lcd_screen.h"

static int lcd_display_page_locked(struct lcd_screen *lcd)
{
    int start_idx = lcd->current_page * LCD_PAGE_SIZE_BYTES;
    int i;
    int ret;

    ret = lcd_hw_send_cmd(lcd->client, 0x01);
    if (ret)
        return ret;
    usleep_range(2000, 3000);

    ret = lcd_hw_send_cmd(lcd->client, 0x80);
    if (ret)
        return ret;

    for (i = 0; i < LCD_COLS; i++) {
        int char_idx = start_idx + i;
        u8 ch = (char_idx < lcd->msg_len) ? lcd->msg_buffer[char_idx] : ' ';

        ret = lcd_hw_send_data(lcd->client, ch);
        if (ret)
            return ret;
    }

    ret = lcd_hw_send_cmd(lcd->client, 0xC0);
    if (ret)
        return ret;

    for (i = 0; i < LCD_COLS; i++) {
        int char_idx = start_idx + LCD_COLS + i;
        u8 ch = (char_idx < lcd->msg_len) ? lcd->msg_buffer[char_idx] : ' ';

        ret = lcd_hw_send_data(lcd->client, ch);
        if (ret)
            return ret;
    }

    return 0;
}

static int lcd_total_pages(const struct lcd_screen *lcd)
{
    if (lcd->msg_len <= 0)
        return 1;

    return DIV_ROUND_UP(lcd->msg_len, LCD_PAGE_SIZE_BYTES);
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

int lcd_chardev_register(struct lcd_screen *lcd)
{
    int ret;

    INIT_DELAYED_WORK(&lcd->page_work, lcd_page_work_handler);

    ret = alloc_chrdev_region(&lcd->devt, 0, 1, LCD_DEVICE_NAME);
    if (ret)
        goto err_timer;

    cdev_init(&lcd->cdev, &lcd_fops);
    lcd->cdev.owner = THIS_MODULE;

    ret = cdev_add(&lcd->cdev, lcd->devt, 1);
    if (ret)
        goto err_unregister;

    lcd->class = class_create(LCD_CLASS_NAME);
    if (IS_ERR(lcd->class)) {
        ret = PTR_ERR(lcd->class);
        goto err_cdev;
    }

    lcd->device = device_create(lcd->class, &lcd->client->dev, lcd->devt, NULL,
                                LCD_DEVICE_NAME);
    if (IS_ERR(lcd->device)) {
        ret = PTR_ERR(lcd->device);
        goto err_class;
    }

    return 0;

err_class:
    class_destroy(lcd->class);
err_cdev:
    cdev_del(&lcd->cdev);
err_unregister:
    unregister_chrdev_region(lcd->devt, 1);
err_timer:
    cancel_delayed_work_sync(&lcd->page_work);
    return ret;
}

void lcd_chardev_unregister(struct lcd_screen *lcd)
{
    cancel_delayed_work_sync(&lcd->page_work);

    device_destroy(lcd->class, lcd->devt);
    class_destroy(lcd->class);
    cdev_del(&lcd->cdev);
    unregister_chrdev_region(lcd->devt, 1);
}
