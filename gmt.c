/***************************************************************************
 *                           gmt.c
 *                       -------------------
 *  begin                : Sun Apr 20 2025
 *  copyright            : (C) 2025 by fm
 *  email                : frank.meyer@cablelink.at
 *
 *      POSIX implementation of a geomagnetic field tracking tool;
 *      this source file implements the data capture from the magnetic
 *      sensor and stores the data in ordered files
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#include <unistd.h>
#include <malloc.h>
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <ctype.h>
#include <signal.h>
#include <string.h>
#include <errno.h>
#include <sys/mman.h>
#include <time.h>

#include <linux/i2c.h>
#include <linux/i2c-dev.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/time.h>

#define _GMT_DATA_      /* declare const tables here */
#include "gmt.h"

// ----- data definitions -----
#define I2C_NAME_BASE       "/dev/i2c-"

/* struct to pass certain parameters to a thread
 */
typedef struct
{
    int            ifh;          /* i2c file handle               */
    int            ofh;          /* streaming pipe file handle    */
    unsigned long  vcount;       /* number values / i2c reads     */
    unsigned long  ecount;       /* number of i2c read errors     */
    magnBuffer     vBuf;         /* sensor value buffer           */
    uchar          addr;         /* i2c slave device address      */
    int            axes;         /* axis sampling configuration   */
    double         fullScale;    /* configured fullscale value    */
    double         scaleVal;     /* physical scale value          */
    double         dx;           /* scaled double values per axis */
    double         dy;
    double         dz;
}
sampler_cfg;

// -------- Prototypes --------

static void   initDefaultCfg   (elfSenseConfig *pcfg);
static int    getConfig        (elfSenseConfig *pecfg, deviceConfig *dcfg);
static void   setSensorConfig  (elfSenseConfig *pecfg, deviceConfig *dcfg);
static int    setupSensor      (deviceConfig *dcfg, int ifh);
static int    i2c_write        (uchar slave_addr, uchar reg, uchar data, int ifh);
static int    i2c_readMagn     (magnBuffer *mBuf, uchar slave_addr, int ifh);
static int    gmSample         (sampler_cfg *gmdata);
static int    writeData        (sampler_cfg *gmdata);

static void   exithandler      (int signumber);

#if 0
static void   updateRLog       (void);
static void   printHelp        (char **);
#endif

extern FILE  *openCfgfile      (char *name);
extern int    getstrcfgitem    (FILE *pf, char *pItem, char *ptarget);
extern int    getintcfgitem    (FILE *pf, char *pItem, int *pvalue);

#ifdef __SIMULATION__
  #define localtime   sim_localtime
  static struct tm   *sim_localtime (const time_t *timep);
#endif

// -------- global variables --------

static runtime_log     rLog = {0, 0, 0};
static char            devName[64] = {'\0'};
static int             iDev        = 0;               /* i2c device handle */
static unsigned short  dData[MINS_PER_DAY][GMT_AXES];
static char            datapath[FILENAME_MAXSIZE] = { GMT_DATA_PATH };

#ifdef __SIMULATION__
  /* in simulation mode, run max 5 minutes */
  #define __SIM_TX_COUNT__        (5000) // * DEFAULT_SAMPLE_FREQUENCY)
static int        txcount         = 0;
#endif

/* work data for the sampler
 */
static sampler_cfg  cbData      = {0};

/*  The concept is simple - take a sensor sample once a minute, and
 *  eventually store it in a file. Timing is thus not of very critical
 *  importance, and simple date/time function and sleep calls are fine to
 *  achieve a sufficient accuracy.
 *  To keep long-time deviations low, the current time is evaluated after
 *  each measurement, and the remainder to the next full minute is used
 *  as parameter for the sleep() call.
 *  Currently, the sampled data are saved to a file and additionally kept
 *  in memory for one day- just because we can.
 *  At a later point, direct TCP access and query for data values might
 *  be implemented.
 */
// *****************************Code************************************


