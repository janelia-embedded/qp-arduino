/*$file${.::dpp.ino} vvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvv*/
/*
* Model: dpp.qm
* File:  ${.::dpp.ino}
*
* This code has been generated by QM 4.5.1 (https://www.state-machine.com/qm).
* DO NOT EDIT THIS FILE MANUALLY. All your changes will be lost.
*
* This program is open source software: you can redistribute it and/or
* modify it under the terms of the GNU General Public License as published
* by the Free Software Foundation.
*
* This program is distributed in the hope that it will be useful, but
* WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
* or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License
* for more details.
*/
/*$endhead${.::dpp.ino} ^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^*/
#include "qpn.h"     // QP-nano framework
#include "Arduino.h" // Arduino API

Q_DEFINE_THIS_MODULE("dpp")

//============================================================================
enum DPPSignals {
    EAT_SIG = Q_USER_SIG, // posted by Table to let a philosopher eat
    DONE_SIG,             // posted by Philosopher when done eating
    PAUSE_SIG,            // posted by BSP to pause the application
    SERVE_SIG,            // posted by BSP to pause the application
    HUNGRY_SIG,           // posted to Table from hungry Philo
    MAX_SIG               // the last signal
};

enum {
    N_PHILO = 5 // number of Philosophers
};

//============================================================================
// declare all AO classes...
/*$declare${AOs::Philo} vvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvv*/
/*${AOs::Philo} ............................................................*/
typedef struct Philo {
/* protected: */
    QActive super;
} Philo;

/* protected: */
static QState Philo_initial(Philo * const me);
static QState Philo_thinking(Philo * const me);
static QState Philo_hungry(Philo * const me);
static QState Philo_eating(Philo * const me);
/*$enddecl${AOs::Philo} ^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^*/
/*$declare${AOs::Table} vvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvv*/
/*${AOs::Table} ............................................................*/
typedef struct Table {
/* protected: */
    QActive super;

/* private: */
    uint8_t fork[N_PHILO];
    uint8_t isHungry[N_PHILO];
} Table;

/* protected: */
static QState Table_initial(Table * const me);
static QState Table_active(Table * const me);
static QState Table_serving(Table * const me);
static QState Table_paused(Table * const me);
/*$enddecl${AOs::Table} ^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^*/
//...

// define all AO instances and event queue buffers for them...
Philo AO_Philo[N_PHILO];
static QEvt l_philoQueue[N_PHILO][N_PHILO];

Table AO_Table;
static QEvt l_tableQueue[2];
//...

//============================================================================
// QF_active[] array defines all active object control blocks ----------------
QActiveCB const Q_ROM QF_active[] = {
    { (QActive *)0,            (QEvt *)0,        0U                      },
    { (QActive *)&AO_Philo[0], l_philoQueue[0],  Q_DIM(l_philoQueue[0])  },
    { (QActive *)&AO_Philo[1], l_philoQueue[1],  Q_DIM(l_philoQueue[1])  },
    { (QActive *)&AO_Philo[2], l_philoQueue[2],  Q_DIM(l_philoQueue[2])  },
    { (QActive *)&AO_Philo[3], l_philoQueue[3],  Q_DIM(l_philoQueue[3])  },
    { (QActive *)&AO_Philo[4], l_philoQueue[4],  Q_DIM(l_philoQueue[4])  },
    { (QActive *)&AO_Table,    l_tableQueue,     Q_DIM(l_tableQueue)     }
};

//============================================================================
// Board Support Package

// various other constants for the application...
enum {
    BSP_TICKS_PER_SEC = 100, // number of system clock ticks in one second
    LED_L             = 13,  // the pin number of the on-board LED (L)
    PHILO_0_PRIO      = 1,   // priority of the first Philo AO
    THINK_TIME        = 3*BSP_TICKS_PER_SEC, // time for thinking
    EAT_TIME          = 2*BSP_TICKS_PER_SEC  // time for eating
};

