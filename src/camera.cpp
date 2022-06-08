/***********************************************************************************************************************
* DISCLAIMER
* This software is supplied by Renesas Electronics Corporation and is only intended for use with Renesas products. No
* other uses are authorized. This software is owned by Renesas Electronics Corporation and is protected under all
* applicable laws, including copyright laws.
* THIS SOFTWARE IS PROVIDED "AS IS" AND RENESAS MAKES NO WARRANTIES REGARDING
* THIS SOFTWARE, WHETHER EXPRESS, IMPLIED OR STATUTORY, INCLUDING BUT NOT LIMITED TO WARRANTIES OF MERCHANTABILITY,
* FITNESS FOR A PARTICULAR PURPOSE AND NON-INFRINGEMENT. ALL SUCH WARRANTIES ARE EXPRESSLY DISCLAIMED. TO THE MAXIMUM
* EXTENT PERMITTED NOT PROHIBITED BY LAW, NEITHER RENESAS ELECTRONICS CORPORATION NOR ANY OF ITS AFFILIATED COMPANIES
* SHALL BE LIABLE FOR ANY DIRECT, INDIRECT, SPECIAL, INCIDENTAL OR CONSEQUENTIAL DAMAGES FOR ANY REASON RELATED TO THIS
* SOFTWARE, EVEN IF RENESAS OR ITS AFFILIATES HAVE BEEN ADVISED OF THE POSSIBILITY OF SUCH DAMAGES.
* Renesas reserves the right, without notice, to make changes to this software and to discontinue the availability of
* this software. By using this software, you agree to the additional terms and conditions found by accessing the
* following link:
* http://www.renesas.com/disclaimer
*
* Copyright (C) 2022 Renesas Electronics Corporation. All rights reserved.
***********************************************************************************************************************/
/***********************************************************************************************************************
* File Name    : camera.cpp
* Version      : 1.10
* Description  : RZ/V2L Sample Application for Camera
***********************************************************************************************************************/

/*****************************************
* Includes
******************************************/
#include "camera.h"

Camera::Camera()
{
    camera_width = IMAGE_WIDTH;
    camera_height = IMAGE_HEIGHT;
    camera_color = IMAGE_CHANNEL_YUY2;
}

Camera::~Camera()
{
}

