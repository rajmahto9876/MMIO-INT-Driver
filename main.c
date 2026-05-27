#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/io.h>
#include <linux/gpio.h>
#include <linux/interrupt.h>
#include <linux/kthread.h>
#include <linux/sched.h>
#include <linux/delay.h>
#include <linux/atomic.h>

#define GPIO1_BASE                      0x4804C000
#define GPIO_SIZE                       0x1000

/* GPIO Registers */
/* ARM Cortex-A8 Interrupts TRM */
#define GPIO1_IRQ                       98
#define GPIO_RISINGEDGE_OFFSET          0x148
#define GPIO_FALLINGDETECT_OFFSET       0x14C

#define GPIO_IRQSTATUS_0_OFFSET         0x2C
#define GPIO_IRQSTATUS_SET_0_OFFSET     0x34

#define GPIO_OE_REGISTER_OFFSET         0x134
#define GPIO_DATAIN_OFFSET              0x138
#define GPIO_DATAOUT                    0x13C

#define GPIO_INTRRUPT_PIN               12 //(GPIO1_12)
#define GPIO_TO_IRQ_PIN                 ((1*32) + GPIO_INTRRUPT_PIN)

#define RED_LED                         13

#define SET_BIT(value, pin)    ((value) | (1U << (pin)))
#define CLEAR_BIT(value, pin)  ((value) & ~(1U << (pin)))
#define TOGGLE_BIT(value, pin) (value^=  (1<<(pin)))

static void __iomem *gpio_base = NULL;
static struct task_struct *kthread_handle = NULL;

atomic_t condition = ATOMIC_INIT(0);

/*================================================================
                        LOCAL CALLS
=================================================================*/
int kthread_thread_fn(void *args)
{
    u32 value = 0;
    pr_info("In Thread \n");
    
    while(!kthread_should_stop())
    {
        if(atomic_read(&condition) == 1)
        {
            atomic_set(&condition, 0);
            value = readl(gpio_base + GPIO_DATAOUT);
            value = value ^(1<<RED_LED);
            writel(value, gpio_base + GPIO_DATAOUT);
        }
        msleep(100);

    }
    return 0;
}

static irqreturn_t gpio_irq_handler(int irq, void *dev_id)
{
    u32 status;
    u32 flag;

    pr_info("ISR Triggered\n");

    /* Read interrupt status */
    status = readl(gpio_base + GPIO_IRQSTATUS_0_OFFSET);
    pr_info("IRQSTATUS = 0x%x\n", status);

    /*
        Initiate Flag ow Wait Queue 
    */
     atomic_set(&condition, 1);

    flag = readl(gpio_base + GPIO_IRQSTATUS_0_OFFSET);
    writel(SET_BIT(flag, GPIO_INTRRUPT_PIN), gpio_base + GPIO_IRQSTATUS_0_OFFSET);
    pr_info("Interrupt Cleared\n");

    return IRQ_HANDLED;
}

static int gpio_thread_init(void)
{
    kthread_handle = kthread_create(kthread_thread_fn, NULL, "kthread_thread");
    if(IS_ERR(kthread_handle))
    {
        pr_err("Cann;t Create kthread_handle");
        return -1;
    }

    pr_info("Statring Threads\n");
    wake_up_process(kthread_handle);
    return 0;
}