// main program
int  main (int argc, char **argv)
{
    elfSenseConfig  escfg;
    deviceConfig    dcfg;
    time_t          t;
    struct tm      *ptime;
    int             i;

    /* open and read the configuration:
     * sensor device  [default = LSM303]
     * - i2c bus      [default = 1]
     * - sample rate  [default = 1.5Hz/LSM303]
     */
    initDefaultCfg (&escfg);

    /* need to load a "dynamic" configuration here later */
#if 1
    getConfig (&escfg, &dcfg);
#endif

#ifndef __SIMULATION__

    /* make device name, and open */
    sprintf (devName, "%s%d",I2C_NAME_BASE, escfg.i2cBus);
    if ((iDev = open (devName, O_RDWR)) < 0)
    {
        perror (devName);
        return 10;
    }

    /* some general i2c settings... */
    ioctl (iDev, I2C_TENBIT, 0);    // 10-bit addressing off
    ioctl (iDev, I2C_RETRIES, 5);

    /* need to dynamically assign the sensor device here later */
    setSensorConfig (&escfg, &dcfg);

    /* configure and start the sensor */
    i = setupSensor (&dcfg, iDev);
    if (i != 0)
    {
        printf ("setupSensor() ret = %d, error !\n", i);
        fflush (stdout);
        return 20;
    }

#else
    escfg.fullScale = FS_VALUE_LSM303;
    fprintf (stdout, "run in simulation mode, with random data !\n");
#endif    /* __simulation__ */


    /* set exit handler */
    signal (SIGTERM, exithandler);


    /* initialize/copy some "work" data */
    cbData.ifh       = iDev;
    cbData.addr      = dcfg.dev_addr;
    cbData.axes      = escfg.sampleAxes;
    cbData.fullScale = (escfg.device == GMT_DEVICE_LSM303) ? FS_VALUE_LSM303 : FS_VALUE_HMC5883;
    cbData.scaleVal  = cbData.fullScale / SHORT_MAX_DBL;

    /* clear out, just to be safe */
    memset (dData, 0, sizeof (dData));

    /* prepare to enter the main loop;
     * first, get near the next minute mark */

#ifdef __SIMULATION__
     usleep (300000);
#else
 /* use the current time */
    t     = time (NULL);
    ptime = localtime (&t);

    if ((ptime->tm_sec > 5) && (ptime->tm_sec < 55))
        sleep (60 - ptime->tm_sec);
#endif

    /* main loop; sample and save once a minute */
    do
    {
        gmSample  (&cbData);
fputc ('+', stdout); fflush (stdout);
        writeData (&cbData);

        i = 60 * ptime->tm_hour + ptime->tm_min;
        dData[i][DI_X] = cbData.dx;
        dData[i][DI_Y] = cbData.dy;
        dData[i][DI_Z] = cbData.dz;

#ifdef __SIMULATION__
        usleep (120000);
#else
        t     = time (NULL);
        ptime = localtime (&t);
        sleep (60 - ptime->tm_sec);
#endif
    }
    while (1);

    return 0;
}



#if 0
static void  printHelp (char **argv)
{
    printf ("\n -- geomagnetism monitoring program --\n");
}
#endif


static void  initDefaultCfg (elfSenseConfig *pcfg)
{
    pcfg->device     = GMT_DEFAULT_DEVICE;
    pcfg->i2cBus     = GMT_DEFAULT_BUS;
    pcfg->sampleRate = GMT_DEFAULT_OD_RATE;
    pcfg->sampleAxes = GMT_AXIS_USE_X | GMT_AXIS_USE_Y | GMT_AXIS_USE_Z;  /* all axes */
}



/* a stub at the moment; to implement later !
 *  - bus (default: 0)
 *  - device (default: ST LSM303DLHC)
 *  - OD rate (default: device max.)
 *  - axes used (default: Z only)
 *  - auto-termination time (default: 0 / none)
 */
