
#include <linux/version.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/usb.h>
#include <linux/usb/input.h>

#define NAME "ParbloCoast10\0"
#define NAME_LEN 15

/** \brief USB vendor ID of the device. */
#define USB_VENDOR_PARBLO_COAST10 0x0B57

/** \brief USB product ID of the device. */
#define USB_DEVICE_PARBLO_COAST10 0x8534

/* XXX probably should check this from the packet descriptor, but meh */
#define PACKET_LENGTH 128

static const int hw_absevents[] = {
  ABS_X, ABS_Y, ABS_PRESSURE,
};

static const struct usb_device_id parblo_devices[] = {
  { USB_DEVICE(USB_VENDOR_PARBLO_COAST10, USB_DEVICE_PARBLO_COAST10) },
  {}
};
MODULE_DEVICE_TABLE(usb, parblo_devices);

struct parblo {
  struct urb *irq;
  struct input_dev *dev;
  unsigned char *data;
  dma_addr_t data_dma;
  struct usb_device *usbdev;
  char name[32];
  char phys[32];
  int blen;
};

static void parblo_irq(struct urb *urb)
{
  struct parblo *parblo = urb->context;
  struct usb_device *dev = parblo->usbdev;
  int retval, x, y, p, c;

  switch (urb->status) {
  case 0:
    c = parblo->data[1];
    x = (parblo->data[2] << 8) + parblo->data[3];
    y = (parblo->data[4] << 8) + parblo->data[5];
    p = (parblo->data[6] << 8) + parblo->data[7];
    printk("C: %d X: %d Y: %d P: %d\n", c, x, y, p);

    input_sync(parblo->dev);
    break;

  case -ECONNRESET: case -ENOENT: case -ESHUTDOWN:
    /* this urb is terminated, clean up */
    dev_err(&dev->dev, "%s - urb shutting down with status: %d",
	    __func__, urb->status);
    return;

  default:
    dev_err(&dev->dev, "%s - nonzero urb status received: %d",
	    __func__, urb->status);
    break;
  }

  retval = usb_submit_urb(urb, GFP_ATOMIC);
  if (retval)
    dev_err(&dev->dev, "%s - usb_submit_urb failed with result %d",
	    __func__, retval);
}

static int parblo_open(struct input_dev *dev)
{
  struct parblo *parblo = input_get_drvdata(dev);
  printk(KERN_ALERT "opened the door!\n");

  parblo->irq->dev = parblo->usbdev;
  if (usb_submit_urb(parblo->irq, GFP_KERNEL))
    return -EIO;

  printk(KERN_ALERT "wabbajack!\n");
  return 0;
}

static void parblo_close(struct input_dev *dev)
{
  struct parblo *parblo = input_get_drvdata(dev);
  printk(KERN_ALERT "closed the door!\n");

  usb_kill_urb(parblo->irq);
}

static int parblo_probe(struct usb_interface *dev, const struct usb_device_id *id)
{
  struct parblo* parblo;
  struct usb_endpoint_descriptor *endpoint;
  struct usb_device *devx = interface_to_usbdev(dev);
  struct input_dev* input_dev;
  int error, i;
  printk(KERN_ALERT "klop!\n");

  parblo = kzalloc(sizeof(struct parblo), GFP_KERNEL);
  input_dev = input_allocate_device();
  if (!parblo || !input_dev) {
    error = -ENOMEM;
    goto fail;
  }

  endpoint = &dev->cur_altsetting->endpoint[0].desc;
  parblo->blen = endpoint->wMaxPacketSize;
  parblo->data = usb_alloc_coherent(devx, endpoint->wMaxPacketSize, GFP_KERNEL, &parblo->data_dma);
  if (!parblo->data) {
    error = -ENOMEM;
    goto fail2;
  }

  parblo->irq = usb_alloc_urb(0, GFP_KERNEL);
  if (!parblo->irq) {
    error = -ENOMEM;
    goto fail3;
  }

  parblo->usbdev = devx;
  parblo->dev = input_dev;

  usb_make_path(devx, parblo->phys, sizeof(parblo->phys));
  strlcat(parblo->phys, "/input0", sizeof(parblo->phys));
  strlcpy(parblo->name, NAME, NAME_LEN);
  input_dev->name = parblo->name;
  input_dev->phys = parblo->phys;
  usb_to_input_id(devx, &input_dev->id);
  input_dev->dev.parent = &dev->dev;

  input_set_drvdata(input_dev, parblo);

  input_dev->open = parblo_open;
  input_dev->close = parblo_close;

  for (i = 0; i < ARRAY_SIZE(hw_absevents); ++i)
    __set_bit(hw_absevents[i], input_dev->absbit);

  input_set_abs_params(input_dev, ABS_X,
		       0, 10206, 4, 0);
  input_set_abs_params(input_dev, ABS_Y,
		       0, 7422, 4, 0);
  input_set_abs_params(input_dev, ABS_PRESSURE,
		       32, 65504, 4, 0);

  usb_fill_int_urb(parblo->irq, devx,
		   usb_rcvintpipe(devx, endpoint->bEndpointAddress),
		   parblo->data, parblo->blen,
		   parblo_irq, parblo, endpoint->bInterval);

  parblo->irq->transfer_dma = parblo->data_dma;
  parblo->irq->transfer_flags |= URB_NO_TRANSFER_DMA_MAP;

  error = input_register_device(parblo->dev);
  if (error)
    goto fail3;

  usb_set_intfdata(dev, parblo);

  return 0;

 fail3:
  usb_free_urb(parblo->irq);
 fail2:
  kfree(parblo);
 fail:
  return error;
}

static void parblo_disconnect(struct usb_interface *dev)
{
  struct parblo* parblo;
  parblo = usb_get_intfdata(dev);

  usb_free_coherent(interface_to_usbdev(dev),
		    parblo->blen, parblo->data,
		    parblo->data_dma);
  kfree(parblo);
  usb_set_intfdata(dev, NULL);

  printk(KERN_ALERT "no more salt\n");
}

static struct usb_driver parblo_driver = {
  .name = "parblo",
  .id_table = parblo_devices,
  .probe = parblo_probe,
  .disconnect = parblo_disconnect,
};
module_usb_driver(parblo_driver);

MODULE_AUTHOR("Joshua Cearley <joshua.cearley@gmail.com>");
/*MODULE_AUTHOR("Xing Wei <weixing@hanwang.com.cn>")*/
MODULE_LICENSE("GPL");
MODULE_VERSION("1");