/*****************************************
* Function Name : start_camera
* Description   : Function to initialize MIPI camera capture
* Arguments     : -
* Return value  : 0 if succeeded
*                 not 0 otherwise
******************************************/
int8_t Camera::start_camera()
{
    int8_t ret ;
    int i;
    int offset;
    struct v4l2_ext_controls extCtls;
    struct v4l2_ext_control extCtl;
#ifdef CORAL_CAM
    const char* commands[4] = {
#ifdef CAM_RES_FULL
        "media-ctl -d /dev/media0 -r",
        "media-ctl -d /dev/media0 -l \"\'rzg2l_csi2 10830400.csi2\':1 -> \'CRU output\':0 [1]\"",
        "media-ctl -d /dev/media0 -V \"\'rzg2l_csi2 10830400.csi2\':1 [fmt:SRGGB10_1X10/2592x1944 field:none]\"",
        "media-ctl -d /dev/media0 -V \"\'ov5645 0-003c\':0 [fmt:SRGGB10_1X10/2592x1944 field:none]\""
#elif CAM_RES_1080P
        "media-ctl -d /dev/media0 -r",
        "media-ctl -d /dev/media0 -l \"\'rzg2l_csi2 10830400.csi2\':1 -> \'CRU output\':0 [1]\"",
        "media-ctl -d /dev/media0 -V \"\'rzg2l_csi2 10830400.csi2\':1 [fmt:SRGGB10_1X10/1920x1080 field:none]\"",
        "media-ctl -d /dev/media0 -V \"\'ov5645 0-003c\':0 [fmt:SRGGB10_1X10/1920x1080 field:none]\""
#elif CAM_RES_720P
		"media-ctl -d /dev/media0 -r",
        "media-ctl -d /dev/media0 -l \"\'rzg2l_csi2 10830400.csi2\':1 -> \'CRU output\':0 [1]\"",
        "media-ctl -d /dev/media0 -V \"\'rzg2l_csi2 10830400.csi2\':1 [fmt:SRGGB10_1X10/1280x720 field:none]\"",
        "media-ctl -d /dev/media0 -V \"\'ov5645 0-003c\':0 [fmt:SRGGB10_1X10/1280x720 field:none]\""
#else
        "media-ctl -d /dev/media0 -r",
        "media-ctl -d /dev/media0 -V \"\'ov5645 0-003c\':0 [fmt:SRGGB10_1X10/640x480 field:none]\"",
        "media-ctl -d /dev/media0 -l \"\'rzg2l_csi2 10830400.csi2\':1 -> \'CRU output\':0 [1]\"",
        "media-ctl -d /dev/media0 -V \"\'rzg2l_csi2 10830400.csi2\':1 [fmt:SRGGB10_1X10/640x480 field:none]\""
#endif
    };

    /* media-ctl command */
    for(i=0; i<4; i++){
        printf("%s\n", commands[i]);
        ret = system(commands[i]);
        if(ret<0){
            printf("%s: failed media-ctl commands. index = %d\n",__func__ , i);
            return -1;
        }
    }
#endif
    if ((ret = open_camera_device())!=0) return ret;

    if ((ret = init_camera_fmt())!=0) return ret;

    if ((ret = init_buffer())!=0) return ret;

    udmabuf_file= open("/dev/udmabuf0", O_RDWR);
    if (udmabuf_file<0)
    {
        return -1;
    }

    for (int i =0;i<CAP_BUF_NUM;i++)
    {
        /* calc offset. Offset is a multiple of 0x1000 */
        offset = (imageLength + MAPSIZE - 1) & MAPMASK;
        buffer[i] =(unsigned char*) mmap(NULL, imageLength ,PROT_READ|PROT_WRITE, MAP_SHARED,  udmabuf_file, offset*i);

        if (buffer[i] == MAP_FAILED)
        {
            return -1;
        }

        /* Write once to allocate physical memory to u-dma-buf virtual space.
        * Note: Do not use memset() for this.
        *       Because it does not work as expected. */
        {
            uint8_t * word_ptr = buffer[i];
            for(int i = 0 ; i < imageLength; i++)
            {
                word_ptr[i] = 0;
            }
        }

        memset(&buf_capture, 0, sizeof(buf_capture));
        buf_capture.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        buf_capture.memory = V4L2_MEMORY_USERPTR;
        buf_capture.index = i;
        /* buffer[i] must be casted to unsigned long type in order to assign it to V4L2 buffer */
        buf_capture.m.userptr =reinterpret_cast<unsigned long>(buffer[i]);
        buf_capture.length = imageLength;
        if (-1==xioctl(m_fd, VIDIOC_QBUF, &buf_capture))
        {
            return -1;
        }
    }

    if ((ret = start_capture())!=0) return ret;

    return 0;
}


/*****************************************
* Function Name : close_capture
* Description   : Close camera and free buffer
* Arguments     : -
* Return value  : 0 if succeeded
*                 not 0 otherwise
******************************************/
int8_t Camera::close_camera()
{
    stop_capture();
    for (int i = 0;i<CAP_BUF_NUM;i++)
    {
        munmap(buffer[i], imageLength);
    }
    close(udmabuf_file);
    close(m_fd);
    return 0;
}


/*****************************************
* Function Name : xioctl
* Description   : ioctl calling
* Arguments     : fd = V4L2 file descriptor
*                 request = V4L2 control ID defined in videodev2.h
*                 arg = set value
* Return value  : int = output parameter
******************************************/
int Camera::xioctl(int fd, int request, void * arg)
{
    int r;
    do r = ioctl(fd, request, arg);
    while(-1 == r && EINTR == errno);
    return r;
}

/*****************************************
* Function Name : start_capture
* Description   : Set STREAMON
* Arguments     : -
* Return value  : 0 if succeeded
*                 not 0 otherwise
******************************************/
int8_t Camera::start_capture()
{
    struct v4l2_buffer buf;
    memset(&buf, 0, sizeof(buf));

    buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;

    if (-1==xioctl(m_fd, VIDIOC_STREAMON, &buf.type))
    {
        return -1;
    }
    return 0;
}

