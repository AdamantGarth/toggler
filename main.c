#include <getopt.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <systemd/sd-bus.h>

// sd-bus error checking
#define SD_CHECK(ret, errmsg, onerror) if (ret < 0) { fprintf(stderr, "ERROR: %s: %s\n", errmsg, strerror(-ret)); onerror; }

// Status property values
char *STATUS_ACTIVE  = "Active";
char *STATUS_PASSIVE = "Passive";

typedef struct Properties {
    char *cmd_on, *cmd_off, *icon_on, *icon_off;
    bool enabled, should_exit;
    // DBus properties v / ^ Private properties
    char *Category, *Id, *Title, *Status, *IconName;
    uint32_t WindowId;
    uint8_t ItemIsMenu;
} Properties;

// Handle main action
static int onActivate(sd_bus_message *msg, void *data, sd_bus_error *error) {
    Properties *properties = data;
    sd_bus *bus = sd_bus_message_get_bus(msg);
    int ret;

    ret = system(properties->enabled ? properties->cmd_off : properties->cmd_on);
    if (ret == -1 || WEXITSTATUS(ret) != 0) {
        fprintf(stderr, "ERROR: Command `%s` returned code %d!\n", properties->enabled ? properties->cmd_off : properties->cmd_on, WEXITSTATUS(ret));
        return 0;
    }
    properties->IconName = properties->enabled ? properties->icon_off : properties->icon_on;
    properties->Status = properties->enabled ? STATUS_PASSIVE : STATUS_ACTIVE;
    properties->enabled = !properties->enabled;

    ret = sd_bus_emit_signal(bus, "/StatusNotifierItem", "org.kde.StatusNotifierItem", "NewIcon", "");
    SD_CHECK(ret, "Failed to emit signal", return ret);
    ret = sd_bus_emit_signal(bus, "/StatusNotifierItem", "org.kde.StatusNotifierItem", "NewStatus", "");
    SD_CHECK(ret, "Failed to emit signal", return ret);

    printf("Activated, changed status to %s\n", properties->Status);

    return 0;
}

// Handle secondary action, usually this is the middle mouse button click
static int onSecondaryActivate(sd_bus_message *msg, void *data, sd_bus_error *error) {
    Properties *properties = data;
    printf("Exiting...\n");
    properties->should_exit = true;
    return 0;
}

static const sd_bus_vtable SNI[] = {
    SD_BUS_VTABLE_START(0),
    SD_BUS_PROPERTY("Category",   "s", NULL, offsetof(Properties, Category),   0),
    SD_BUS_PROPERTY("Id",         "s", NULL, offsetof(Properties, Id),         0),
    SD_BUS_PROPERTY("Title",      "s", NULL, offsetof(Properties, Title),      0),
    SD_BUS_PROPERTY("Status",     "s", NULL, offsetof(Properties, Status),     0),
    SD_BUS_PROPERTY("WindowId",   "u", NULL, offsetof(Properties, WindowId),   0),
    SD_BUS_PROPERTY("IconName",   "s", NULL, offsetof(Properties, IconName),   0),
    SD_BUS_PROPERTY("ItemIsMenu", "b", NULL, offsetof(Properties, ItemIsMenu), 0),
    SD_BUS_METHOD_WITH_ARGS("Activate",          SD_BUS_ARGS("i", x, "i", y), SD_BUS_NO_RESULT, onActivate,          0),
    SD_BUS_METHOD_WITH_ARGS("SecondaryActivate", SD_BUS_ARGS("i", x, "i", y), SD_BUS_NO_RESULT, onSecondaryActivate, 0),
    SD_BUS_SIGNAL("NewIcon",   "", 0),
    SD_BUS_SIGNAL("NewStatus", "", 0),
    SD_BUS_VTABLE_END
};

