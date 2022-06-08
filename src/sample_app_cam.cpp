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
* File Name    : sample_app_cam.cpp
* Version      : 1.10
* Description  : RZ/V2L Sample Application for Camera
***********************************************************************************************************************/

/*****************************************
* Includes
******************************************/
/*Definition of Macros & other variables*/
#include "define.h"
/*MIPI camera control*/
#include "camera.h"
/*Image control*/
#include "image.h"
/*Wayland control*/
#include "wayland.h"

/*****************************************
* Global Variables
******************************************/
/*Multithreading*/
static sem_t terminate_req_sem;
static pthread_t kbhit_thread;
static pthread_t capture_thread;
/*Flags*/
static std::atomic<uint8_t> img_obj_ready   (0);
/*Global Variables*/
static uint32_t capture_address;
static Image img;
static Wayland wayland;

/* ISP Param set */
#define ISP_DATA_SIZE       (512)
#define ISP_DATA_SIZEV10    (429)
#define ISP_DATA_SIZEV11    (512)
#define INPUT_WAIT_TIME (1000)
#define INPUT_MAX_SIZE  (ISP_DATA_SIZE * 2 + 8)

static uint8_t isp_set_data[ISP_DATA_SIZE];
static uint32_t isp_set_req;
static uint32_t isp_comment_f;

/*****************************************
* Function Name : timedifference_msec
* Description   : compute the time diffences in ms between two moments
* Arguments     : t0 = start time
*                 t1 = stop time
* Return value  : the time diffence in ms
******************************************/
static double timedifference_msec(struct timespec t0, struct timespec t1)
{
    return (t1.tv_sec - t0.tv_sec) * 1000.0 + (t1.tv_nsec - t0.tv_nsec) / 1000000.0;
}

/*****************************************
* Function Name : wait_join
* Description   : waits for a fixed amount of time for the thread to exit
* Arguments     : p_join_thread = thread that the function waits for to Exit
*                 join_time = the timeout time for the thread for exiting
* Return value  : 0 if successful
*                 not 0 otherwise
******************************************/
static int wait_join(pthread_t *p_join_thread, uint32_t join_time)
{
    int ret_err;
    struct timespec join_timeout;
    ret_err = clock_gettime(CLOCK_REALTIME, &join_timeout);
    if( ret_err == 0 )
    {
        join_timeout.tv_sec += join_time;
        ret_err = pthread_timedjoin_np(*p_join_thread, NULL, &join_timeout);
    }
    return ret_err;
}

/*****************************************
* Function Name : R_Capture_Thread
* Description   : Executes the V4L2 capture with Capture thread.
* Arguments     : threadid = thread identification
* Return value  : -
******************************************/
void *R_Capture_Thread(void *threadid)
{
    Camera* capture = (Camera*) threadid;
    /*Semaphore Variable*/
    int capture_sem_check;

    uint32_t capture_addr;
    uint8_t ret = 0;
    unsigned char* img_buffer;

    printf("Capture Thread Starting\n");
    while(1)
    {
        /*Gets the Termination request semaphore value, if different then 1 Termination was requested*/
        /*Checks if sem_getvalue is executed wihtout issue*/
        if(sem_getvalue(&terminate_req_sem, &capture_sem_check) != 0)
        {
            fprintf(stderr, "[ERROR] Failed to Get Semaphore Value %d\n", errno);
            goto err;
        }

        /* ISP set */
        if( isp_set_req != 0 )
        {
            isp_set_req = 0;
            if( 0 != capture->set_isp_all_param(&isp_set_data[0]) )
            {
                printf( "request isp err\n");
                goto err;
            }
        }

        /*Checks the semaphore value*/
        if(capture_sem_check != 1)
        {
            goto capture_end;
        }

        /* Capture MIPI camera image and stop updating the capture buffer */
        capture_addr = capture->capture_image();
        if (capture_addr == 0)
        {
            fprintf(stderr, "[ERROR] Camera::capture_image failed\n");
            goto err;
        }
        else
        {
            /* Store captured image data memory address to global variable. */
            capture_address = capture_addr;
            /*Wait until img_obj_ready flag is cleared by the Main Thread.*/
            if (!img_obj_ready.load())
            {
                img_buffer = capture->get_img();
                img.camera_to_image(img_buffer, capture->get_size());
                img_obj_ready.store(1); /* Flag for Display Thread. */
            }


            img_obj_ready.store(1); /* Flag for Main Thread. */
        }

        /* IMPORTANT: Place back the image buffer to the capture queue */
        if (0 != capture->capture_qbuf())
        {
            fprintf(stderr, "[ERROR] Camera::capture_qbuf failed\n");
            goto capture_end;
        }
    } /*End of Loop*/

err:
    sem_trywait(&terminate_req_sem);
    goto capture_end;

capture_end:
    printf("Capture Thread Terminated\n");
    pthread_exit(NULL);
}


