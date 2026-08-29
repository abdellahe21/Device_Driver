#include <linux/module.h>
#include <linux/init.h>
#include <linux/fs.h>       // Header for the character device driver support
#include <linux/cdev.h>     // Header for cdev structure and functions
#include <linux/device.h>   // Header for class_create and device_create
#include <linux/uaccess.h>  // Header for copy_to_user and copy_from_user
#include <linux/kernel.h>

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Kernel Dev");
MODULE_DESCRIPTION("This is a Template !!");


#define DEVICE_NAME "my_char_dev"
#define CLASS_NAME  "my_char_class"


static dev_t dev_num;                // Holds the dynamically allocated major/minor number
static struct cdev my_cdev;          // The character device structure
static struct class *my_class = NULL; // The device class structure used for udev /dev/ creation

// Basic internal buffer to hold data written to the device file
static char device_buffer[1024];
static size_t buffer_data_size = 0;


// Called when a user-space process opens /dev/my_char_dev
static int dev_open(struct inode *inod, struct file *fil) {
    pr_info("%s: Device opened successfully\n", DEVICE_NAME);
    return 0;
}

// Called when a user-space process reads from /dev/my_char_dev (e.g., cat /dev/my_char_dev)
static ssize_t dev_read(struct file *fil, char __user *buf, size_t len, loff_t *off) {
    // If the offset is past our data size, return 0 (End of File)
    if (*off >= buffer_data_size) {
        return 0;
    }

    // Adjust read length if the user asks for more data than we have left
    if (*off + len > buffer_data_size) {
        len = buffer_data_size - *off;
    }

    // Safely copy data from the kernel space buffer to the user space buffer
    if (copy_to_user(buf, device_buffer + *off, len) != 0) {
        return -EFAULT;
    }

    *off += len; // Advance the file offset pointer
    pr_info("%s: Read %zu bytes\n", DEVICE_NAME, len);
    return len;
}

// Called when a user-space process writes to /dev/my_char_dev (e.g., echo "test" > /dev/my_char_dev)
static ssize_t dev_write(struct file *fil, const char __user *buf, size_t len, loff_t *off) {
    // Prevent buffer overflow
    if (len > sizeof(device_buffer) - 1) {
        len = sizeof(device_buffer) - 1;
    }

    // Safely copy data from the user space buffer into our kernel space buffer
    if (copy_from_user(device_buffer, buf, len) != 0) {
        return -EFAULT;
    }

    device_buffer[len] = '\0'; // Null-terminate the string safely
    buffer_data_size = len;
    
    pr_info("%s: Received %zu bytes from user space: %s\n", DEVICE_NAME, len, device_buffer);
    return len;
}

// Called when a user-space process closes /dev/my_char_dev
static int dev_release(struct inode *inod, struct file *fil) {
    pr_info("%s: Device closed successfully\n", DEVICE_NAME);
    return 0;
}

// Bind file operation system calls to our driver functions
static struct file_operations fops = {
    .owner = THIS_MODULE,
    .open = dev_open,
    .read = dev_read,
    .write = dev_write,
    .release = dev_release,
};


// Module Initialization
static int __init char_dev_init(void) {
    pr_info("%s: Initializing the driver\n", DEVICE_NAME);

    // 1. Dynamically allocate a Major and Minor number from the kernel
    if (alloc_chrdev_region(&dev_num, 0, 1, DEVICE_NAME) < 0) {
        pr_err("%s: Failed to allocate major number\n", DEVICE_NAME);
        return -1;
    }
    pr_info("%s: Registered successfully with Major: %d, Minor: %d\n", 
            DEVICE_NAME, MAJOR(dev_num), MINOR(dev_num));

    // 2. Initialize the cdev structure and link it to our file operations
    cdev_init(&my_cdev, &fops);
    my_cdev.owner = THIS_MODULE;

    // 3. Add the character device to the kernel architecture
    if (cdev_add(&my_cdev, dev_num, 1) < 0) {
        pr_err("%s: Failed to add cdev structure\n", DEVICE_NAME);
        unregister_chrdev_region(dev_num, 1);
        return -1;
    }

    // 4. Create a struct class visible under /sys/class/
    my_class = class_create( CLASS_NAME);
    if (IS_ERR(my_class)) {
        pr_err("%s: Failed to create device class\n", DEVICE_NAME);
        cdev_del(&my_cdev);
        unregister_chrdev_region(dev_num, 1);
        return PTR_ERR(my_class);
    }

    // 5. Create the physical device node under /dev/my_char_dev
    if (IS_ERR(device_create(my_class, NULL, dev_num, NULL, DEVICE_NAME))) {
        pr_err("%s: Failed to create the device node file\n", DEVICE_NAME);
        class_destroy(my_class);
        cdev_del(&my_cdev);
        unregister_chrdev_region(dev_num, 1);
        return -1;
    }

    pr_info("%s: /dev/%s device file created successfully!\n", DEVICE_NAME, DEVICE_NAME);
    return 0;
}

// Module Cleanup
static void __exit char_dev_exit(void) {
    pr_info("%s: Cleaning up driver structures\n", DEVICE_NAME);

    device_destroy(my_class, dev_num);     // Removes /dev/my_char_dev
    class_destroy(my_class);               // Removes /sys/class/my_char_class
    cdev_del(&my_cdev);                     // Removes the cdev registration
    unregister_chrdev_region(dev_num, 1);   // Frees major/minor pool reservation

    pr_info("%s: Driver unloaded cleanly\n", DEVICE_NAME);
}

module_init(char_dev_init);
module_exit(char_dev_exit);




