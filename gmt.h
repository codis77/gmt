// **************************Definitions********************************

// #define ENABLE_DEBUG


/* supported magnetometer sensor devices */
#define GMT_DEVICE_LSM303       0
#define GMT_DEVICE_HMC5883      1

#define DEVICE_ADDRESS_LSM303   (0x3C >> 1)
#define DEVICE_ADDRESS_HMC5883  (0x3C >> 1)

/* supported/used output rates (per sensor) */
/*     LSM303DLHC
 * Rate Hz)| 0.75 | 1.5 | 3.0 | 7.5 |  15 |  30 |  75 | 220
 * --------|------+-----+-----+-----+-----+-----+-----+-----
 * OD-Bits | 0x0  | 0x1 | 0x2 | 0x3 | 0x4 | 0x5 | 0x6 | 0x7
 */
/*     HMC5883L
 * Rate Hz)| 0.75 | 1.5 | 3.0 | 7.5 |  15 |  30 |  75 | ---
 * --------|------+-----+-----+-----+-----+-----+-----+-----
 * OD-Bits | 0x0  | 0x1 | 0x2 | 0x3 | 0x4 | 0x5 | 0x6 | 0x7
 */
#define OD_LSM303_SHIFT         2        /* shift 0 bits (GN = bit 4..2)*/
#define OD_HMC5883_SHIFT        2        /* shift 0 bits (GN = bit 4..2)*/

#define GMT_OD_RATE_LSM303      1        /* table index, 1.5Hz */
#define GMT_OD_RATE_HMC5883     1        /* table index, 1.5Hz */

/* axes to sample; OR-ed into one value */
#define GMT_AXIS_USE_X          0x01
#define GMT_AXIS_USE_Y          (0x01 << 1)
#define GMT_AXIS_USE_Z          (0x01 << 2)

/* config string / register value tables, output data rate
 * attention: HMC5883 supports max. 75Hz (0x07 not supported !)
 */
#ifdef _GMT_DATA_
const double         OD_rate_rtable[8] = {0.75,  1.5,  3.0,  7.5, 15.0, 30.0, 75.0, 220.0};
const unsigned char  OD_rate_vtable[8] = {0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07 };
#endif

/* supported/used fullscle values (per sensor) */
/*    LSM303DLHC
 * FS  (G) |  1.3 | 1.9 | 2.5 | 4.0 | 4.7 | 5.6 | 8.1
 * --------|------+-----+-----+-----+-----+-----+------
 * GN-Bits |  0x1 | 0x2 | 0x3 | 0x4 | 0x5 | 0x6 | 0x7
 *
 *    HMC5883L
 * FS  (G) | 0.88 | 1.3 | 1.9 | 2.5 | 4.0 | 4.7 | 5.6 | 8.1
 * --------|------+-----+-----+-----+-----+-----+-----+-----
 * GN-Bits |  0x0 | 0x1 | 0x2 | 0x3 | 0x4 | 0x5 | 0x6 | 0x7
 */
#define FS_LSM303_SHIFT          4        /* shift 4 bits (GN = bit 7..4)*/
#define FS_HMC5883_SHIFT         5        /* shift 5 bits (GN = bit 7..5)*/

/* used full-scale values */
#define FS_VALUE_LSM303          1.3
#define FS_VALUE_HMC5883         0.88

/* config string / register value tables, fullscale value;
 * the config strings have to be given as is in the config file;
 * values for the LSM303 and the HMC5883 are kept in different arrays
 */
#ifdef _GMT_DATA_
const char           FS_ntableLSM[7][8] = { "1.3G", "1.9G", "2.5G", "4.0G", "4.7G", "5.6G", "8.1G"};
const unsigned char  FS_vtableLSM[7]    = {  0x01,    0x02,   0x03,   0x04,   0x05,   0x06,  0x07 };
const char           FS_ntableHMC[8][8] = {"0.88G", "1.3G", "1.9G", "2.5G", "4.0G", "4.7G", "5.6G", "8.1G"};
const unsigned char  FS_vtableHMC[8]    = {   0x00,   0x01,   0x02,   0x03,   0x04,   0x05,   0x06,  0x07 };
#endif

/* number i2c bus used, a device as /dev/i2c-*;
 * default is 1, i.e. '/dev/i2c-1' */
#define GMT_DEFAULT_BUS          1

#define GMT_DEFAULT_DEVICE       GMT_DEVICE_LSM303
#if (GMT_DEFAULT_DEVICE == GMT_DEVICE_LSM303)
  #define GMT_DEFAULT_OD_RATE    GMT_OD_RATE_LSM303
#else
  #define GMT_DEFAULT_OD_RATE    GMT_OD_RATE_HMC5883
#endif
#define GMT_DEFAULT_AXIS         GMT_AXIS_USE_Z

/* maximal rate = maximal block buffer size */
#define GMT_MAX_OD_RATE          GMT_OD_RATE_LSM303

