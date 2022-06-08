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
* File Name    : define.h
* Version      : 1.10
* Description  : RZ/V2L Sample Application for Camera
***********************************************************************************************************************/

#ifndef DEFINE_MACRO_H
#define DEFINE_MACRO_H

// #define DEBUG_CONSOLE
/*****************************************
* includes
******************************************/
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <errno.h>
#include <vector>
#include <map>
#include <fstream>
#include <iomanip>
#include <cstring>
#include <atomic>
#include <semaphore.h>

/*****************************************
* Macro
******************************************/
/*Camera:: Capture Image Information*/
#ifdef CAM_RES_FULL
  #define IMAGE_WIDTH             (2592)
  #define IMAGE_HEIGHT            (1944)
#elif CAM_RES_1080P
  #define IMAGE_WIDTH             (1920)
  #define IMAGE_HEIGHT            (1080)
#elif CAM_RES_720P
  #define IMAGE_WIDTH             (1280)
  #define IMAGE_HEIGHT            (720)
#else
  #define IMAGE_WIDTH             (640)
  #define IMAGE_HEIGHT            (480)
#endif

#define INPUT_IMAGE_WIDTH       (IMAGE_WIDTH)
#define INPUT_IMAGE_HEIGHT      (IMAGE_HEIGHT)
#define IMAGE_CHANNEL_YUY2      (2)

#ifdef CAM_RES_FULL
  #define OUTPUT_WIDTH            (2592)
  #define OUTPUT_HEIGHT           (1944)
#elif CAM_RES_1080P
  #define OUTPUT_WIDTH            (1920)
  #define OUTPUT_HEIGHT           (1080)
#elif CAM_RES_720P
  #define OUTPUT_WIDTH            (1280)
  #define OUTPUT_HEIGHT           (720)
#else
  #define OUTPUT_WIDTH            (640)
  #define OUTPUT_HEIGHT           (480)
#endif

#define IMAGE_CHANNEL_BGRA      (4)

#define MAPSIZE                 (0x1000)
#define MAPMASK                 (0xFFFFF000)

/*Camera:: Capture Information */
#define CAP_BUF_NUM              (6)

/*udmabuf memory area Information*/
#define UDMABUF_ADDRESS         (0xB9000000)
#define UDMABUF_IMGSIZE         (IMAGE_WIDTH * IMAGE_HEIGHT * IMAGE_CHANNEL_YUY2)
#define UDMABUF_OFFSET          (((UDMABUF_IMGSIZE + MAPSIZE - 1) & MAPMASK ) * CAP_BUF_NUM)
/*Wayland:: Number of Wayland buffer */
#define WL_BUF_NUM              (2)
#define WAYLAND_IMGSIZE ((IMAGE_WIDTH * IMAGE_HEIGHT * IMAGE_CHANNEL_YUY2 + 0xFFF) & MAPMASK)


/*RESIZE_SCALE=((OUTPUT_WIDTH/IMAGE_WIDTH > OUTPUT_HEIGHT/IMAGE_HEIGHT) ?
        OUTPUT_HEIGHT/IMAGE_HEIGHT : OUTPUT_WIDTH/IMAGE_WIDTH)*/
#define RESIZE_SCALE            (1.5)
#define CHAR_THICKNESS          (2)
/*Waiting Time*/
#define WAIT_TIME               (1000) /* microseconds */

/*Timer Related*/
#define CAPTURE_TIMEOUT         (20)  /* seconds */
#define KEY_THREAD_TIMEOUT      (5)   /* seconds */

#endif
