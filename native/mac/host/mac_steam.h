/* mac_steam.h -- the Mac Build's Steam bridge (M9).  See docs/mac/steam-bridge.md. */
#ifndef MAC_STEAM_H
#define MAC_STEAM_H

#include "isaac_host.h"

/* Called from imp_steam_api__SteamInternal_ContextInit (host_shims_steam.c)
 * under ISAAC_NATIVE.  Lazily brings the bridge up on the first call.
 * Returns 0 when the bridge is not live (the caller then does exactly what it
 * did before M9); returns 1 when it has answered (EAX set, slot written). */
int mac_steam_context_init(CpuState *restrict cpu, uint32_t slot);

/* 1 once the native Steam API is loaded, initialised and wired. */
int mac_steam_is_live(void);

#endif
