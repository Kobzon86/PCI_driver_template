#include <linux/module.h> /* Main module lib */
#include <linux/init.h> /* Macros definitions */
#include <linux/pci.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/kthread.h>
#include <linux/sched.h>
#include <linux/delay.h>
#include <linux/mutex.h>



#define DEVBOARD_VENDOR_ID 0x16C3
#define DEVBOARD_DEVICE_ID 0x0002
#define DEVBOARD_OFFSET_GPIO_O 0x0
#define DEVBOARD_OFFSET_GPIO_I 0x10
#define DEVBOARD_OFFSET_INTMASK DEVBOARD_OFFSET_GPIO_I + 0x8
#define DEVBOARD_OFFSET_INTCLEAN DEVBOARD_OFFSET_GPIO_I + 0xC

#define DEVICE_NAME "devboard" /* Имя нашего устройства */
 

// You can see info below using modinfo command
MODULE_LICENSE( "GPL" );
MODULE_AUTHOR( "Makson" );
MODULE_DESCRIPTION( "Template of PCI device module.\n\tIt contain main functionalities that could be used in my work as FPGA developer:\n\tDev registering, IRQ handlers, Threads, Mutex\n\tWorkability checked on Altera devboard" );

/* Global variables for the threads */
static struct task_struct *kthread_1;
static struct task_struct *kthread_2;
static int t1 = 1, t2 = 2;
static struct mutex lock;


/**
 * @brief Function which will be executed by the threads
 *
 * @param thread_nr	Pointer to number of the thread
 */
int thread_function(void * thread_nr) {
	int delay[] = { 0, 10000, 5000};
	int t_nr = *(int *) thread_nr;

	/* Working loop */
	printk("mymutex - Thread %d is executed!\n", t_nr);

	mutex_lock(&lock);
	printk("mymutex - Thread %d is in critical section!\n", t_nr);
	msleep(delay[t_nr]);
	printk("mymutex - Thread %d is leaving the critical section!\n", t_nr);
	mutex_unlock(&lock);

	printk("mymutex - Thread %d finished execution!\n", t_nr);
	return 0;
}
// int thread_function(void * thread_nr) {
// 	unsigned int i = 0;
// 	int t_nr = *(int *) thread_nr;

// 	/* Working loop */
// 	while(!kthread_should_stop()){
// 			printk("kthread - Thread %d is executed! Counter val: %d\n", t_nr, i++);
// 			msleep(t_nr * 1000);
// 	}

// 	printk("kthread - Thread %d finished execution!\n", t_nr);
// 	return 0;
// }

static struct pci_device_id devboard_ids[] = {
       { PCI_DEVICE(DEVBOARD_VENDOR_ID, DEVBOARD_DEVICE_ID) },
       { }
};

MODULE_DEVICE_TABLE(pci, devboard_ids);

struct drv_data
{
       void __iomem *ptr_bar0;
};

struct drv_data *my_data;



/**
 * @brief Read input GPIOs state 
 */
static ssize_t driver_read(struct file *File, char *user_buffer, size_t count, loff_t *offs) {
	char out_string[20];

       int buttons_state = ioread32(my_data->ptr_bar0 + DEVBOARD_OFFSET_GPIO_I);

	snprintf(out_string, sizeof(out_string), "buttons state = %d\n", buttons_state);

       copy_to_user(user_buffer, out_string, sizeof(out_string));
	return count;
}

/**
 * @brief Write output GPIO state
 */

static ssize_t driver_write(struct file *File, const char *user_buffer, size_t count, loff_t *offs) {
       char to_gpio[8];
       int to_copy, not_copied, delta;

       to_copy = (count < sizeof(to_gpio)) ? count : sizeof(to_gpio);
       not_copied = copy_from_user(to_gpio, user_buffer, to_copy);

       delta = to_copy - not_copied;

       /* In this template I use only 2 LEDS - using first byte*/
       iowrite8(to_gpio[0]-'0',my_data->ptr_bar0 + DEVBOARD_OFFSET_GPIO_O); 

	return to_copy;
}

/**
 * @brief This function is called, when the device file is opened
 */
static int driver_open(struct inode *device_file, struct file *instance) {
	// printk("devboard - open was called!\n");
	return 0;
}

/**
 * @brief This function is called, when the device file is opened
 */
static int driver_close(struct inode *device_file, struct file *instance) {
	// printk("devboard - close was called!\n");
	return 0;
}

static struct file_operations fops = {
	.owner = THIS_MODULE,
	.open = driver_open,
	.release = driver_close,
	.read = driver_read,
	.write = driver_write
};



irqreturn_t my_irq_handler(int irq, void *data) {
       printk("devboard - IRQ proc routin was invoked\n");
       
       if (my_data != NULL)
       {
              iowrite16(0x0, my_data->ptr_bar0 + DEVBOARD_OFFSET_INTMASK);
              iowrite8(0xff, my_data->ptr_bar0 + DEVBOARD_OFFSET_INTCLEAN);
              
              printk("devboard - GPIO state DWord: 0x:%x\n", ioread32(my_data->ptr_bar0 + DEVBOARD_OFFSET_GPIO_I));

              u8 readed = ioread8(my_data->ptr_bar0 + DEVBOARD_OFFSET_GPIO_O);
              iowrite8(readed - 1,my_data->ptr_bar0 + DEVBOARD_OFFSET_GPIO_O);

              iowrite16(0x1, my_data->ptr_bar0 + DEVBOARD_OFFSET_INTMASK);
              
       }

       

       return IRQ_HANDLED;      

}

