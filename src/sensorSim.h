#pragma once

// TEMPORARY bench simulator. Publishes a triangle-wave `cu` (non-retained) to
// whichever paired sensors the active role consumes (ts<A> and/or hs<A>), so app
// cards appear and control loops run without the real sensor boards.
//
// It does NOT publish daily_max / daily_min — those belong to Node-RED, which has
// the timezone and survives a reboot. See the comment block in sensorSim.cpp.
//
// Set SIMULATE=false in sensorSim.cpp once a real sensor board publishes to the
// same topics, rather than letting two sources feed one stream.
void sensorSim_loop();
