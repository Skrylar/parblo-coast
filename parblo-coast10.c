/*
 *  Parblo Coast10 tablet monitor support
 *
 *  Copyright (c) 2017 Joshua "Skrylar" Cearley <joshua.cearley@gmail.com>
 *
 * Xing Wei's original Hanwang tablet driver was used as the
 * structural template for this driver, although the actual reverse
 * engineering and implementation was done by me.
 *
 * Available from
 * http://lxr.free-electrons.com/source/drivers/input/tablet/hanwang.c
 *
 */

/*
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
 *
 */

#include <linux/version.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/usb.h>
#include <linux/usb/input.h>

#include <uapi/linux/input-event-codes.h>

#define NAME "ParbloCoast10\0"
#define NAME_LEN 15

/** \brief USB vendor ID of the device. */
#define USB_VENDOR_PARBLO_COAST10 0x0B57

/** \brief USB product ID of the device. */
#define USB_DEVICE_PARBLO_COAST10 0x8534

/* XXX probably should check this from the packet descriptor, but meh */
#define PACKET_LENGTH 128

#define TOOL_NONE 0
#define TOOL_PEN 1
#define TOOL_ERASER 2

static const int hw_absevents[] = {
  ABS_X, ABS_Y, ABS_PRESSURE
};

static const int hw_buttons[] = {
  BTN_TOOL_PEN, BTN_LEFT, BTN_STYLUS, BTN_TOOL_RUBBER
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

  // cache X and Y positions of the pointer, needed so we don't throw the
  // cursor around when changing tool modes
  ushort x, y;
  ushort tool;
};

static void parblo_irq(struct urb *urb)
{
  struct parblo *parblo = urb->context;
  struct usb_device *dev = parblo->usbdev;
  struct input_dev* input_device = parblo->dev;
  int retval, p, c;

  switch (urb->status) {
  case 0:
    c = parblo->data[1];

    // c = 128 (no tool)
    // c = 160 (hovering)
    // c = 194 (set tool number)

    if (c == 194) {
      int toolcode = (parblo->data[2] << 8) + parblo->data[3];
      if (toolcode == 32) {
	parblo->tool = TOOL_PEN;
	input_report_key(input_device, BTN_TOOL_PEN, 1);
	input_report_key(input_device, BTN_TOOL_RUBBER, 0);
      } else {
	parblo->tool = TOOL_ERASER;
	input_report_key(input_device, BTN_TOOL_PEN, 0);
	input_report_key(input_device, BTN_TOOL_RUBBER, 1);
      }
      //input_event(input_device, EV_MSC, MSC_SERIAL, 0xffffffff);
    } // check if the pen is in the field
    else if (c >= 160 && c <= 165) {
      if (parblo->tool == TOOL_PEN) {
	input_report_key(input_device, BTN_TOOL_PEN, 1);
	input_report_key(input_device, BTN_TOOL_RUBBER, 0);
      } else {
	input_report_key(input_device, BTN_TOOL_PEN, 0);
	input_report_key(input_device, BTN_TOOL_RUBBER, 1);
      }

      input_report_key(input_device, BTN_LEFT, ((c & 0x1) > 0));
      input_report_key(input_device, BTN_STYLUS, ((c & 0x4) > 0));

      /* calculate the stuff */
      parblo->x = (parblo->data[2] << 8) + parblo->data[3];
      parblo->y = (parblo->data[4] << 8) + parblo->data[5];

      /* report the stuff */
      input_report_abs(input_device, ABS_X, parblo->x);
      input_report_abs(input_device, ABS_Y, parblo->y);

      if (c > 160) {
	p = (parblo->data[6] << 8) + parblo->data[7];
	input_report_abs(input_device, ABS_PRESSURE, p);
      } else {
	input_report_abs(input_device, ABS_PRESSURE, 0);
      }

      //input_report_abs(input_device, ABS_MISC, parblo->tool);
      //input_event(input_device, EV_MSC, MSC_SERIAL, 0xffffffff);
    } else {
      input_report_key(input_device, BTN_TOOL_PEN, 0);
      input_report_key(input_device, BTN_TOOL_RUBBER, 0);
      input_report_key(input_device, BTN_LEFT, 0);
      input_report_key(input_device, BTN_STYLUS, 0);
      input_report_abs(input_device, ABS_X, parblo->x);
      input_report_abs(input_device, ABS_Y, parblo->y);
      input_report_abs(input_device, ABS_PRESSURE, 0);
      //input_report_abs(input_device, ABS_MISC, 0);
      //input_event(input_device, EV_MSC, MSC_SERIAL, 0xffffffff);
    }

    //printk("C: %d X: %d Y: %d P: %d\n", c, x, y, p);

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
  //printk(KERN_ALERT "opened the door!\n");

  parblo->irq->dev = parblo->usbdev;
  if (usb_submit_urb(parblo->irq, GFP_KERNEL))
    return -EIO;

  //printk(KERN_ALERT "wabbajack!\n");
  return 0;
}

static void parblo_close(struct input_dev *dev)
{
  struct parblo *parblo = input_get_drvdata(dev);
  //printk(KERN_ALERT "closed the door!\n");

  usb_kill_urb(parblo->irq);
}

static int parblo_probe(struct usb_interface *dev, const struct usb_device_id *id)
{
  struct parblo* parblo;
  struct usb_endpoint_descriptor *endpoint;
  struct usb_device *devx = interface_to_usbdev(dev);
  struct input_dev* input_dev;
  int error, i;
  //printk(KERN_ALERT "klop!\n");

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

  __set_bit(EV_KEY, input_dev->evbit);
  __set_bit(EV_ABS, input_dev->evbit);

  for (i = 0; i < ARRAY_SIZE(hw_absevents); ++i)
    __set_bit(hw_absevents[i], input_dev->absbit);

  for (i = 0; i < ARRAY_SIZE(hw_buttons); ++i)
    __set_bit(hw_buttons[i], input_dev->keybit);

  __set_bit(MSC_SERIAL, input_dev->mscbit);

  input_set_abs_params(input_dev, ABS_X,
		       0, 10206, 4, 0);
  input_set_abs_params(input_dev, ABS_Y,
		       0, 7422, 4, 0);
  input_set_abs_params(input_dev, ABS_PRESSURE,
		       0, 65504, 0, 0);

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

  input_unregister_device(parblo->dev);

  usb_free_coherent(interface_to_usbdev(dev),
		    parblo->blen, parblo->data,
		    parblo->data_dma);
  kfree(parblo);
  usb_set_intfdata(dev, NULL);

  //printk(KERN_ALERT "no more salt\n");
}

static struct usb_driver parblo_driver = {
  .name = "parblo",
  .id_table = parblo_devices,
  .probe = parblo_probe,
  .disconnect = parblo_disconnect,
};
module_usb_driver(parblo_driver);

MODULE_AUTHOR("Joshua Cearley <joshua.cearley@gmail.com>");
MODULE_LICENSE("GPL");
MODULE_VERSION("1");