//............................................................................
void BSP_displayPhilStat(uint8_t n, char_t const *stat) {
    if (stat[0] == 'e') {
        digitalWrite(LED_L, HIGH);
    }
    else {
        digitalWrite(LED_L, LOW);
    }

    Serial.print(F("Philosopher "));
    Serial.print(n, DEC);
    Serial.print(F(" "));
    Serial.println(stat);
}
//............................................................................
void BSP_displayPaused(uint8_t paused) {
    if (paused) {
        Serial.println(F("Paused ON"));
    }
    else {
        Serial.println(F("Paused OFF"));
    }
}

//............................................................................
void setup() {
    // initialize the QF-nano framework
    QF_init(Q_DIM(QF_active));

    // initialize all AOs...
    QActive_ctor(&AO_Philo[0].super, Q_STATE_CAST(&Philo_initial));
    QActive_ctor(&AO_Philo[1].super, Q_STATE_CAST(&Philo_initial));
    QActive_ctor(&AO_Philo[2].super, Q_STATE_CAST(&Philo_initial));
    QActive_ctor(&AO_Philo[3].super, Q_STATE_CAST(&Philo_initial));
    QActive_ctor(&AO_Philo[4].super, Q_STATE_CAST(&Philo_initial));
    QActive_ctor(&AO_Table.super,    Q_STATE_CAST(&Table_initial));

    // initialize the hardware used in this sketch...
    pinMode(LED_L, OUTPUT); // set the LED-L pin to output

    Serial.begin(115200);   // set the highest stanard baud rate of 115200 bps
    Serial.print(F("QP-nano: "));
    Serial.print(F(QP_VERSION_STR));
    Serial.println(F(""));
}

//............................................................................
void loop() {
    QF_run(); // run the QP-nano application
}

//============================================================================
// interrupts
ISR(TIMER2_COMPA_vect) {
    QF_tickXISR(0); // process time events for tick rate 0

    if (Serial.available() > 0) {
        switch (Serial.read()) { // read the incoming byte
            case 'p':
            case 'P':
                QACTIVE_POST_ISR(&AO_Table, PAUSE_SIG, 0U);
                break;
            case 's':
            case 'S':
                QACTIVE_POST_ISR(&AO_Table, SERVE_SIG, 0U);
                break;
        }
    }
}

//============================================================================
// QF callbacks...
void QF_onStartup(void) {
    // set Timer2 in CTC mode, 1/1024 prescaler, start the timer ticking...
    TCCR2A = (1U << WGM21) | (0U << WGM20);
    TCCR2B = (1U << CS22 ) | (1U << CS21) | (1U << CS20); // 1/2^10
    ASSR  &= ~(1U << AS2);
    TIMSK2 = (1U << OCIE2A); // enable TIMER2 compare Interrupt
    TCNT2  = 0U;

    // set the output-compare register based on the desired tick frequency
    OCR2A  = (F_CPU / BSP_TICKS_PER_SEC / 1024U) - 1U;
}
//............................................................................
void QV_onIdle(void) {   // called with interrupts DISABLED
    // Put the CPU and peripherals to the low-power mode. You might
    // need to customize the clock management for your application,
    // see the datasheet for your particular AVR MCU.
    SMCR = (0 << SM0) | (1 << SE); // idle mode, adjust to your project
    QV_CPU_SLEEP();  // atomically go to sleep and enable interrupts
}
//............................................................................
void Q_onAssert(char const Q_ROM * const file, int line) {
    // implement the error-handling policy for your application!!!
    Serial.print(F("ASSERTION:"));
    Serial.print(file);
    Serial.print(line, DEC);
    for (uint32_t volatile i = 100000; i > 0; --i) {
    }
    QF_INT_DISABLE(); // disable all interrupts
    QF_RESET();  // reset the CPU
}

