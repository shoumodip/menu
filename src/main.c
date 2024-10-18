#include "app.h"

int main(void) {
    App app = {0};
    if (!app_init(&app)) {
        app_free(&app);
        return 1;
    }

    app_loop(&app);
    app_free(&app);
    return 0;
}