static int __init my_driver_init(void)
{
    u32 val;
    u32 rValue = 0;
    int ret = 0;

    pr_info("Driver Init\n");
    gpio_base = ioremap(GPIO1_BASE, GPIO_SIZE);
    if (gpio_base == NULL)
    {
        pr_err("ioremap failed\n");
        return -ENOMEM;
    }

    /*
        Kthread to Do Some Task
    */
    ret = gpio_thread_init();
    if(ret< 0)
    {
        iounmap(gpio_base);
        pr_err("ioremap failed\n");
        return -ENOMEM;
    }
    /*
        Set Gpio1_13 as Output and clear it.
    */
    rValue = readl(gpio_base + GPIO_OE_REGISTER_OFFSET);
    pr_info("Register GPIO_OE_REGISTER_OFFSET value: 0x%02x\n", rValue);
    writel(CLEAR_BIT(rValue, RED_LED), gpio_base + GPIO_OE_REGISTER_OFFSET);

    rValue = readl(gpio_base + GPIO_DATAIN_OFFSET);
    pr_info("Register GPIO_DATAIN_OFFSET value: 0x%02x\n", rValue);
    writel(CLEAR_BIT(rValue, RED_LED), gpio_base + GPIO_DATAOUT);

    /*
        Set Gpio1_12 as Input Interrupt Mode
        Rising Edge Detection.
    */
    rValue = readl(gpio_base + GPIO_OE_REGISTER_OFFSET);
    pr_info("Register GPIO_OE_REGISTER_OFFSET value: 0x%02x\n", rValue);
    writel(SET_BIT(rValue, GPIO_INTRRUPT_PIN), gpio_base + GPIO_OE_REGISTER_OFFSET);

    val = readl(gpio_base + GPIO_RISINGEDGE_OFFSET);
    pr_info("Register GPIO_RISINGEDGE_OFFSET value: 0x%02x\n", val);

    writel(SET_BIT(val, GPIO_INTRRUPT_PIN), gpio_base + GPIO_RISINGEDGE_OFFSET);

    /*
     * STEP 4:
     * Enable interrupt generation
     */

    val = readl(gpio_base + GPIO_IRQSTATUS_SET_0_OFFSET);
    pr_info("Register GPIO_IRQSTATUS_SET_0_OFFSET value: 0x%02x\n", val);
    writel(SET_BIT(val, GPIO_INTRRUPT_PIN), gpio_base + GPIO_IRQSTATUS_SET_0_OFFSET);


    ret = gpio_request(GPIO_TO_IRQ_PIN, "gpio_intr");
    if (ret)
    {
        pr_err("gpio_request failed\n");
        goto end;
    }
    
    pr_info("Interrupt enabled in GPIO module\n");
    ret = request_irq(gpio_to_irq(GPIO_TO_IRQ_PIN), gpio_irq_handler, IRQF_TRIGGER_RISING,
                      "my_gpio_irq", &gpio_base);

    if (ret!= 0) 
    {
        pr_err("request_irq failed\n");
        goto end;
    }

    pr_info("ISR Registered\n");
    return 0;

end:
    if(kthread_handle!= NULL)
    {
        kthread_stop(kthread_handle);
    }
    iounmap(gpio_base);
    return ret;
}

static void __exit my_driver_exit(void)
{
    u32 rValue = 0;
    pr_info("Driver Exit\n");
    if(kthread_handle!= NULL)
    {
        kthread_stop(kthread_handle);
    }

    /* setting pin to Gnd. */
    rValue = readl(gpio_base + GPIO_DATAOUT);
    rValue = rValue & ~(1<<RED_LED);
    rValue = rValue & ~(1<<GPIO_INTRRUPT_PIN);
    writel(rValue, gpio_base + GPIO_DATAOUT);

    /* setting pin to INPUT Mode. */
    rValue = readl(gpio_base + GPIO_OE_REGISTER_OFFSET);
    writel(SET_BIT(rValue, RED_LED), gpio_base + GPIO_OE_REGISTER_OFFSET);
    writel(SET_BIT(rValue, GPIO_INTRRUPT_PIN), gpio_base + GPIO_OE_REGISTER_OFFSET);

    free_irq(gpio_to_irq(GPIO_TO_IRQ_PIN), &gpio_base);
    iounmap(gpio_base);

    pr_info("Resources Freed\n");
}

module_init(my_driver_init);
module_exit(my_driver_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Raj");
MODULE_DESCRIPTION("BeagleBone MMIO + Interrupt Driver");