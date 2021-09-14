
#define NB_BUFFER 16
#define DHT_SIZE 420

struct vdIn {
    int fd;
    char *videodevice;
    char *status;
    char *pictName;
    struct v4l2_capability cap;
    struct v4l2_format fmt;
    struct v4l2_buffer buf;
    struct v4l2_requestbuffers rb;
    void *mem[NB_BUFFER];
    unsigned char *tmpbuffer;
    unsigned char *framebuffer;
    int isstreaming;
    int grabmethod;
    int width;
    int height;
    int formatIn;
    int framesizeIn;
	int byteUsed;//add by dql

};

int init_videoIn (struct vdIn *vd, char *device, int width, int height);
int uvcGrab (struct vdIn *vd);
int close_v4l2 (struct vdIn *vd);