static int  getConfig (elfSenseConfig *pecfg, deviceConfig *dcfg)
{
    FILE *pcf;
    char  pd[128], px[128];
    int   bus, rate, i, k;

    if (!(pcf = openCfgfile (GMT_CFG)))
    {
        printf ("\n no config file (%d) !", errno);
        return 0;
    }

    /* string items */
    if ((i = getstrcfgitem  (pcf, GMT_CFG_DEVICE, pd)))
    {
        if (strstr (pd, GMT_CFG_DEV_LSM303))
        {
            pecfg->device = GMT_DEVICE_LSM303;
            setSensorConfig (pecfg, dcfg);
        }
        else if (strstr (pd, GMT_CFG_DEV_HMC5883))
        {
            pecfg->device = GMT_DEVICE_HMC5883;
            setSensorConfig (pecfg, dcfg);
        }
    }

    printf ("\nconfigured device = <%s>", pd);

    /* data output file path; set directly */
    if ((k = getstrcfgitem  (pcf, GMT_CFG_DATAPATH, px)))
    {
        if (strlen (px) > 0)
        {
            strncpy (datapath, px, FILENAME_MAXSIZE);
            datapath[FILENAME_MAXSIZE-1] = '\0';
            printf ("\noutput file path  = <%s>", datapath);
        }
    }

    /* data output mode; axes separately, or vector sum;
     * vector sum option not yet implemented */
    if ((k = getstrcfgitem  (pcf, GMT_CFG_MODE, px)))
    {
        pecfg->outputMode = GMT_AXIS_ALL;
        if (strstr (px, GMT_MD_SUM))
            pecfg->outputMode = GMT_AXIS_SUM;
    }

    printf ("\nmode config       = <%s>", px);

    /* integer items */
    bus = rate = -1;
    if ((i = getintcfgitem  (pcf, GMT_CFG_BUS, &bus)))
        pecfg->i2cBus = bus;

    if (i)
        printf ("\nbus               = <%d>", bus);
    else
        printf ("\nbus not configured");
    if (k)
        printf ("\nrate              = <%d>\n", rate);
    else
        printf ("\nOD rate not configured\n");

    return 0;
}



/* sensor-specific device configuration setup;
 * the sample rate value is a table index
 */
static void  setSensorConfig (elfSenseConfig *pecfg, deviceConfig *dcfg)
{
    if (pecfg->device == GMT_DEVICE_LSM303)
    {
        dcfg->dev_addr   = DEVICE_ADDRESS_LSM303;
        dcfg->adr_cra    = (unsigned char) (pecfg->sampleRate << 2);  /* bits 4..2 */
        dcfg->adr_crb    = 0x01;
        dcfg->adr_mr     = 0x02;
        dcfg->regm_cra   = 0x1C;
        dcfg->regm_crb   = 0x20;
        dcfg->regm_mr    = 0x00;
        pecfg->fullScale = FS_VALUE_LSM303;
    }
    else
    {
        dcfg->dev_addr   = DEVICE_ADDRESS_HMC5883;
        dcfg->adr_cra    = (unsigned char) (pecfg->sampleRate << 2);
        dcfg->adr_crb    = 0x01;
        dcfg->adr_mr     = 0x02;
        dcfg->regm_cra   = 0x10;
        dcfg->regm_crb   = 0x00;
        dcfg->regm_mr    = 0x00;
        pecfg->fullScale = FS_VALUE_HMC5883;
    }
}



/* configure and start the sensor,
 * setting up the i2c config registers CRA, RCB and MR
 * return 0 if everything went fine,
 * or an error number > 0
 */
static int  setupSensor (deviceConfig *dcfg, int ifh)
{
    if (i2c_write (dcfg->dev_addr, dcfg->adr_cra, dcfg->regm_cra, ifh) < 0)
        return 1;

    if (i2c_write (dcfg->dev_addr, dcfg->adr_crb, dcfg->regm_crb, ifh) < 0)
        return 2;

    if (i2c_write (dcfg->dev_addr, dcfg->adr_mr, dcfg->regm_mr, ifh) < 0)
        return 3;

    return 0;
}



// Write to an I2C slave device's register
static int  i2c_write (uchar slave_addr, uchar reg, uchar data, int ifh)
{
    uchar                       outbuf[2];
    struct i2c_msg              msgs[1];
    struct i2c_rdwr_ioctl_data  msgset[1];

    outbuf[0] = reg;
    outbuf[1] = data;

    msgs[0].addr  = slave_addr;
    msgs[0].flags = 0;
    msgs[0].len   = 2;
    msgs[0].buf   = outbuf;

    msgset[0].msgs  = msgs;
    msgset[0].nmsgs = 1;

    if (ioctl (ifh, I2C_RDWR, &msgset) < 0)
    {
        perror("i2c_write");
        return -1;
    }

    return 0;
}



