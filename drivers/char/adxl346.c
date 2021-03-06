/*
 * adxl346.c
 *
 *  Created on: Sep 16, 2010
 *      Author: dakila
 */

#include <linux/module.h>
#include <linux/init.h>
#include <linux/cdev.h>
#include <linux/sched.h>
#include <linux/spi/spi.h>
#include <linux/gpio.h>
#include <linux/fs.h>
#include <linux/uaccess.h>

MODULE_DESCRIPTION("Analog Devices adxl346 3-axis Digital Accelerometer");
MODULE_LICENSE("GPL");
MODULE_AUTHOR("David Kiland <david.kiland@combitech.se>");


#define DEVID				0x00
#define THRESH_TAP			0x1d
#define OFSX				0x1e
#define OFSY				0x1f
#define OFSZ				0x20
#define DUR					0x21
#define LATENT				0x22
#define WINDOW				0x23
#define THRESH_ACT			0x24
#define THRESH_INACT		0x25
#define TIME_INAC			0x26
#define ACT_INACT_CTL		0x27
#define THRESH_FF			0x28
#define TIME_FF				0x29
#define TAP_AXES			0x2a
#define ACT_TAP_STATUS		0x2b
#define BW_RATE				0x2c
#define POWER_CTL			0x2d
#define INT_ENABLE			0x2e
#define INT_MAP				0x2f
#define INT_SOURCE			0x30
#define DATA_FORMAT			0x31
#define DATAX0				0x32
#define DATAX1				0x33
#define DATAY0				0x34
#define DATAY1				0x35
#define DATAZ0				0x36
#define DATAZ1				0x37
#define FIFO_CTL			0x38
#define FIFO_STATUS			0x39
#define TAP_SIGN			0x3a
#define ORIENT_CONF			0x3b

#define WRITE_SINGLE		0x00
#define WRITE_MULT			0x40
#define READ_SINGLE			0x80
#define READ_MULT			0xc0

struct adxl346_priv {
	struct spi_device *spi_dev;
	int cs_gpio;
	struct cdev cdev;
	struct device *device;
	dev_t dev;
};

static struct class *adxl_class = 0;



static int adxl346_read_id(struct adxl346_priv *priv)
{
	u8 tx[1];
	u8 rx[1];

	tx[0] = READ_SINGLE | DEVID;
	spi_write_then_read(priv->spi_dev, tx, 1, rx, 1);
	return rx[0];
}


static ssize_t adxl346_read(struct file *file, char __user *buf, size_t count, loff_t *ppos)
{
	u8 rx[6];
	u8 tx[1];

	struct adxl346_priv *priv = file->private_data;

	if(count < 6) {
		return 0;
	}

	tx[0] = READ_MULT | DATAX0;
	spi_write_then_read(priv->spi_dev, tx, 1, rx, 6);

	if(copy_to_user(buf, rx, 6)) {
		return 0;
	}

	return 6;
}

static ssize_t adxl346_write(struct file *file, const char __user *buf, size_t count, loff_t *ppos)
{
	struct adxl346_priv *priv = file->private_data;
	u8 tx[2];
	u8 inbuf[2];


	if(count != 2) {
		return -EINVAL;
	}

	if(copy_from_user(inbuf, buf, 2)) {
		return -ENOMEM;
	}

	tx[0] = WRITE_SINGLE | inbuf[0];
	tx[1] = inbuf[1];
	spi_write(priv->spi_dev, tx, 2);


	return count;
}


static int adxl346_open(struct inode *inode, struct file *file)
{
	struct adxl346_priv *priv = container_of(inode->i_cdev, struct adxl346_priv, cdev);

	file->private_data = priv;

	return 0;
}

static int adxl346_release(struct inode *inode, struct file *file)
{
	return 0;
}

static const struct file_operations adxl346_ops = {
	.owner		= THIS_MODULE,
	.read		= adxl346_read,
	.write		= adxl346_write,
	.open		= adxl346_open,
	.release	= adxl346_release,
};

static int __devinit adxl346_probe(struct spi_device *spi)
{
	struct adxl346_priv *priv;
	int err;
	int id;

	dev_info(&spi->dev, "Probing adxl346..\n");

	priv = kzalloc(sizeof(struct adxl346_priv), GFP_KERNEL);

	if (!priv) {
		err = -ENOMEM;
		goto exit_free;
	}

	/* Initialize spi and read device id */
	priv->spi_dev = spi;
	dev_set_drvdata(&spi->dev, priv);
	id = adxl346_read_id(priv);
	if(id != 0xe6) {
		dev_err(&spi->dev, "Invalid device id. (Read 0x%02x)", id);
		err = -EINVAL;
		goto exit_free;

	}


	alloc_chrdev_region(&priv->dev, 0, 1, "adxl");
	cdev_init(&priv->cdev, &adxl346_ops);
	priv->cdev.owner = THIS_MODULE;
	priv->cdev.ops = &adxl346_ops;

	cdev_add(&priv->cdev, priv->dev, 1);
	priv->device = device_create(adxl_class, NULL, priv->dev, NULL, "adxl346");

	dev_info(&spi->dev, "adxl346 device registered\n");

	return 0;

exit_free:
	kzfree(priv);
	return err;
}




static int __devexit adxl346_remove(struct spi_device *spi)
{
	struct adxl346_priv *priv = dev_get_drvdata(&spi->dev);

	device_del(priv->device);
	cdev_del(&priv->cdev);
	unregister_chrdev_region(priv->dev, 1);

	return 0;
}

static struct spi_driver adxl346_driver = {
	.probe	= adxl346_probe,
	.remove	= adxl346_remove,
	.driver = {
		.name	= "adxl346",
		.owner	= THIS_MODULE,
	},
};

static __init int adxl346_init_module(void)
{
	adxl_class = class_create(THIS_MODULE, "adxl");

	if(adxl_class == NULL) {
		printk("Could not create class counter\n");
		return -ENOMEM;
	}

	return spi_register_driver(&adxl346_driver);
}

static __exit void adxl346_cleanup_module(void)
{
	spi_unregister_driver(&adxl346_driver);
}

module_init(adxl346_init_module);
module_exit(adxl346_cleanup_module);

MODULE_ALIAS("spi:adxl346");
