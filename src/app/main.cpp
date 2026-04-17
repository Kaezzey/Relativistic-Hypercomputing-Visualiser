#include "app/Application.h"

#include <iostream>

int main()
{
    rhv::app::Application application;

    if (!application.Initialize())
    {
        std::cerr << "BOOT FAIL: " << application.GetLastError() << '\n';
        return 1;
    }

    application.Run();
    return 0;
}