/*****************************************
* Function Name : capture_qbuf
* Description   : Function to enqueue the buffer.
*                 (Call this function after capture_image() to restart filling image data into buffer)
* Arguments     : -
* Return value  : 0 if succeeded
*                 not 0 otherwise
******************************************/
int8_t Camera::capture_qbuf()
{
    if (-1==xioctl(m_fd, VIDIOC_QBUF, &buf_capture))
    {
        return -1;
    }
    return 0;
}


/*****************************************
* Function Name : capture_image
* Description   : Function to capture image and return the physical memory address where the captured image stored.
*                 Must call capture_qbuf after calling this function.
* Arguments     : -
* Return value  : the physical memory address where the captured image stored.
******************************************/
unsigned long Camera::capture_image()
{
    static int index_cnt=0;
    fd_set fds;
    int ret;
    /*Delete all file descriptor from fds*/
    FD_ZERO(&fds);
    /*Add m_fd to file descriptor set fds*/
    FD_SET(m_fd, &fds);

    while (1)
    {
        /* Check when a new frame is available */
        while (1)
        {
            int r = select(m_fd + 1, &fds, NULL, NULL, NULL);
            if (r < 0)
            {
                if (errno == EINTR) continue;
                return 0;
            }
            break;
        }

        /* Get buffer where camera stored data */
        ret = xioctl(m_fd, VIDIOC_DQBUF, &buf_capture);
        if (-1==ret)
        {
        	/* If there is EBUSY, continue */
            if (errno == EBUSY){
                errno = 0;
                continue;
            }
            return 0;
        }
        break;
    }

    return UDMABUF_ADDRESS + buf_capture.index * imageLength;
}

/*****************************************
* Function Name : stop_capture
* Description   : Set STREAMOFF
* Arguments     : -
* Return value  : 0 if succeeded
*                 not 0 otherwise
******************************************/
int8_t Camera::stop_capture()
{
    struct v4l2_buffer buf;
    memset(&buf, 0, sizeof(buf));

    buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    buf.memory = V4L2_MEMORY_USERPTR;

    if (-1==xioctl(m_fd, VIDIOC_STREAMOFF, &buf.type))
    {
        return -1;
    }
    return 0;
}

/*****************************************
* Function Name : open_camera_device
* Description   : Function to open camera *called by start_camera
* Arguments     : -
* Return value  : 0 if succeeded
*                 not 0 otherwise
******************************************/
int8_t Camera::open_camera_device()
{
  if (device.empty())
  {
      char dev_name[4096] = {0};
      int i = 0;
      struct v4l2_capability fmt;

      for (i = 0; i < 15; i++)
      {
        snprintf(dev_name, sizeof(dev_name), "/dev/video%d", i);
        m_fd = open(dev_name, O_RDWR);

        if (m_fd==-1){
            printf("[error] video 0: open\n");
            return -1;
        }

        /* Check device is valid (Query Device information) */
        memset(&fmt, 0, sizeof(fmt));
        if(-1==xioctl(m_fd, VIDIOC_QUERYCAP, &fmt))
        {
            printf("[error] VIDIOC_QUERYCAP\n");
            return -1;
        }
    #ifdef CORAL_CAM
        /* Check ISP */
        if (strcmp((const char*)fmt.driver, "rzv2l_isp")==0)
        {
            printf("[INFO] CAMERA: %s\n", dev_name);
            break;
        }
    #else
        /* Search USB camera */
        if (strcmp((const char*)fmt.driver, "uvcvideo") == 0 )
        {
            printf("[INFO] USB Camera: %s\n", dev_name);
            break;
        }
    #endif
        close(m_fd);
        printf("[error] CAMERA: %s\n", dev_name);
        printf("[error] FMT: %s\n", fmt.driver);

    }

    if (15 <= i)
    {
        return -1;
    }
  }
  else
  {
      return -1;
  }
  return 0;
}

/*****************************************
* Function Name : init_camera_fmt
* Description   : Function to request format *called by start_camera
* Arguments     : -
* Return value  : 0 if succeeded
*                 not 0 otherwise
******************************************/
int8_t Camera::init_camera_fmt()
{
    struct v4l2_format fmt;
    memset(&fmt, 0, sizeof(fmt));
    fmt.type=V4L2_BUF_TYPE_VIDEO_CAPTURE;
    fmt.fmt.pix.width = camera_width;
    fmt.fmt.pix.height = camera_height;
    fmt.fmt.pix.pixelformat = V4L2_PIX_FMT_YUYV;
    fmt.fmt.pix.field = V4L2_FIELD_NONE;

    if (-1==xioctl(m_fd, VIDIOC_S_FMT, &fmt)){
        return -1;
    }
    return 0;
}

