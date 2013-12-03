#include "libcflat.h"
#include "libio.h"
#include "iomaps.h"
#include "heap.h"
#include "virtio.h"

#define to_virtio_mmio_dev(vdev) \
	container_of(vdev, struct virtio_mmio_dev, vdev)

static void vm_get(struct virtio_dev *vdev, unsigned offset,
		   void *buf, unsigned len)
{
	struct virtio_mmio_dev *vmdev = to_virtio_mmio_dev(vdev);
	void *addr = vmdev->base + VIRTIO_MMIO_CONFIG + offset;
	read_len(addr, buf, len);
}

static void vm_set(struct virtio_dev *vdev, unsigned offset,
		   const void *buf, unsigned len)
{
	struct virtio_mmio_dev *vmdev = to_virtio_mmio_dev(vdev);
	void *addr = vmdev->base + VIRTIO_MMIO_CONFIG + offset;
	write_len(addr, buf, len);
}

static struct virtio_dev *virtio_mmio_bind(const struct iomap *m, u32 device)
{
	struct virtio_mmio_dev *vmdev;
	void *page;
	u32 devid, i;

	page = alloc_page();
	vmdev = page;
	vmdev->vdev.config = page + sizeof(struct virtio_mmio_dev);

	vmdev->vdev.id.device = device;
	vmdev->vdev.id.vendor = -1;
	vmdev->vdev.config->get = vm_get;
	vmdev->vdev.config->set = vm_set;

	device &= 0xffff;

	for (i = 0; i < m->nr; ++i) {
		vmdev->base = compat_ptr(m->addrs[i]);
		devid = readl(vmdev->base + VIRTIO_MMIO_DEVICE_ID);
		if ((devid & 0xffff) == device)
			break;
	}

	if (i >= m->nr) {
		printf("%s: Can't find device 0x%x.\n", __func__, device);
		free_page(page);
		return NULL;
	}

	return &vmdev->vdev;
}

struct virtio_dev *virtio_bind(u32 device)
{
	const struct iomap *m;

	/* currently we only support virtio-mmio */
	m = iomaps_find_compatible("virtio,mmio");
	if (!m) {
		printf("%s: No virtio-mmio transports found!\n", __func__);
		return NULL;
	}
	return virtio_mmio_bind(m, device);
}
