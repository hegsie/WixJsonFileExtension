#pragma once

#ifndef WINVER                  // Specifies that the minimum required platform is Windows 7.
#define WINVER 0x0601           // Windows 7: the oldest OS the v143 toolset and WiX v4+ support.
#endif

#ifndef _WIN32_WINNT            // Specifies that the minimum required platform is Windows 7.
#define _WIN32_WINNT 0x0601     // Windows 7: the oldest OS the v143 toolset and WiX v4+ support.
#endif

#ifndef _WIN32_IE               // Specifies that the minimum required platform is Internet Explorer 6.0 SP3.
#define _WIN32_IE 0x0603        // Change this to the appropriate value to target other versions of IE.
#endif

#ifndef _WIN32_MSI              // Specifies that the minimum required MSI version is MSI 3.1
#define _WIN32_MSI 310          // Change this to the appropriate value to target other versions of MSI.
#endif
