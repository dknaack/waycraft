#include <errno.h>
#include <fcntl.h>
#include <string.h>
#include <xcb/xcb.h>
#include <xcb/composite.h>
#include <xcb/xfixes.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/wait.h>
#include <unistd.h>

static const char lock_file_fmt[] = "/tmp/.X%d-lock";
static const char socket_fmt[] = "/tmp/.X11-unix/X%d";

static const char *xwm_atom_names[XWM_ATOM_COUNT] = {
	[XWM_NET_WM_NAME] = "_NET_WM_NAME",
	[XWM_NET_SUPPORTING_WM_CHECK] = "_NET_SUPPORTING_WM_CHECK",
	[XWM_NET_ACTIVE_WINDOW] = "_NET_ACTIVE_WINDW",
	[XWM_WINDOW] = "WINDOW",
	[XWM_WL_SURFACE_ID] = "WL_SURFACE_ID",
	[XWM_WM_PROTOCOLS] = "WM_PROTOCOLS",
	[XWM_WM_S0] = "WM_S0",
};

static i32
set_cloexec(i32 fd, i32 value)
{
	i32 flags = fcntl(fd, F_GETFD);
	if (value) {
		flags |= FD_CLOEXEC;
	} else {
		flags &= ~FD_CLOEXEC;
	}
	return fcntl(fd, F_SETFD, flags);
}

static void
xwayland_assign_role(struct wl_resource *resource, xcb_window_t xcb_window, xcb_connection_t *connection)
{
	struct surface *surface = wl_resource_get_user_data(resource);

	surface->pending.flags |= SURFACE_NEW_ROLE;
	surface->pending.role = SURFACE_ROLE_XWAYLAND;
	surface->xwayland_surface.window = xcb_window;

	u32 mask = XCB_CW_EVENT_MASK;
	u32 values[1];
	values[0] = XCB_EVENT_MASK_PROPERTY_CHANGE;
	xcb_change_window_attributes(connection, xcb_window, mask, values);

	mask = XCB_CONFIG_WINDOW_BORDER_WIDTH;
	values[0] = 0;
	xcb_configure_window(connection, xcb_window, mask, values);
}

static void
xwm_handle_create_notify(struct xwm *xwm, xcb_create_notify_event_t *event)
{
	xcb_window_t window = event->window;
	u32 values[1];

	if (window != xwm->window) {
		values[0] = XCB_EVENT_MASK_PROPERTY_CHANGE |
			XCB_EVENT_MASK_FOCUS_CHANGE;
		xcb_change_window_attributes(xwm->connection, window,
			XCB_CW_EVENT_MASK, values);
	}
}

static void
xwm_handle_destroy_notify(struct xwm *xwm, xcb_destroy_notify_event_t *event)
{
	wl_signal_emit(&xwm->destroy_notify, event);
}

static void
xwm_handle_property_notify(struct xwm *xwm, xcb_property_notify_event_t *event)
{
	// TODO
}

static void
xwm_handle_client_message(struct xwm *xwm, xcb_client_message_event_t *event)
{
	if (event->type == xwm->atoms[XWM_WL_SURFACE_ID]) {
		u32 wl_surface_id = event->data.data32[0];
		struct wl_resource *resource = wl_client_get_object(xwm->client,
			wl_surface_id);

		// NOTE: surface may not have been created yet.
		if (resource) {
			xwayland_assign_role(resource, event->window, xwm->connection);
		} else {
			struct xwayland_surface *surface =
				&xwm->unpaired_surfaces[xwm->unpaired_surface_count++];

			surface->wl_surface = wl_surface_id;
			surface->window = event->window;
		}
	} else {
		log_info("unpaired window!");
	}
}

static void
xwm_handle_configure_request(struct xwm *xwm, xcb_configure_request_event_t *event)
{
	(void)event;
}

static void
xwm_handle_map_request(struct xwm *xwm, xcb_map_request_event_t *event)
{
	xcb_map_window(xwm->connection, event->window);
}

