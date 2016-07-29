# TI AM335x PRU firmware for soft-PWM output via shift registers

PRU_halt contains a very basic firmware from the TI examples that immediately halts the PRU. This is run on the unused PRU.

PRU_RPMsg_shiftreg_driver contains the firmware for high-speed PWM output via shift registers. The Linux remoteproc messaging system is used for communications between the ARM and PRU.
