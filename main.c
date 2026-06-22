#include<linux/module.h>
#include<linux/device.h>
#include<linux/cdev.h>
#include<linux/uaccess.h>
#include<linux/fs.h>
#include<linux/platform_device.h>
#include<linux/kernel.h>
#include<linux/of.h>
#include<linux/gpio.h>
#include<linux/interrupt.h>
#include <linux/math64.h> // need to perform divison for 64bit in 32bit machine 

//ilm - interrupt latency measurment
// do uname -r on your target device to which version of kernel use same documentation

#define READ_BUFF_SIZE 256

struct ilm_prvdata {
    dev_t device_number;
    int interrupt_number;
    uint64_t interrupt_count;
    struct cdev ilm_cdev;
    struct device *ilm_device;
    struct class *ilm_class;
    ktime_t irq_time_th;
    ktime_t irq_time_bh;
    uint64_t current_latency_ns;
    uint64_t total_latency_ns;
    uint64_t avg_latency_ns;
    uint64_t max_latency_ns;
    uint64_t min_latency_ns;
};

static int ilm_open(struct inode *inode, struct file *filp){

    struct ilm_prvdata *prvdata = container_of(inode->i_cdev, struct ilm_prvdata, ilm_cdev);
    filp->private_data = prvdata;
    return 0;
}

static ssize_t ilm_read(struct file *filp, char __user *buf, size_t count, loff_t *offset) {
    // Retrieve our private structure from the file pointer

    int len = 0;
    struct ilm_prvdata *prvdata = filp->private_data;

    char k_buf[READ_BUFF_SIZE];

    len = scnprintf(k_buf, sizeof(k_buf),
        "Interrupt Count: %llu \n"
        "Current Latency: %llu ns\n"
        "Average Latency: %llu ns\n"
        "Minimum Latency: %llu ns\n"
        "Maximum Latency: %llu ns\n",
        prvdata->interrupt_count,
        (unsigned long long)prvdata->current_latency_ns,
        (unsigned long long)prvdata->avg_latency_ns,
        (unsigned long long)prvdata->min_latency_ns,
        (unsigned long long)prvdata->max_latency_ns);

    if(*offset >= len) return 0;
    if(count > len - *offset) count = len - *offset;
    if(copy_to_user(buf,k_buf + *offset,count)!=0) return -EFAULT;

    *offset += count;
    return count;
}

struct file_operations f_ops = {
    .owner = THIS_MODULE,
    .open = ilm_open,
    .read = ilm_read,
};

/*understand the below function in detail*/
static irqreturn_t ilm_handler(int irq,void *dev_id){

    struct ilm_prvdata *prvdata = dev_id; //why declare struct this way? where does device id point to?
    prvdata->irq_time_th = ktime_get();
    return IRQ_WAKE_THREAD;
}


static irqreturn_t ilm_bh(int irq,void *dev_id){

    struct ilm_prvdata *prvdata = dev_id; 
    prvdata->irq_time_bh = ktime_get();
    prvdata->interrupt_count++;

    ktime_t diff;
    diff = ktime_sub(prvdata->irq_time_bh,prvdata->irq_time_th);

    prvdata->current_latency_ns = ktime_to_ns(diff);

    prvdata->total_latency_ns = prvdata->total_latency_ns + prvdata->current_latency_ns;

    //prvdata->avg_latency_ns =  (prvdata->total_latency_ns / prvdata->interrupt_count); //since variable are 64bit we cannot perform division or modoulo operaton on 32bit machine

    prvdata->avg_latency_ns = div_u64(prvdata->total_latency_ns,prvdata->interrupt_count);

    if(prvdata->current_latency_ns > prvdata->max_latency_ns){

        prvdata->max_latency_ns = prvdata->current_latency_ns;
    }

    if(prvdata->min_latency_ns == 0 || prvdata->current_latency_ns < prvdata->min_latency_ns){
        
        prvdata->min_latency_ns = prvdata->current_latency_ns;

    }

    return IRQ_HANDLED;
}

static int ilm_probe(struct platform_device *pdev){

    int ret = 0;
    struct device *dev = &pdev->dev;
    struct ilm_prvdata *prvdata;

    prvdata = devm_kzalloc(dev,sizeof(*prvdata),GFP_KERNEL);
    if(prvdata == NULL){
        dev_err(dev,"Memory allocation failed\n");
        return -ENOMEM;
    }

    platform_set_drvdata(pdev,prvdata); //why this line get detialed info later

    prvdata->interrupt_number = platform_get_irq(pdev,0);
    if(prvdata->interrupt_number < 0){
        dev_err(dev,"error trying to read the interrupt number\n");
        return prvdata->interrupt_number;
    }

    ret = request_threaded_irq(prvdata->interrupt_number ,ilm_handler , ilm_bh,
					IRQF_TRIGGER_RISING | IRQF_ONESHOT, "ilm_device"
					, prvdata);


    ret = alloc_chrdev_region(&prvdata->device_number,0,1,"ilm_chr_driver");
    if(ret < 0){
        dev_err(dev,"chrdev_allocation failed\n");
        return ret;
    }

    cdev_init(&prvdata->ilm_cdev,&f_ops);

    prvdata->ilm_cdev.owner = THIS_MODULE;

    ret = cdev_add(&prvdata->ilm_cdev,prvdata->device_number,1);
    if(ret < 0){
        dev_err(dev,"error binding device number to cdev\n");
        goto cdev_add_failed;
    }

    prvdata->ilm_class = class_create("ilm_class");
    if(IS_ERR(prvdata->ilm_class)){
        dev_err(dev,"creating ilm class failed\n");
        goto class_create_failed;
    }

    prvdata->ilm_device = device_create(prvdata->ilm_class,NULL,prvdata->device_number,NULL,"ilm_char_driver");
    if(IS_ERR(prvdata->ilm_device)){
        dev_err(dev,"ilm device creation failed\n");
        goto device_create_failed;
    }

    dev_info(dev,"ilm device sucessfully probed\n");
    return 0;

device_create_failed :
    class_destroy(prvdata->ilm_class);

class_create_failed :
    cdev_del(&prvdata->ilm_cdev);


cdev_add_failed : 
    unregister_chrdev_region(prvdata->device_number,1);
    return ret;

}

static int ilm_remove(struct platform_device *pdev){

    struct ilm_prvdata *prvdata = platform_get_drvdata(pdev);

    device_destroy(prvdata->ilm_class,prvdata->device_number);
    class_destroy(prvdata->ilm_class);
    cdev_del(&prvdata->ilm_cdev);
    unregister_chrdev_region(prvdata->device_number,1);

    pr_info("ilm device removed sucessfully\n");
    return 0;

}


const struct of_device_id ilm_match[] = {

    {.compatible = "vik,testdriver"},
    { },

};

MODULE_DEVICE_TABLE(of, ilm_match);

static struct platform_driver ilm_driver = {

    .probe = ilm_probe,
    .remove = ilm_remove,
    .driver = {
        .name = "ilm-driver",
        .of_match_table = ilm_match,
    },

};

module_platform_driver(ilm_driver);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("vikram D");
MODULE_DESCRIPTION("A char driver that measures interrupt latency"); // edit this later