static i32
xwm_handle_event(i32 fd, u32 mask, void *data)
{
	static const char *xwm_event_str[256] = {
		[XCB_CREATE_NOTIFY] = "create_notify",
		[XCB_DESTROY_NOTIFY] = "destroy_notify",
		[XCB_MAP_NOTIFY] = "map_notify",
		[XCB_PROPERTY_NOTIFY] = "property_notify",
		[XCB_CLIENT_MESSAGE] = "client_message",
		[XCB_CONFIGURE_REQUEST] = "configure_request",
		[XCB_MAPPING_NOTIFY] = "mapping_notify",
		[XCB_MAP_REQUEST] = "map_request",
	};

	struct xwm *xwm = data;
	xcb_generic_event_t *event;
	u32 count = 0;

	while ((event = xcb_poll_for_event(xwm->connection))) {
		u8 event_type = event->response_type & ~0x80;
		const char *event_name = xwm_event_str[event_type];
		if (!event_name) {
			event_name = "unknown event";
		}

		switch (event->response_type & ~0x80) {
		case XCB_CREATE_NOTIFY:
			xwm_handle_create_notify(xwm, (xcb_create_notify_event_t *)event);
			break;
		case XCB_DESTROY_NOTIFY:
			xwm_handle_destroy_notify(xwm, (xcb_destroy_notify_event_t *)event);
			break;
		case XCB_PROPERTY_NOTIFY:
			xwm_handle_property_notify(xwm, (xcb_property_notify_event_t *)event);
			break;
		case XCB_CLIENT_MESSAGE:
			xwm_handle_client_message(xwm, (xcb_client_message_event_t *)event);
			break;
		case XCB_CONFIGURE_REQUEST:
			xwm_handle_configure_request(xwm, (xcb_configure_request_event_t *)event);
			break;
		case XCB_MAP_REQUEST:
			xwm_handle_map_request(xwm, (xcb_map_request_event_t *)event);
			break;
		}

		free(event);
		count++;
	}

	xcb_flush(xwm->connection);
	return count;
}

static i32
xwm_init(struct xwm *xwm, i32 wm_fd, struct wl_display *display)
{
	xwm->connection = xcb_connect_to_fd(wm_fd, 0);
	if (xcb_connection_has_error(xwm->connection)) {
		perror("Failed to connect to file descriptor");
		return -1;
	}

	xcb_prefetch_extension_data(xwm->connection, &xcb_composite_id);

	xwm->screen = xcb_setup_roots_iterator(xcb_get_setup(xwm->connection)).data;
	xwm->window = xcb_generate_id(xwm->connection);

	u32 values[] = {
		XCB_EVENT_MASK_SUBSTRUCTURE_NOTIFY |
			XCB_EVENT_MASK_SUBSTRUCTURE_REDIRECT |
			XCB_EVENT_MASK_PROPERTY_CHANGE,
	};

	xcb_change_window_attributes(xwm->connection, xwm->screen->root,
		XCB_CW_EVENT_MASK, values);

	xcb_intern_atom_cookie_t cookies[XWM_ATOM_COUNT];
	for (u32 i = 0; i < XWM_ATOM_COUNT; i++) {
		cookies[i] = xcb_intern_atom(xwm->connection, 0,
			strlen(xwm_atom_names[i]), xwm_atom_names[i]);
	}

	for (u32 i = 0; i < XWM_ATOM_COUNT; i++) {
		xcb_intern_atom_reply_t *reply = xcb_intern_atom_reply(
			xwm->connection, cookies[i], 0);
		xwm->atoms[i] = reply->atom;
		free(reply);
	}

	xcb_create_window(xwm->connection, XCB_COPY_FROM_PARENT, xwm->window,
		xwm->screen->root, 0, 0, 10, 10, 0,
		XCB_WINDOW_CLASS_INPUT_OUTPUT, xwm->screen->root_visual, 0, 0);

	char title[] = "waycraft wm";
	xcb_change_property(xwm->connection, XCB_PROP_MODE_REPLACE, xwm->window,
		xwm->atoms[XWM_NET_WM_NAME], XCB_ATOM_STRING, 8, strlen(title), title);

	xcb_change_property(xwm->connection, XCB_PROP_MODE_REPLACE,
		xwm->screen->root, xwm->atoms[XWM_NET_SUPPORTING_WM_CHECK],
		XCB_ATOM_WINDOW, 32, 1, &xwm->window);
	xcb_change_property(xwm->connection, XCB_PROP_MODE_REPLACE,
		xwm->window, xwm->atoms[XWM_NET_SUPPORTING_WM_CHECK],
		XCB_ATOM_WINDOW, 32, 1, &xwm->window);

	xcb_window_t no_window = XCB_WINDOW_NONE;
	xcb_change_property(xwm->connection, XCB_PROP_MODE_REPLACE,
			xwm->screen->root, xwm->atoms[XWM_NET_ACTIVE_WINDOW],
			xwm->atoms[XWM_WINDOW], 32, 1, &no_window);

	xcb_set_selection_owner(xwm->connection, xwm->window,
		xwm->atoms[XWM_WM_S0], XCB_CURRENT_TIME);

	xcb_composite_redirect_subwindows(xwm->connection, xwm->screen->root,
		XCB_COMPOSITE_REDIRECT_MANUAL);

	struct wl_event_loop *loop = wl_display_get_event_loop(display);
	xwm->event_source = wl_event_loop_add_fd(loop, wm_fd, WL_EVENT_READABLE,
		xwm_handle_event, xwm);
	wl_event_source_check(xwm->event_source);

	xcb_flush(xwm->connection);

	return 0;
}

