// SPDX-License-Identifier: GPL-2.0-only
#include <linux/acpi.h>
#include <linux/debugfs.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/slab.h>
#include <linux/uaccess.h>
#include <linux/wmi.h>

#define MAX_CALL_INPUT 512
#define MAX_LAST_DUMP 65536

struct ilwmi_dev {
	struct wmi_device *wdev;
	struct dentry *dir;
	struct dentry *call_file;
	struct dentry *query_file;
	struct dentry *last_file;
	struct mutex lock;
	char last[MAX_LAST_DUMP];
	size_t last_len;
};

static struct dentry *ilwmi_root;

static void ilwmi_set_last(struct ilwmi_dev *idev, const char *fmt, ...)
{
	va_list ap;

	mutex_lock(&idev->lock);
	va_start(ap, fmt);
	idev->last_len = vscnprintf(idev->last, sizeof(idev->last), fmt, ap);
	va_end(ap);
	mutex_unlock(&idev->lock);
}

static void ilwmi_append_last(struct ilwmi_dev *idev, const char *fmt, ...)
{
	va_list ap;
	size_t off;

	mutex_lock(&idev->lock);
	off = idev->last_len;
	if (off < sizeof(idev->last)) {
		va_start(ap, fmt);
		idev->last_len += vscnprintf(idev->last + off,
					     sizeof(idev->last) - off, fmt, ap);
		va_end(ap);
	}
	mutex_unlock(&idev->lock);
}

static void ilwmi_mark_truncated(struct ilwmi_dev *idev)
{
	mutex_lock(&idev->lock);
	if (idev->last_len >= sizeof(idev->last) - 1) {
		static const char suffix[] = "\n[truncated]\n";
		size_t need = sizeof(suffix) - 1;
		size_t start;

		if (sizeof(idev->last) <= need + 1) {
			mutex_unlock(&idev->lock);
			return;
		}

		start = sizeof(idev->last) - 1 - need;
		memcpy(idev->last + start, suffix, need);
		idev->last[sizeof(idev->last) - 1] = '\0';
		idev->last_len = sizeof(idev->last) - 1;
	}
	mutex_unlock(&idev->lock);
}

static void ilwmi_dump_bytes(struct ilwmi_dev *idev, const u8 *buf, size_t len)
{
	size_t i;

	for (i = 0; i < len; i++) {
		ilwmi_append_last(idev, "%02x%s", buf[i],
				  (i + 1 == len) ? "" : " ");
	}
	ilwmi_append_last(idev, "\n");
	ilwmi_mark_truncated(idev);
}

static void ilwmi_dump_acpi_object(struct ilwmi_dev *idev, union acpi_object *obj)
{
	u32 i;

	if (!obj) {
		ilwmi_set_last(idev, "null\n");
		return;
	}

	switch (obj->type) {
	case ACPI_TYPE_INTEGER:
		ilwmi_set_last(idev, "type=integer value=0x%llx\n",
			       (unsigned long long)obj->integer.value);
		break;
	case ACPI_TYPE_BUFFER:
		ilwmi_set_last(idev, "type=buffer len=%u data=", obj->buffer.length);
		ilwmi_dump_bytes(idev, obj->buffer.pointer, obj->buffer.length);
		break;
	case ACPI_TYPE_STRING:
		ilwmi_set_last(idev, "type=string len=%u value=%s\n",
			       obj->string.length, obj->string.pointer);
		break;
	case ACPI_TYPE_PACKAGE:
		ilwmi_set_last(idev, "type=package count=%u\n", obj->package.count);
		for (i = 0; i < obj->package.count; i++) {
			union acpi_object *elem = &obj->package.elements[i];

			switch (elem->type) {
			case ACPI_TYPE_INTEGER:
				ilwmi_append_last(idev, "[%u] integer=0x%llx\n", i,
						  (unsigned long long)elem->integer.value);
				break;
			case ACPI_TYPE_BUFFER:
				ilwmi_append_last(idev, "[%u] buffer len=%u data=", i,
						  elem->buffer.length);
				ilwmi_dump_bytes(idev, elem->buffer.pointer,
						 elem->buffer.length);
				break;
			case ACPI_TYPE_STRING:
				ilwmi_append_last(idev, "[%u] string=%s\n", i,
						  elem->string.pointer);
				break;
			default:
				ilwmi_append_last(idev, "[%u] type=%u unsupported\n", i,
						  elem->type);
				break;
			}
		}
		break;
	default:
		ilwmi_set_last(idev, "type=%u unsupported\n", obj->type);
		break;
	}
}

