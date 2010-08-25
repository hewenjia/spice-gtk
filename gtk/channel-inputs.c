#include "spice-client.h"
#include "spice-common.h"

#define SPICE_INPUTS_CHANNEL_GET_PRIVATE(obj)                                  \
    (G_TYPE_INSTANCE_GET_PRIVATE((obj), SPICE_TYPE_INPUTS_CHANNEL, spice_inputs_channel))

struct spice_inputs_channel {
    int                         bs;
    int                         dx, dy;
    unsigned int                x, y, dpy;
    int                         motion_count;
};

G_DEFINE_TYPE(SpiceInputsChannel, spice_inputs_channel, SPICE_TYPE_CHANNEL)

static void spice_inputs_handle_msg(SpiceChannel *channel, spice_msg_in *msg);

/* ------------------------------------------------------------------ */

static void spice_inputs_channel_init(SpiceInputsChannel *channel)
{
    spice_inputs_channel *c;

    fprintf(stderr, "%s\n", __FUNCTION__);

    c = channel->priv = SPICE_INPUTS_CHANNEL_GET_PRIVATE(channel);
    memset(c, 0, sizeof(*c));
}

static void spice_inputs_channel_finalize(GObject *obj)
{
    fprintf(stderr, "%s\n", __FUNCTION__);

    if (G_OBJECT_CLASS(spice_inputs_channel_parent_class)->finalize)
        G_OBJECT_CLASS(spice_inputs_channel_parent_class)->finalize(obj);
}

static void spice_inputs_channel_class_init(SpiceInputsChannelClass *klass)
{
    GObjectClass *gobject_class = G_OBJECT_CLASS(klass);
    SpiceChannelClass *channel_class = SPICE_CHANNEL_CLASS(klass);

    fprintf(stderr, "%s\n", __FUNCTION__);

    gobject_class->finalize     = spice_inputs_channel_finalize;
    channel_class->handle_msg   = spice_inputs_handle_msg;

    g_type_class_add_private(klass, sizeof(spice_inputs_channel));
}

/* ------------------------------------------------------------------ */

static void send_motion(SpiceInputsChannel *channel)
{
    spice_inputs_channel *c = channel->priv;
    SpiceMsgcMouseMotion motion;
    spice_msg_out *msg;

    if (!c->dx && !c->dy)
        return;

    motion.buttons_state = c->bs;
    motion.dx            = c->dx;
    motion.dy            = c->dy;
    msg = spice_msg_out_new(SPICE_CHANNEL(channel),
                            SPICE_MSGC_INPUTS_MOUSE_MOTION);
    msg->marshallers->msgc_inputs_mouse_motion(msg->marshaller, &motion);
    spice_msg_out_send(msg);
    spice_msg_out_put(msg);

    c->motion_count++;
    c->dx = 0;
    c->dy = 0;
}

static void send_position(SpiceInputsChannel *channel)
{
    spice_inputs_channel *c = channel->priv;
    SpiceMsgcMousePosition position;
    spice_msg_out *msg;

    if (c->dpy == -1)
        return;

#if 0
    fprintf(stderr, "%s: +%d+%d\n", __FUNCTION__, c->x, c->y);
#endif
    position.buttons_state = c->bs;
    position.x             = c->x;
    position.y             = c->y;
    position.display_id    = c->dpy;
    msg = spice_msg_out_new(SPICE_CHANNEL(channel),
                            SPICE_MSGC_INPUTS_MOUSE_POSITION);
    msg->marshallers->msgc_inputs_mouse_position(msg->marshaller, &position);
    spice_msg_out_send(msg);
    spice_msg_out_put(msg);

    c->motion_count++;
    c->dpy = -1;
}

static void inputs_handle_init(SpiceChannel *channel, spice_msg_in *in)
{
}

static void inputs_handle_modifiers(SpiceChannel *channel, spice_msg_in *in)
{
    fprintf(stderr, "%s: TODO\n", __FUNCTION__);
}

static void inputs_handle_ack(SpiceChannel *channel, spice_msg_in *in)
{
    spice_inputs_channel *c = SPICE_INPUTS_CHANNEL(channel)->priv;
    c->motion_count -= SPICE_INPUT_MOTION_ACK_BUNCH;
    send_motion(SPICE_INPUTS_CHANNEL(channel));
    send_position(SPICE_INPUTS_CHANNEL(channel));
}