static void
xwm_focus(struct xwm *xwm, struct xwayland_surface *surface)
{
	xcb_connection_t *connection = xwm->connection;
	xcb_window_t window = surface->window;
	assert(window != XCB_WINDOW_NONE);

    uint32_t values[] = { XCB_STACK_MODE_ABOVE };
    xcb_configure_window(connection, window, XCB_CONFIG_WINDOW_STACK_MODE, values);
	xcb_set_input_focus(connection, XCB_INPUT_FOCUS_NONE,
		XCB_WINDOW_NONE, XCB_CURRENT_TIME);
	xcb_flush(connection);
	xcb_set_input_focus(connection, XCB_INPUT_FOCUS_NONE, window,
		XCB_CURRENT_TIME);
	xcb_flush(connection);
}

static void
xwm_finish(struct xwm *xwm)
{
	if (xwm->connection) {
		xcb_destroy_window(xwm->connection, xwm->window);
		xcb_disconnect(xwm->connection);
	}
}

static i32
server_ready(i32 sig, void *data)
{
	struct xwayland *xwayland = data;

	if (xwm_init(&xwayland->xwm, xwayland->wm_fd[0], xwayland->display) != 0) {
		fprintf(stderr, "Failed to initialize xwm\n");
		return 0;
	}

	wl_event_source_remove(xwayland->usr1_source);
	return 0;
}

static i32
socket_open(struct sockaddr_un *address, i32 address_length)
{
	i32 fd = socket(PF_LOCAL, SOCK_STREAM | SOCK_CLOEXEC, 0);
	if (fd < 0) {
		perror("socket");
		return -1;
	}

	if (address->sun_path[0] != '\0') {
		unlink(address->sun_path);
	}

	address_length += offsetof(struct sockaddr_un, sun_path) + 1;
	if (bind(fd, (struct sockaddr *)address, address_length) == -1) {
		perror("bind");
		close(fd);
		return -1;
	}

	if (listen(fd, 1) != 0) {
		perror("listen");
		close(fd);
		return -1;
	}

	return fd;
}

static void
xwayland_new_surface(struct wl_listener *listener, void *data)
{
	struct xwm *xwm = wl_container_of(listener, xwm, new_surface);
	struct wl_resource *resource = data;
	u32 id = wl_resource_get_id(resource);

	for (u32 i = 0; i < xwm->unpaired_surface_count; i++) {
		u32 unpaired_id = xwm->unpaired_surfaces[i].wl_surface;
		if (id == unpaired_id) {
			xcb_window_t window = xwm->unpaired_surfaces[i].window;
			assert(window != XCB_WINDOW_NONE);

			xwm->unpaired_surfaces[i] =
				xwm->unpaired_surfaces[--xwm->unpaired_surface_count];
			xwayland_assign_role(resource, window, xwm->connection);
			break;
		}
	}
}

