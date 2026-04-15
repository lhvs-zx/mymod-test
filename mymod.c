 
#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/netdevice.h>
#include <linux/inetdevice.h>
#include <net/arp.h>
#include <linux/proc_fs.h>
#include <linux/uaccess.h>
#include <linux/string.h>

#define BUF_SIZE 255

static struct net_device *my_dev;
static struct proc_dir_entry *mypfile;
static struct in_ifaddr *ifa;
static char bifa[4]={192,168,15,1};

static ssize_t myproc_write(struct file *file, const char __user *ubuf, size_t count, loff_t *ppos);
static int mymod_dev_init(struct net_device *dev);
static int mymod_open(struct net_device *dev);
static int mymod_close(struct net_device *dev);
static int mymod_ioctl(struct net_device *dev, struct ifreq *ifr, int cmd);
static netdev_tx_t mymod_xmit(struct sk_buff *skb, struct net_device *dev);

static struct mymod_priv
{
    int kol;
};

static struct net_device_ops mymod_ops =
{
    .ndo_init = mymod_dev_init,
    .ndo_open = mymod_open,
    .ndo_stop = mymod_close,
    .ndo_start_xmit   = mymod_xmit,
    .ndo_do_ioctl = mymod_ioctl
};
static const struct proc_ops my_props = {
    .proc_write = myproc_write,
};
static ssize_t myproc_write(struct file *file, const char __user *ubuf, size_t count, loff_t *ppos) {
    char kernel_buffer[BUF_SIZE];
    if (count > BUF_SIZE) count = BUF_SIZE;


    if (copy_from_user(kernel_buffer, ubuf, count)) {
        return -EFAULT;
    }

    kernel_buffer[count-1] = '\0'; 

    char tb[4];
    int len;
    int rez;
    char *str1,*str2;
    str1=strstr(kernel_buffer,".");
    if (str1==0)return count;
    len=str1-kernel_buffer;
    if(len>0&&len<4){
        memcpy(tb,kernel_buffer,len);
        tb[len]=0;
        kstrtouint(tb,10,&rez);
        bifa[0]=rez;
    }else return count; 
    str1++;
    str2=strstr(str1,".");
    if (str2==0)return count;
    len=str2-str1;  
    if(len>0&&len<4){
        memcpy(tb,str1,len);
        tb[len]=0;
        kstrtouint(tb,10,&rez);
        bifa[1]=rez;
    }else return count;     
    str2++;
    str1=strstr(str2,".");
    if (str1==0)return count;
    len=str1-str2;  
    if(len>0&&len<4){
        memcpy(tb,str2,len);
        tb[len]=0;
        kstrtouint(tb,10,&rez);
        bifa[2]=rez;
    }else return count;
    str1++;
    len=strlen(str1);  
    if(len>0&&len<4){
        memcpy(tb,str1,len);
        tb[len]=0;
        kstrtouint(tb,10,&rez);
        bifa[3]=rez;
    }

    return count;    
    
}
static int mymod_dev_init(struct net_device *dev){

    return 0;
}
static int mymod_open(struct net_device *dev){
  
    struct in_device *in_dev;
    in_dev = __in_dev_get_rtnl(dev);
    if (!in_dev) return 0;
    memcpy(&ifa->ifa_address,bifa,4);
    memcpy(&ifa->ifa_local,bifa,4);
    ifa->ifa_mask = 0x00ffffff;
    ifa->ifa_dev = in_dev;
    ifa->ifa_scope = RT_SCOPE_UNIVERSE;
    strcpy(ifa->ifa_label, dev->name);
    dev->ip_ptr->ifa_list=ifa;
    memcpy(dev->dev_addr, "\0a2340", ETH_ALEN);
    return 0;
}
static int mymod_close(struct net_device *dev){
 
    return 0;
}
static int mymod_ioctl(struct net_device *dev, struct ifreq *ifr, int cmd){
 
    return 0;
}
static netdev_tx_t mymod_xmit(struct sk_buff *skb, struct net_device *dev)
{

    return NETDEV_TX_OK;
}

static int __init mymodi(void) {

    ifa = kzalloc(sizeof(struct in_ifaddr), GFP_KERNEL);
    if (!ifa) return 0;

    my_dev = alloc_netdev_mqs(sizeof(struct mymod_priv), "my_dev",0, ether_setup,1,1);
    if (my_dev == NULL)
        return -ENOMEM;

    my_dev->netdev_ops = &mymod_ops;
    mypfile=proc_create("mymod", 0666, NULL, &my_props);
    if (mypfile == NULL)
        return -ENOMEM;
    register_netdev(my_dev);
    return 0;
}
static void __exit mymode(void) {
    unregister_netdev(my_dev);
    proc_remove(mypfile);
    free_netdev(my_dev);
   
}
module_init(mymodi);
module_exit(mymode);
MODULE_LICENSE("GPL");
MODULE_AUTHOR("lhVS");
MODULE_DESCRIPTION("virtual");
MODULE_VERSION("0.01");