//============================================================================
// define all AO classes...
/*$skip${QP_VERSION} vvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvv*/
/* Check for the minimum required QP version */
#if (QP_VERSION < 650U) || (QP_VERSION != ((QP_RELEASE^4294967295U) % 0x3E8U))
#error qpn version 6.5.0 or higher required
#endif
/*$endskip${QP_VERSION} ^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^*/
/*$define${AOs::Philo} vvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvv*/
/*${AOs::Philo} ............................................................*/
/*${AOs::Philo::SM} ........................................................*/
static QState Philo_initial(Philo * const me) {
    /*${AOs::Philo::SM::initial} */
    return Q_TRAN(&Philo_thinking);
}
/*${AOs::Philo::SM::thinking} ..............................................*/
static QState Philo_thinking(Philo * const me) {
    QState status_;
    switch (Q_SIG(me)) {
        /*${AOs::Philo::SM::thinking} */
        case Q_ENTRY_SIG: {
            QActive_armX(&me->super, 0U, THINK_TIME, 0U);
            status_ = Q_HANDLED();
            break;
        }
        /*${AOs::Philo::SM::thinking} */
        case Q_EXIT_SIG: {
            QActive_disarmX(&me->super, 0U);
            status_ = Q_HANDLED();
            break;
        }
        /*${AOs::Philo::SM::thinking::Q_TIMEOUT} */
        case Q_TIMEOUT_SIG: {
            status_ = Q_TRAN(&Philo_hungry);
            break;
        }
        /*${AOs::Philo::SM::thinking::EAT, DONE} */
        case EAT_SIG: /* intentionally fall through */
        case DONE_SIG: {
            Q_ERROR(); /* these events should never arrive in this state */
            status_ = Q_HANDLED();
            break;
        }
        default: {
            status_ = Q_SUPER(&QHsm_top);
            break;
        }
    }
    return status_;
}
/*${AOs::Philo::SM::hungry} ................................................*/
static QState Philo_hungry(Philo * const me) {
    QState status_;
    switch (Q_SIG(me)) {
        /*${AOs::Philo::SM::hungry} */
        case Q_ENTRY_SIG: {
            QACTIVE_POST(&AO_Table, HUNGRY_SIG, me->super.prio);
            status_ = Q_HANDLED();
            break;
        }
        /*${AOs::Philo::SM::hungry::EAT} */
        case EAT_SIG: {
            status_ = Q_TRAN(&Philo_eating);
            break;
        }
        /*${AOs::Philo::SM::hungry::DONE} */
        case DONE_SIG: {
            Q_ERROR(); /* this event should never arrive in this state */
            status_ = Q_HANDLED();
            break;
        }
        default: {
            status_ = Q_SUPER(&QHsm_top);
            break;
        }
    }
    return status_;
}
/*${AOs::Philo::SM::eating} ................................................*/
static QState Philo_eating(Philo * const me) {
    QState status_;
    switch (Q_SIG(me)) {
        /*${AOs::Philo::SM::eating} */
        case Q_ENTRY_SIG: {
            QActive_armX(&me->super, 0U, EAT_TIME, 0U);
            status_ = Q_HANDLED();
            break;
        }
        /*${AOs::Philo::SM::eating} */
        case Q_EXIT_SIG: {
            QActive_disarmX(&me->super, 0U);
            QACTIVE_POST(QF_ACTIVE_CAST(&AO_Table), DONE_SIG, me->super.prio);
            status_ = Q_HANDLED();
            break;
        }
        /*${AOs::Philo::SM::eating::Q_TIMEOUT} */
        case Q_TIMEOUT_SIG: {
            status_ = Q_TRAN(&Philo_thinking);
            break;
        }
        /*${AOs::Philo::SM::eating::EAT, DONE} */
        case EAT_SIG: /* intentionally fall through */
        case DONE_SIG: {
            Q_ERROR(); /* these events should never arrive in this state */
            status_ = Q_HANDLED();
            break;
        }
        default: {
            status_ = Q_SUPER(&QHsm_top);
            break;
        }
    }
    return status_;
}
/*$enddef${AOs::Philo} ^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^*/

