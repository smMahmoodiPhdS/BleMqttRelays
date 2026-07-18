#pragma once

// TEMPORARY bench simulator. Publishes triangle-wave current values (+ rolling
// daily max/min) to whichever paired sensors the active role consumes
// (ts<A> and/or hs<A>), so app cards appear and control loops run without the
// real sensor boards. Set SIMULATE=false in sensorSim.cpp to disable.
void sensorSim_loop();