/*****************************************
* Function Name : init_buffer
* Description   : Initialize camera buffer *called by start_camera
* Arguments     : -
* Return value  : 0 if succeeded
*                 not 0 otherwise
******************************************/
int8_t Camera::init_buffer()
{
    struct v4l2_requestbuffers req;
    int ret = 0;
    struct v4l2_ext_controls extCtls;
    struct v4l2_ext_control extCtl;

    /* Set input image format */
    memset(&extCtls, 0, sizeof(extCtls));
    memset(&extCtl, 0, sizeof(extCtl));
    extCtl.id = V4L2_CID_RZ_ISP_IN_FMT;
    extCtl.value = V4L2_RZ_ISP_IN_FMT_RAW10;
//    extCtl.value = V4L2_RZ_ISP_IN_FMT_RAW8;
    extCtl.size = 0;
    extCtls.controls = &extCtl;
    extCtls.count = 1;
    extCtls.ctrl_class = V4L2_CTRL_CLASS_CAMERA;
    ret = xioctl(m_fd, VIDIOC_S_EXT_CTRLS, &extCtls);
    
    /*Request a buffer that will be kept in the device*/
    memset(&req, 0, sizeof(req));
    req.count=CAP_BUF_NUM;
    req.type=V4L2_BUF_TYPE_VIDEO_CAPTURE;
    req.memory = V4L2_MEMORY_USERPTR;

    /*Request a buffer that will be kept in the device*/
    if (-1==xioctl(m_fd, VIDIOC_REQBUFS, &req)){
        return -1;
    }

    struct v4l2_buffer buf;
    for (int i =0;i<CAP_BUF_NUM;i++)
    {
        memset(&buf, 0, sizeof(buf));
        buf.type=V4L2_BUF_TYPE_VIDEO_CAPTURE;
        buf.memory=V4L2_MEMORY_USERPTR;
        buf.index=i;

        /* Extract buffer information */
        if (-1==xioctl(m_fd, VIDIOC_QUERYBUF, &buf))
        {
            return -1;
        }

    }
    imageLength=buf.length;

    /* Start AE On */
    memset(&extCtls, 0, sizeof(extCtls));
    memset(&extCtl, 0, sizeof(extCtl));
    extCtl.id = V4L2_CID_RZ_ISP_AE;
    extCtl.value = 1;   //AE ON
    extCtls.controls = &extCtl;
    extCtls.count = 1;
    extCtls.ctrl_class = V4L2_CTRL_CLASS_CAMERA;
    usleep(1);
    ret = xioctl(m_fd, VIDIOC_S_EXT_CTRLS, &extCtls);

    return ret;
}

