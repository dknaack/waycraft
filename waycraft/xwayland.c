#include <xcb/xcb.h>
#include <stdio.h>
#include <unistd.h>
#include <signal.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/wait.h>

#include <waycraft/compositor.h>
#include <waycraft/xwayland.h>

static const char lock_file_fmt[] = "/tmp/.X%d-lock";
static const char socket_fmt[] = "/tmp/.X11-unix/X%d";

static const char *xwm_atom_names[XWM_ATOM_COUNT] = {
	[XWM_NET_WM_NAME] = "_NET_WM_NAME",
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

static i32
xwm_handle_event(i32 fd, u32 mask, void *data)
{
	xcb_generic_event_t *event;
	struct xwm *xwm = data;
	u32 count = 0;

	puts("!!! handle event");
	while ((event = xcb_poll_for_event(xwm->connection))) {
		printf("received event: %d\n", event->response_type & 0x7f);
		free(event);
		count++;
	}

	if (count != 0) {
		xcb_flush(xwm->connection);
	}

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

	xcb_flush(xwm->connection);

	struct wl_event_loop *loop = wl_display_get_event_loop(display);
	xwm->event_source = wl_event_loop_add_fd(
		loop, wm_fd, WL_EVENT_READABLE, xwm_handle_event, xwm);
	wl_event_source_check(xwm->event_source);

	return 0;
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

	return 0;
}

i32
xwayland_exec(struct xwayland *xwayland)
{
	char display_number_str[16];
	char listen_fd_str[2][16];
	char wm_fd_str[16];
	char *argv[32] = {0};
	char **arg = argv;

	set_cloexec(xwayland->x_fd[0], 0);
	set_cloexec(xwayland->x_fd[1], 0);
	set_cloexec(xwayland->wl_fd[1], 0);
	set_cloexec(xwayland->wm_fd[1], 0);

	snprintf(display_number_str, 16, ":%d", xwayland->display_number);
	snprintf(listen_fd_str[0], 16, "%d", xwayland->x_fd[0]);
	snprintf(listen_fd_str[1], 16, "%d", xwayland->x_fd[1]);
	snprintf(wm_fd_str, 16, "%d", xwayland->wm_fd[1]);

	*arg++ = "Xwayland";
	*arg++ = display_number_str;
	*arg++ = "-core";
	*arg++ = "-terminate";
	*arg++ = "-rootless";
	*arg++ = "-listenfd";
	*arg++ = listen_fd_str[0];
	*arg++ = "-listenfd";
	*arg++ = listen_fd_str[1];
	*arg++ = "-wm";
	*arg++ = wm_fd_str;

	struct wl_event_loop *event_loop =
		wl_display_get_event_loop(xwayland->display);
	wl_event_loop_add_signal(event_loop, SIGUSR1, server_ready, xwayland);

	char wl_fd_str[16];
	snprintf(wl_fd_str, sizeof(wl_fd_str), "%d", xwayland->wl_fd[1]);
	setenv("WAYLAND_SOCKET", wl_fd_str, 1);

	switch (fork()) {
	case -1:
		perror("fork");
		return -1;
	case 0:
		signal(SIGUSR1, SIG_IGN);
		execvp(argv[0], argv);
		_exit(EXIT_FAILURE);
	}

	close(xwayland->wm_fd[1]);
	close(xwayland->wl_fd[1]);
	return 0;
}

static i32
socket_open(struct sockaddr_un *address, i32 address_length)
{
	i32 fd = socket(AF_UNIX, SOCK_STREAM, 0);
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

i32
xwayland_init(struct xwayland *xwayland, struct wl_display *wl_display)
{
	xwayland->display = wl_display;

	if (socketpair(AF_UNIX, SOCK_STREAM, 0, xwayland->wm_fd) != 0) {
		perror("socketpair");
		return -1;
	}
	set_cloexec(xwayland->wm_fd[0], 1);
	set_cloexec(xwayland->wm_fd[1], 1);

	if (socketpair(AF_UNIX, SOCK_STREAM, 0, xwayland->wl_fd) != 0) {
		perror("socketpair");
		return -1;
	}
	set_cloexec(xwayland->wl_fd[0], 1);
	set_cloexec(xwayland->wl_fd[1], 1);

	xwayland->client = wl_client_create(wl_display, xwayland->wl_fd[0]);
	xwayland->wl_fd[0] = -1;

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
			snprintf(pid_str, sizeof(pid_str), "%10d", getpid());
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

	xwayland_exec(xwayland);
	return 0;
}

void
xwayland_finish(struct xwayland *xwayland)
{
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
