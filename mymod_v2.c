 
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/netdevice.h>
#include <linux/etherdevice.h>
#include <linux/rtnetlink.h>
#include <net/rtnetlink.h>
#include <linux/inetdevice.h>
#include <linux/inet.h>
#include <linux/proc_fs.h>
#include <linux/uaccess.h>
#include <linux/string.h>

#define BUF_SIZE 255

static struct net_device *my_dev;
static struct proc_dir_entry *mypfile;

static struct socket  * sk;




static ssize_t myproc_write(struct file *file, const char __user *ubuf, size_t count, loff_t *ppos);

static void link_setup(struct net_device *dev);
static int link_validate(struct nlattr *tb[], struct nlattr *data[],struct netlink_ext_ack *extack);

static int mymod_dev_init(struct net_device *dev);
static int mymod_open(struct net_device *dev);
static int mymod_close(struct net_device *dev);
static netdev_tx_t mymod_xmit(struct sk_buff *skb, struct net_device *dev);
static int __net_init rtnetlink_net_init(struct net *net);
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
    .ndo_validate_addr = eth_validate_addr,
 
};



static struct rtnl_link_ops mymod_link_ops __read_mostly = {
	.kind		= "mymod",
	.setup		= link_setup,
	.validate	= link_validate,
};


static const struct proc_ops my_props = {
    .proc_write = myproc_write,
};
static void nl_recv_msg(struct sk_buff *skb) {
    // Process received netlink message here
}
static struct netlink_kernel_cfg cfg = {
    .groups		= 0,
    .flags		= 0,
    
};



void addattr_l(struct nlmsghdr *n, int maxlen, int type, const void *data, int alen) {
    int len = RTA_LENGTH(alen);
    struct rtattr *rta = (struct rtattr *)(((char *)n) + NLMSG_ALIGN(n->nlmsg_len));
    rta->rta_type = type;
    rta->rta_len = len;
    memcpy(RTA_DATA(rta), data, alen);
    n->nlmsg_len = NLMSG_ALIGN(n->nlmsg_len) + RTA_ALIGN(len);
}


void insert_ip(char *madr){
    struct {
        struct nlmsghdr n;
        struct ifaddrmsg ifa;
        char buf[1024];
    } req;
    struct sockaddr_nl nladdr;
 
   
    memset(&nladdr, 0, sizeof(nladdr));
	nladdr.nl_family = AF_NETLINK;

    struct msghdr msg;
    struct kvec iov;

    memset(&msg, 0, sizeof(msg));
    msg.msg_name = &nladdr;           // Destination address
    msg.msg_namelen = sizeof(struct sockaddr_nl);
    msg.msg_flags = 0;

    /**/
    memset(&req, 0, sizeof(req));
    req.n.nlmsg_len = NLMSG_LENGTH(sizeof(struct ifaddrmsg));
    req.n.nlmsg_flags = NLM_F_REQUEST | NLM_F_CREATE | NLM_F_EXCL | NLM_F_ACK;
    req.n.nlmsg_type = RTM_NEWADDR;

    req.ifa.ifa_family = AF_INET;
    req.ifa.ifa_prefixlen = 24;
    req.ifa.ifa_index = my_dev->ifindex;

    __be32 addr = in_aton(madr);
    unsigned char *uc=(unsigned char*)&addr;
    addattr_l(&req.n, sizeof(req), IFA_LOCAL, &addr, 4);
    addattr_l(&req.n, sizeof(req), IFA_ADDRESS, &addr, 4);
	uc[3]=255;
    addattr_l(&req.n, sizeof(req), IFA_BROADCAST, &addr, 4);
    
    
    iov.iov_base = &req;
    iov.iov_len = req.n.nlmsg_len;

    

    int err = kernel_sendmsg(sk, &msg, &iov, 1, req.n.nlmsg_len);
    if (!err)
	{
		printk( ": send socket, error = %d\n", err);
    }

        
}

void del_ip(void){
    static struct in_ifaddr *ifa;
    ifa=my_dev->ip_ptr->ifa_list;
    if(!ifa)return ;
    printk( "ifa = %x\n", ifa->ifa_local);
    struct {
        struct nlmsghdr n;
        struct ifaddrmsg ifa;
        char buf[1024];
    } req;
    struct sockaddr_nl nladdr;
 
   
    memset(&nladdr, 0, sizeof(nladdr));
	nladdr.nl_family = AF_NETLINK;

    struct msghdr msg;
    struct kvec iov;

    memset(&msg, 0, sizeof(msg));
    msg.msg_name = &nladdr;           // Destination address
    msg.msg_namelen = sizeof(struct sockaddr_nl);
    msg.msg_flags = 0;

    /**/
    memset(&req, 0, sizeof(req));
    req.n.nlmsg_len = NLMSG_LENGTH(sizeof(struct ifaddrmsg));
    req.n.nlmsg_flags = NLM_F_REQUEST | NLM_F_ACK;
    req.n.nlmsg_type = RTM_DELADDR;

    req.ifa.ifa_family = AF_INET;
    req.ifa.ifa_prefixlen = 24;
    req.ifa.ifa_index = my_dev->ifindex;

    
    struct rtattr* rta = (struct rtattr*)(((char*)&req) + NLMSG_ALIGN(req.n.nlmsg_len));
    rta->rta_type = IFA_LOCAL;
    rta->rta_len = RTA_LENGTH(4); // IPv4 size
    memcpy(RTA_DATA(rta), &ifa->ifa_local, 4);
    req.n.nlmsg_len = NLMSG_ALIGN(req.n.nlmsg_len) + RTA_LENGTH(4);
    
     
    iov.iov_base = &req;
    iov.iov_len = req.n.nlmsg_len;

    

    int err = kernel_sendmsg(sk, &msg, &iov, 1, req.n.nlmsg_len);
    if (!err)
	{
		printk( ": send socket, error = %d\n", err);
    }
 
    
    
    
    return;
}

