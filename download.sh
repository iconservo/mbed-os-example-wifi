#!/bin/sh
JLinkExe -device NRF52 -if SWD -speed 4000 -jtagconf -1,-1 -autoconnect 1 -CommanderScript download.jlink