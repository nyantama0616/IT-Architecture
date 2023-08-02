/*
複数デバイスには対応しています。
排他的処理はまだ実装できていませんが、一旦提出させていただきます。
*/

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
#define MINOR_BASE 0
#define MINOR_NUM 9

#define DATA_PIN 23
#define CLOCK_PIN 24
#define LED_NUM 8

void send1bit(unsigned pin, int value);
void set_led(int pos, uint8_t r, uint8_t g, uint8_t b, uint8_t a); //pos番目のLEDの値を、rgbasにセット
void on_led(int pos);
void off_led(int pos);
bool is_led_on(int pos);
void apply_leds(void); //rgbasの値を元に、LEDに適応
void controle_device(uint8_t status);
int init_gpio(void);
void exit_gpio(void);

static struct cdev c_dev[MINOR_NUM]; // キャラクタデバイス構造体
static uint8_t blight_status = 0;

static unsigned rgbas[][4] = {
    {0, 0, 0, 0},
    {0, 0, 0, 0},
    {0, 0, 0, 0},
    {0, 0, 0, 0},
    {0, 0, 0, 0},
    {0, 0, 0, 0},
    {0, 0, 0, 0},
    {0, 0, 0, 0},
};

static DEFINE_MUTEX(my_mutex);
// struct mutex my_mutex;
// mutex_init(&my_mutex);

/* open時に呼ばれる関数 */
static int myDevice_open(struct inode *inode, struct file *file) {
    printk("myDevice_open\n");
    int minor = iminor(inode); // マイナー番号取得
	file->private_data = (void *)minor; // マイナー番号をwrite、read処理に渡す
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
    
    if (count == 0) {
		return 0;
	}

    int minor = (int)filp->private_data; // マイナー番号
    u_int8_t returnValue;

    if (minor == 0) {
        returnValue = blight_status;
    } else if (minor <= LED_NUM){
        returnValue = is_led_on(minor - 1);
    }

    // デバイスからバッファへデータをコピー
    if (copy_to_user(buf, &returnValue, count) != 0) {
        return -EFAULT;
    }

    return count; // 読み込んだバイト数を返す
}

/* write時に呼ばれる関数 */
static ssize_t myDevice_write(struct file *filp, const char __user *buf, size_t count, loff_t *f_pos)
{
    printk("mydevice_write");
    
    if (mutex_lock_interruptible(&my_mutex) != 0)
	{
		printk("fale to mutex\n");
	}

    uint8_t receive;
    if (copy_from_user(&receive, buf, count) != 0) {
        return -EFAULT;
    }

    int minor = (int)filp->private_data; // マイナー番号
    if (minor <= 0) {
        controle_device(receive);
    } else if (minor <= LED_NUM) {
        if (receive) {
            on_led(minor - 1);
        } else {
            off_led(minor - 1);
        }
        apply_leds();
    }

    mutex_unlock(&my_mutex);
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
    dev_t dev = MKDEV(MAJOR_NUMBER, MINOR_BASE);
    register_chrdev_region(dev, MINOR_NUM, DRIVER_NAME); // デバイス番号を確保
    
    for(int i = 0; i < MINOR_NUM; i++){
		cdev_init(&c_dev[i],&s_myDevice_fops); // cdev構造体の初期化とシステムコールハンドラテーブルの登録
		c_dev[i].owner = THIS_MODULE;
	}
    
    int err = cdev_add(&c_dev[0], dev, MINOR_NUM); // デバイスドライバをカーネルに登録
	if (err < 0){
		printk(KERN_INFO "fale to cdev_add\n");
	}

    if (init_gpio() < 0) {
        printk("gpio pin is NOT available!\n");
        printk("DATA_PIN = %d.\n", gpio_is_valid(DATA_PIN));
        printk("CLOCK_PIN = %d.\n", gpio_is_valid(CLOCK_PIN));
    }
    return 0;
}

/* アンロード(rmmod)時に呼ばれる関数 */
static void __exit myDevice_exit(void)
{
    printk("myDevice_exit\n");

    for (int i = 0; i < LED_NUM; i++) {
        off_led(i);
    }
    apply_leds();

    // デバイスドライバをカーネルから削除
	for(int i = 0; i < MINOR_NUM; i++){
		cdev_del(&c_dev[i]);
	}

    // デバイスドライバで使用していたメジャー番号の登録を削除
    dev_t dev = MKDEV(MAJOR_NUMBER, MINOR_BASE);
	unregister_chrdev_region(dev, MINOR_NUM);

    exit_gpio();
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

void exit_gpio() {
    gpio_free(DATA_PIN);
	gpio_free(CLOCK_PIN);
}

void send1bit(unsigned pin, int value) {
    gpio_set_value(DATA_PIN, value);
    gpio_set_value(CLOCK_PIN, 1);
    ndelay(500);
    gpio_set_value(CLOCK_PIN, 0);
    ndelay(500);
}

void set_led(int pos, uint8_t r, uint8_t g, uint8_t b, uint8_t a) {
    rgbas[pos][0] = r;
    rgbas[pos][1] = g;
    rgbas[pos][2] = b;
    rgbas[pos][3] = a;
}

void on_led(int pos) {
    const u_int8_t n = (LED_NUM - pos - 1);
    blight_status |= 1 << n;
    set_led(pos, 30, 0, 30, 5);
}

void off_led(int pos) {
    const u_int8_t n = (LED_NUM - pos - 1);
    blight_status &= ~(1 << n);
    set_led(pos, 0, 0, 0, 0);
}

bool is_led_on(int pos) {
    return (blight_status & (1 << pos)) == (1 << pos);
}

void apply_leds() {
    //最初に0を32bit送る
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

    //最後に1を32bit送る
    for (int i = 0; i < 32; i++) {
        send1bit(DATA_PIN, 1);
    }
}

void controle_device(uint8_t status)
{
    printk("controle device.\n");

    for (int i = 0; i < LED_NUM; i++) {
        int offset = LED_NUM - i - 1;
        if (status & 1 << offset) {
            on_led(i);
        } else {
            off_led(i);
        }
    }

    apply_leds();
}

module_init(myDevice_init);
module_exit(myDevice_exit);

MODULE_LICENSE("GPL v2");
MODULE_AUTHOR("Ryoma Nakakita");