/* read the magnetometer data
 * returns 0 if read was ok,
 * or != 0 (1) on error
 */
static int  i2c_readMagn (magnBuffer *mBuf, uchar slave_addr, int ifh)
{
    short                       hh, hl;
    uchar                       regadr;
    uchar                       inbuf[6] = {0};  // clear
    struct i2c_msg              msgs[2];
    struct i2c_rdwr_ioctl_data  msgset[1];

    regadr = 0x03;  // register address of OUT_X_H_M
    msgs[0].addr  = slave_addr;
    msgs[0].flags = 0;
    msgs[0].len   = 1;
    msgs[0].buf   = &regadr;

    msgs[1].addr  = slave_addr;
    msgs[1].flags = I2C_M_RD | I2C_M_NOSTART;
    msgs[1].len   = 6;
    msgs[1].buf   = inbuf;

    msgset[0].msgs  = msgs;
    msgset[0].nmsgs = 2;

    if (ioctl (ifh, I2C_RDWR, &msgset) < 0)
    {
        perror ("i2c_readMagn");
        return 1;
    }

    hh = inbuf[0];
    hl = inbuf[1];
    mBuf->mgnX = (short) ((hh << 8) + hl);
    hh = inbuf[2];
    hl = inbuf[3];
    mBuf->mgnY = (short) ((hh << 8) + hl);
    hh = inbuf[4];
    hl = inbuf[5];
    mBuf->mgnZ = (short) ((hh << 8) + hl);
    return 0;
}



/* magnetometer data sampling code;
 * read <n> samples of every axis, average, and return them;
 * return value is the number of samples taken & averaged;
 * value <n> is defined at compile time
 */
static int  gmSample  (sampler_cfg *gmdata)
{
    double      x, y, z, div;
    int         i, r, valid[GMT_AVG_COUNT];
    databuffer  buffer;
    magnBuffer  vBuf;

    memset (valid, 0, GMT_AVG_COUNT * sizeof (int));

#ifndef __SIMULATION__
    for (i=0; i<GMT_AVG_COUNT; i++)
    {
        if (i2c_readMagn (&vBuf, gmdata->addr, gmdata->ifh) == 0)
        {
            buffer.x[i] = vBuf.mgnX * gmdata->scaleVal;
            buffer.y[i] = vBuf.mgnY * gmdata->scaleVal;
            buffer.z[i] = vBuf.mgnZ * gmdata->scaleVal;
            valid[i]    = 1;
        }
        if (i<(GMT_AVG_COUNT-1))  // wait a bit between samples
            usleep (250000);
    }

#else  /* simulation; no need for averaging */

    for (i=0; i<GMT_AVG_COUNT; i++)
    {
        gmdata->dx = (2.0 * drand48() - 1.0) * gmdata->scaleVal;
        gmdata->dy = (2.0 * drand48() - 1.0) * gmdata->scaleVal;
        gmdata->dz = (2.0 * drand48() - 1.0) * gmdata->scaleVal;
    }
    return 3;

#endif

    /* average valid data */
    x = y = z = div = 0.0;
    r = 0;
    for (i=0; i<GMT_AVG_COUNT; i++)
    {
        if (valid[i])
        {
            r++;
            div += 1.0;
            x += buffer.x[i];
            y += buffer.y[i];
            z += buffer.z[i];
        }
    }

    if (div > 0.0)
    {
        x /= div;
        y /= div;
        z /= div;
    }

    /* copy data */
    gmdata->dx = x;
    gmdata->dy = y;
    gmdata->dz = z;
    gmdata->vcount++;

    return (r);
}



/* update the runtime counter;
 * called every minute, and counts up on this base
 */
static void  updateRLog (void)
{
    rLog.minutes++;
    if (rLog.minutes >= 60)
    {
        rLog.minutes = 0;
        rLog.hours++;
    }
    if (rLog.hours >= 24)
    {
        rLog.hours = 0;
        rLog.days++;
    }
}



/* handler for the exit / break signals
 */
static void  exithandler (int signumber)
{
    if (iDev)
        close (iDev);
    iDev = 0;

}


