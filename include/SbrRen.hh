/*=========================================================================

  Program:   Visualization Library
  Module:    SbrRen.hh
  Language:  C++
  Date:      $Date$
  Version:   $Revision$

This file is part of the Visualization Library. No part of this file or its
contents may be copied, reproduced or altered in any way without the express
written consent of the authors.

Copyright (c) Ken Martin, Will Schroeder, Bill Lorensen 1993, 1994

=========================================================================*/
// .NAME vlSbrRenderer - HP starbase renderer
// .SECTION Description
// vlSbrRenderer is a concrete implementation of the abstract class
// vlRenderer. vlSbrRenderer interfaces to the Hewlett-Packard starbase
// graphics library.

#ifndef __vlSbrRenderer_hh
#define __vlSbrRenderer_hh

#include <stdlib.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include "Renderer.hh"
#include "starbase.c.h"

class vlSbrRenderer : public vlRenderer
{
public:
  vlSbrRenderer();
  char *GetClassName() {return "vlSbrRenderer";};
  void PrintSelf(ostream& os, vlIndent indent);

  void Render(void);
  vlGeometryPrimitive *GetPrimitive(char *);
  void ClearLights(void);
  int UpdateActors(void);
  int UpdateCameras(void);
  int UpdateLights(void);
  vlGetMacro(Fd,int);
  vlGetMacro(LightSwitch,int);

protected:
  int NumberOfLightsBound;
  int LightSwitch;
  int Fd;

};

#endif