/* number of consecutive samples, averaged to one value */
#define GMT_AVG_COUNT            3

#define GMT_PN_SIZE              64

/* internal data storage */
#define MINS_PER_DAY             1400  /* 60 minutes * 24 hours */
#define GMT_AXES                 3
#define DI_X                     0
#define DI_Y                     1
#define DI_Z                     2

typedef unsigned char   uchar;

/* sensor scan application config
 */
typedef struct
{
    int     device;                       /* sensor device type */
    int     i2cBus;                       /* i2c bus number     */
    int     sampleRate;                   /* sampling rate      */
    int     sampleAxes;                   /* axes to sample     */
    double  fullScale;                    /* fullscale value    */
    char    outputPipe[GMT_PN_SIZE];      /* default pipe name  */
}
elfSenseConfig;


/* i2c sensor config;
 * the LSM303DLHC and HMC5883L are identical so far
 */
typedef struct
{
    unsigned char  dev_addr;              /* device i2c address   */
    unsigned char  adr_cra;               /* address register CRA */
    unsigned char  adr_crb;               /* address register CRB */
    unsigned char  adr_mr;                /* address register MR  */
    unsigned char  regm_cra;              /* value register CRA   */
    unsigned char  regm_crb;              /* value register CRB   */
    unsigned char  regm_mr;               /* value register MR    */
}
deviceConfig;

typedef struct
{
    short          mgnX;                  /* X axis data, 16-bit  */
    short          mgnY;                  /* X axis data, 16-bit  */
    short          mgnZ;                  /* X axis data, 16-bit  */
    unsigned char  ctrlb;                 /* CTRL reg. B value    */
}
magnBuffer;

typedef struct
{
    unsigned int  days;
    unsigned int  hours;
    unsigned int  minutes;
}
runtime_log;

typedef struct
{
    double   x[GMT_AVG_COUNT];
    double   y[GMT_AVG_COUNT];
    double   z[GMT_AVG_COUNT];
}
databuffer;


/* --- sampler task states (shm variable) ---
 */
#define STS_OFF_INIT                0x00   /* initialized or starting up    */
#define STS_READY                   0x01   /* task started, no sampling yet */
#define STS_RUNNING                 0x02   /* task up, and running running  */
#define STS_ERROR                   0xF0   /* error, sampling has stopped   */
#define STS_TERMINATING             0xFF   /* sampler task is terminating   */

/* --- shared memory buffer states ---
 */
#define BUF_DORMANT                 0x00
#define BUF_SMPL_ACTIVE             0x01
#define BUF_DREADY                  0x02
#define BUF_PROCESSING              0x04

/* --- data header / runtime data ---
 */
#define ELFD_HEADER_ID              "#ESD"
#define ELFD_DTYPE_FLOAT            'F'
#define ELFD_DTYPE_INT              'I'

/* --- runtime-loaded config files ---
 */
#define ELFWATCH_CFG                "./elfwatch.config"
#define ELFDP_CFG                   "./elfdp.config"
#define ELFIMAGE_CFG                "./elfimage.config"
#define ELFMSPEC_CFG                "./elfwmspec.config"

/* elfwatch config */
#define CFG_STR_MAX                 256
#define ELFW_CFG_BUS                "I2C_BUS"
#define ELFW_CFG_DEVICE             "DEVICE"
#define ELFW_CFG_AXES               "AXES"

#define ELFW_CFG_DEV_LSM303         "LSM303"
#define ELFW_CFG_DEV_HMC5883        "HMC5883"
#define GMT_AXIS_X                  'X'
#define GMT_AXIS_Y                  'Y'
#define GMT_AXIS_Z                  'Z'

#define LB_SIZE                     2048

#define GMT_PCK_INVALID             0
#define GMT_PCK_IS_HEADER           1
#define GMT_PCK_IS_DATA             2

#define BLOCKS_PER_CONVERSION       5

#define DEFAULT_SAMPLE_FREQUENCY    200  /* 200Hz           */
#define DEFAULT_BITSPERSAMPLE       16   /* 16Bits per item */

#define SHORT_MAX_DBL               32767.0

#define GMT_DATA_PATH               "./data"
#define FILENAME_MAXSIZE            256
#define FILENAME_BASE               "./specData"
#define FILENAME_SIZE               64     /* used name string limit*/

#define MAX_MISSED_DATA_COUNT       3

/* -------- UDP network settings --------
 */
#define GMT_UDP_PORTBASE            10000
#define GMT_UDP_PORTOFFSET_SEC      2
#define GMT_UDP_PORTOFFSET_MH       3
#define GMT_UDP_DATA_SECONDS        (GMT_UDP_PORTBASE + GMT_UDP_PORTOFFSET_SEC)
#define GMT_UDP_DATA_MIN_HOURS      (GMT_UDP_PORTBASE + GMT_UDP_PORTOFFSET_MH)

#define GMT_DEFAULT_IP              "127.0.0.1"      /* default to local host */
