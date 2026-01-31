/*!****************************************************************************
 * @file remotes.h
 * @brief Top remotes configuration file.
 * This file contain all available remotes configuation supported by the project.
 * 
 * Keep this section generic. It could be share by different projects (DIY vehicle, DIY remote ...)
 *******************************************************************************/// 
#pragma once

  //Devices decoded directly by the onboard controller
#if REMOTE == MY_REMOTE    // PS4 Dualshock 4 controller 
  #include "my_remote.h"
#endif

#ifndef REMOTE 
 #error "No remote defined for compilation. Check config.h file or -D REMOTE=... command line parameter to fix the problem" 
#endif

// EOF remotes.h