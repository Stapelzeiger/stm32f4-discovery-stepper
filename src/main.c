#include "ch.h"
#include "hal.h"
#include "test.h"

#include "chprintf.h"
#include "shell.h"


#include "usbcfg.h"

/* Virtual serial port over USB.*/
SerialUSBDriver SDU1;


#define SHELL_WA_SIZE     THD_WORKING_AREA_SIZE(2048)
#define TEST_WA_SIZE        THD_WORKING_AREA_SIZE(256)

static void cmd_mem(BaseSequentialStream *chp, int argc, char *argv[]) {
    size_t n, size;

    (void)argv;
    if (argc > 0) {
        chprintf(chp, "Usage: mem\r\n");
        return;
    }
    n = chHeapStatus(NULL, &size);
    chprintf(chp, "core free memory : %u bytes\r\n", chCoreGetStatusX());
    chprintf(chp, "heap fragments   : %u\r\n", n);
    chprintf(chp, "heap free total  : %u bytes\r\n", size);
}

static void cmd_threads(BaseSequentialStream *chp, int argc, char *argv[]) {
    static const char *states[] = {CH_STATE_NAMES};
    thread_t *tp;

    (void)argv;
    if (argc > 0) {
        chprintf(chp, "Usage: threads\r\n");
        return;
    }
    chprintf(chp, "    addr    stack prio refs     state\r\n");
    tp = chRegFirstThread();
    do {
        chprintf(chp, "%08lx %08lx %4lu %4lu %9s\r\n",
                         (uint32_t)tp, (uint32_t)tp->p_ctx.r13,
                         (uint32_t)tp->p_prio, (uint32_t)(tp->p_refs - 1),
                         states[tp->p_state]);
        tp = chRegNextThread(tp);
    } while (tp != NULL);
}

static void cmd_test(BaseSequentialStream *chp, int argc, char *argv[]) {
    thread_t *tp;

    (void)argv;
    if (argc > 0) {
        chprintf(chp, "Usage: test\r\n");
        return;
    }
    tp = chThdCreateFromHeap(NULL, TEST_WA_SIZE, chThdGetPriorityX(),
                                                     TestThread, chp);
    if (tp == NULL) {
        chprintf(chp, "out of memory\r\n");
        return;
    }
    chThdWait(tp);
}

static const ShellCommand commands[] = {
    {"mem", cmd_mem},
    {"threads", cmd_threads},
    {"test", cmd_test},
    {NULL, NULL}
};

static const ShellConfig shell_cfg1 = {
    (BaseSequentialStream *)&SDU1,
    commands
};



#define PWM_MAX 128

#define STEPPER_BLUE    0
#define STEPPER_PINK    1
#define STEPPER_YELLOW  2
#define STEPPER_ORANGE  3

static THD_WORKING_AREA(wa_stepper_thd, 128);
static THD_FUNCTION(_stepper_thd, arg)
{
    pwmEnableChannel(&PWMD3, 0, 0);
    pwmEnableChannel(&PWMD3, 1, 0);
    pwmEnableChannel(&PWMD3, 2, 0);
    pwmEnableChannel(&PWMD3, 3, 0);
    // 4096 steps per rev

    while (1) {
        pwmEnableChannel(&PWMD3, STEPPER_ORANGE, PWM_MAX);
        chThdSleepMilliseconds(1);
        pwmEnableChannel(&PWMD3, STEPPER_YELLOW, PWM_MAX);
        chThdSleepMilliseconds(1);
        pwmEnableChannel(&PWMD3, STEPPER_ORANGE, 0);
        chThdSleepMilliseconds(1);
        pwmEnableChannel(&PWMD3, STEPPER_PINK, PWM_MAX);
        chThdSleepMilliseconds(1);
        pwmEnableChannel(&PWMD3, STEPPER_YELLOW, 0);
        chThdSleepMilliseconds(1);
        pwmEnableChannel(&PWMD3, STEPPER_BLUE, PWM_MAX);
        chThdSleepMilliseconds(1);
        pwmEnableChannel(&PWMD3, STEPPER_PINK, 0);
        chThdSleepMilliseconds(1);
        pwmEnableChannel(&PWMD3, STEPPER_ORANGE, PWM_MAX);
        chThdSleepMilliseconds(1);
        pwmEnableChannel(&PWMD3, STEPPER_BLUE, 0);
    }
    return 0;
}




int main(void) {

    halInit();
    chSysInit();


    shellInit();

    sduObjectInit(&SDU1);
    sduStart(&SDU1, &serusbcfg);

    usbDisconnectBus(serusbcfg.usbp);
    chThdSleepMilliseconds(1000);
    usbStart(serusbcfg.usbp, &usbcfg);
    usbConnectBus(serusbcfg.usbp);


    static const PWMConfig pwmcfg = {
        20000, // PWM freq
        PWM_MAX,   // period [cycles]
        NULL,
        {
         {PWM_OUTPUT_ACTIVE_HIGH, NULL},
         {PWM_OUTPUT_ACTIVE_HIGH, NULL},
         {PWM_OUTPUT_ACTIVE_HIGH, NULL},
         {PWM_OUTPUT_ACTIVE_HIGH, NULL}
        },
        0,
        0
    };

    // timer 3 (alternate 2)
    pwmStart(&PWMD3, &pwmcfg);
    palSetPadMode(GPIOB, GPIOB_PIN4, PAL_MODE_ALTERNATE(2)); // TIM_CH1 (blue)
    palSetPadMode(GPIOB, GPIOB_PIN5, PAL_MODE_ALTERNATE(2)); // TIM_CH2 (pink)
    palSetPadMode(GPIOB, GPIOB_PIN0, PAL_MODE_ALTERNATE(2)); // TIM_CH3 (yellow)
    palSetPadMode(GPIOB, GPIOB_PIN1, PAL_MODE_ALTERNATE(2)); // TIM_CH4 (orange)

    chThdCreateStatic(wa_stepper_thd, sizeof(wa_stepper_thd), NORMALPRIO + 10, _stepper_thd, NULL);

    thread_t *shelltp = NULL;
    while (TRUE) {
        if (!shelltp) {
            if (SDU1.config->usbp->state == USB_ACTIVE) {
                shelltp = shellCreate(&shell_cfg1, SHELL_WA_SIZE, NORMALPRIO);
            }
        }
        else {
            if (chThdTerminatedX(shelltp)) {
                chThdRelease(shelltp);
                shelltp = NULL;
            }
        }
        chThdSleepMilliseconds(500);
    }
}