/*****************************************
* Function Name : set_isp_all_param
* Description   : Set All isp parameters
* Arguments     : Pointer of data to set 
* Return value  : 0 if succeeded
*                 not 0 otherwise
******************************************/
int8_t Camera::set_isp_all_param(unsigned char *senddata)
{
    int8_t ret;
    struct v4l2_buffer buf;
    struct v4l2_ext_controls extCtls;
    struct v4l2_ext_control extCtl;

    /* Stream off */
    if ((ret = stop_capture())!=0) return ret;
    usleep(100);

    /* Extract buffer information */
    for (int i =0;i<CAP_BUF_NUM;i++)
    {
        memset(&buf, 0, sizeof(buf));
        buf.type=V4L2_BUF_TYPE_VIDEO_CAPTURE;
        buf.memory=V4L2_MEMORY_USERPTR;
        buf.index=i;
        if (-1==xioctl(m_fd, VIDIOC_QUERYBUF, &buf))
        {
            return -1;
        }
    }

    /* Camera qbuf */
    for (int i =0;i<CAP_BUF_NUM;i++)
    {
        memset(&buf_capture, 0, sizeof(buf_capture));
        buf_capture.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        buf_capture.memory = V4L2_MEMORY_USERPTR;
        buf_capture.index = i;
        /* buffer[i] must be casted to unsigned long type in order to assign it to V4L2 buffer */
        buf_capture.m.userptr =reinterpret_cast<unsigned long>(buffer[i]);
        buf_capture.length = imageLength;
        if (-1==xioctl(m_fd, VIDIOC_QBUF, &buf_capture))
        {
            return -1;
        }
    }

    /* Start AE On */
    memset(&extCtls, 0, sizeof(extCtls));
    memset(&extCtl, 0, sizeof(extCtl));
    extCtl.id = V4L2_CID_RZ_ISP_AE;
    extCtl.value = 1;   //AE ON
    extCtls.controls = &extCtl;
    extCtls.count = 1;
    extCtls.ctrl_class = V4L2_CTRL_CLASS_CAMERA;
    usleep(1);
    ret = xioctl(m_fd, VIDIOC_S_EXT_CTRLS, &extCtls);
    if (0 != ret)
    {
        return ret;
    }

    /* Set Alldata */
    memset(&extCtls, 0, sizeof(extCtls));
    memset(&extCtl, 0, sizeof(extCtl));
    extCtl.id = V4L2_CID_RZ_ISP_DETAIL;
    extCtl.value = 1;
    extCtl.ptr = senddata;
    extCtl.size = 512;
    extCtls.controls = &extCtl;
    extCtls.count = 1;
    extCtls.ctrl_class = V4L2_CTRL_CLASS_CAMERA;
    ret = xioctl(m_fd, VIDIOC_S_EXT_CTRLS, &extCtls);
    if (0 != ret)
    {
        return ret;
    }

    /* Stream on */
    if ((ret = start_capture())!=0)
    {
        return ret;
    }
    return 0;
}

/*****************************************
* Function Name : save_bin
* Description   : Get the capture image from buffer and save it into binary file
* Arguments     : filename = binary file name to be saved
* Return value  : 0 if succeeded
*                 not 0 otherwise
******************************************/
int8_t Camera::save_bin(std::string filename)
{
    FILE * fp;
    if (!(fp = fopen(filename.c_str(), "wb")))
    {
        return -1;
    }

    /* Get data from buffer and write to binary file */
    fwrite(buffer[buf_capture.index], sizeof(unsigned char), imageLength, fp);

    fclose(fp);
    return 0;
}


/*****************************************
* Function Name : get_img
* Description   : Function to return the camera buffer
* Arguments     : -
* Return value  : camera buffer
******************************************/
unsigned char* Camera::get_img()
{
    return buffer[buf_capture.index];
}


/*****************************************
* Function Name : get_size
* Description   : Function to return the camera buffer size (W x H x C)
* Arguments     : -
* Return value  : camera buffer size (W x H x C )
******************************************/
int Camera::get_size()
{
    return imageLength;
}

/*****************************************
* Function Name : get_w
* Description   : Get camera_width. This function is currently NOT USED.
* Arguments     : -
* Return value  : camera_width = width of camera capture image.
******************************************/
int Camera::get_w()
{
    return camera_width;
}

/*****************************************
* Function Name : set_w
* Description   : Set camera_width. This function is currently NOT USED.
* Arguments     : w = new camera capture image width
* Return value  : -
******************************************/
void Camera::set_w(int w)
{
    camera_width= w;
    return;
}

/*****************************************
* Function Name : get_h
* Description   : Get camera_height. This function is currently NOT USED.
* Arguments     : -
* Return value  : camera_height = height of camera capture image.
******************************************/
int Camera::get_h()
{
    return camera_height;
}

/*****************************************
* Function Name : set_h
* Description   : Set camera_height. This function is currently NOT USED.
* Arguments     : w = new camera capture image height
* Return value  : -
******************************************/
void Camera::set_h(int h)
{
    camera_height = h;
    return;
}

/*****************************************
* Function Name : get_c
* Description   : Get camera_color. This function is currently NOT USED.
* Arguments     : -
* Return value  : camera_color = color channel of camera capture image.
******************************************/
int Camera::get_c()
{
    return camera_color;
}

/*****************************************
* Function Name : set_c
* Description   : Set camera_color. This function is currently NOT USED.
* Arguments     : c = new camera capture image color channel
* Return value  : -
******************************************/
void Camera::set_c(int c)
{
    camera_color= c;
    return;
}