static ssize_t ilwmi_last_read(struct file *file, char __user *buf, size_t len,
			       loff_t *ppos)
{
	struct ilwmi_dev *idev = file->private_data;
	ssize_t ret;

	mutex_lock(&idev->lock);
	ret = simple_read_from_buffer(buf, len, ppos, idev->last, idev->last_len);
	mutex_unlock(&idev->lock);

	return ret;
}

static const struct file_operations ilwmi_last_fops = {
	.owner = THIS_MODULE,
	.open = simple_open,
	.read = ilwmi_last_read,
	.llseek = default_llseek,
};

static int ilwmi_parse_hex_byte(const char *token, u8 *value)
{
	unsigned int tmp;
	int ret;

	ret = kstrtouint(token, 16, &tmp);
	if (ret)
		return ret;
	if (tmp > 0xff)
		return -ERANGE;
	*value = tmp;
	return 0;
}

static ssize_t ilwmi_call_write(struct file *file, const char __user *buf,
				size_t len, loff_t *ppos)
{
	struct ilwmi_dev *idev = file->private_data;
	char *kbuf, *cur, *tok;
	u8 instance;
	u32 method_id;
	u8 in_data[MAX_CALL_INPUT];
	size_t in_len = 0;
	unsigned int tmp;
	struct acpi_buffer in = { 0 };
	struct acpi_buffer out = { ACPI_ALLOCATE_BUFFER, NULL };
	acpi_status status;
	int ret;

	if (!len || len > PAGE_SIZE)
		return -EINVAL;

	kbuf = memdup_user_nul(buf, len);
	if (IS_ERR(kbuf))
		return PTR_ERR(kbuf);

	cur = strim(kbuf);
	tok = strsep(&cur, " \t\r\n");
	if (!tok || !*tok) {
		ret = -EINVAL;
		goto out_free;
	}
	ret = kstrtouint(tok, 0, &tmp);
	if (ret || tmp > 0xff) {
		ret = -EINVAL;
		goto out_free;
	}
	instance = tmp;

	tok = strsep(&cur, " \t\r\n");
	if (!tok || !*tok) {
		ret = -EINVAL;
		goto out_free;
	}
	ret = kstrtouint(tok, 0, &tmp);
	if (ret || tmp > 0xffffffffU) {
		ret = -EINVAL;
		goto out_free;
	}
	method_id = tmp;
	if (method_id > 0xffffffffU) {
		ret = -EINVAL;
		goto out_free;
	}

	while (cur && *cur) {
		tok = strsep(&cur, " \t\r\n");
		if (!tok || !*tok)
			continue;
		if (in_len >= sizeof(in_data)) {
			ret = -E2BIG;
			goto out_free;
		}
		ret = ilwmi_parse_hex_byte(tok, &in_data[in_len]);
		if (ret)
			goto out_free;
		in_len++;
	}

	in.length = in_len;
	in.pointer = in_len ? in_data : NULL;

	status = wmidev_evaluate_method(idev->wdev, instance, method_id,
					&in, &out);
	if (ACPI_FAILURE(status)) {
		ilwmi_set_last(idev,
			       "call failed status=0x%x instance=%u method=0x%02x in_len=%zu\n",
			       status, instance, method_id, in_len);
		ret = -EIO;
		goto out_freebuf;
	}

	ilwmi_set_last(idev,
		       "call ok instance=%u method=0x%02x in_len=%zu out_len=%zu\n",
		       instance, method_id, in_len, out.length);
	ilwmi_dump_acpi_object(idev, out.pointer);
	ret = len;

out_freebuf:
	kfree(out.pointer);
out_free:
	kfree(kbuf);
	return ret;
}