/*****************************************
* Function Name : R_Kbhit_Thread
* Description   : Executes the Keyboard hit thread (checks if enter key is hit)
* Arguments     : threadid = thread identification
* Return value  : -
******************************************/
void *R_Kbhit_Thread(void *threadid)
{
    int ret_fcntl;
    int semget_err;
    /*Semaphore Variable*/
    int kh_sem_check;
    int c;
    int readp;
    int readtime;
    unsigned char readdata[INPUT_MAX_SIZE];

    printf("Key Hit Thread Started\n");

    printf("************************************************\n");
    printf("* Press 'Q' and ENTER key to quit. *\n");
    printf("* Send Param Data to change ISP Setting. *\n");
    printf("************************************************\n");

    /*Set Standard Input to Non Blocking*/
    ret_fcntl = fcntl(0, F_SETFL, O_NONBLOCK);
    if(ret_fcntl == -1)
    {
        fprintf(stderr, "[ERROR] Failed to fctnl() %d\n", errno);
        goto err;
    }

    readp = 0;
    readtime = 0;
    isp_set_req = 0;
    isp_comment_f = 0;

    while(1)
    {
        /*Gets the Termination request semaphore value, if different then 1 Termination was requested*/
        /*Checks if sem_getvalue is executed wihtout issue*/
        if(sem_getvalue(&terminate_req_sem, &kh_sem_check) != 0)
        {
            fprintf(stderr, "[ERROR] Failed to Get Semaphore Value %d\n", errno);
            goto err;
        }
        /*Checks the semaphore value*/
        if(kh_sem_check != 1) /* Termination Request Detected on other threads */
        {
            goto key_hit_end;
        }

        {
            int indata = getchar();
            while( indata != EOF )
            {
                /* Detect enter key and exit */
                if( ( readp == 0 ) && ( readtime == 0) && ( (indata == 'Q' ) || (indata == 'q' ) ) )
                {
                    printf("Q key Detected\n");
                    goto err;
                }

                readtime = INPUT_WAIT_TIME;
                /* Detect apostrophe, it's a comment until the end of the line  */
                if( indata == 0x27 )    // apostrophe
                {
                    isp_comment_f = 1;
                }

                /* Detect line end, no comment after this */
                if( ( indata == 0x0a ) || ( indata == 0x0d ) )
                {
                    isp_comment_f = 0;
                }

                if( isp_comment_f == 0 )
                {
                    /* Detects hexadecimal characters, store this data */
                    if( ( indata >= '0' ) && ( indata <= '9' ) )
                    {
                        readdata[readp++] = (unsigned char)(indata - '0');
                    }
                    if( ( indata >= 'A' ) && ( indata <= 'F' ) )
                    {
                        readdata[readp++] = (unsigned char)(indata - 'A' + 10);
                    }
                    if( ( indata >= 'a' ) && ( indata <= 'f' ) )
                    {
                        readdata[readp++] = (unsigned char)(indata - 'a' + 10);
                    }

                    /* Detects 'Z', Set ISP Parameters */
                    if( ( indata == 'Z' ) || ( indata == 'z' ) )
                    {
                        int rcv_total_size = 0;
                        int checksum;
                        int isp_data_size;

                        /* If there is not enough data, this data is invalid */
                        if( readp ==  ISP_DATA_SIZEV11 * 2 + 8 )
                        {
                        	isp_data_size = ISP_DATA_SIZEV11;
                        }
                        else if( readp ==  ISP_DATA_SIZEV10 * 2 + 8 )
                        {
                        	isp_data_size = ISP_DATA_SIZEV10;
                        }
                        else
                        {
                            printf( "data size error\n");
                            readp = 0;
                            break;                        
                        }

                        /* Create data for settings, and calculate checksum */
                        for( int i = 0; i< isp_data_size; i++ )
                        {
                            isp_set_data[i] = readdata[i*2] * 16 + readdata[i*2+1];
                            rcv_total_size += isp_set_data[i];
                        }
                        checksum = readdata[isp_data_size*2+7]*0x1000000 + readdata[isp_data_size*2+6]*0x10000000 +
                                   readdata[isp_data_size*2+5]*0x10000   + readdata[isp_data_size*2+4]*0x100000   +
                                   readdata[isp_data_size*2+3]*0x100     + readdata[isp_data_size*2+2]*0x1000     +
                                   readdata[isp_data_size*2+1]           + readdata[isp_data_size*2  ]*0x10;

                        /* If the checksums do not match,  this data is invalid */
                        if( rcv_total_size != checksum )
                        {
                            printf( "checksum error! total(%x) != checksum(%x) \n",rcv_total_size, checksum);
                            readp = 0;
                            break;                        
                        }
                        else
                        {
                            /* ISP data send request */
                            isp_set_req = 1;
                            readp = 0;
                        }
                         
                    }

                    /* If the size becomes excessive before detecting 'Z',  this data is invalid */ 
                    if( readp > INPUT_MAX_SIZE )
                    {
                        printf( "oversize data!\n");
                        readp = 0;
                        break;
                    }
                }
                indata = getchar();
            }
            
            /* If data reception is interrupted for 1 second, this data is invalid */ 
            if( readtime > 0 )
            {
                readtime--;
                if( readtime == 0 )
                {
                    if( readp != 0 )
                    {
                        printf("%d byte data disposed\n",readp);
                        readp = 0;
                    }
                }
            }
            usleep(WAIT_TIME);
        }
    }

err:
    sem_trywait(&terminate_req_sem);
    goto key_hit_end;
key_hit_end:
    printf("Key Hit Thread Terminated\n");
    pthread_exit(NULL);
}

