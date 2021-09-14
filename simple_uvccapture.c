#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <unistd.h>
#include <linux/videodev2.h>

#include "v4l2uvc.h"

void sigcatch(int sig)
{
    fprintf(stderr, "Exiting...\n");
}

// usage: uvccapture /dev/video0 1920 1080 22.jpg
int main(int argc, char *argv[])
{
    char *videodevice = "/dev/video0";
    int width = 1920;
    int height = 1080;
    char *outputfile="9876543210.jpg";

    (void) signal(SIGINT, sigcatch);
    (void) signal(SIGQUIT, sigcatch);
    (void) signal(SIGKILL, sigcatch);
    (void) signal(SIGTERM, sigcatch);
    (void) signal(SIGABRT, sigcatch);
    (void) signal(SIGTRAP, sigcatch);

    videodevice = argv[1];
    width = atoi (argv[2]);
    height = atoi (argv[3]);
    outputfile = argv[4];

    struct vdIn *videoIn = (struct vdIn *) calloc(1, sizeof(struct vdIn));
    if (init_videoIn(videoIn, (char *) videodevice, width, height) < 0)
    {
        exit(1);
    }
    int cnt = 10;//第一帧曝光不足，比较黑
    while(cnt--)
    {
        if (uvcGrab(videoIn) < 0)
        {
            fprintf(stderr, "Error grabbing\n");
            close_v4l2(videoIn);
            free(videoIn);
            exit(1);
        }
        FILE *file = fopen(outputfile, "wb");
        if (file != NULL)
        {
            //fprintf(stderr, "Saving image to jpeg \"%s\",byteUsed=%d\n", outputfile,videoIn->byteUsed);
            fwrite(videoIn->tmpbuffer, videoIn->byteUsed + DHT_SIZE, 1, file);
        }
        fclose(file);
    }
    close_v4l2(videoIn);
    free(videoIn);
    return 0;
}
