#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/kthread.h>
#include <linux/mutex.h>
#include <linux/delay.h>
#include <linux/list.h>
#include <linux/slab.h>
#include <linux/syscalls.h>

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Your Name");
MODULE_DESCRIPTION("Elevator Kernel Module");

#define OFFLINE  0
#define IDLE     1
#define LOADING  2
#define UP       3
#define DOWN     4

#define MAX_FLOORS     5
#define MAX_PASSENGERS 5
#define MAX_WEIGHT     70

#define PART_TIME  0
#define LAWYER     1
#define BOSS       2
#define VISITOR    3

#define PART_TIME_WEIGHT  10
#define LAWYER_WEIGHT     15
#define BOSS_WEIGHT       20
#define VISITOR_WEIGHT    5

struct passenger {
    int type;
    int start_floor;
    int dest_floor;
    int weight;
    struct list_head list;
};

struct floor_data {
    struct list_head passengers;
    int num_passengers;
};

struct elevator_data {
    int state;
    int current_floor;
    int current_weight;
    int num_passengers;
    int total_serviced;
    int total_waiting;
    int stopping;
    struct list_head passengers;
    struct task_struct *thread;
};

static struct elevator_data elevator;
static struct floor_data floors[MAX_FLOORS];
static struct mutex elevator_mutex;
static struct proc_dir_entry *proc_entry;

extern int (*STUB_start_elevator)(void);
extern int (*STUB_issue_request)(int, int, int);
extern int (*STUB_stop_elevator)(void);

static int get_weight(int type) {
    switch (type) {
        case PART_TIME: return PART_TIME_WEIGHT;
        case LAWYER:    return LAWYER_WEIGHT;
        case BOSS:      return BOSS_WEIGHT;
        case VISITOR:   return VISITOR_WEIGHT;
        default:        return 0;
    }
}

static char get_type_char(int type) {
    switch (type) {
        case PART_TIME: return 'P';
        case LAWYER:    return 'L';
        case BOSS:      return 'B';
        case VISITOR:   return 'V';
        default:        return '?';
    }
}

static void unload_passengers(void) {
    struct passenger *p, *tmp;
    list_for_each_entry_safe(p, tmp, &elevator.passengers, list) {
        if (p->dest_floor == elevator.current_floor) {
            elevator.current_weight -= p->weight;
            elevator.num_passengers--;
            elevator.total_serviced++;
            list_del(&p->list);
            kfree(p);
            ssleep(1);
        }
    }
}

static void load_passengers(void) {
    struct passenger *p, *tmp;
    int floor_idx = elevator.current_floor - 1;

    list_for_each_entry_safe(p, tmp, &floors[floor_idx].passengers, list) {
        if (elevator.num_passengers >= MAX_PASSENGERS)
            break;
        if (elevator.current_weight + p->weight > MAX_WEIGHT)
            break;
        list_del(&p->list);
        floors[floor_idx].num_passengers--;
        elevator.total_waiting--;
        list_add_tail(&p->list, &elevator.passengers);
        elevator.num_passengers++;
        elevator.current_weight += p->weight;
        ssleep(1);
    }
}

static int has_waiting_passengers(void) {
    int i;
    for (i = 0; i < MAX_FLOORS; i++) {
        if (floors[i].num_passengers > 0)
            return 1;
    }
    return 0;
}

static int has_passengers_above(void) {
    int i;
    /* check for passengers on elevator going above current floor */
    struct passenger *p;
    list_for_each_entry(p, &elevator.passengers, list) {
        if (p->dest_floor > elevator.current_floor)
            return 1;
    }
    /* check for waiting passengers above current floor */
    for (i = elevator.current_floor; i < MAX_FLOORS; i++) {
        if (floors[i].num_passengers > 0)
            return 1;
    }
    return 0;
}

static int has_passengers_below(void) {
    int i;
    struct passenger *p;
    list_for_each_entry(p, &elevator.passengers, list) {
        if (p->dest_floor < elevator.current_floor)
            return 1;
    }
    for (i = 0; i < elevator.current_floor - 1; i++) {
        if (floors[i].num_passengers > 0)
            return 1;
    }
    return 0;
}