/*****************************************
* Function Name : R_Main_Process
* Description   : Runs the main process loop
* Arguments     : -
* Return value  : 0 if succeeded
*                 not 0 otherwise
******************************************/
uint8_t R_Main_Process()
{
    /*Main Process Variables*/
    uint8_t main_ret = 0;
    /*Semaphore Related*/
    int sem_check;
    int semget_err;
    unsigned char img_buf_id;
    printf("Main Loop Starts\n");
    while(1)
    {
        /*Checks if there is any Terminationrequest in the other threads*/
        semget_err = sem_getvalue(&terminate_req_sem, &sem_check);
        if(semget_err != 0)
        {
            fprintf(stderr, "[ERROR] Failed to get Semaphore Value\n");
            goto err;
        }
        /*Checks the Termination Request Semaphore Value*/
        if(sem_check != 1)
        {
            goto main_end;
        }

        /* Check img_obj_ready flag which is set in Capture Thread. */
        if (img_obj_ready.load())
        {
            /*Update Wayland*/
            img_buf_id = img.get_buf_id();
            wayland.commit(img_buf_id);

            img_obj_ready.store(0);
        }
        /*Wait for 1 TICK.*/
        usleep(WAIT_TIME);
    }

err:
    sem_trywait(&terminate_req_sem);
    main_ret = 1;
    goto main_end;
main_end:
    /*To terminate the loop in Capture Thread.*/
    img_obj_ready.store(0);
    printf("Main Process Terminated\n");
    return main_ret;
}


