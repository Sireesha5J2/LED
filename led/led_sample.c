#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/uaccess.h>
#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/leds.h>

#define DEVICE_NAME "act_led_control"
#define CLASS_NAME "act_led"

static dev_t dev_number;
static struct class *led_class = NULL;
static struct cdev act_led_cdev;

static ssize_t act_led_write(struct file *file, const char *buffer, size_t len, loff_t *offset) {
    char command;
    int brightness;
    
    // Copy data from user space
    if (copy_from_user(&command, buffer, 1)) {
        return -EFAULT;
    }

    // Interpret command to set brightness
    if (command == '0') {
        brightness = 0;  // Turn off LED
    } else if (command == '1') {
        brightness = 255;  // Full brightness
    } else {
        printk(KERN_INFO "Invalid command: %c\n", command);
        return -EINVAL;
    }
    
    // Write brightness value to ACT LED's brightness sysfs attribute
    struct file *f = filp_open("/sys/devices/platform/leds/leds/ACT/brightness", O_WRONLY, 0);
    if (IS_ERR(f)) {
        printk(KERN_ALERT "Failed to open ACT LED brightness control\n");
        return PTR_ERR(f);
    }

    char buf[4];
    int len_written = snprintf(buf, sizeof(buf), "%d\n", brightness);
    kernel_write(f, buf, len_written, &f->f_pos);
    filp_close(f, NULL);
    if(command == '0'){
        printk(KERN_INFO "Successfully switched it off\n");
    }
    else{
        printk(KERN_INFO "Successfully switched it on\n");
    }
    return len;
}

static int act_led_open(struct inode *inodep, struct file *filep) {
    printk(KERN_INFO "ACT LED control device opened\n");
    return 0;
}

static int act_led_release(struct inode *inodep, struct file *filep) {
    printk(KERN_INFO "ACT LED control device closed\n");
    return 0;
}

static struct file_operations fops = {
    .open = act_led_open,
    .write = act_led_write,
    .release = act_led_release,
};

static int __init act_led_init(void) {
    int result;

    // Allocate a major and minor number for the device
    result = alloc_chrdev_region(&dev_number, 0, 1, DEVICE_NAME);
    if (result < 0) {
        printk(KERN_ALERT "Failed to allocate major number\n");
        return result;
    }

    // Register device class
    led_class = class_create(CLASS_NAME);
    if (IS_ERR(led_class)) {
        unregister_chrdev_region(dev_number, 1);
        printk(KERN_ALERT "Failed to register device class\n");
        return PTR_ERR(led_class);
    }

    // Create the device
    device_create(led_class, NULL, dev_number, NULL, DEVICE_NAME);

    // Initialize and add cdev structure
    cdev_init(&act_led_cdev, &fops);
    act_led_cdev.owner = THIS_MODULE;
    result = cdev_add(&act_led_cdev, dev_number, 1);
    if (result < 0) {
        device_destroy(led_class, dev_number);
        class_destroy(led_class);
        unregister_chrdev_region(dev_number, 1);
        printk(KERN_ALERT "Failed to add cdev\n");
        return result;
    }

    printk(KERN_INFO "ACT LED control driver initialized\n");
    return 0;
}

static void __exit act_led_exit(void) {
    cdev_del(&act_led_cdev);
    device_destroy(led_class, dev_number);
    class_unregister(led_class);
    class_destroy(led_class);
    unregister_chrdev_region(dev_number, 1);
    printk(KERN_INFO "ACT LED control driver exited\n");
}

module_init(act_led_init);
module_exit(act_led_exit);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("Character Driver to Control ACT LED Brightness");
MODULE_AUTHOR("Sai Sireesha Yekoji");