/* Kernel Module's Paramerters*/
static bool irq_en = true;
static bool thread_en = false;

module_param(irq_en, bool, S_IRUGO);
module_param(thread_en, bool, S_IRUGO);

MODULE_PARM_DESC(irq_en, "This IRQ processes button #0 state");
MODULE_PARM_DESC(thread_en, "These threads do nothing but write messages in dmesg");


static int devboard_probe(struct pci_dev *dev, const struct pci_device_id *id){
       int status;
       u32 bar0_addr;
       u8 irq_line;
       // struct drv_data *my_data;


       printk("devboard on probe function. \n");
       
       if(0 != pci_read_config_dword(dev, 0x10, &bar0_addr)){
              printk(KERN_ERR "devboard - Error reading from config space\n");
              return -1;
       }
       printk("devboard - bar0_addr =  0x%x\n", bar0_addr);


       status = pci_resource_len(dev, 0);
       printk("devboard - Size of BAR0 is %d bytes\n",status);

       printk("devboard - BAR0 is mapped to 0x%llx\n",pci_resource_start(dev, 0));
       
       status = pcim_enable_device(dev);
       if(status < 0){
              printk(KERN_ERR "devboard - Error! Could not enable the device\n"); 
              return status;
       }

       status = pcim_iomap_regions(dev, BIT(0), KBUILD_MODNAME);
       if(status < 0){
              printk(KERN_ERR "devboard - Error! BAR0 is already in use!\n"); 
              return status;
       }

       my_data = devm_kzalloc(&dev->dev, sizeof(struct drv_data), GFP_KERNEL);
       if (my_data == NULL)
       {
              printk(KERN_ERR "devboard - Error! Out of memory!\n"); 
              return ENOMEM;
       }
       
       my_data->ptr_bar0 = pcim_iomap_table(dev)[0];
       if (my_data->ptr_bar0 == NULL)
       {
              printk(KERN_ERR "devboard - Error! invalid pointer to BAR0!\n"); 
              return -1;
       }


       iowrite16(0xf, my_data->ptr_bar0 + DEVBOARD_OFFSET_GPIO_O);
       printk("devboard - LEDS turned on\n"); 
       
       if(irq_en)
              /*Set up the interrupt*/
              if (dev->irq)
              {
                     status = devm_request_irq(&dev->dev, dev->irq, my_irq_handler, 0, KBUILD_MODNAME, NULL);
                     if (status)
                     {
                            printk(KERN_ERR "devboard - Error requesting IRQ\n");
                            return -1;
                     }
                     printk("devboard - Requesting IRQ %d was successful\n", dev->irq);

                     iowrite16(0x1, my_data->ptr_bar0 + DEVBOARD_OFFSET_INTMASK);
                     
              }
       



       return 0;
}

static void devboard_remove(struct pci_dev *dev){
       printk("devboard on remove function. \n");
}

static struct pci_driver devboard_driver = 
{
       .name = "devboard",
       .id_table = devboard_ids,
       .probe = devboard_probe,
       .remove = devboard_remove,
};




static int __init my_init(void){
        // Registering device and getting major number
       int major_number = register_chrdev( 0, DEVICE_NAME, &fops );

       if ( major_number < 0 )
       {
              printk( KERN_ERR "Registering the character device failed with %d\n", major_number );
              return major_number;
       }
      
       printk( "Devboard - Test module is loaded!\n" );
       printk( "Please, create a dev file with 'mknod /dev/test c %d 0'.\n", major_number );
       printk("Devboard - Registering the PCI devicew\n");
              
       if (thread_en)
       {
              printk("mymutex - Init threads\n");
              mutex_init(&lock);

              kthread_1 = kthread_create(thread_function, &t1, "kthread_1");
              if(kthread_1 != NULL){
                     /* Let's start the thread */
                     wake_up_process(kthread_1);
                     printk("mymutex - Thread 1 was created and is running now!\n");
              }
              else {
                     printk("mymutex - Thread 1 could not be created!\n");
                     return -1;
              }
              kthread_2 = kthread_run(thread_function, &t2, "kthread_2");
              if(kthread_2 != NULL)
                     printk("mymutex - Thread 2 was created and is running now!\n");
              else {
                     printk("mymutex - Thread 2 could not be created!\n");
                     kthread_stop(kthread_1);
                     return -1;
              }
              printk("mymutex - Both threads are running now!\n");

       }
       
	

       return pci_register_driver(&devboard_driver);
}


static void __exit my_exit( void )
{
       // printk("kthread - Stop both threads\n");
       // kthread_stop(kthread_1);
       // kthread_stop(kthread_2);
       printk("Devboard - Unregistering the PCI device\n");
       kfree(my_data);
       return pci_unregister_driver(&devboard_driver);
}



module_init( my_init );
module_exit( my_exit );