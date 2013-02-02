#if !defined(EXTPOWERCTL_DRV_H)
#define EXTPOWERCTL_DRV_H


// Debug zones
// Even on Release Mode, We don't care about GPE message
#define ZONEID_FATAL                0
#define ZONEID_ERROR                1
#define ZONEID_WARNING              2
#define ZONEID_MESSAGE              3
#define ZONEID_VERBOSE              4
#define ZONEID_CALLTRACE            5
#define ZONEID_ALLOC                6
#define ZONEID_FLIP                 7

#define ZONEMASK_FATAL         (1 << ZONEID_FATAL)
#define ZONEMASK_ERROR         (1 << ZONEID_ERROR)
#define ZONEMASK_WARNING       (1 << ZONEID_WARNING)
#define ZONEMASK_MESSAGE       (1 << ZONEID_MESSAGE)
#define ZONEMASK_VERBOSE       (1 << ZONEID_VERBOSE)
#define ZONEMASK_CALLTRACE     (1 << ZONEID_CALLTRACE)
#define ZONEMASK_ALLOC         (1 << ZONEID_ALLOC)
#define ZONEMASK_FLIP          (1 << ZONEID_FLIP)



#define DEFAULT_MSG_LEVEL        (ZONEMASK_FATAL|ZONEMASK_ERROR|ZONEMASK_WARNING)//|ZONEMASK_MESSAGE|ZONEMASK_VERBOSE)


typedef enum
{
	LCD_ON_SET,
    USB_ON_SET,
    CAMERA_ON_SET,
    DAC0_ON_SET,
    DAC1_ON_SET,
	ETH_ON_SET,
	WIFI_ON_SET,
	LCD_OFF_SET,
    USB_OFF_SET,
    CAMERA_OFF_SET,
    DAC0_OFF_SET,
    DAC1_OFF_SET,
	ETH_OFF_SET,
	WIFI_OFF_SET,
	ALL_ON_SET,
	ALL_OFF_SET,
	AWAKE_SET,
	SLEEP_SET,
    EPCTL_MAX
} EPCTL_SET;

typedef enum
{
    EPCTL_SUCCESS,  
    EPCTL_ERROR_XXX
} EPCTL_ERROR;



#endif