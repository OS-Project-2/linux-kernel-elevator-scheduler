#include <linux/init.h>
#include <linux/module.h>
#include <linux/proc_fs.h>
#include <linux/ktime.h>
#include <linux/seq_file.h>
#include <linux/time64.h>

MODULE_LICENSE("GPL");

static struct timespec64 last_time;
static int has_last_time = 0;
static struct proc_dir_entry *proc_entry;

static int timer_show(struct seq_file *m, void *v) {
    struct timespec64 current_time;
    struct timespec64 elapsed;

    ktime_get_real_ts64(&current_time);

    seq_printf(m, "current time: %lld.%09ld\n",
               (long long)current_time.tv_sec,
               current_time.tv_nsec);

    if (has_last_time) {
        elapsed = timespec64_sub(current_time, last_time);
        seq_printf(m, "elapsed time: %lld.%09ld\n",
                   (long long)elapsed.tv_sec,
                   elapsed.tv_nsec);
    }

    last_time = current_time;
    has_last_time = 1;

    return 0;
}

static int timer_open(struct inode *inode, struct file *file) {
    return single_open(file, timer_show, NULL);
}

static const struct proc_ops timer_fops = {
    .proc_open    = timer_open,
    .proc_read    = seq_read,
    .proc_lseek   = seq_lseek,
    .proc_release = single_release,
};

static int __init my_timer_init(void) {
    proc_entry = proc_create("timer", 0, NULL, &timer_fops);
    if (!proc_entry)
        return -ENOMEM;
    printk(KERN_INFO "my_timer: module loaded\n");
    return 0;
}

static void __exit my_timer_exit(void) {
    proc_remove(proc_entry);
    printk(KERN_INFO "my_timer: module unloaded\n");
}

module_init(my_timer_init);
module_exit(my_timer_exit);
