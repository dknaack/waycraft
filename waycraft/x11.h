enum x11_atom {
	X11_NET_WM_NAME,
	X11_WM_DELETE_WINDOW,
	X11_WM_PROTOCOLS,
	X11_WM_NAME,
	X11_UTF8_STRING,
	X11_ATOM_COUNT
};

union x11_event {
	xcb_generic_event_t *generic;
	xcb_client_message_event_t *client_message;
	xcb_button_press_event_t *button_press;
	xcb_button_release_event_t *button_release;
	xcb_key_press_event_t *key_press;
	xcb_key_release_event_t *key_release;
	xcb_motion_notify_event_t *motion_notify;
	xcb_configure_notify_event_t *configure_notify;
	xcb_property_notify_event_t *property_notify;
};

struct x11_state {
	xcb_connection_t *connection;
	xcb_screen_t *screen;
	xcb_window_t window;
	xcb_atom_t atoms[X11_ATOM_COUNT];

	u32 width;
	u32 height;
	bool is_open;
	bool is_focused;
	bool lock_cursor;

	i32 keymap;
	i32 keymap_size;
	struct xkb_state *xkb_state;
};