void downup_if(int m){
    
    struct {
       struct nlmsghdr n;
       struct ifinfomsg ifi;
        char buf[1024];
    } req;
    struct sockaddr_nl nladdr;
 
   
    memset(&nladdr, 0, sizeof(nladdr));
	nladdr.nl_family = AF_NETLINK;

    struct msghdr msg;
    struct kvec iov;

    memset(&msg, 0, sizeof(msg));
    msg.msg_name = &nladdr;           // Destination address
    msg.msg_namelen = sizeof(struct sockaddr_nl);
    msg.msg_flags = 0;

    /**/
    memset(&req, 0, sizeof(req));
    req.n.nlmsg_len = NLMSG_LENGTH(sizeof(struct ifinfomsg));
    req.n.nlmsg_flags = NLM_F_REQUEST | NLM_F_ACK;;
    req.n.nlmsg_type = RTM_NEWLINK;

    req.ifi.ifi_family = AF_UNSPEC;
    if(m==1){
        req.ifi.ifi_change = IFF_UP; // We are changing the UP flag
        req.ifi.ifi_flags = IFF_UP;  // Set UP flag
    }else {
        req.ifi.ifi_change = IFF_UP; // We are changing the UP flag
        req.ifi.ifi_flags = 0;  // Set UP flag
    }
    req.ifi.ifi_index = my_dev->ifindex;
    
    struct rtattr *rta = (struct rtattr *)(((char *)&req) + NLMSG_ALIGN(req.n.nlmsg_len));
    rta->rta_type = IFLA_IFNAME;
    rta->rta_len = RTA_LENGTH(6);
    memcpy(RTA_DATA(rta), "my_dev", 6);
    req.n.nlmsg_len = NLMSG_ALIGN(req.n.nlmsg_len) + RTA_LENGTH(6);

     
    iov.iov_base = &req;
    iov.iov_len = req.n.nlmsg_len;
    
    int err = kernel_sendmsg(sk, &msg, &iov, 1, req.n.nlmsg_len);
    if (!err)
	{
		printk( ": send socket, error = %d\n", err);
    }

    return;
    
}




void change_ip_address(char *madr) {
    
    downup_if(0);
    del_ip();
    insert_ip(madr);
    downup_if(1);
    printk(KERN_INFO "change %s\n",madr);
    return;
 

}




static ssize_t myproc_write(struct file *file, const char __user *ubuf, size_t count, loff_t *ppos) {
       char kernel_buffer[BUF_SIZE];
    if (count > BUF_SIZE) count = BUF_SIZE;


    if (copy_from_user(kernel_buffer, ubuf, count)) {
        return -EFAULT;
    }

    kernel_buffer[count-1] = '\0'; 
    change_ip_address(kernel_buffer);
    return count;  
}


static int mymod_dev_init(struct net_device *dev){

    return 0;
}
static int mymod_open(struct net_device *dev){

    netif_tx_start_all_queues(dev);
	return 0;
}
static int mymod_close(struct net_device *dev){
  	netif_tx_stop_all_queues(dev);
	return 0;  
    
}
static netdev_tx_t mymod_xmit(struct sk_buff *skb, struct net_device *dev)
{

    dev_kfree_skb(skb);
    return NETDEV_TX_OK;
}

static void link_setup(struct net_device *dev)
{
    unsigned char *ubk;
	ether_setup(dev);
    dev->netdev_ops = &mymod_ops;

    eth_hw_addr_random(dev);
    
}

static int link_validate(struct nlattr *tb[], struct nlattr *data[],
			  struct netlink_ext_ack *extack)
{
	if (tb[IFLA_ADDRESS]) {
		if (nla_len(tb[IFLA_ADDRESS]) != ETH_ALEN)
			return -EINVAL;
		if (!is_valid_ether_addr(nla_data(tb[IFLA_ADDRESS])))
			return -EADDRNOTAVAIL;
	}
	return 0;
}




static int __init mymodi(void) {
  
    rtnl_link_register(&mymod_link_ops);
    my_dev = alloc_netdev(sizeof(struct mymod_priv), "my_dev",NET_NAME_UNKNOWN, link_setup);
    if (my_dev == NULL)
        return -ENOMEM;
    my_dev->rtnl_link_ops = &mymod_link_ops;

    mypfile=proc_create("mymod", 0666, NULL, &my_props);
    if (mypfile == NULL)
        return -ENOMEM;
    register_netdev(my_dev);

    int err = 0;
    
	err = sock_create(AF_NETLINK, SOCK_RAW, NETLINK_ROUTE, &sk);
	if (err)
	{
		printk( ":socket, error = %d\n", -ENXIO);
		return 0;
	}

    return 0;
}
static void __exit mymode(void) {
    
    
   
    rtnl_link_unregister(&mymod_link_ops);    
    proc_remove(mypfile);
    free_netdev(my_dev);
    sock_release(sk);
   
}
module_init(mymodi);
module_exit(mymode);
MODULE_LICENSE("GPL");
MODULE_AUTHOR("lhVS");
MODULE_DESCRIPTION("virtual");
MODULE_VERSION("0.01");