int main(int argc, char * argv[])
{
    uint8_t main_proc;
    uint8_t ret;
    /*Multithreading Variables*/
    int create_thread_key = -1;
    int create_thread_capture = -1;
    int sem_create = -1;

    printf("RZ/V2L Camera Sample Application\n");
#if CORAL_CAM
    printf("Input : MIPI Camera\n");
#else
    printf("Input : USB Camera\n");
#endif
    /* Create Camera Instance */
    Camera* capture = new Camera();

    /* Init and Start Camera */
    if (0 != capture->start_camera())
    {
        fprintf(stderr, "[ERROR] Camera::start_camera failed\n");
        delete capture;
        return -1;
    }

    /*Initialize Image object.*/
    if (0 != img.init(INPUT_IMAGE_WIDTH, INPUT_IMAGE_HEIGHT, IMAGE_CHANNEL_YUY2, OUTPUT_WIDTH, OUTPUT_HEIGHT, IMAGE_CHANNEL_YUY2))
    {
        fprintf(stderr, "[ERROR] Image::init failed\n");
        return -1;
    }

    /* Initialize waylad */
    if(wayland.init(img.udmabuf_fd, OUTPUT_WIDTH, OUTPUT_HEIGHT, IMAGE_CHANNEL_YUY2))
    {
        fprintf(stderr, "[ERROR] Wayland::init failed\n");
        delete capture;
        return -1;
    }

    /*Termination Request Semaphore Initialization*/
    /*Initialized value at 1.*/
    sem_create = sem_init(&terminate_req_sem, 0, 1);
    if(sem_create != 0)
    {
        fprintf(stderr, "[ERROR] Failed to Initialize Termination Request Semaphore\n");
        goto main_end;
    }

    /*Key Hit Thread*/
    create_thread_key = pthread_create(&kbhit_thread, NULL, R_Kbhit_Thread, NULL);
    if(create_thread_key != 0)
    {
        fprintf(stderr, "[ERROR] Key Hit Thread Creation Failed\n");
        goto main_end;
    }

    /*Capture Thread*/
    create_thread_capture = pthread_create(&capture_thread, NULL, R_Capture_Thread, (void *) capture);
    if(create_thread_capture != 0)
    {
        sem_trywait(&terminate_req_sem);
        fprintf(stderr, "[ERROR] Capture Thread Creation Failed\n");
        goto main_end;
    }

    /*Main Processing*/
    main_proc = R_Main_Process();
    if(main_proc != 0)
    {
        fprintf(stderr, "[ERROR] Error during Main Process\n");
        goto main_end;
    }
    goto main_end;


main_end:
    if(create_thread_capture == 0)
    {
        if(wait_join(&capture_thread, CAPTURE_TIMEOUT) != 0)
        {
            fprintf(stderr, "[ERROR] Capture Thread Failed to Exit on time\n");
        }
    }

    if(create_thread_key == 0)
    {
        if(wait_join(&kbhit_thread, KEY_THREAD_TIMEOUT) != 0)
        {
            fprintf(stderr, "[ERROR] Key Hit Thread Failed to Exit on time\n");
        }
    }

    /*Delete Terminate Request Semaphore.*/
    if(sem_create == 0)
    {
        sem_destroy(&terminate_req_sem);
    }

    /* Exit waylad */
    wayland.exit();

    /*Close MIPI Camera.*/
    if (0 != capture->close_camera())
    {
        fprintf(stderr, "[ERROR] Camera::close_camera failed\n");
    }

    delete capture;

    printf("Application End\n");
    return 0;
}
