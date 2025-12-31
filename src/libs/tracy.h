#pragma once

#ifdef USE_TRACY
#include "tracy/TracyC.h"
#else
typedef const void* TracyCZoneCtx;

typedef const void* TracyCLockCtx;

#define TracyCZone(c,x)
#define TracyCZoneN(c,x,y)
#define TracyCZoneC(c,x,y)
#define TracyCZoneNC(c,x,y,z)
#define TracyCZoneEnd(c)
#define TracyCZoneText(c,x,y)
#define TracyCZoneName(c,x,y)
#define TracyCZoneColor(c,x)
#define TracyCZoneValue(c,x)

#define TracyCAlloc(x,y)
#define TracyCFree(x)
#define TracyCMemoryDiscard(x)
#define TracyCSecureAlloc(x,y)
#define TracyCSecureFree(x)
#define TracyCSecureMemoryDiscard(x)

#define TracyCAllocN(x,y,z)
#define TracyCFreeN(x,y)
#define TracyCSecureAllocN(x,y,z)
#define TracyCSecureFreeN(x,y)

#define TracyCFrameMark
#define TracyCFrameMarkNamed(x)
#define TracyCFrameMarkStart(x)
#define TracyCFrameMarkEnd(x)
#define TracyCFrameImage(x,y,z,w,a)

#define TracyCPlot(x,y)
#define TracyCPlotF(x,y)
#define TracyCPlotI(x,y)
#define TracyCPlotConfig(x,y,z,w,a)

#define TracyCMessage(x,y)
#define TracyCMessageL(x)
#define TracyCMessageC(x,y,z)
#define TracyCMessageLC(x,y)
#define TracyCAppInfo(x,y)

#define TracyCZoneS(x,y,z)
#define TracyCZoneNS(x,y,z,w)
#define TracyCZoneCS(x,y,z,w)
#define TracyCZoneNCS(x,y,z,w,a)

#define TracyCAllocS(x,y,z)
#define TracyCFreeS(x,y)
#define TracyCMemoryDiscardS(x,y)
#define TracyCSecureAllocS(x,y,z)
#define TracyCSecureFreeS(x,y)
#define TracyCSecureMemoryDiscardS(x,y)

#define TracyCAllocNS(x,y,z,w)
#define TracyCFreeNS(x,y,z)
#define TracyCSecureAllocNS(x,y,z,w)
#define TracyCSecureFreeNS(x,y,z)

#define TracyCMessageS(x,y,z)
#define TracyCMessageLS(x,y)
#define TracyCMessageCS(x,y,z,w)
#define TracyCMessageLCS(x,y,z)

#define TracyCLockCtx(l)
#define TracyCLockAnnounce(l)
#define TracyCLockTerminate(l)
#define TracyCLockBeforeLock(l)
#define TracyCLockAfterLock(l)
#define TracyCLockAfterUnlock(l)
#define TracyCLockAfterTryLock(l,x)
#define TracyCLockMark(l)
#define TracyCLockCustomName(l,x,y)

#define TracyCIsConnected 0
#define TracyCIsStarted 0

#define TracyCBeginSamplingProfiling() 0
#define TracyCEndSamplingProfiling()
#endif