static int elevator_thread_fn(void *data) {
    while (!kthread_should_stop()) {
        mutex_lock(&elevator_mutex);

        if (elevator.state == OFFLINE) {
            mutex_unlock(&elevator_mutex);
            ssleep(1);
            continue;
        }

        if (elevator.state == IDLE) {
            if (has_waiting_passengers()) {
                if (has_passengers_above())
                    elevator.state = UP;
                else
                    elevator.state = DOWN;
            } else if (elevator.stopping && elevator.num_passengers == 0) {
                elevator.state = OFFLINE;
            }
            mutex_unlock(&elevator_mutex);
            ssleep(1);
            continue;
        }

        /* unload and load on current floor */
        elevator.state = LOADING;
        mutex_unlock(&elevator_mutex);

        mutex_lock(&elevator_mutex);
        unload_passengers();
        load_passengers();
        mutex_unlock(&elevator_mutex);

        mutex_lock(&elevator_mutex);
        /* decide next move */
        if (elevator.stopping) {
            if (elevator.num_passengers == 0 && !has_waiting_passengers()) {
                elevator.state = OFFLINE;
                mutex_unlock(&elevator_mutex);
                continue;
            }
        }

        if (has_passengers_above()) {
            elevator.state = UP;
            mutex_unlock(&elevator_mutex);
            ssleep(2);
            mutex_lock(&elevator_mutex);
            if (elevator.current_floor < MAX_FLOORS)
                elevator.current_floor++;
            mutex_unlock(&elevator_mutex);
        } else if (has_passengers_below()) {
            elevator.state = DOWN;
            mutex_unlock(&elevator_mutex);
            ssleep(2);
            mutex_lock(&elevator_mutex);
            if (elevator.current_floor > 1)
                elevator.current_floor--;
            mutex_unlock(&elevator_mutex);
        } else {
            elevator.state = IDLE;
            mutex_unlock(&elevator_mutex);
        }

        continue;
        mutex_unlock(&elevator_mutex);
    }
    return 0;
}

static int start_elevator_fn(void) {
    mutex_lock(&elevator_mutex);
    if (elevator.state != OFFLINE) {
        mutex_unlock(&elevator_mutex);
        return 1;
    }
    elevator.state = IDLE;
    elevator.stopping = 0;
    mutex_unlock(&elevator_mutex);
    return 0;
}

static int issue_request_fn(int start_floor, int dest_floor, int type) {
    struct passenger *p;
    int floor_idx;

    if (start_floor < 1 || start_floor > MAX_FLOORS ||
        dest_floor < 1 || dest_floor > MAX_FLOORS ||
        start_floor == dest_floor ||
        type < 0 || type > 3)
        return 1;

    p = kmalloc(sizeof(*p), GFP_KERNEL);
    if (!p)
        return -ENOMEM;

    p->type = type;
    p->start_floor = start_floor;
    p->dest_floor = dest_floor;
    p->weight = get_weight(type);
    INIT_LIST_HEAD(&p->list);

    floor_idx = start_floor - 1;

    mutex_lock(&elevator_mutex);
    list_add_tail(&p->list, &floors[floor_idx].passengers);
    floors[floor_idx].num_passengers++;
    elevator.total_waiting++;
    mutex_unlock(&elevator_mutex);

    return 0;
}

static int stop_elevator_fn(void) {
    mutex_lock(&elevator_mutex);
    if (elevator.stopping) {
        mutex_unlock(&elevator_mutex);
        return 1;
    }
    elevator.stopping = 1;
    mutex_unlock(&elevator_mutex);
    return 0;
}