static const struct file_operations ilwmi_call_fops = {
	.owner = THIS_MODULE,
	.open = simple_open,
	.write = ilwmi_call_write,
	.llseek = default_llseek,
};

static ssize_t ilwmi_query_write(struct file *file, const char __user *buf,
				 size_t len, loff_t *ppos)
{
	struct ilwmi_dev *idev = file->private_data;
	char kbuf[32];
	unsigned int instance;
	union acpi_object *obj;
	int ret;

	if (!len || len >= sizeof(kbuf))
		return -EINVAL;

	if (copy_from_user(kbuf, buf, len))
		return -EFAULT;
	kbuf[len] = '\0';

	ret = kstrtouint(strim(kbuf), 0, &instance);
	if (ret || instance > 0xff)
		return -EINVAL;

	obj = wmidev_block_query(idev->wdev, instance);
	if (!obj) {
		ilwmi_set_last(idev, "query failed instance=%u\n", instance);
		return -EIO;
	}

	ilwmi_set_last(idev, "query ok instance=%u\n", instance);
	ilwmi_dump_acpi_object(idev, obj);
	kfree(obj);

	return len;
}

static const struct file_operations ilwmi_query_fops = {
	.owner = THIS_MODULE,
	.open = simple_open,
	.write = ilwmi_query_write,
	.llseek = default_llseek,
};

static int ilwmi_probe(struct wmi_device *wdev, const void *context)
{
	struct ilwmi_dev *idev;
	const char *name = dev_name(&wdev->dev);

	idev = devm_kzalloc(&wdev->dev, sizeof(*idev), GFP_KERNEL);
	if (!idev)
		return -ENOMEM;

	idev->wdev = wdev;
	mutex_init(&idev->lock);
	ilwmi_set_last(idev, "ready\n");

	if (ilwmi_root) {
		idev->dir = debugfs_create_dir(name, ilwmi_root);
		debugfs_create_file("call", 0200, idev->dir, idev,
				    &ilwmi_call_fops);
		debugfs_create_file("query", 0200, idev->dir, idev,
				    &ilwmi_query_fops);
		debugfs_create_file("last", 0400, idev->dir, idev,
				    &ilwmi_last_fops);
	}

	dev_set_drvdata(&wdev->dev, idev);
	dev_info(&wdev->dev, "intel_lightbar_wmi bound\n");
	return 0;
}

static void ilwmi_remove(struct wmi_device *wdev)
{
	struct ilwmi_dev *idev = dev_get_drvdata(&wdev->dev);

	debugfs_remove_recursive(idev->dir);
}

static const struct wmi_device_id ilwmi_ids[] = {
	{ "8C5DA44C-CDC3-46B3-8619-4E26D34390B7", NULL },
	{ "F3517D45-0E66-41EF-8472-FCB7C98AE932", NULL },
	{ "2BC49DEF-7B15-4F05-8BB7-EE37B9547C0B", NULL },
	{ "1F13AB7F-6220-4210-8F8E-8BB5E71EE969", NULL },
	{ "05901221-D566-11D1-B2F0-00A0C9062910", NULL },
	{ }
};
MODULE_DEVICE_TABLE(wmi, ilwmi_ids);

static struct wmi_driver ilwmi_driver = {
	.driver = {
		.name = "intel_lightbar_wmi",
	},
	.id_table = ilwmi_ids,
	.probe = ilwmi_probe,
	.remove = ilwmi_remove,
	.no_singleton = true,
};

static int __init ilwmi_init(void)
{
	ilwmi_root = debugfs_create_dir("intel_lightbar_wmi", NULL);
	return wmi_driver_register(&ilwmi_driver);
}

static void __exit ilwmi_exit(void)
{
	wmi_driver_unregister(&ilwmi_driver);
	debugfs_remove_recursive(ilwmi_root);
}

module_init(ilwmi_init);
module_exit(ilwmi_exit);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("Debug WMI helper for Intel/Uniwill lightbar reverse engineering");
MODULE_AUTHOR("vap-tech contributors");