static spice_msg_handler inputs_handlers[] = {
    [ SPICE_MSG_SET_ACK ]                  = spice_channel_handle_set_ack,
    [ SPICE_MSG_PING ]                     = spice_channel_handle_ping,
    [ SPICE_MSG_NOTIFY ]                   = spice_channel_handle_notify,

    [ SPICE_MSG_INPUTS_INIT ]              = inputs_handle_init,
    [ SPICE_MSG_INPUTS_KEY_MODIFIERS ]     = inputs_handle_modifiers,
    [ SPICE_MSG_INPUTS_MOUSE_MOTION_ACK ]  = inputs_handle_ack,
};

static void spice_inputs_handle_msg(SpiceChannel *channel, spice_msg_in *msg)
{
    int type = spice_msg_in_type(msg);
    assert(type < SPICE_N_ELEMENTS(inputs_handlers));
    assert(inputs_handlers[type] != NULL);
    inputs_handlers[type](channel, msg);
}

void spice_inputs_motion(SpiceInputsChannel *channel, gint dx, gint dy,
                         gint button_state)
{
    spice_inputs_channel *c = channel->priv;

    c->bs  = button_state;
    c->dx += dx;
    c->dy += dy;

    if (c->motion_count < SPICE_INPUT_MOTION_ACK_BUNCH * 2) {
        send_motion(channel);
    }
}

void spice_inputs_position(SpiceInputsChannel *channel, gint x, gint y,
                           gint display, gint button_state)
{
    spice_inputs_channel *c = channel->priv;

    c->bs  = button_state;
    c->x   = x;
    c->y   = y;
    c->dpy = display;

    if (c->motion_count < SPICE_INPUT_MOTION_ACK_BUNCH * 2) {
        send_position(channel);
    }
}

void spice_inputs_button_press(SpiceInputsChannel *channel, gint button,
                               gint button_state)
{
    spice_inputs_channel *c = channel->priv;
    SpiceMsgcMousePress press = {
        .button = button,
        .buttons_state = button_state,
    };
    spice_msg_out *msg;

    c->bs  = button_state;

    send_motion(channel);
    send_position(channel);

    msg = spice_msg_out_new(SPICE_CHANNEL(channel),
                            SPICE_MSGC_INPUTS_MOUSE_PRESS);
    msg->marshallers->msgc_inputs_mouse_press(msg->marshaller, &press);
    spice_msg_out_send(msg);
    spice_msg_out_put(msg);
}

void spice_inputs_button_release(SpiceInputsChannel *channel, gint button,
                                 gint button_state)
{
    spice_inputs_channel *c = channel->priv;
    SpiceMsgcMouseRelease release = {
        .button = button,
        .buttons_state = button_state,
    };
    spice_msg_out *msg;

    c->bs  = button_state;

    send_motion(channel);
    send_position(channel);

    msg = spice_msg_out_new(SPICE_CHANNEL(channel),
                            SPICE_MSGC_INPUTS_MOUSE_RELEASE);
    msg->marshallers->msgc_inputs_mouse_release(msg->marshaller, &release);
    spice_msg_out_send(msg);
    spice_msg_out_put(msg);
}

void spice_inputs_key_press(SpiceInputsChannel *channel, guint scancode)
{
    SpiceMsgcKeyDown down;
    spice_msg_out *msg;

#if 0
    fprintf(stderr, "%s: scancode %d\n", __FUNCTION__, scancode);
#endif
    if (scancode < 0x100) {
        down.code = scancode;
    } else {
        down.code = 0xe0 | ((scancode - 0x100) << 8);
    }

    msg = spice_msg_out_new(SPICE_CHANNEL(channel),
                            SPICE_MSGC_INPUTS_KEY_DOWN);
    msg->marshallers->msgc_inputs_key_down(msg->marshaller, &down);
    spice_msg_out_send(msg);
    spice_msg_out_put(msg);
}

void spice_inputs_key_release(SpiceInputsChannel *channel, guint scancode)
{
    SpiceMsgcKeyUp up;
    spice_msg_out *msg;

#if 0
    fprintf(stderr, "%s: scancode %d\n", __FUNCTION__, scancode);
#endif
    if (scancode < 0x100) {
        up.code = scancode | 0x80;
    } else {
        up.code = 0x80e0 | ((scancode - 0x100) << 8);
    }

    msg = spice_msg_out_new(SPICE_CHANNEL(channel),
                            SPICE_MSGC_INPUTS_KEY_UP);
    msg->marshallers->msgc_inputs_key_up(msg->marshaller, &up);
    spice_msg_out_send(msg);
    spice_msg_out_put(msg);
}