/* save current data to file;
 * return 0 if writing was ok
 * an error number otherwise
 */
static int  writeData (sampler_cfg *gmdata)
{
    FILE        *hFile;
    char         fbuf[256];
    time_t       t;
    struct tm   *ptime;
    struct stat  st = { 0 };
    static int   dsCount = 0;  /* dataset counter */
    static int   mday    = 0;  /* last recent day, 'invalid' start value */

    /* use the current time */
    t     = time (NULL);
    ptime = localtime (&t);

    /* create data files in a sub-directory; might need to create the directory first...
     * have to set "executable" rights, or else creating files fails */
    if (stat (GMT_DATA_PATH, &st) == -1)
    {
        if (mkdir (GMT_DATA_PATH, S_IRUSR | S_IWUSR | S_IXUSR | S_IRGRP | S_IWGRP | S_IXGRP) != 0)
            perror ("creating data directory");
    }

    /* for the moment, just create a file in the local sub-folder;
     * using the current date as name automatically creates a new file each day;
     * and for one write access per minute, open/close it each time is fine */
    sprintf (fbuf, "%s/%4d_%02d_%02d.dat", GMT_DATA_PATH, ptime->tm_year + 2000, ptime->tm_mon+1, ptime->tm_mday);
    if (!(hFile = fopen (fbuf, "a+")))
    {
        perror ("accessing data file");
        return 1;
    }

    /* reset dataset counter upon date transit */
    if (ptime->tm_mday != mday)
        dsCount = 0;

    /* write header to (each) output file once */
    if (!dsCount)
    {
        sprintf (fbuf, "# -- geomagnetism data, per minute --\n");
        fputs (fbuf, hFile);
        sprintf (fbuf, "#start time : %02d.%02d.%4d, %02d:%02d\n", ptime->tm_mon+1, ptime->tm_mday,
             ptime->tm_year + 1900, ptime->tm_hour, ptime->tm_min);
        fputs (fbuf, hFile);
        sprintf (fbuf, "# format :\n# HH:MM, X_data, Y_data, Z_data\n");
        fputs (fbuf, hFile);
        sprintf (fbuf, "# fullscale value = %.5lf Ga\n", gmdata->fullScale);
        fputs (fbuf, hFile);
        fflush (hFile);
    }
    dsCount++;
    mday = ptime->tm_mday;

    /* write data */
    sprintf (fbuf, "%02d:%02d, %.5lf, %.05lf, %05lf\n", ptime->tm_hour, ptime->tm_min, gmdata->dx, gmdata->dy, gmdata->dz);
    fputs (fbuf, hFile);

    fflush (hFile);
    fclose (hFile);
    return 0;
}



/* a debug function to "speed up" simulated runs
 * does <not> try to emulate fully compatible behavior !
 */
static struct tm  *sim_localtime (const time_t *timep)
{
    static struct tm  lstime;
    static int        ccount = 0;

    /*  on first call, get the actual time, and keep a copy */
    if (ccount == 0)
    {
        lstime.tm_sec  = 0;
        lstime.tm_min  = 1;
        lstime.tm_hour = 13;
        lstime.tm_mday = 4;
        lstime.tm_mon  = 4;
        lstime.tm_year = 25;
        ccount++;
        return (&lstime);
    }

    /* on subsequent calls, just modify the local copy
     * and advance the minute */
    else
    {
        lstime.tm_sec = 0;
        lstime.tm_min++;
        if (lstime.tm_min >= 60)
        {
            lstime.tm_min = 0;
            lstime.tm_hour++;
        }
        if (lstime.tm_hour >= 23)
        {
            lstime.tm_hour = 0;
            lstime.tm_mday++;
        }
        if (lstime.tm_mday >= 31)
        {
            lstime.tm_mday = 1;
            lstime.tm_mon++;
        }
        if (lstime.tm_mon >= 12)
            lstime.tm_year++;
    }
    return (&lstime);
}



/* a debug function to "speed up" simulated runs
 * does <not> try to emulate fully compatible behavior !
 */
static unsigned int sim_sleep (unsigned int seconds)
{
    usleep (seconds * 100);
    return 0;
}