int main(int argc, char **argv) {
    Properties properties = {
        .cmd_on = "echo Enabled",
        .cmd_off = "echo Disabled",
        .icon_on = "checkbox-checked-symbolic",
        .icon_off = "checkbox-symbolic",
        .enabled = false,
        .should_exit = false,
        // DBus properties v / ^ Private properties
        .Category = "SystemServices",
        .Id = "Toggler",
        .Title = "Toggler",
        .Status = STATUS_PASSIVE,
        .IconName = properties.icon_off,
        .WindowId = 0,
        .ItemIsMenu = false,
    };

    static const char *usage =
        "Usage: %s [options]\n\n"
        "  -h, --help             Show help message and exit.\n"
        "  -o, --on <cmd>         Command to run when the state changes to 'on'.\n"
        "  -O, --off <cmd>        Command to run when the state changes to 'off'.\n"
        "  -i, --icon-on <icon>   Icon name to use when the state is 'on'.\n"
        "  -I, --icon-off <icon>  Icon name to use when the state is 'off'.\n"
        "  -t, --title <title>    Set the title.\n"
        "  -s, --state (on|off)   Set the initial state.\n\n"
        "To close the applet, use the secondary action of your StatusNotifier service.\n"
        "Usually this means - click with middle mouse button on the tray icon :)\n\n";

    static struct option long_options[] = {
        {"help", no_argument, NULL, 'h'},
        {"on", required_argument, NULL, 'o'},
        {"off", required_argument, NULL, 'O'},
        {"icon-on", required_argument, NULL, 'i'},
        {"icon-off", required_argument, NULL, 'I'},
        {"title", required_argument, NULL, 't'},
        {"state", required_argument, NULL, 's'}
    };

    int opt = 0;
    while((opt = getopt_long(argc, argv, "ho:O:i:I:t:s:", long_options, NULL)) != -1) {
        switch (opt) {
        case 'h':
            printf(usage, argv[0]);
            exit(EXIT_SUCCESS);
            break;
        case 'o':
            properties.cmd_on = optarg;
            break;
        case 'O':
            properties.cmd_off = optarg;
            break;
        case 'i':
            properties.icon_on = optarg;
            properties.IconName = properties.icon_on;
            break;
        case 'I':
            properties.icon_off = optarg;
            properties.IconName = properties.icon_off;
            break;
        case 't':
            properties.Title = optarg;
            break;
        case 's':
            if (strncmp("on", optarg, sizeof("on")-1) == 0) {
                properties.Status = STATUS_ACTIVE;
                properties.IconName = properties.icon_on;
                break;
            }
            if (strncmp("off", optarg, sizeof("off")-1) == 0) {
                properties.Status = STATUS_PASSIVE;
                properties.IconName = properties.icon_off;
                break;
            }
            fprintf(stderr, "ERROR: Unknown state '%s', use 'on' or 'off'\n", optarg);
            exit(EXIT_FAILURE);
            break;
        default:
            fprintf(stderr, usage, argv[0]);
            exit(EXIT_FAILURE);
            break;
        }
    }

    int ret = 0;

    // Open DBus connection
    sd_bus *bus = NULL;
    ret = sd_bus_open_user(&bus);
    SD_CHECK(ret, "Failed to open DBus connection", goto sd_bus);

    // Register DBus object
    sd_bus_slot *slot = NULL;
    ret = sd_bus_add_object_vtable(bus, &slot, "/StatusNotifierItem", "org.kde.StatusNotifierItem", SNI, &properties);
    SD_CHECK(ret, "Failed to register DBus object", goto sd_bus_slot);

    // Store our unique name
    const char *unique_name;
    ret = sd_bus_get_unique_name(bus, &unique_name);
    SD_CHECK(ret, "Failed to read unique DBus name", goto sd_bus_slot)

    // Register as StatusNotifierItem
    sd_bus_error error = SD_BUS_ERROR_NULL;
    sd_bus_message *response = NULL;
    ret = sd_bus_call_method(bus, "org.kde.StatusNotifierWatcher", "/StatusNotifierWatcher", "org.kde.StatusNotifierWatcher", "RegisterStatusNotifierItem", &error, &response, "s", unique_name);
    if (ret < 0) {
        fprintf(stderr, "ERROR: Failed to register as StatusNotifierItem: %s\n", error.message);
        goto finish;
    }

    // Process requests
    for (;;) {
        ret = sd_bus_process(bus, NULL);
        SD_CHECK(ret, "Failed to process DBus events", goto finish);

        if (ret > 0) continue;
        if (properties.should_exit) break;

        ret = sd_bus_wait(bus, UINT64_MAX);
        SD_CHECK(ret, "Failed to wait for an event", goto finish);
    }

finish:
    sd_bus_error_free(&error);
    sd_bus_message_unref(response);
sd_bus_slot:
    sd_bus_slot_unref(slot);
sd_bus:
    sd_bus_unref(bus);

    return ret < 0 ? EXIT_FAILURE : EXIT_SUCCESS;
}