static int elevator_proc_show(struct seq_file *m, void *v) {
    struct passenger *p;
    int i;
    char *state_str;

    mutex_lock(&elevator_mutex);

    switch (elevator.state) {
        case OFFLINE:  state_str = "OFFLINE";  break;
        case IDLE:     state_str = "IDLE";     break;
        case LOADING:  state_str = "LOADING";  break;
        case UP:       state_str = "UP";       break;
        case DOWN:     state_str = "DOWN";     break;
        default:       state_str = "UNKNOWN";  break;
    }

    seq_printf(m, "Elevator state: %s\n", state_str);
    seq_printf(m, "Current floor: %d\n", elevator.current_floor);
    seq_printf(m, "Current load: %d lbs\n", elevator.current_weight);

    seq_printf(m, "Elevator status:");
    list_for_each_entry(p, &elevator.passengers, list) {
        seq_printf(m, " %c%d", get_type_char(p->type), p->dest_floor);
    }
    seq_printf(m, "\n\n");

    for (i = MAX_FLOORS; i >= 1; i--) {
        int floor_idx = i - 1;
        seq_printf(m, "[%s] Floor %d: %d",
                   elevator.current_floor == i ? "*" : " ",
                   i,
                   floors[floor_idx].num_passengers);
        list_for_each_entry(p, &floors[floor_idx].passengers, list) {
            seq_printf(m, " %c%d", get_type_char(p->type), p->dest_floor);
        }
        seq_printf(m, "\n");
    }

    seq_printf(m, "\nNumber of passengers: %d\n", elevator.num_passengers);
    seq_printf(m, "Number of passengers waiting: %d\n", elevator.total_waiting);
    seq_printf(m, "Number of passengers serviced: %d\n", elevator.total_serviced);

    mutex_unlock(&elevator_mutex);
    return 0;
}

static int elevator_proc_open(struct inode *inode, struct file *file) {
    return single_open(file, elevator_proc_show, NULL);
}

static const struct proc_ops elevator_fops = {
    .proc_open    = elevator_proc_open,
    .proc_read    = seq_read,
    .proc_lseek   = seq_lseek,
    .proc_release = single_release,
};

static int __init elevator_init(void) {
    int i;

    mutex_init(&elevator_mutex);

    elevator.state = OFFLINE;
    elevator.current_floor = 1;
    elevator.current_weight = 0;
    elevator.num_passengers = 0;
    elevator.total_serviced = 0;
    elevator.total_waiting = 0;
    elevator.stopping = 0;
    INIT_LIST_HEAD(&elevator.passengers);

    for (i = 0; i < MAX_FLOORS; i++) {
        INIT_LIST_HEAD(&floors[i].passengers);
        floors[i].num_passengers = 0;
    }

    STUB_start_elevator = start_elevator_fn;
    STUB_issue_request  = issue_request_fn;
    STUB_stop_elevator  = stop_elevator_fn;

    elevator.thread = kthread_run(elevator_thread_fn, NULL, "elevator_thread");
    if (IS_ERR(elevator.thread)) {
        printk(KERN_ERR "elevator: failed to create thread\n");
        return PTR_ERR(elevator.thread);
    }

    proc_entry = proc_create("elevator", 0, NULL, &elevator_fops);
    if (!proc_entry) {
        kthread_stop(elevator.thread);
        return -ENOMEM;
    }

    printk(KERN_INFO "elevator: module loaded\n");
    return 0;
}

static void __exit elevator_exit(void) {
    struct passenger *p, *tmp;
    int i;

    STUB_start_elevator = NULL;
    STUB_issue_request  = NULL;
    STUB_stop_elevator  = NULL;

    kthread_stop(elevator.thread);
    proc_remove(proc_entry);

    mutex_lock(&elevator_mutex);
    list_for_each_entry_safe(p, tmp, &elevator.passengers, list) {
        list_del(&p->list);
        kfree(p);
    }
    for (i = 0; i < MAX_FLOORS; i++) {
        list_for_each_entry_safe(p, tmp, &floors[i].passengers, list) {
            list_del(&p->list);
            kfree(p);
        }
    }
    mutex_unlock(&elevator_mutex);

    printk(KERN_INFO "elevator: module unloaded\n");
}

module_init(elevator_init);
module_exit(elevator_exit);
