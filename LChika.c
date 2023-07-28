//ok!!! 7/7
#include <linux/init.h>
#include <linux/module.h>
#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/sched.h>
#include <asm/current.h>
#include <asm/uaccess.h>

#include <linux/gpio.h>
#include <linux/delay.h>


#define DRIVER_NAME "MyDevice_NAME"
#define MAJOR_NUMBER 238

#define DATA_PIN 23
#define CLOCK_PIN 24
#define LED_NUM 8

void send1bit(unsigned pin, int value);
void set_leds(unsigned rgbas[][4]);
void controle_device(uint8_t status);
int init_gpio(void);

uint8_t blight_status;

/* open時に呼ばれる関数 */
static int myDevice_open(struct inode *inode, struct file *file) {
    printk("myDevice_open\n");
    return 0;
}

/* close時に呼ばれる関数 */
static int myDevice_close(struct inode *inode, struct file *file) {
    printk("myDevice_close\n");

    return 0;
}

/* read時に呼ばれる関数 */
static ssize_t myDevice_read(struct file *filp, char __user *buf, size_t count, loff_t *f_pos) {
    printk("mydevice_read");

    size_t remaining_bytes = (sizeof blight_status)  - *f_pos;

    if (remaining_bytes == 0) {
        // 読み込むデータがもうない場合は終了
        return 0;
    }

    // 実際に読み込むバイト数を計算
    size_t bytes_to_read = min(remaining_bytes, count);

    // デバイスからバッファへデータをコピー
    if (copy_to_user(buf, &blight_status + *f_pos, bytes_to_read)) {
        // copy_to_userが失敗した場合はエラーを返す
        return -EFAULT;
    }

    // 読み込み位置を更新
    *f_pos += bytes_to_read;

    return bytes_to_read; // 読み込んだバイト数を返す
}

/* write時に呼ばれる関数 */
static ssize_t myDevice_write(struct file *filp, const char __user *buf, size_t count, loff_t *f_pos)
{
    printk("mydevice_write");

    uint8_t receive;
    if (copy_from_user(&receive, buf, count) != 0) {
        return -EFAULT;
    }

    blight_status = receive;
    controle_device(blight_status);
    return count;
}

/* 各種システムコールに対応するハンドラテーブル */
struct file_operations s_myDevice_fops = {
    .open    = myDevice_open,
    .release = myDevice_close,
    .read    = myDevice_read,
    .write   = myDevice_write,
};

/* ロード(insmod)時に呼ばれる関数 */
static int __init myDevice_init(void)
{
    printk("myDevice_init\n");
    register_chrdev(MAJOR_NUMBER, DRIVER_NAME, &s_myDevice_fops);
    return 0;
}

/* アンロード(rmmod)時に呼ばれる関数 */
static void __exit myDevice_exit(void)
{
    printk("myDevice_exit\n");
    unregister_chrdev(MAJOR_NUMBER, DRIVER_NAME);
}

int init_gpio() {
    if (!gpio_is_valid(DATA_PIN) || !gpio_is_valid(CLOCK_PIN)) return -1;

    printk("gpio pin is available.\n");
    gpio_request(DATA_PIN, "sysfs");
    gpio_request(CLOCK_PIN, "sysfs");
    gpio_direction_output(DATA_PIN, 0);
    gpio_direction_output(CLOCK_PIN, 0);

    return 1;
}

void send1bit(unsigned pin, int value) {
    gpio_set_value(DATA_PIN, value);
    gpio_set_value(CLOCK_PIN, 1);
    ndelay(500);
    gpio_set_value(CLOCK_PIN, 0);
    ndelay(500);
}

void set_leds(unsigned rgbas[][4]) {
    //controle each LED
    //begin
    for (int i = 0; i < 32; i++) {
        send1bit(DATA_PIN, 0);
    }

    for (int i = 0; i < LED_NUM; i++) {
        unsigned a = rgbas[i][3] | 0xe0;
        for (int j = 7; j >= 0; j--) {
            send1bit(DATA_PIN, a >> j);
        }
        
        for (int j = 7; j >= 0; j--) {
            send1bit(DATA_PIN, rgbas[i][2] >> j);
        }
        
        for (int j = 7; j >= 0; j--) {
            send1bit(DATA_PIN, rgbas[i][1] >> j);
        }
        
        for (int j = 7; j >= 0; j--) {
            send1bit(DATA_PIN, rgbas[i][0] >> j);
        }
    }

    //end
    for (int i = 0; i < 32; i++) {
        send1bit(DATA_PIN, 1);
    }
}

void controle_device(uint8_t status)
{
    printk("controle device.\n");

    if (init_gpio() > 0) {

        unsigned rgbas[][4] = {
            {0, 50, 0, 5},
            {0, 50, 0, 5},
            {0, 50, 0, 5},
            {0, 50, 0, 5},
            {0, 50, 0, 5},
            {0, 50, 0, 5},
            {0, 50, 0, 5},
            {0, 50, 0, 5}
        };

        for (int i = 0; i < LED_NUM; i++) {
            int offset = LED_NUM - i - 1;
            if (status & 1 << offset) {
                rgbas[i][1] = 50;
                rgbas[i][3] = 5;
            } else {
                rgbas[i][1] = 0;
                rgbas[i][3] = 0;
            }
        }

        printk("LED is Lighting!\n");

        set_leds(rgbas);

    } else {
        printk("gpio pin is NOT available!\n");
        printk("DATA_PIN = %d.\n", gpio_is_valid(DATA_PIN));
        printk("CLOCK_PIN = %d.\n", gpio_is_valid(CLOCK_PIN));
    }
}

module_init(myDevice_init);
module_exit(myDevice_exit);

MODULE_LICENSE("GPL v2");
MODULE_AUTHOR("Ryoma Nakakita");
