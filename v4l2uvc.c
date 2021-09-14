#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <fcntl.h>
#include <linux/videodev2.h>
#include <sys/mman.h>
#include <sys/ioctl.h>
#include "v4l2uvc.h"

static int debug = 0;

static int init_v4l2(struct vdIn *vd);

int init_videoIn(struct vdIn *vd, char *device, int width, int height)
{
    if (vd == NULL || device == NULL)
    {
        return -1;
    }
    if (width == 0 || height == 0)
    {
        return -1;
    }
    vd->videodevice = NULL;
    vd->status = NULL;
    vd->pictName = NULL;
    vd->videodevice = (char *) calloc(1, 16 * sizeof(char));
    vd->status = (char *) calloc(1, 100 * sizeof(char));
    vd->pictName = (char *) calloc(1, 80 * sizeof(char));
    snprintf(vd->videodevice, 12, "%s", device);
    vd->width = width;
    vd->height = height;
    vd->formatIn = V4L2_PIX_FMT_MJPEG;
    vd->grabmethod = 1;//mmap by default
    if (init_v4l2(vd) < 0)
    {
        fprintf(stderr, " Init v4L2 failed !! exit fatal \n");
        goto error;;
    }
    /* alloc a temp buffer to reconstruct the pict */
    vd->framesizeIn = (vd->width * vd->height << 1);
    switch (vd->formatIn)
    {
        case V4L2_PIX_FMT_MJPEG:
            vd->tmpbuffer = (unsigned char *) calloc(1, (size_t) vd->framesizeIn);
            if (!vd->tmpbuffer)
            {
                goto error;
            }
            vd->framebuffer = (unsigned char *) calloc(1, (size_t) vd->width * (vd->height + 8) * 2);
            break;
        default:
            fprintf(stderr, " should never arrive exit fatal !!\n");
            goto error;
            break;
    }
    if (!vd->framebuffer)
    {
        goto error;
    }
    return 0;
    error:
    free(vd->videodevice);
    free(vd->status);
    free(vd->pictName);
    close(vd->fd);
    return -1;
}

static int init_v4l2(struct vdIn *vd)
{
    int i;
    int ret = 0;

    if ((vd->fd = open(vd->videodevice, O_RDWR)) == -1)
    {
        perror("ERROR opening V4L interface \n");
        exit(1);
    }
    memset(&vd->cap, 0, sizeof(struct v4l2_capability));
    ret = ioctl(vd->fd, VIDIOC_QUERYCAP, &vd->cap);
    if (ret < 0)
    {
        fprintf(stderr, "Error opening device %s: unable to query device.\n", vd->videodevice);
        goto fatal;
    }

    if ((vd->cap.capabilities & V4L2_CAP_VIDEO_CAPTURE) == 0)
    {
        fprintf(stderr, "Error opening device %s: video capture not supported.\n", vd->videodevice);
        goto fatal;;
    }
    if (vd->grabmethod)
    {
        if (!(vd->cap.capabilities & V4L2_CAP_STREAMING))
        {
            fprintf(stderr, "%s does not support streaming i/o\n", vd->videodevice);
            goto fatal;
        }
    }
    else
    {
        if (!(vd->cap.capabilities & V4L2_CAP_READWRITE))
        {
            fprintf(stderr, "%s does not support read i/o\n", vd->videodevice);
            goto fatal;
        }
    }
    /* set format in */
    memset(&vd->fmt, 0, sizeof(struct v4l2_format));
    vd->fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    vd->fmt.fmt.pix.width = vd->width;
    vd->fmt.fmt.pix.height = vd->height;
    vd->fmt.fmt.pix.pixelformat = vd->formatIn;
    vd->fmt.fmt.pix.field = V4L2_FIELD_ANY;
    ret = ioctl(vd->fd, VIDIOC_S_FMT, &vd->fmt);
    if (ret < 0)
    {
        fprintf(stderr, "Unable to set format: %d.\n", errno);
        goto fatal;
    }
    if ((vd->fmt.fmt.pix.width != vd->width) || (vd->fmt.fmt.pix.height != vd->height))
    {
        fprintf(stderr, " format asked unavailable get width %d height %d \n", vd->fmt.fmt.pix.width, vd->fmt.fmt.pix.height);
        vd->width = vd->fmt.fmt.pix.width;
        vd->height = vd->fmt.fmt.pix.height;
        /* look the format is not part of the deal ??? */
        //vd->formatIn = vd->fmt.fmt.pix.pixelformat;
    }
    /* request buffers */
    memset(&vd->rb, 0, sizeof(struct v4l2_requestbuffers));
    vd->rb.count = NB_BUFFER;
    vd->rb.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    vd->rb.memory = V4L2_MEMORY_MMAP;

    ret = ioctl(vd->fd, VIDIOC_REQBUFS, &vd->rb);
    if (ret < 0)
    {
        fprintf(stderr, "Unable to allocate buffers: %d.\n", errno);
        goto fatal;
    }
    /* map the buffers */
    for (i = 0; i < NB_BUFFER; i++)
    {
        memset(&vd->buf, 0, sizeof(struct v4l2_buffer));
        vd->buf.index = i;
        vd->buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        vd->buf.memory = V4L2_MEMORY_MMAP;
        ret = ioctl(vd->fd, VIDIOC_QUERYBUF, &vd->buf);
        if (ret < 0)
        {
            fprintf(stderr, "Unable to query buffer (%d).\n", errno);
            goto fatal;
        }
        if (debug)
        {
            fprintf(stderr, "length: %u offset: %u\n", vd->buf.length, vd->buf.m.offset);
        }
        vd->mem[i] = mmap(0 /* start anywhere */ , vd->buf.length, PROT_READ, MAP_SHARED, vd->fd, vd->buf.m.offset);
        if (vd->mem[i] == MAP_FAILED)
        {
            fprintf(stderr, "Unable to map buffer (%d)\n", errno);
            goto fatal;
        }
        if (debug)
        {
            fprintf(stderr, "Buffer mapped at address %p.\n", vd->mem[i]);
        }
    }
    /* Queue the buffers. */
    for (i = 0; i < NB_BUFFER; ++i)
    {
        memset(&vd->buf, 0, sizeof(struct v4l2_buffer));
        vd->buf.index = i;
        vd->buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        vd->buf.memory = V4L2_MEMORY_MMAP;
        ret = ioctl(vd->fd, VIDIOC_QBUF, &vd->buf);
        if (ret < 0)
        {
            fprintf(stderr, "Unable to queue buffer (%d).\n", errno);
            goto fatal;;
        }
    }
    return 0;
    fatal:
    return -1;

}