static i32
xwayland_init(struct xwayland *xwayland, struct compositor *compositor)
{
	struct wl_display *wl_display = compositor->display;
	xwayland->display = wl_display;

	if (socketpair(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC, 0, xwayland->wm_fd) != 0) {
		perror("socketpair");
		return -1;
	}

	if (socketpair(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC, 0, xwayland->wl_fd) != 0) {
		perror("socketpair");
		return -1;
	}

	u32 display;
	for (display = 0; display <= 32; display++) {
		char lock_file[32] = {0};
		snprintf(lock_file, sizeof(lock_file), lock_file_fmt, display);

		i32 flags = O_WRONLY | O_CLOEXEC | O_CREAT | O_EXCL;
		i32 lock_fd = open(lock_file, flags, 0444);
		if (lock_fd >= 0) {
			struct sockaddr_un addr = { .sun_family = AF_UNIX };
			char *addr_path = addr.sun_path;
			u32 addr_size = sizeof(addr.sun_path);
			i32 addr_len = 0;

			addr_len = snprintf(addr_path, addr_size, socket_fmt, display);
			if ((xwayland->x_fd[0] = socket_open(&addr, addr_len)) < 0) {
				fprintf(stderr, "Failed to open first socket\n");
				return -1;
			}

			*addr_path++ = '\0';
			addr_size--;
			addr_len = snprintf(addr_path, addr_size, socket_fmt, display);
			if ((xwayland->x_fd[1] = socket_open(&addr, addr_len)) < 0) {
				fprintf(stderr, "Failed to open second socket\n");
				return -1;
			}

			set_cloexec(xwayland->x_fd[0], 1);
			set_cloexec(xwayland->x_fd[1], 1);

			char pid_str[12];
			snprintf(pid_str, sizeof(pid_str), "%10d\n", getpid());
			write(lock_fd, pid_str, sizeof(pid_str) - 1);
			close(lock_fd);

			xwayland->display_number = display;
			break;
		}
	}

	if (display > 32) {
		fprintf(stderr, "Failed to open the x sockets\n");
		return -1;
	}

	xwayland->xwm.new_surface.notify = xwayland_new_surface;
	wl_signal_add(&compositor->new_surface, &xwayland->xwm.new_surface);

	xwayland->xwm.client = wl_client_create(wl_display, xwayland->wl_fd[0]);
	if (!xwayland->xwm.client) {
		fprintf(stderr, "Failed to create xwayland client\n");
		return -1;
	}

	xwayland->wl_fd[0] = -1;

	struct wl_event_loop *event_loop =
		wl_display_get_event_loop(xwayland->display);
	xwayland->usr1_source = wl_event_loop_add_signal(event_loop, SIGUSR1,
		server_ready, xwayland);

	switch (fork()) {
	case -1:
		perror("fork");
		return -1;
	case 0:
		set_cloexec(xwayland->x_fd[0], 0);
		set_cloexec(xwayland->x_fd[1], 0);
		set_cloexec(xwayland->wl_fd[1], 0);
		set_cloexec(xwayland->wm_fd[1], 0);

		char display[16];
		char listen[2][16];
		char wm[16];
		char wl[16];

		snprintf(display, sizeof(display), ":%d", xwayland->display_number);
		snprintf(listen[0], sizeof(*listen), "%d", xwayland->x_fd[0]);
		snprintf(listen[1], sizeof(*listen), "%d", xwayland->x_fd[1]);
		snprintf(wm, sizeof(wm), "%d", xwayland->wm_fd[1]);
		snprintf(wl, sizeof(wl), "%d", xwayland->wl_fd[1]);
		setenv("WAYLAND_SOCKET", wl, 1);

		char *argv[32] = {0};
		char **arg = argv;
		*arg++ = "Xwayland";
		*arg++ = display;
		*arg++ = "-rootless";
		*arg++ = "-terminate";
		*arg++ = "-listenfd";
		*arg++ = listen[0];
		*arg++ = "-listenfd";
		*arg++ = listen[1];
		*arg++ = "-wm";
		*arg++ = wm;
		*arg++ = 0;

		signal(SIGUSR1, SIG_IGN);
		execvp("Xwayland", argv);
		exit(EXIT_FAILURE);
	}

	close(xwayland->wl_fd[1]);
	close(xwayland->wm_fd[1]);

	wl_signal_init(&xwayland->xwm.destroy_notify);

	return 0;
}

void
xwayland_finish(struct xwayland *xwayland)
{
	wl_client_destroy(xwayland->xwm.client);
	xwm_finish(&xwayland->xwm);
	close(xwayland->x_fd[0]);
	close(xwayland->x_fd[1]);

	u32 display = xwayland->display_number;
	char addr[256];
	char lock_file[32];
	snprintf(addr, 256, socket_fmt, display);
	snprintf(lock_file, 32, lock_file_fmt, display);

	unlink(addr);
	unlink(lock_file);
}
