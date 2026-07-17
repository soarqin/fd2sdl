#include <stdio.h>

#include "app_flow.h"

#define CHECK(expr) do { \
    if (!(expr)) { \
        fprintf(stderr, "CHECK failed at %s:%d: %s\n", \
                __FILE__, __LINE__, #expr); \
        return 1; \
    } \
} while (0)

int main(void) {
    CHECK(fd2_title_action_from_selection(0, 1) ==
          FD2_TITLE_ACTION_NEW_GAME);
    CHECK(fd2_title_action_from_selection(1, 1) ==
          FD2_TITLE_ACTION_LOAD);
    CHECK(fd2_title_action_from_selection(2, 1) ==
          FD2_TITLE_ACTION_EXIT);
    CHECK(fd2_title_action_from_selection(1, 0) ==
          FD2_TITLE_ACTION_EXIT);
    CHECK(fd2_title_action_from_selection(-1, 1) ==
          FD2_TITLE_ACTION_HOST_QUIT);
    CHECK(fd2_title_action_from_selection(2, 0) ==
          (fd2_title_action)99);
    CHECK(fd2_app_flow_from_title(FD2_TITLE_ACTION_NEW_GAME) ==
          FD2_APP_FLOW_START_NEW_GAME);
    CHECK(fd2_app_flow_from_title(FD2_TITLE_ACTION_LOAD) ==
          FD2_APP_FLOW_LOAD_GAME);
    CHECK(fd2_app_flow_from_title(FD2_TITLE_ACTION_EXIT) ==
          FD2_APP_FLOW_EXIT);
    CHECK(fd2_app_flow_from_title(FD2_TITLE_ACTION_HOST_QUIT) ==
          FD2_APP_FLOW_EXIT);
    CHECK(fd2_app_flow_from_field(FD2_APP_FIELD_TITLE) ==
          FD2_APP_FLOW_SHOW_TITLE);
    CHECK(fd2_app_flow_from_field(FD2_APP_FIELD_COMPLETE) ==
          FD2_APP_FLOW_SHOW_TITLE);
    CHECK(fd2_app_flow_from_field(FD2_APP_FIELD_HOST_QUIT) ==
          FD2_APP_FLOW_EXIT);
    CHECK(fd2_app_flow_from_field(FD2_APP_FIELD_ERROR) ==
          FD2_APP_FLOW_ERROR);
    CHECK(fd2_app_flow_from_title((fd2_title_action)99) ==
          FD2_APP_FLOW_ERROR);
    CHECK(fd2_app_flow_from_field((fd2_app_field_result)99) ==
          FD2_APP_FLOW_ERROR);
    puts("app_flow_test: ok");
    return 0;
}