static int video_enable(struct vdIn *vd)
{
    int type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    int ret;
    ret = ioctl(vd->fd, VIDIOC_STREAMON, &type);
    if (ret < 0)
    {
        fprintf(stderr, "Unable to %s capture: %d.\n", "start", errno);
        return ret;
    }
    vd->isstreaming = 1;
    return 0;
}

static int video_disable(struct vdIn *vd)
{
    int type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    int ret;
    ret = ioctl(vd->fd, VIDIOC_STREAMOFF, &type);
    if (ret < 0)
    {
        fprintf(stderr, "Unable to %s capture: %d.\n", "stop", errno);
        return ret;
    }
    vd->isstreaming = 0;
    return 0;
}

int uvcGrab(struct vdIn *vd)
{
    int ret;
    if (!vd->isstreaming)
    {
        if (video_enable(vd))
        {
            goto err;
        }
    }
    memset(&vd->buf, 0, sizeof(struct v4l2_buffer));
    vd->buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    vd->buf.memory = V4L2_MEMORY_MMAP;
    ret = ioctl(vd->fd, VIDIOC_DQBUF, &vd->buf);
    if (ret < 0)
    {
        fprintf(stderr, "Unable to dequeue buffer (%d).\n", errno);
        goto err;
    }
    switch (vd->formatIn)
    {
        case V4L2_PIX_FMT_MJPEG:
            memcpy(vd->tmpbuffer, vd->mem[vd->buf.index], vd->buf.bytesused);
            vd->byteUsed = vd->buf.bytesused;
            if (debug)
            {
                fprintf(stderr, "uvc Grab bytes in used %d \n", vd->buf.bytesused);
            }
            break;
        default:
            goto err;
            break;
    }
    ret = ioctl(vd->fd, VIDIOC_QBUF, &vd->buf);
    if (ret < 0)
    {
        fprintf(stderr, "Unable to requeue buffer (%d).\n", errno);
        goto err;
    }

    return 0;
    err:
    return -1;
}

int close_v4l2(struct vdIn *vd)
{
    int i;

    if (vd->isstreaming)
    {
        video_disable(vd);
    }

    /* If the memory maps are not released the device will remain opened even
     after a call to close(); */
    for (i = 0; i < NB_BUFFER; i++)
    {
        munmap(vd->mem[i], vd->buf.length);
    }

    if (vd->tmpbuffer)
    {
        free(vd->tmpbuffer);
    }
    vd->tmpbuffer = NULL;
    free(vd->framebuffer);
    vd->framebuffer = NULL;
    free(vd->videodevice);
    free(vd->status);
    free(vd->pictName);
    vd->videodevice = NULL;
    vd->status = NULL;
    vd->pictName = NULL;
    close(vd->fd);
    return 0;
}