static inline uint8_t RIGHT(uint8_t n) {
    return (n + (N_PHILO - 1)) % N_PHILO;
}
static inline uint8_t LEFT(uint8_t n) {
    return (n + 1) % N_PHILO;
}
enum {
    FREE = 0,
    USED = 1
};

/*$define${AOs::Table} vvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvv*/
/*${AOs::Table} ............................................................*/
/*${AOs::Table::SM} ........................................................*/
static QState Table_initial(Table * const me) {
    /*${AOs::Table::SM::initial} */
    uint8_t n;
    for (n = 0U; n < N_PHILO; ++n) {
        me->fork[n] = FREE;
        me->isHungry[n] = 0U;
        BSP_displayPhilStat(n, "thinking");
    }
    return Q_TRAN(&Table_serving);
}
/*${AOs::Table::SM::active} ................................................*/
static QState Table_active(Table * const me) {
    QState status_;
    switch (Q_SIG(me)) {
        /*${AOs::Table::SM::active::EAT} */
        case EAT_SIG: {
            Q_ERROR();
            status_ = Q_HANDLED();
            break;
        }
        default: {
            status_ = Q_SUPER(&QHsm_top);
            break;
        }
    }
    return status_;
}
/*${AOs::Table::SM::active::serving} .......................................*/
static QState Table_serving(Table * const me) {
    QState status_;
    switch (Q_SIG(me)) {
        /*${AOs::Table::SM::active::serving} */
        case Q_ENTRY_SIG: {
            uint8_t n;
            for (n = 0U; n < N_PHILO; ++n) { /* give permissions to eat... */
                if ((me->isHungry[n] != 0U)
                    && (me->fork[LEFT(n)] == FREE)
                    && (me->fork[n] == FREE))
                {
                    QMActive *philo;

                    me->fork[LEFT(n)] = USED;
                    me->fork[n] = USED;
                    philo = QF_ACTIVE_CAST(Q_ROM_PTR(QF_active[PHILO_0_PRIO + n].act));
                    QACTIVE_POST(philo, EAT_SIG, n);
                    me->isHungry[n] = 0U;
                    BSP_displayPhilStat(n, "eating  ");
                }
            }
            status_ = Q_HANDLED();
            break;
        }
        /*${AOs::Table::SM::active::serving::HUNGRY} */
        case HUNGRY_SIG: {
            uint8_t n, m;

            n = (uint8_t)(Q_PAR(me) - PHILO_0_PRIO);
            /* phil ID must be in range and he must be not hungry */
            Q_ASSERT((n < N_PHILO) && (me->isHungry[n] == 0U));

            BSP_displayPhilStat(n, "hungry  ");
            m = LEFT(n);
            /*${AOs::Table::SM::active::serving::HUNGRY::[bothfree]} */
            if ((me->fork[m] == FREE) && (me->fork[n] == FREE)) {
                me->fork[m] = USED;
                me->fork[n] = USED;
                QACTIVE_POST(&AO_Philo[n], EAT_SIG, n);
                BSP_displayPhilStat(n, "eating  ");
                status_ = Q_HANDLED();
            }
            /*${AOs::Table::SM::active::serving::HUNGRY::[else]} */
            else {
                me->isHungry[n] = 1U;
                status_ = Q_HANDLED();
            }
            break;
        }
        /*${AOs::Table::SM::active::serving::DONE} */
        case DONE_SIG: {
            uint8_t n, m;
            QMActive *philo;

            n = (uint8_t)(Q_PAR(me) - PHILO_0_PRIO);
            /* phil ID must be in range and he must be not hungry */
            Q_ASSERT((n < N_PHILO) && (me->isHungry[n] == 0U));

            BSP_displayPhilStat(n, "thinking");
            m = LEFT(n);
            /* both forks of Phil[n] must be used */
            Q_ASSERT((me->fork[n] == USED) && (me->fork[m] == USED));

            me->fork[m] = FREE;
            me->fork[n] = FREE;
            m = RIGHT(n); /* check the right neighbor */

            if ((me->isHungry[m] != 0U) && (me->fork[m] == FREE)) {
                me->fork[n] = USED;
                me->fork[m] = USED;
                me->isHungry[m] = 0U;
                philo = QF_ACTIVE_CAST(Q_ROM_PTR(QF_active[PHILO_0_PRIO + m].act));
                QACTIVE_POST(philo, EAT_SIG, m);
                BSP_displayPhilStat(m, "eating  ");
            }
            m = LEFT(n); /* check the left neighbor */
            n = LEFT(m); /* left fork of the left neighbor */
            if ((me->isHungry[m] != 0U) && (me->fork[n] == FREE)) {
                me->fork[m] = USED;
                me->fork[n] = USED;
                me->isHungry[m] = 0U;
                philo = QF_ACTIVE_CAST(Q_ROM_PTR(QF_active[PHILO_0_PRIO + m].act));
                QACTIVE_POST(philo, EAT_SIG, m);
                BSP_displayPhilStat(m, "eating  ");
            }
            status_ = Q_HANDLED();
            break;
        }
        /*${AOs::Table::SM::active::serving::EAT} */
        case EAT_SIG: {
            Q_ERROR();
            status_ = Q_HANDLED();
            break;
        }
        /*${AOs::Table::SM::active::serving::PAUSE} */
        case PAUSE_SIG: {
            status_ = Q_TRAN(&Table_paused);
            break;
        }
        default: {
            status_ = Q_SUPER(&Table_active);
            break;
        }
    }
    return status_;
}
/*${AOs::Table::SM::active::paused} ........................................*/
static QState Table_paused(Table * const me) {
    QState status_;
    switch (Q_SIG(me)) {
        /*${AOs::Table::SM::active::paused} */
        case Q_ENTRY_SIG: {
            BSP_displayPaused(1U);
            status_ = Q_HANDLED();
            break;
        }
        /*${AOs::Table::SM::active::paused} */
        case Q_EXIT_SIG: {
            BSP_displayPaused(0U);
            status_ = Q_HANDLED();
            break;
        }
        /*${AOs::Table::SM::active::paused::SERVE} */
        case SERVE_SIG: {
            status_ = Q_TRAN(&Table_serving);
            break;
        }
        /*${AOs::Table::SM::active::paused::HUNGRY} */
        case HUNGRY_SIG: {
            uint8_t n = (uint8_t)(Q_PAR(me) - PHILO_0_PRIO);
            /* philo ID must be in range and he must be not hungry */
            Q_ASSERT((n < N_PHILO) && (me->isHungry[n] == 0U));
            me->isHungry[n] = 1U;
            BSP_displayPhilStat(n, "hungry  ");
            status_ = Q_HANDLED();
            break;
        }
        /*${AOs::Table::SM::active::paused::DONE} */
        case DONE_SIG: {
            uint8_t n, m;

            n = (uint8_t)(Q_PAR(me) - PHILO_0_PRIO);
            /* phil ID must be in range and he must be not hungry */
            Q_ASSERT((n < N_PHILO) && (me->isHungry[n] == 0U));

            BSP_displayPhilStat(n, "thinking");
            m = LEFT(n);
            /* both forks of Phil[n] must be used */
            Q_ASSERT((me->fork[n] == USED) && (me->fork[m] == USED));

            me->fork[m] = FREE;
            me->fork[n] = FREE;
            status_ = Q_HANDLED();
            break;
        }
        default: {
            status_ = Q_SUPER(&Table_active);
            break;
        }
    }
    return status_;
}
/*$enddef${AOs::Table} ^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^*/
//...
