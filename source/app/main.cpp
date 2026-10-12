// 3dsFM entry point: graphics, services, then the app loop.

#include "app.hpp"
#include "platform.hpp"
#include "ui.hpp"

#include <3ds.h>
#include <citro2d.h>

u32 __stacksize__ = 0x40000;

int main()
{
    // The New 3DS's faster clock and L2 cache help the decoders; on an Old
    // 3DS this does nothing.
    osSetSpeedupEnable(true);
    gfxInitDefault();
    gfxSet3D(false);
    C3D_Init(C3D_DEFAULT_CMDBUF_SIZE);
    C2D_Init(C2D_DEFAULT_MAX_OBJECTS);
    C2D_Prepare();
    // Let the decoder thread run on the second core when the system allows.
    APT_SetAppCpuTimeLimit(30);

    romfsInit();
    platform::init();
    ui::init();
    {
        App app;
        app.run();
    }
    ui::shutdown();
    platform::shutdown();
    romfsExit();

    C2D_Fini();
    C3D_Fini();
    gfxExit();
    return 0;
}
