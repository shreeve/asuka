// asuka_linux.h: GLib, GObject, GIO, cairo, Pango, GDK, GTK 4, libadwaita
// and the optional libraries, as the Linux host and native packages'
// Linux C (RFC 0008) call them: one set of declarations, the types here
// (opaque, or the few structs whose layout is ABI, as GTK 4 keeps it) and
// the functions in asuka_linux.def, resolved with dlopen and dlsym the
// first time asuka_linux() is called (src/linux_syms.c), so the host and
// packages build from any system with Zig, no GTK sysroot. A package's
// call is G(name)(args): G(g_variant_new)("(s)", s); the host calls them
// by name (its own pointers, platform/linux/src/gtk.h).
#ifndef ASUKA_LINUX_H
#define ASUKA_LINUX_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <time.h>

#include "asuka_desktop.h"

#ifdef __cplusplus
extern "C" {
#endif

// ---- GLib ----

typedef int gint;
typedef unsigned int guint;
typedef int gboolean;
typedef char gchar;
typedef unsigned char guchar;
typedef short gshort;
typedef unsigned short gushort;
typedef long glong;
typedef unsigned long gulong;
typedef int8_t gint8;
typedef uint8_t guint8;
typedef int16_t gint16;
typedef uint16_t guint16;
typedef int32_t gint32;
typedef uint32_t guint32;
typedef int64_t gint64;
typedef uint64_t guint64;
typedef float gfloat;
typedef double gdouble;
typedef size_t gsize;
typedef ptrdiff_t gssize;
typedef void *gpointer;
typedef const void *gconstpointer;
typedef uint32_t gunichar;
typedef uint16_t gunichar2;
typedef uint32_t GQuark;
typedef gsize GType;
typedef gint64 goffset;
typedef int GPid;

typedef void (*GCallback)(void);
typedef void (*GDestroyNotify)(gpointer data);
typedef gboolean (*GSourceFunc)(gpointer user_data);
typedef void (*GFunc)(gpointer data, gpointer user_data);
typedef enum {
	G_SPAWN_DEFAULT = 0,
	G_SPAWN_LEAVE_DESCRIPTORS_OPEN = 1 << 0,
	G_SPAWN_DO_NOT_REAP_CHILD = 1 << 1,
	G_SPAWN_SEARCH_PATH = 1 << 2,
	G_SPAWN_STDOUT_TO_DEV_NULL = 1 << 3,
	G_SPAWN_STDERR_TO_DEV_NULL = 1 << 4,
} GSpawnFlags;
typedef void (*GSpawnChildSetupFunc)(gpointer data);
#define G_USEC_PER_SEC 1000000
typedef void (*GChildWatchFunc)(GPid pid, gint wait_status, gpointer user_data);

typedef struct _GError {
	GQuark domain;
	gint code;
	gchar *message;
} GError;

typedef struct _GList GList;
struct _GList {
	gpointer data;
	GList *next, *prev;
};
typedef struct _GSList GSList;
struct _GSList {
	gpointer data;
	GSList *next;
};

typedef struct _GArray {
	gchar *data;
	guint len;
} GArray;
typedef struct _GPtrArray {
	gpointer *pdata;
	guint len;
} GPtrArray;
typedef struct _GByteArray {
	guint8 *data;
	guint len;
} GByteArray;

typedef struct _GMainContext GMainContext;
typedef struct _GMainLoop GMainLoop;
typedef struct _GSource GSource;
typedef struct _GBytes GBytes;
typedef struct _GHashTable GHashTable;
typedef struct _GVariant GVariant;
typedef struct _GVariantType GVariantType;
typedef struct _GKeyFile GKeyFile;
typedef struct _GBookmarkFile GBookmarkFile;
typedef struct _GDateTime GDateTime;
typedef struct _GTimeZone GTimeZone;
typedef struct _GRegex GRegex;
typedef struct _GMatchInfo GMatchInfo;
typedef struct _GUri GUri;
typedef struct _GThread GThread;
typedef gpointer (*GThreadFunc)(gpointer data);
typedef guint (*GHashFunc)(gconstpointer key);
typedef gboolean (*GEqualFunc)(gconstpointer a, gconstpointer b);
typedef struct _SoupSession SoupSession;
typedef struct _SoupMessage SoupMessage;
typedef struct _SoupMessageHeaders SoupMessageHeaders;
typedef struct _SoupCookie SoupCookie;
typedef struct _SoupWebsocketConnection SoupWebsocketConnection;

// WebKitGTK 6.0 and JavaScriptCore.
typedef struct _WebKitWebView WebKitWebView;
typedef struct _WebKitSettings WebKitSettings;
typedef struct _WebKitUserContentManager WebKitUserContentManager;
typedef struct _WebKitUserScript WebKitUserScript;
typedef struct _WebKitUserContentFilter WebKitUserContentFilter;
typedef struct _WebKitUserContentFilterStore WebKitUserContentFilterStore;
typedef struct _WebKitBackForwardList WebKitBackForwardList;
typedef struct _WebKitPolicyDecision WebKitPolicyDecision;
typedef struct _WebKitResponsePolicyDecision WebKitResponsePolicyDecision;
typedef struct _WebKitURIResponse WebKitURIResponse;
typedef struct _WebKitURIRequest WebKitURIRequest;
typedef struct _WebKitNavigationAction WebKitNavigationAction;
typedef struct _WebKitWebResource WebKitWebResource;
typedef struct _WebKitCookieManager WebKitCookieManager;
typedef struct _WebKitNetworkSession WebKitNetworkSession;
typedef struct _JSCValue JSCValue;

// GVariantBuilder and GVariantIter are on the stack: opaque, as big as
// GLib makes them.
typedef struct _GVariantBuilder {
	gsize x[16];
} GVariantBuilder;
typedef struct _GVariantIter {
	gsize x[16];
} GVariantIter;
typedef struct _GVariantDict {
	gsize x[16];
} GVariantDict;

typedef enum {
	G_IO_IN = 1,
	G_IO_OUT = 4,
	G_IO_PRI = 2,
	G_IO_ERR = 8,
	G_IO_HUP = 16,
	G_IO_NVAL = 32,
} GIOCondition;

typedef gboolean (*GUnixFDSourceFunc)(gint fd, GIOCondition condition, gpointer user_data);

// ---- GObject ----

typedef struct _GTypeClass {
	GType g_type;
} GTypeClass;
typedef struct _GTypeInstance {
	GTypeClass *g_class;
} GTypeInstance;
typedef struct _GObject GObject;
typedef struct _GObjectClass GObjectClass;
typedef struct _GInitiallyUnowned GInitiallyUnowned;
typedef struct _GParamSpec GParamSpec;
typedef struct _GClosure GClosure;
typedef struct _GTypeInterface GTypeInterface;
typedef struct _GTypeModule GTypeModule;
typedef struct _GBinding GBinding;
typedef struct _GEnumClass GEnumClass;

typedef void (*GClosureNotify)(gpointer data, GClosure *closure);
typedef void (*GClassInitFunc)(gpointer g_class, gpointer class_data);
typedef void (*GInstanceInitFunc)(GTypeInstance *instance, gpointer g_class);
typedef void (*GBaseInitFunc)(gpointer g_class);
typedef void (*GBaseFinalizeFunc)(gpointer g_class);
typedef void (*GClassFinalizeFunc)(gpointer g_class, gpointer class_data);
typedef void (*GWeakNotify)(gpointer data, GObject *where_the_object_was);

typedef struct _GTypeQuery {
	GType type;
	const gchar *type_name;
	guint class_size;
	guint instance_size;
} GTypeQuery;

typedef struct _GEnumValue {
	gint value;
	const gchar *value_name;
	const gchar *value_nick;
} GEnumValue;

typedef struct _GValue {
	GType g_type;
	union {
		gint v_int;
		guint v_uint;
		glong v_long;
		gulong v_ulong;
		gint64 v_int64;
		guint64 v_uint64;
		gfloat v_float;
		gdouble v_double;
		gpointer v_pointer;
	} data[2];
} GValue;

typedef enum {
	G_CONNECT_DEFAULT = 0,
	G_CONNECT_AFTER = 1,
	G_CONNECT_SWAPPED = 2,
} GConnectFlags;

typedef enum {
	G_SIGNAL_MATCH_ID = 1,
	G_SIGNAL_MATCH_DETAIL = 2,
	G_SIGNAL_MATCH_CLOSURE = 4,
	G_SIGNAL_MATCH_FUNC = 8,
	G_SIGNAL_MATCH_DATA = 16,
	G_SIGNAL_MATCH_UNBLOCKED = 32,
} GSignalMatchType;

typedef enum {
	G_TYPE_FLAG_NONE = 0,
} GTypeFlags;

// The fundamental types (G_TYPE_MAKE_FUNDAMENTAL).
#define G_TYPE_NONE ((GType)(1 << 2))
#define G_TYPE_CHAR ((GType)(3 << 2))
#define G_TYPE_UCHAR ((GType)(4 << 2))
#define G_TYPE_BOOLEAN ((GType)(5 << 2))
#define G_TYPE_INT ((GType)(6 << 2))
#define G_TYPE_UINT ((GType)(7 << 2))
#define G_TYPE_LONG ((GType)(8 << 2))
#define G_TYPE_ULONG ((GType)(9 << 2))
#define G_TYPE_INT64 ((GType)(10 << 2))
#define G_TYPE_UINT64 ((GType)(11 << 2))
#define G_TYPE_ENUM ((GType)(12 << 2))
#define G_TYPE_FLAGS ((GType)(13 << 2))
#define G_TYPE_FLOAT ((GType)(14 << 2))
#define G_TYPE_DOUBLE ((GType)(15 << 2))
#define G_TYPE_STRING ((GType)(16 << 2))
#define G_TYPE_POINTER ((GType)(17 << 2))
#define G_TYPE_BOXED ((GType)(18 << 2))
#define G_TYPE_PARAM ((GType)(19 << 2))
#define G_TYPE_OBJECT ((GType)(20 << 2))
#define G_TYPE_VARIANT ((GType)(21 << 2))

// ---- GIO ----

typedef struct _GApplication GApplication;
typedef struct _GApplicationCommandLine GApplicationCommandLine;
typedef struct _GFile GFile;
typedef struct _GFileInfo GFileInfo;
typedef struct _GFileEnumerator GFileEnumerator;
typedef struct _GFileMonitor GFileMonitor;
typedef struct _GInputStream GInputStream;
typedef struct _GOutputStream GOutputStream;
typedef struct _GFileInputStream GFileInputStream;
typedef struct _GFileOutputStream GFileOutputStream;
typedef struct _GMemoryInputStream GMemoryInputStream;
typedef struct _GMemoryOutputStream GMemoryOutputStream;
typedef struct _GCancellable GCancellable;
typedef struct _GAsyncResult GAsyncResult;
typedef struct _GTask GTask;
typedef struct _GSettings GSettings;
typedef struct _GSettingsSchema GSettingsSchema;
typedef struct _GSettingsSchemaSource GSettingsSchemaSource;
typedef struct _GAction GAction;
typedef struct _GActionGroup GActionGroup;
typedef struct _GActionMap GActionMap;
typedef struct _GSimpleAction GSimpleAction;
typedef struct _GSimpleActionGroup GSimpleActionGroup;
typedef struct _GMenu GMenu;
typedef struct _GMenuItem GMenuItem;
typedef struct _GMenuModel GMenuModel;
typedef struct _GListModel GListModel;
typedef struct _GListStore GListStore;
typedef struct _GIcon GIcon;
typedef struct _GThemedIcon GThemedIcon;
typedef struct _GBytesIcon GBytesIcon;
typedef struct _GAppInfo GAppInfo;
typedef struct _GDesktopAppInfo GDesktopAppInfo;
typedef struct _GAppLaunchContext GAppLaunchContext;
typedef struct _GDBusConnection GDBusConnection;
typedef struct _GDBusProxy GDBusProxy;
typedef struct _GDBusMethodInvocation GDBusMethodInvocation;
typedef struct _GDBusNodeInfo GDBusNodeInfo;
typedef struct _GDBusInterfaceInfo GDBusInterfaceInfo;
typedef struct _GDBusMessage GDBusMessage;
typedef struct _GUnixFDList GUnixFDList;
typedef struct _GNotification GNotification;
typedef struct _GMemoryMonitor GMemoryMonitor;
typedef struct _GNetworkMonitor GNetworkMonitor;
typedef struct _GPowerProfileMonitor GPowerProfileMonitor;
typedef struct _GSubprocess GSubprocess;
typedef struct _GSubprocessLauncher GSubprocessLauncher;
typedef struct _GSocketClient GSocketClient;
typedef struct _GSocketConnection GSocketConnection;
typedef struct _GSocketConnectable GSocketConnectable;
typedef struct _GIOStream GIOStream;
typedef struct _GTlsCertificate GTlsCertificate;
typedef struct _GTlsDatabase GTlsDatabase;
typedef struct _GTlsInteraction GTlsInteraction;
// GTlsCertificateFlags: why a certificate isn't trusted.
typedef enum {
	G_TLS_CERTIFICATE_UNKNOWN_CA = 1 << 0,
	G_TLS_CERTIFICATE_BAD_IDENTITY = 1 << 1,
	G_TLS_CERTIFICATE_NOT_ACTIVATED = 1 << 2,
	G_TLS_CERTIFICATE_EXPIRED = 1 << 3,
	G_TLS_CERTIFICATE_REVOKED = 1 << 4,
	G_TLS_CERTIFICATE_INSECURE = 1 << 5,
	G_TLS_CERTIFICATE_GENERIC_ERROR = 1 << 6,
} GTlsCertificateFlags;
typedef enum { G_TLS_DATABASE_VERIFY_NONE = 0 } GTlsDatabaseVerifyFlags;
#define G_TLS_DATABASE_PURPOSE_AUTHENTICATE_SERVER "1.3.6.1.5.5.7.3.1"
typedef struct _GResolver GResolver;
typedef struct _GVolumeMonitor GVolumeMonitor;
typedef struct _GMount GMount;

typedef enum {
	G_OUTPUT_STREAM_SPLICE_NONE = 0,
	G_OUTPUT_STREAM_SPLICE_CLOSE_SOURCE = 1,
	G_OUTPUT_STREAM_SPLICE_CLOSE_TARGET = 2,
} GOutputStreamSpliceFlags;
typedef void (*GAsyncReadyCallback)(GObject *source_object, GAsyncResult *res, gpointer data);

typedef enum {
	G_APPLICATION_FLAGS_NONE = 0,
	G_APPLICATION_DEFAULT_FLAGS = 0,
	G_APPLICATION_IS_SERVICE = 1 << 0,
	G_APPLICATION_IS_LAUNCHER = 1 << 1,
	G_APPLICATION_HANDLES_OPEN = 1 << 2,
	G_APPLICATION_HANDLES_COMMAND_LINE = 1 << 3,
	G_APPLICATION_SEND_ENVIRONMENT = 1 << 4,
	G_APPLICATION_NON_UNIQUE = 1 << 5,
	G_APPLICATION_CAN_OVERRIDE_APP_ID = 1 << 6,
	G_APPLICATION_ALLOW_REPLACEMENT = 1 << 7,
	G_APPLICATION_REPLACE = 1 << 8,
} GApplicationFlags;

typedef enum {
	G_BUS_TYPE_STARTER = -1,
	G_BUS_TYPE_NONE = 0,
	G_BUS_TYPE_SYSTEM = 1,
	G_BUS_TYPE_SESSION = 2,
} GBusType;

typedef enum {
	G_DBUS_CALL_FLAGS_NONE = 0,
	G_DBUS_CALL_FLAGS_NO_AUTO_START = 1,
	G_DBUS_CALL_FLAGS_ALLOW_INTERACTIVE_AUTHORIZATION = 2,
} GDBusCallFlags;

typedef enum {
	G_DBUS_PROXY_FLAGS_NONE = 0,
	G_DBUS_PROXY_FLAGS_DO_NOT_LOAD_PROPERTIES = 1,
	G_DBUS_PROXY_FLAGS_DO_NOT_CONNECT_SIGNALS = 2,
	G_DBUS_PROXY_FLAGS_DO_NOT_AUTO_START = 4,
} GDBusProxyFlags;

typedef enum {
	G_DBUS_SIGNAL_FLAGS_NONE = 0,
	G_DBUS_SIGNAL_FLAGS_NO_MATCH_RULE = 1,
	G_DBUS_SIGNAL_FLAGS_MATCH_ARG0_NAMESPACE = 2,
	G_DBUS_SIGNAL_FLAGS_MATCH_ARG0_PATH = 4,
} GDBusSignalFlags;

typedef enum {
	G_DBUS_CONNECTION_FLAGS_NONE = 0,
	G_DBUS_CONNECTION_FLAGS_AUTHENTICATION_CLIENT = 1,
	G_DBUS_CONNECTION_FLAGS_MESSAGE_BUS_CONNECTION = 8,
} GDBusConnectionFlags;

typedef enum {
	G_BUS_NAME_OWNER_FLAGS_NONE = 0,
	G_BUS_NAME_OWNER_FLAGS_ALLOW_REPLACEMENT = 1,
	G_BUS_NAME_OWNER_FLAGS_REPLACE = 2,
	G_BUS_NAME_OWNER_FLAGS_DO_NOT_QUEUE = 4,
} GBusNameOwnerFlags;

typedef enum {
	G_BUS_NAME_WATCHER_FLAGS_NONE = 0,
	G_BUS_NAME_WATCHER_FLAGS_AUTO_START = 1,
} GBusNameWatcherFlags;

typedef enum {
	G_FILE_QUERY_INFO_NONE = 0,
	G_FILE_QUERY_INFO_NOFOLLOW_SYMLINKS = 1,
} GFileQueryInfoFlags;

typedef enum {
	G_FILE_CREATE_NONE = 0,
	G_FILE_CREATE_PRIVATE = 1,
	G_FILE_CREATE_REPLACE_DESTINATION = 2,
} GFileCreateFlags;

typedef enum {
	G_FILE_MONITOR_NONE = 0,
	G_FILE_MONITOR_WATCH_MOVES = 8,
} GFileMonitorFlags;

typedef enum {
	G_FILE_MONITOR_EVENT_CHANGED = 0,
	G_FILE_MONITOR_EVENT_CHANGES_DONE_HINT = 1,
	G_FILE_MONITOR_EVENT_DELETED = 2,
	G_FILE_MONITOR_EVENT_CREATED = 3,
} GFileMonitorEvent;

typedef enum {
	G_SUBPROCESS_FLAGS_NONE = 0,
	G_SUBPROCESS_FLAGS_STDIN_PIPE = 1,
	G_SUBPROCESS_FLAGS_STDOUT_PIPE = 4,
	G_SUBPROCESS_FLAGS_STDOUT_SILENCE = 8,
	G_SUBPROCESS_FLAGS_STDERR_PIPE = 16,
	G_SUBPROCESS_FLAGS_STDERR_SILENCE = 32,
	G_SUBPROCESS_FLAGS_STDERR_MERGE = 64,
} GSubprocessFlags;

typedef enum {
	G_USER_DIRECTORY_DESKTOP, G_USER_DIRECTORY_DOCUMENTS, G_USER_DIRECTORY_DOWNLOAD, G_USER_DIRECTORY_MUSIC,
	G_USER_DIRECTORY_PICTURES, G_USER_DIRECTORY_PUBLIC_SHARE, G_USER_DIRECTORY_TEMPLATES, G_USER_DIRECTORY_VIDEOS,
} GUserDirectory;

// GLib's Unicode, as far as the host reads it.
typedef enum {
	G_UNICODE_CONTROL, G_UNICODE_FORMAT, G_UNICODE_UNASSIGNED, G_UNICODE_PRIVATE_USE, G_UNICODE_SURROGATE,
	G_UNICODE_LOWERCASE_LETTER, G_UNICODE_MODIFIER_LETTER, G_UNICODE_OTHER_LETTER, G_UNICODE_TITLECASE_LETTER,
	G_UNICODE_UPPERCASE_LETTER, G_UNICODE_SPACING_MARK, G_UNICODE_ENCLOSING_MARK, G_UNICODE_NON_SPACING_MARK,
	G_UNICODE_DECIMAL_NUMBER, G_UNICODE_LETTER_NUMBER, G_UNICODE_OTHER_NUMBER,
} GUnicodeType;
typedef enum { G_NORMALIZE_DEFAULT, G_NORMALIZE_DEFAULT_COMPOSE, G_NORMALIZE_ALL, G_NORMALIZE_ALL_COMPOSE } GNormalizeMode;

typedef enum { G_CHECKSUM_MD5, G_CHECKSUM_SHA1, G_CHECKSUM_SHA256, G_CHECKSUM_SHA512, G_CHECKSUM_SHA384 } GChecksumType;

typedef enum {
	G_MEMORY_MONITOR_WARNING_LEVEL_LOW = 50,
	G_MEMORY_MONITOR_WARNING_LEVEL_MEDIUM = 100,
	G_MEMORY_MONITOR_WARNING_LEVEL_CRITICAL = 255,
} GMemoryMonitorWarningLevel;

typedef enum {
	G_NETWORK_CONNECTIVITY_LOCAL = 1,
	G_NETWORK_CONNECTIVITY_LIMITED = 2,
	G_NETWORK_CONNECTIVITY_PORTAL = 3,
	G_NETWORK_CONNECTIVITY_FULL = 4,
} GNetworkConnectivity;

typedef enum {
	G_NOTIFICATION_PRIORITY_NORMAL = 0,
	G_NOTIFICATION_PRIORITY_LOW = 1,
	G_NOTIFICATION_PRIORITY_HIGH = 2,
	G_NOTIFICATION_PRIORITY_URGENT = 3,
} GNotificationPriority;

typedef void (*GDBusSignalCallback)(GDBusConnection *connection, const gchar *sender_name, const gchar *object_path,
	const gchar *interface_name, const gchar *signal_name, GVariant *parameters, gpointer user_data);
typedef void (*GDBusInterfaceMethodCallFunc)(GDBusConnection *connection, const gchar *sender, const gchar *object_path,
	const gchar *interface_name, const gchar *method_name, GVariant *parameters, GDBusMethodInvocation *invocation, gpointer user_data);
typedef GVariant *(*GDBusInterfaceGetPropertyFunc)(GDBusConnection *connection, const gchar *sender, const gchar *object_path,
	const gchar *interface_name, const gchar *property_name, GError **error, gpointer user_data);
typedef gboolean (*GDBusInterfaceSetPropertyFunc)(GDBusConnection *connection, const gchar *sender, const gchar *object_path,
	const gchar *interface_name, const gchar *property_name, GVariant *value, GError **error, gpointer user_data);
typedef void (*GBusNameAppearedCallback)(GDBusConnection *connection, const gchar *name, const gchar *name_owner, gpointer user_data);
typedef void (*GBusNameVanishedCallback)(GDBusConnection *connection, const gchar *name, gpointer user_data);
typedef void (*GBusAcquiredCallback)(GDBusConnection *connection, const gchar *name, gpointer user_data);
typedef void (*GBusNameAcquiredCallback)(GDBusConnection *connection, const gchar *name, gpointer user_data);
typedef void (*GBusNameLostCallback)(GDBusConnection *connection, const gchar *name, gpointer user_data);

typedef struct _GDBusInterfaceVTable {
	GDBusInterfaceMethodCallFunc method_call;
	GDBusInterfaceGetPropertyFunc get_property;
	GDBusInterfaceSetPropertyFunc set_property;
	gpointer padding[8];
} GDBusInterfaceVTable;

typedef struct _GActionEntry {
	const gchar *name;
	void (*activate)(GSimpleAction *action, GVariant *parameter, gpointer user_data);
	const gchar *parameter_type;
	const gchar *state;
	void (*change_state)(GSimpleAction *action, GVariant *value, gpointer user_data);
	gsize padding[3];
} GActionEntry;

// ---- cairo, Pango, graphene ----

typedef struct _cairo cairo_t;
typedef struct _cairo_surface cairo_surface_t;
typedef struct _cairo_region cairo_region_t;
typedef struct _cairo_rectangle_int {
	int x, y, width, height;
} cairo_rectangle_int_t;
typedef enum {
	CAIRO_FORMAT_ARGB32 = 0,
	CAIRO_FORMAT_RGB24 = 1,
	CAIRO_FORMAT_A8 = 2,
} cairo_format_t;
typedef enum {
	CAIRO_STATUS_SUCCESS = 0,
} cairo_status_t;

typedef struct _PangoLayout PangoLayout;
typedef struct _PangoLayoutLine PangoLayoutLine;
typedef struct _PangoLayoutIter PangoLayoutIter;
typedef struct _PangoContext PangoContext;
typedef struct _PangoFontMap PangoFontMap;
typedef struct _PangoFcFontMap PangoFcFontMap;
typedef struct _PangoFontDescription PangoFontDescription;
typedef struct _PangoAttrList PangoAttrList;
typedef struct _PangoTabArray PangoTabArray;
typedef struct _PangoAttrIterator PangoAttrIterator;
typedef struct _PangoAttrClass PangoAttrClass;
typedef struct _PangoAttribute {
	const PangoAttrClass *klass;
	guint start_index;
	guint end_index;
} PangoAttribute;
typedef struct _PangoRectangle {
	int x, y, width, height;
} PangoRectangle;
typedef struct _PangoLogAttr PangoLogAttr;

typedef enum { PANGO_WRAP_WORD, PANGO_WRAP_CHAR, PANGO_WRAP_WORD_CHAR } PangoWrapMode;
typedef enum { PANGO_ELLIPSIZE_NONE, PANGO_ELLIPSIZE_START, PANGO_ELLIPSIZE_MIDDLE, PANGO_ELLIPSIZE_END } PangoEllipsizeMode;
typedef enum { PANGO_ALIGN_LEFT, PANGO_ALIGN_CENTER, PANGO_ALIGN_RIGHT } PangoAlignment;
typedef enum {
	PANGO_WEIGHT_THIN = 100,
	PANGO_WEIGHT_NORMAL = 400,
	PANGO_WEIGHT_BOLD = 700,
} PangoWeight;
typedef enum { PANGO_STYLE_NORMAL, PANGO_STYLE_OBLIQUE, PANGO_STYLE_ITALIC } PangoStyle;
typedef enum {
	PANGO_UNDERLINE_NONE,
	PANGO_UNDERLINE_SINGLE,
	PANGO_UNDERLINE_DOUBLE,
	PANGO_UNDERLINE_LOW,
	PANGO_UNDERLINE_ERROR,
} PangoUnderline;
typedef enum { PANGO_DIRECTION_LTR = 0, PANGO_DIRECTION_RTL = 1, PANGO_DIRECTION_NEUTRAL = 6 } PangoDirection;
typedef enum { PANGO_VARIANT_NORMAL, PANGO_VARIANT_SMALL_CAPS } PangoVariant;
typedef enum { PANGO_TAB_LEFT } PangoTabAlign;
#define PANGO_SCALE 1024

typedef struct _graphene_point_t {
	float x, y;
} graphene_point_t;
typedef struct _graphene_size_t {
	float width, height;
} graphene_size_t;
typedef struct _graphene_rect_t {
	graphene_point_t origin;
	graphene_size_t size;
} graphene_rect_t;
typedef struct _graphene_point3d_t {
	float x, y, z;
} graphene_point3d_t;
typedef struct _graphene_matrix_t {
	float m[16];
} __attribute__((aligned(16))) graphene_matrix_t;
typedef struct _graphene_vec4_t {
	float v[4];
} __attribute__((aligned(16))) graphene_vec4_t;

// ---- GDK, GSK ----

typedef struct _GdkRGBA {
	float red, green, blue, alpha;
} GdkRGBA;
typedef struct _GdkRectangle {
	int x, y, width, height;
} GdkRectangle;

typedef struct _GdkDisplay GdkDisplay;
typedef struct _GdkMonitor GdkMonitor;
typedef struct _GdkSurface GdkSurface;
typedef struct _GdkToplevel GdkToplevel;
typedef struct _GdkTexture GdkTexture;
typedef struct _GdkMemoryTexture GdkMemoryTexture;
typedef struct _GdkPaintable GdkPaintable;
typedef struct _GdkClipboard GdkClipboard;
typedef struct _GdkContentProvider GdkContentProvider;
typedef struct _GdkContentFormats GdkContentFormats;
typedef struct _GdkContentFormatsBuilder GdkContentFormatsBuilder;
typedef struct _GdkDrop GdkDrop;
typedef struct _GdkDrag GdkDrag;
typedef struct _GdkEvent GdkEvent;
typedef struct _GdkDevice GdkDevice;
typedef struct _GdkSeat GdkSeat;
typedef struct _GdkFrameClock GdkFrameClock;
typedef struct _GdkGLContext GdkGLContext;
typedef struct _GdkGLTextureBuilder GdkGLTextureBuilder;
typedef struct _GdkTextureDownloader GdkTextureDownloader;
typedef struct _GdkFileList GdkFileList;
typedef struct _GdkAppLaunchContext GdkAppLaunchContext;
typedef struct _GdkCursor GdkCursor;
typedef enum { GDK_SOURCE_MOUSE, GDK_SOURCE_PEN, GDK_SOURCE_KEYBOARD, GDK_SOURCE_TOUCHSCREEN, GDK_SOURCE_TOUCHPAD, GDK_SOURCE_TRACKPOINT, GDK_SOURCE_TABLET_PAD } GdkInputSource;
typedef struct _GdkPixbuf GdkPixbuf;
typedef struct _GdkSnapshot GdkSnapshot;
typedef struct _GdkPixbufAnimation GdkPixbufAnimation;
typedef struct _GdkPixbufAnimationIter GdkPixbufAnimationIter;
typedef struct _GdkPixbufLoader GdkPixbufLoader;
typedef struct _GdkKeymapKey GdkKeymapKey;

typedef struct _GskRoundedRect {
	graphene_rect_t bounds;
	graphene_size_t corner[4];
} GskRoundedRect;
typedef struct _GskColorStop {
	float offset;
	GdkRGBA color;
} GskColorStop;
typedef struct _GskShadow {
	GdkRGBA color;
	float dx, dy, radius;
} GskShadow;

typedef struct _GskRenderNode GskRenderNode;
typedef struct _GskRenderer GskRenderer;
typedef struct _GskTransform GskTransform;
typedef struct _GskPath GskPath;
typedef struct _GskPathBuilder GskPathBuilder;
typedef struct _GskPathMeasure GskPathMeasure;
typedef struct _GskStroke GskStroke;
typedef struct _GskPathPoint {
	gsize x[8];
} GskPathPoint;

typedef enum {
	GDK_SHIFT_MASK = 1 << 0,
	GDK_LOCK_MASK = 1 << 1,
	GDK_CONTROL_MASK = 1 << 2,
	GDK_ALT_MASK = 1 << 3,
	GDK_BUTTON1_MASK = 1 << 8,
	GDK_SUPER_MASK = 1 << 26,
	GDK_HYPER_MASK = 1 << 27,
	GDK_META_MASK = 1 << 28,
} GdkModifierType;

typedef enum {
	GDK_ACTION_COPY = 1 << 0,
	GDK_ACTION_MOVE = 1 << 1,
	GDK_ACTION_LINK = 1 << 2,
	GDK_ACTION_ASK = 1 << 3,
} GdkDragAction;

typedef enum {
	GDK_MEMORY_B8G8R8A8_PREMULTIPLIED = 0,
	GDK_MEMORY_A8R8G8B8_PREMULTIPLIED = 1,
	GDK_MEMORY_R8G8B8A8_PREMULTIPLIED = 2,
	GDK_MEMORY_B8G8R8A8 = 3,
	GDK_MEMORY_A8R8G8B8 = 4,
	GDK_MEMORY_R8G8B8A8 = 5,
} GdkMemoryFormat;

typedef enum {
	GDK_TOPLEVEL_STATE_MINIMIZED = 1 << 0,
	GDK_TOPLEVEL_STATE_MAXIMIZED = 1 << 1,
	GDK_TOPLEVEL_STATE_STICKY = 1 << 2,
	GDK_TOPLEVEL_STATE_FULLSCREEN = 1 << 3,
	GDK_TOPLEVEL_STATE_ABOVE = 1 << 4,
	GDK_TOPLEVEL_STATE_BELOW = 1 << 5,
	GDK_TOPLEVEL_STATE_FOCUSED = 1 << 6,
	GDK_TOPLEVEL_STATE_TILED = 1 << 7,
	GDK_TOPLEVEL_STATE_SUSPENDED = 1 << 16,
} GdkToplevelState;

typedef enum {
	GDK_GL_API_GL = 1 << 0,
	GDK_GL_API_GLES = 1 << 1,
} GdkGLAPI;

typedef enum {
	GDK_EVENT_PROPAGATE = 0,
	GDK_EVENT_STOP = 1,
} GdkEventPropagate;

typedef enum {
	GDK_SCROLL_UNIT_WHEEL,
	GDK_SCROLL_UNIT_SURFACE,
} GdkScrollUnit;

typedef enum {
	GSK_FILL_RULE_WINDING,
	GSK_FILL_RULE_EVEN_ODD,
} GskFillRule;
typedef enum { GSK_LINE_CAP_BUTT, GSK_LINE_CAP_ROUND, GSK_LINE_CAP_SQUARE } GskLineCap;
typedef enum { GSK_LINE_JOIN_MITER, GSK_LINE_JOIN_ROUND, GSK_LINE_JOIN_BEVEL } GskLineJoin;
typedef enum {
	GSK_SCALING_FILTER_LINEAR,
	GSK_SCALING_FILTER_NEAREST,
	GSK_SCALING_FILTER_TRILINEAR,
} GskScalingFilter;
typedef enum {
	GSK_BLEND_MODE_DEFAULT = 0,
} GskBlendMode;
typedef enum {
	GSK_MASK_MODE_ALPHA = 0,
	GSK_MASK_MODE_INVERTED_ALPHA,
	GSK_MASK_MODE_LUMINANCE,
	GSK_MASK_MODE_INVERTED_LUMINANCE,
} GskMaskMode;
typedef enum {
	GSK_PATH_FROM_START,
	GSK_PATH_TO_START,
	GSK_PATH_TO_END,
	GSK_PATH_FROM_END,
} GskPathDirection;

// ---- GTK ----

typedef struct _GtkWidget GtkWidget;
typedef struct _GtkWidgetClass GtkWidgetClass;
typedef struct _GtkWindow GtkWindow;
typedef struct _GtkApplication GtkApplication;
typedef struct _GtkApplicationWindow GtkApplicationWindow;
typedef struct _GtkLabel GtkLabel;
typedef struct _GtkButton GtkButton;
typedef struct _GtkToggleButton GtkToggleButton;
typedef struct _GtkCheckButton GtkCheckButton;
typedef struct _GtkSwitch GtkSwitch;
typedef struct _GtkScale GtkScale;
typedef struct _GtkRange GtkRange;
typedef struct _GtkAdjustment GtkAdjustment;
typedef struct _GtkEntry GtkEntry;
typedef struct _GtkText GtkText;
typedef struct _GtkPasswordEntry GtkPasswordEntry;
typedef struct _GtkEditable GtkEditable;
typedef struct _GtkEntryBuffer GtkEntryBuffer;
typedef struct _GtkTextView GtkTextView;
typedef struct _GtkTextBuffer GtkTextBuffer;
typedef struct _GtkTextTag GtkTextTag;
typedef struct _GtkTextTagTable GtkTextTagTable;
typedef struct _GtkTextMark GtkTextMark;
typedef struct _GtkTextChildAnchor GtkTextChildAnchor;
// GtkTextIter lives on the stack: GTK's layout.
typedef struct _GtkTextIter {
	gpointer dummy1, dummy2;
	gint dummy3, dummy4, dummy5, dummy6, dummy7, dummy8;
	gpointer dummy9, dummy10;
	gint dummy11, dummy12, dummy13;
	gpointer dummy14;
} GtkTextIter;
typedef struct _GtkScrolledWindow GtkScrolledWindow;
typedef struct _GtkViewport GtkViewport;
typedef struct _GtkBox GtkBox;
typedef struct _GtkSpinner GtkSpinner;
typedef struct _GtkProgressBar GtkProgressBar;
typedef struct _GtkDropDown GtkDropDown;
typedef struct _GtkStringList GtkStringList;
typedef struct _GtkStringObject GtkStringObject;
typedef struct _GtkListItem GtkListItem;
typedef struct _GtkListItemFactory GtkListItemFactory;
typedef struct _GtkColorDialog GtkColorDialog;
typedef struct _GtkColorDialogButton GtkColorDialogButton;
typedef struct _GtkCalendar GtkCalendar;
typedef struct _GtkSpinButton GtkSpinButton;
typedef struct _GtkPopover GtkPopover;
typedef struct _GtkPopoverMenu GtkPopoverMenu;
typedef struct _GtkMenuButton GtkMenuButton;
typedef struct _GtkSearchBar GtkSearchBar;
typedef struct _GtkSearchEntry GtkSearchEntry;
typedef struct _GtkImage GtkImage;
typedef struct _GtkPicture GtkPicture;
typedef struct _GtkVideo GtkVideo;
typedef struct _GtkMediaStream GtkMediaStream;
typedef struct _GtkMediaFile GtkMediaFile;
typedef struct _GtkSnapshot GtkSnapshot;
typedef struct _GtkStyleContext GtkStyleContext;
typedef struct _GtkStyleProvider GtkStyleProvider;
typedef struct _GtkCssProvider GtkCssProvider;
typedef struct _GtkSettings GtkSettings;
typedef struct _GtkIconTheme GtkIconTheme;
typedef struct _GtkIconPaintable GtkIconPaintable;
typedef struct _GtkNative GtkNative;
typedef struct _GtkRoot GtkRoot;
typedef struct _GtkEventController GtkEventController;
typedef struct _GtkEventControllerKey GtkEventControllerKey;
typedef struct _GtkEventControllerMotion GtkEventControllerMotion;
typedef struct _GtkEventControllerScroll GtkEventControllerScroll;
typedef struct _GtkEventControllerFocus GtkEventControllerFocus;
typedef struct _GtkGesture GtkGesture;
typedef struct _GtkGestureSingle GtkGestureSingle;
typedef struct _GtkGestureClick GtkGestureClick;
typedef struct _GtkGestureDrag GtkGestureDrag;
typedef struct _GtkGestureLongPress GtkGestureLongPress;
typedef struct _GtkGestureSwipe GtkGestureSwipe;
typedef struct _GtkGestureZoom GtkGestureZoom;
typedef struct _GtkGestureRotate GtkGestureRotate;
typedef struct _GtkShortcutController GtkShortcutController;
typedef struct _GtkShortcut GtkShortcut;
typedef struct _GtkShortcutTrigger GtkShortcutTrigger;
typedef struct _GtkShortcutAction GtkShortcutAction;
typedef struct _GtkDragSource GtkDragSource;
typedef struct _GtkDropTargetAsync GtkDropTargetAsync;
typedef struct _GtkDropTarget GtkDropTarget;
typedef struct _GtkUriLauncher GtkUriLauncher;
typedef struct _GtkFileLauncher GtkFileLauncher;
typedef struct _GtkFileDialog GtkFileDialog;
typedef struct _GtkFileFilter GtkFileFilter;
typedef struct _GtkAlertDialog GtkAlertDialog;
typedef struct _GtkPrintOperation GtkPrintOperation;
typedef struct _GtkAccessible GtkAccessible;
typedef struct _GtkATContext GtkATContext;
typedef struct _GtkLayoutManager GtkLayoutManager;
typedef struct _GtkOverlay GtkOverlay;
typedef struct _GtkStack GtkStack;
typedef struct _GtkGrid GtkGrid;
typedef struct _GtkFixed GtkFixed;
typedef struct _GtkSeparator GtkSeparator;
typedef struct _GtkListBox GtkListBox;
typedef struct _GtkListBoxRow GtkListBoxRow;
typedef struct _GtkTooltip GtkTooltip;
typedef struct _GtkGLArea GtkGLArea;
typedef struct _GtkMediaControls GtkMediaControls;
typedef struct _GtkWindowGroup GtkWindowGroup;
typedef struct _GtkSymbolicPaintable GtkSymbolicPaintable;
typedef struct _GtkActionBar GtkActionBar;

typedef gboolean (*GtkTickCallback)(GtkWidget *widget, GdkFrameClock *frame_clock, gpointer user_data);
typedef gboolean (*GtkShortcutFunc)(GtkWidget *widget, GVariant *args, gpointer user_data);

typedef enum { GTK_ORIENTATION_HORIZONTAL, GTK_ORIENTATION_VERTICAL } GtkOrientation;
typedef enum { GTK_ALIGN_FILL, GTK_ALIGN_START, GTK_ALIGN_END, GTK_ALIGN_CENTER, GTK_ALIGN_BASELINE } GtkAlign;
typedef enum { GTK_OVERFLOW_VISIBLE, GTK_OVERFLOW_HIDDEN } GtkOverflow;
typedef enum { GTK_POLICY_ALWAYS, GTK_POLICY_AUTOMATIC, GTK_POLICY_NEVER, GTK_POLICY_EXTERNAL } GtkPolicyType;
typedef enum { GTK_JUSTIFY_LEFT, GTK_JUSTIFY_RIGHT, GTK_JUSTIFY_CENTER, GTK_JUSTIFY_FILL } GtkJustification;
typedef enum { GTK_TEXT_DIR_NONE, GTK_TEXT_DIR_LTR, GTK_TEXT_DIR_RTL } GtkTextDirection;
typedef enum { GTK_CONTENT_FIT_FILL, GTK_CONTENT_FIT_CONTAIN, GTK_CONTENT_FIT_COVER, GTK_CONTENT_FIT_SCALE_DOWN } GtkContentFit;
typedef enum { GTK_PHASE_NONE, GTK_PHASE_CAPTURE, GTK_PHASE_BUBBLE, GTK_PHASE_TARGET } GtkPropagationPhase;
typedef enum { GTK_LIMIT_NONE, GTK_LIMIT_SAME_NATIVE } GtkPropagationLimit;
typedef enum { GTK_EVENT_SEQUENCE_NONE, GTK_EVENT_SEQUENCE_CLAIMED, GTK_EVENT_SEQUENCE_DENIED } GtkEventSequenceState;
typedef enum {
	GTK_EVENT_CONTROLLER_SCROLL_NONE = 0,
	GTK_EVENT_CONTROLLER_SCROLL_VERTICAL = 1,
	GTK_EVENT_CONTROLLER_SCROLL_HORIZONTAL = 2,
	GTK_EVENT_CONTROLLER_SCROLL_BOTH_AXES = 3,
	GTK_EVENT_CONTROLLER_SCROLL_DISCRETE = 4,
	GTK_EVENT_CONTROLLER_SCROLL_KINETIC = 8,
} GtkEventControllerScrollFlags;
typedef enum { GTK_SIZE_REQUEST_HEIGHT_FOR_WIDTH, GTK_SIZE_REQUEST_WIDTH_FOR_HEIGHT, GTK_SIZE_REQUEST_CONSTANT_SIZE } GtkSizeRequestMode;
typedef enum { GTK_POS_LEFT, GTK_POS_RIGHT, GTK_POS_TOP, GTK_POS_BOTTOM } GtkPositionType;
typedef enum { GTK_WRAP_NONE, GTK_WRAP_CHAR, GTK_WRAP_WORD, GTK_WRAP_WORD_CHAR } GtkWrapMode;
typedef enum {
	GTK_INPUT_PURPOSE_FREE_FORM,
	GTK_INPUT_PURPOSE_ALPHA,
	GTK_INPUT_PURPOSE_DIGITS,
	GTK_INPUT_PURPOSE_NUMBER,
	GTK_INPUT_PURPOSE_PHONE,
	GTK_INPUT_PURPOSE_URL,
	GTK_INPUT_PURPOSE_EMAIL,
	GTK_INPUT_PURPOSE_NAME,
	GTK_INPUT_PURPOSE_PASSWORD,
	GTK_INPUT_PURPOSE_PIN,
	GTK_INPUT_PURPOSE_TERMINAL,
} GtkInputPurpose;
typedef enum {
	GTK_INPUT_HINT_NONE = 0,
	GTK_INPUT_HINT_SPELLCHECK = 1 << 0,
	GTK_INPUT_HINT_NO_SPELLCHECK = 1 << 1,
	GTK_INPUT_HINT_WORD_COMPLETION = 1 << 2,
	GTK_INPUT_HINT_LOWERCASE = 1 << 3,
	GTK_INPUT_HINT_UPPERCASE_CHARS = 1 << 4,
	GTK_INPUT_HINT_UPPERCASE_WORDS = 1 << 5,
	GTK_INPUT_HINT_UPPERCASE_SENTENCES = 1 << 6,
	GTK_INPUT_HINT_INHIBIT_OSK = 1 << 7,
	GTK_INPUT_HINT_VERTICAL_WRITING = 1 << 8,
	GTK_INPUT_HINT_EMOJI = 1 << 9,
	GTK_INPUT_HINT_NO_EMOJI = 1 << 10,
	GTK_INPUT_HINT_PRIVATE = 1 << 11,
} GtkInputHints;
typedef enum {
	GTK_APPLICATION_INHIBIT_LOGOUT = 1,
	GTK_APPLICATION_INHIBIT_SWITCH = 2,
	GTK_APPLICATION_INHIBIT_SUSPEND = 4,
	GTK_APPLICATION_INHIBIT_IDLE = 8,
} GtkApplicationInhibitFlags;
typedef enum { GTK_SHORTCUT_SCOPE_LOCAL, GTK_SHORTCUT_SCOPE_MANAGED, GTK_SHORTCUT_SCOPE_GLOBAL } GtkShortcutScope;
typedef enum { GTK_POPOVER_MENU_SLIDING = 0, GTK_POPOVER_MENU_NESTED = 1 } GtkPopoverMenuFlags;
typedef enum { GTK_TEXT_WINDOW_WIDGET = 1, GTK_TEXT_WINDOW_TEXT = 2 } GtkTextWindowType;
typedef enum { GTK_STYLE_PROVIDER_PRIORITY_APPLICATION = 600 } GtkStyleProviderPriority;
typedef enum { GTK_STATE_FLAG_NORMAL = 0, GTK_STATE_FLAG_ACTIVE = 1, GTK_STATE_FLAG_PRELIGHT = 2, GTK_STATE_FLAG_SELECTED = 4, GTK_STATE_FLAG_INSENSITIVE = 8, GTK_STATE_FLAG_FOCUSED = 32, GTK_STATE_FLAG_FOCUS_VISIBLE = 1 << 14, GTK_STATE_FLAG_FOCUS_WITHIN = 1 << 15 } GtkStateFlags;
typedef enum { GTK_PICK_DEFAULT = 0, GTK_PICK_INSENSITIVE = 1, GTK_PICK_NON_TARGETABLE = 2 } GtkPickFlags;
typedef enum { GTK_TEXT_SEARCH_VISIBLE_ONLY = 1, GTK_TEXT_SEARCH_TEXT_ONLY = 2 } GtkTextSearchFlags;
typedef enum { GTK_DIR_TAB_FORWARD, GTK_DIR_TAB_BACKWARD, GTK_DIR_UP, GTK_DIR_DOWN, GTK_DIR_LEFT, GTK_DIR_RIGHT } GtkDirectionType;

// Accessibility (GtkAccessible's roles, states, properties).
typedef int GtkAccessibleRole;
typedef int GtkAccessibleState;
typedef int GtkAccessibleProperty;
typedef int GtkAccessibleRelation;
typedef enum { GTK_ACCESSIBLE_ANNOUNCEMENT_PRIORITY_LOW, GTK_ACCESSIBLE_ANNOUNCEMENT_PRIORITY_MEDIUM, GTK_ACCESSIBLE_ANNOUNCEMENT_PRIORITY_HIGH } GtkAccessibleAnnouncementPriority;

// ---- libadwaita ----

typedef struct _AdwApplication AdwApplication;
typedef struct _AdwApplicationWindow AdwApplicationWindow;
typedef struct _AdwWindow AdwWindow;
typedef struct _AdwToolbarView AdwToolbarView;
typedef struct _AdwHeaderBar AdwHeaderBar;
typedef struct _AdwWindowTitle AdwWindowTitle;
typedef struct _AdwToastOverlay AdwToastOverlay;
typedef struct _AdwToast AdwToast;
typedef struct _AdwNavigationView AdwNavigationView;
typedef struct _AdwNavigationPage AdwNavigationPage;
typedef struct _AdwNavigationSplitView AdwNavigationSplitView;
typedef struct _AdwOverlaySplitView AdwOverlaySplitView;
typedef struct _AdwViewStack AdwViewStack;
typedef struct _AdwViewStackPage AdwViewStackPage;
typedef struct _AdwViewSwitcher AdwViewSwitcher;
typedef struct _AdwViewSwitcherBar AdwViewSwitcherBar;
typedef struct _AdwDialog AdwDialog;
typedef struct _AdwAlertDialog AdwAlertDialog;
typedef struct _AdwEntryRow AdwEntryRow;
typedef struct _AdwPreferencesRow AdwPreferencesRow;
typedef struct _AdwActionRow AdwActionRow;
typedef struct _AdwPasswordEntryRow AdwPasswordEntryRow;
typedef struct _AdwPreferencesGroup AdwPreferencesGroup;
typedef struct _AdwStyleManager AdwStyleManager;
typedef struct _AdwSpinner AdwSpinner;
typedef struct _AdwBin AdwBin;
typedef struct _AdwButtonContent AdwButtonContent;
typedef struct _AdwBreakpoint AdwBreakpoint;
typedef struct _AdwBreakpointCondition AdwBreakpointCondition;
typedef struct _AdwCarousel AdwCarousel;
typedef struct _AdwStatusPage AdwStatusPage;
typedef struct _AdwClamp AdwClamp;
typedef struct _AdwAnimation AdwAnimation;
typedef struct _AdwAnimationTarget AdwAnimationTarget;
typedef struct _AdwTimedAnimation AdwTimedAnimation;
typedef enum { ADW_LINEAR, ADW_EASE_IN_QUAD, ADW_EASE_OUT_QUAD, ADW_EASE_IN_OUT_QUAD, ADW_EASE_IN_CUBIC, ADW_EASE_OUT_CUBIC } AdwEasing;

typedef enum { ADW_COLOR_SCHEME_DEFAULT, ADW_COLOR_SCHEME_FORCE_LIGHT, ADW_COLOR_SCHEME_PREFER_LIGHT, ADW_COLOR_SCHEME_PREFER_DARK, ADW_COLOR_SCHEME_FORCE_DARK } AdwColorScheme;
typedef enum { ADW_RESPONSE_DEFAULT, ADW_RESPONSE_SUGGESTED, ADW_RESPONSE_DESTRUCTIVE } AdwResponseAppearance;
typedef enum { ADW_TOOLBAR_FLAT, ADW_TOOLBAR_RAISED, ADW_TOOLBAR_RAISED_BORDER } AdwToolbarStyle;
typedef enum { ADW_VIEW_SWITCHER_POLICY_NARROW, ADW_VIEW_SWITCHER_POLICY_WIDE } AdwViewSwitcherPolicy;
typedef enum { ADW_TOAST_PRIORITY_NORMAL, ADW_TOAST_PRIORITY_HIGH } AdwToastPriority;
// Evolution Data Server's and libical-glib's.
typedef struct _ESourceRegistry ESourceRegistry;
typedef struct _ESource ESource;
typedef struct _ESourceExtension ESourceExtension;
typedef struct _EClient EClient;
typedef struct _EBookClient EBookClient;
typedef struct _ECalClient ECalClient;
typedef struct _EContact EContact;
typedef struct _EVCard EVCard;
typedef struct _ICalComponent ICalComponent;
typedef struct _ICalTime ICalTime;
typedef struct _ICalTimezone ICalTimezone;
typedef gboolean (*ECalRecurInstanceCb)(ICalComponent *comp, ICalTime *start, ICalTime *end, gpointer data, GCancellable *cancellable, GError **error);

typedef enum { ADW_DIALOG_AUTO, ADW_DIALOG_FLOATING, ADW_DIALOG_BOTTOM_SHEET } AdwDialogPresentationMode;


#define G_VARIANT_TYPE(s) ((const GVariantType *)(s))
// GLib's main loop priorities.
enum { G_PRIORITY_HIGH = -100, G_PRIORITY_DEFAULT = 0, G_PRIORITY_HIGH_IDLE = 100, G_PRIORITY_DEFAULT_IDLE = 200 };

// ---- GStreamer (optional) ----

typedef struct _GstObject GstObject;
typedef struct _GstElement GstElement;
typedef struct _GstElementFactory GstElementFactory;
typedef struct _GstBus GstBus;
typedef struct _GstMessage GstMessage;
typedef struct _GstEvent GstEvent;
typedef struct _GstCaps GstCaps;
typedef struct _GstStructure GstStructure;
typedef struct _GstSample GstSample;
typedef struct _GstBuffer GstBuffer;
typedef struct _GstAppSink GstAppSink;
typedef guint64 GstClockTime;
#define GST_CLOCK_TIME_NONE ((GstClockTime)-1)
#define GST_SECOND ((gint64)1000000000)

// GstMapInfo, as GStreamer 1.x lays it out.
typedef struct {
	gpointer memory;
	int flags;
	guint8 *data;
	gsize size;
	gsize maxsize;
	gpointer user_data[4];
	gpointer _gst_reserved[4];
} GstMapInfo;

typedef enum {
	GST_STATE_VOID_PENDING = 0,
	GST_STATE_NULL = 1,
	GST_STATE_READY = 2,
	GST_STATE_PAUSED = 3,
	GST_STATE_PLAYING = 4,
} GstState;
typedef enum {
	GST_STATE_CHANGE_FAILURE = 0,
	GST_STATE_CHANGE_SUCCESS = 1,
	GST_STATE_CHANGE_ASYNC = 2,
	GST_STATE_CHANGE_NO_PREROLL = 3,
} GstStateChangeReturn;
typedef enum { GST_FORMAT_TIME = 3 } GstFormat;
typedef enum {
	GST_SEEK_FLAG_NONE = 0,
	GST_SEEK_FLAG_FLUSH = 1,
	GST_SEEK_FLAG_ACCURATE = 2,
	GST_SEEK_FLAG_KEY_UNIT = 4,
} GstSeekFlags;
typedef enum { GST_SEEK_TYPE_NONE = 0, GST_SEEK_TYPE_SET = 1 } GstSeekType;
typedef enum { GST_MAP_READ = 1, GST_MAP_WRITE = 2 } GstMapFlags;

// ---- gdk-pixbuf, libshumate, geocode-glib (optional) ----

typedef enum { GDK_COLORSPACE_RGB = 0 } GdkColorspace;

// ---- GTK's printing, libsecret (optional) ----

typedef struct _GtkPrintSettings GtkPrintSettings;
typedef struct _GtkPageSetup GtkPageSetup;
typedef struct _GtkPaperSize GtkPaperSize;
typedef struct _GtkPrintJob GtkPrintJob;
typedef struct _GtkPrinter GtkPrinter;
typedef struct _SecretSchema SecretSchema;
typedef struct _SecretValue SecretValue;
typedef struct _GeocodeLocation GeocodeLocation;
typedef struct _GeocodeReverse GeocodeReverse;
typedef struct _GeocodePlace GeocodePlace;

// The desktop portal's bus name and object.
#define ASUKA_PORTAL "org.freedesktop.portal.Desktop"
#define ASUKA_PORTAL_PATH "/org/freedesktop/portal/desktop"

// The libraries, by the tags asuka_linux.def names them with: GLib's
// stack, which asuka_linux() opens, and the optional ones, opened as
// something first needs them (asuka_linux_load).
enum {
	ASUKA_LIB_GLIB,
	ASUKA_LIB_GOBJECT,
	ASUKA_LIB_GIO,
	ASUKA_LIB_CAIRO,
	ASUKA_LIB_PANGO,
	ASUKA_LIB_PANGOCAIRO,
	ASUKA_LIB_GTK,
	ASUKA_LIB_ADW,
	// Optional: loaded when something needs them (lx_lib_load).
	ASUKA_LIB_WEBKIT,
	ASUKA_LIB_JSC,
	ASUKA_LIB_GST,
	ASUKA_LIB_GSTVIDEO,
	ASUKA_LIB_GSTAPP,
	ASUKA_LIB_GSTAUDIO,
	ASUKA_LIB_X11,
	ASUKA_LIB_XEXT,
	ASUKA_LIB_FC,
	ASUKA_LIB_PANGOFT2,
	ASUKA_LIB_EPOXY,
	ASUKA_LIB_SECRET,
	ASUKA_LIB_SOUP,
	ASUKA_LIB_EDS,  // Evolution Data Server and libical-glib
	ASUKA_LIB_EBOOK,
	ASUKA_LIB_EBOOKC,
	ASUKA_LIB_ECAL,
	ASUKA_LIB_ICAL,
	ASUKA_LIB_PIXBUF,   // gdk-pixbuf
	ASUKA_LIB_SHUMATE,  // libshumate: maps
	ASUKA_LIB_GEOCODE,  // geocode-glib: place names
	ASUKA_LIB_XTST,     // libXtst: the host's automation's real input (X11)
	ASUKA_LIB_COUNT,
};

// asuka_linux_load opens a library (ASUKA_LIB_*) and resolves its
// functions, once: false when the system hasn't it. Any thread.
bool asuka_linux_load(int lib);
// asuka_linux_soname is the first file name a library is looked for by.
const char *asuka_linux_soname(int lib);
// asuka_linux_sym is any symbol of the libraries opened, NULL if none has
// it.
void *asuka_linux_sym(const char *name);

#define SYM(lib, ret, name, params) ret(*name) params;
typedef struct {
#include "asuka_linux.def"
} asuka_linux_t;
#undef SYM

// asuka_linux is the functions, loaded the first time (any thread): NULL
// when GLib isn't there. A function this system's libraries lack is NULL.
const asuka_linux_t *asuka_linux(void);

// G(name) is a function of asuka_linux's: it must be loaded (asuka_linux()
// isn't NULL) first.
#define G(name) (asuka_linux_fns.name)
extern asuka_linux_t asuka_linux_fns;

// asuka_linux_image_load decodes an image file with GdkPixbuf (what its
// loaders read: PNG, JPEG, GIF, BMP, TIFF, WebP and more where they're
// installed), upright (its EXIF orientation applied): *pixels its rows of
// RGBA, not premultiplied, width*4 bytes each, of malloc's. false when
// it isn't an image GdkPixbuf reads (or there's no GdkPixbuf). Any
// thread.
bool asuka_linux_image_load(const char *path, int *width, int *height, uint8_t **pixels);

// asuka_soup_tls has a session's message go as t says (src/tls.c; NULL:
// the system's): the server's certificate checked against t's authorities
// (the session's database; with trust_system, the system's then t's), or
// not at all (insecure), the client's certificate shown when it's asked
// for. A server that isn't trusted fails the message: asuka_soup_untrusted
// then says why (NULL when that isn't why). Call it before the message is
// sent.
void asuka_soup_tls(SoupSession *s, SoupMessage *msg, const asuka_tls *t);
const char *asuka_soup_untrusted(SoupMessage *msg);

// asuka_linux_fail fails err with a GError's message, and frees it:
// asuka_error_failed with the domain "gio" and its code (what for none).
void asuka_linux_fail(asuka_error *err, GError *e, const char *what);

// asuka_linux_main runs fn(ctx) on the main thread through GLib's main
// context (the GTK host's main loop): for what must run where GLib's
// signals come.
void asuka_linux_main(void (*fn)(void *ctx), void *ctx);

// asuka_linux_bus is the session's or the system's bus (G_BUS_TYPE_*),
// connected once and kept; NULL with *e set when there's none.
GDBusConnection *asuka_linux_bus(int kind, GError **e);

// asuka_linux_gst loads and initializes GStreamer (libgstreamer-1.0,
// libgstapp-1.0), once: false when it isn't there.
bool asuka_linux_gst(void);

// asuka_linux_is_a reports whether obj is of the type named (a GType a
// library registers: GdkX11Display), false when there's no such type.
bool asuka_linux_is_a(gpointer obj, const char *type);

// asuka_linux_cancelled reports whether a GError says the user cancelled
// (G_IO_ERROR_CANCELLED, a GTK dialog cancelled or dismissed); it
// doesn't free it.
bool asuka_linux_cancelled(const GError *e);

#ifndef ASUKA_LINUX_HOST
#define g_signal_connect(o, sig, cb, data) G(g_signal_connect_data)((o), (sig), (GCallback)(cb), (data), NULL, 0)
#endif

#ifdef __cplusplus
}
#endif

#endif
