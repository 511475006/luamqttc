#include <memory.h>

#include "lua.h"
#include "lauxlib.h"

#include "MQTTPacket.h"
#include "MQTTConnect.h"

#include "luamqttpacket.h"

static int buff_len(int est) {
    return (est / LUAL_BUFFERSIZE + 1) * LUAL_BUFFERSIZE;
}

static MQTTString make_MQTTString(const char *cstring) {
    MQTTString mqttString = MQTTString_initializer;
    mqttString.cstring = (char *) cstring;
    return mqttString;
}

static MQTTPacket_willOptions parse_will_options(lua_State *L, int idx, unsigned char will_flag) {
    MQTTPacket_willOptions options = MQTTPacket_willOptions_initializer;
    if (will_flag == 1) {
        luaL_checktype(L, idx, LUA_TTABLE);

        lua_getfield(L, idx, "topic_name");
        if (lua_isstring(L, -1)) {
            options.topicName = make_MQTTString(lua_tostring(L, -1));
        } else {
            luaL_error(L, "invalid topic_name");
        }
        lua_pop(L, 1);

        lua_getfield(L, idx, "message");
        if (lua_isstring(L, -1)) {
            options.message = make_MQTTString(lua_tostring(L, -1));
        } else {
            luaL_error(L, "invalid message");
        }
        lua_pop(L, 1);

        lua_getfield(L, idx, "retained");
        if (lua_isstring(L, -1)) {
            options.retained = (unsigned char) (lua_toboolean(L, -1) ? 1 : 0);
        }
        lua_pop(L, 1);

        lua_getfield(L, idx, "qos");
        if (lua_isstring(L, -1)) {
            options.qos = (unsigned char) lua_tonumber(L, -1);
        }
        lua_pop(L, 1);
    }

    return options;
}

static MQTTPacket_connectData parse_connect_options(lua_State *L, int idx) {
    MQTTPacket_connectData options = MQTTPacket_connectData_initializer;
    unsigned char will_flag = 0;

    luaL_checktype(L, idx, LUA_TTABLE);

    lua_getfield(L, idx, "client_id");
    if (lua_isstring(L, -1)) {
        options.clientID = make_MQTTString(lua_tostring(L, -1));
    } else {
        luaL_error(L, "invalid client_id");
    }
    lua_pop(L, 1);

    lua_getfield(L, idx, "username");
    if (lua_isstring(L, -1)) {
        options.username = make_MQTTString(lua_tostring(L, -1));
    } else {
        luaL_error(L, "invalid username");
    }
    lua_pop(L, 1);

    lua_getfield(L, idx, "password");
    if (lua_isstring(L, -1)) {
        options.password = make_MQTTString(lua_tostring(L, -1));
    } else {
        luaL_error(L, "invalid password");
    }
    lua_pop(L, 1);

    lua_getfield(L, idx, "keep_alive_interval");
    if (lua_isnumber(L, -1)) {
        options.keepAliveInterval = (unsigned short) lua_tointeger(L, -1);
    }
    lua_pop(L, 1);

    lua_getfield(L, idx, "clean_session");
    if (lua_isboolean(L, -1)) {
        options.cleansession = (unsigned char) (lua_toboolean(L, -1) ? 1 : 0);
    }
    lua_pop(L, 1);


    lua_getfield(L, idx, "will_flag");
    if (lua_isboolean(L, -1)) {
        will_flag = (unsigned char) (lua_toboolean(L, -1) ? 1 : 0);
        options.willFlag = will_flag;
    }
    lua_pop(L, 1);

    lua_getfield(L, idx, "will_options");
    options.will = parse_will_options(L, -1, will_flag);
    lua_pop(L, 1);

    return options;
}

typedef struct PublishOptions PublishOptions;

struct PublishOptions {
    int qos;
    unsigned char retained;
    unsigned char dup;
    unsigned short msg_id;
};

static PublishOptions parse_publish_options(lua_State *L, int idx) {
    PublishOptions options = {0, 0, 0, 0};

    luaL_checktype(L, idx, LUA_TTABLE);
    lua_getfield(L, idx, "qos");
    if (lua_isnumber(L, -1)) {
        options.qos = (int) lua_tointeger(L, -1);
    }
    lua_pop(L, 1);

    lua_getfield(L, idx, "retained");
    if (lua_isboolean(L, -1)) {
        options.retained = (unsigned char) (lua_toboolean(L, -1) ? 1 : 0);;
    }
    lua_pop(L, 1);

    lua_getfield(L, idx, "dup");
    if (lua_isboolean(L, -1)) {
        options.dup = (unsigned char) (lua_toboolean(L, -1) ? 1 : 0);;
    }
    lua_pop(L, 1);

    lua_getfield(L, idx, "id");
    if (lua_isnumber(L, -1)) {
        options.msg_id = (unsigned short) lua_tointeger(L, -1);
    }
    lua_pop(L, 1);

    return options;
}

static int serialize_connect(lua_State *L) {
    MQTTPacket_connectData options = parse_connect_options(L, 1);

    luaL_Buffer result;
    int len = 0;
    int est_len = buff_len(0);
    luaL_buffinitsize(L, &result, (size_t) est_len);

    len = MQTTSerialize_connect((unsigned char *) result.b, est_len, &options);
    if (len <= 0) {
        luaL_error(L, "failed to serialize connect");
    }
    luaL_pushresultsize(&result, (size_t) len);

    return 1;
}

static int deserialize_connack(lua_State *L) {
    unsigned char session_present = 0;
    unsigned char connack_rc = 255;

    luaL_checktype(L, 1, LUA_TSTRING);
    MQTTDeserialize_connack(&session_present, &connack_rc,
                            (unsigned char *) lua_tostring(L, 1), luaL_len(L, 1));
    lua_pushboolean(L, connack_rc == 0);
    lua_pushinteger(L, connack_rc);
    return 2;
}

static int serialize_pingreq(lua_State *L) {
    luaL_Buffer result;
    int len = 0;
    int est_len = buff_len(0);
    luaL_buffinitsize(L, &result, (size_t) est_len);

    len = MQTTSerialize_pingreq((unsigned char *) result.b, est_len);
    if (len <= 0) {
        luaL_error(L, "failed to serialize ping");
    }
    luaL_pushresultsize(&result, (size_t) len);
    return 1;
}

static int serialize_publish(lua_State *L) {
    char *topic = NULL;
    unsigned char *payload = NULL;
    int payload_len = 0;
    PublishOptions options = {0, 0, 0, 0};

    luaL_Buffer result;
    int len = 0;
    int est_len = buff_len(0);


    int n = lua_gettop(L);
    if (n < 2) {
        luaL_error(L, "invalid parameters");
    }

    if (lua_isstring(L, 1) && lua_isstring(L, 2)) {
        topic = (char *) lua_tostring(L, 1);
        payload = (unsigned char *) lua_tostring(L, 2);
        payload_len = luaL_len(L, 2);
        est_len = buff_len(luaL_len(L, 1) + payload_len);
    } else {
        luaL_error(L, "invalid parameters");
    }

    if (n == 3) {
        options = parse_publish_options(L, 3);
    }
    luaL_buffinitsize(L, &result, (size_t) est_len);

    len = MQTTSerialize_publish((unsigned char *) result.b, est_len, options.dup,
                                options.qos, options.retained, options.msg_id,
                                make_MQTTString(topic), payload, payload_len);
    if (len <= 0) {
        luaL_error(L, "failed to serialize publish");
    }
    luaL_pushresultsize(&result, (size_t) len);
    return 1;
}

static int serialize_subscribe(lua_State *L) {
    luaL_Buffer result;
    int len = 0;
    int qos = 0;
    int est_len = buff_len(0);
    MQTTString topic;

    luaL_checktype(L, 1, LUA_TSTRING);
    topic = make_MQTTString((char *) lua_tostring(L, 1));
    luaL_buffinitsize(L, &result, (size_t) est_len);

    luaL_checktype(L, 2, LUA_TNUMBER);
    qos = (int) lua_tointeger(L, 2);

    //todo
    len = MQTTSerialize_subscribe((unsigned char *) result.b, est_len, 0, 0, 1, &topic, &qos);
    if (len <= 0) {
        luaL_error(L, "failed to serialize subscribe");
    }

    luaL_pushresultsize(&result, (size_t) len);
    return 1;
}

static int deserialize_suback(lua_State *L) {
    int count = 0, grantedQoS = 128;
    unsigned short packetid;

    luaL_checktype(L, 1, LUA_TSTRING);
    if (MQTTDeserialize_suback(&packetid, 1, &count, &grantedQoS,
                               (unsigned char *) lua_tostring(L, 1), luaL_len(L, 1)) == 1)

        lua_pushboolean(L, grantedQoS != 128);
    lua_pushinteger(L, grantedQoS);
    return 2;
}

static int serialize_unsubscribe(lua_State *L) {
    luaL_Buffer result;
    int len = 0;
    int est_len = buff_len(0);
    MQTTString topic;

    luaL_checktype(L, 1, LUA_TSTRING);
    topic = make_MQTTString((char *) lua_tostring(L, 1));
    luaL_buffinitsize(L, &result, (size_t) est_len);
    //todo: packet id?
    len = MQTTSerialize_unsubscribe((unsigned char *) result.b, est_len, 0, 0, 1, &topic);
    if (len <= 0) {
        luaL_error(L, "failed to serialize unsubscribe");
    }

    luaL_pushresultsize(&result, (size_t) len);
    return 1;
}

static int deserialize_ack(lua_State *L) {
    unsigned short packetid;
    unsigned char dup = 0, type;

    luaL_checktype(L, 1, LUA_TSTRING);
    lua_pushboolean(L, MQTTDeserialize_ack(
            &type, &dup, &packetid,
            (unsigned char *) lua_tostring(L, 1),
            luaL_len(L, 1)) == 1);
    lua_pushnumber(L, type);
    lua_pushnumber(L, packetid);
    return 3;
}

static int serialize_disconnect(lua_State *L) {
    luaL_Buffer result;
    int len = 0;
    int est_len = buff_len(0);
    luaL_buffinitsize(L, &result, (size_t) est_len);

    len = MQTTSerialize_disconnect((unsigned char *) result.b, est_len);
    if (len <= 0) {
        luaL_error(L, "failed to serialize disconnect");
    }
    luaL_pushresultsize(&result, (size_t) len);
    return 1;
}

static const struct luaL_Reg mqttpacket[] = {
        {"serialize_connect",     serialize_connect},
        {"deserialize_connack",   deserialize_connack},
        {"serialize_pingreq",     serialize_pingreq},
        {"serialize_publish",     serialize_publish},
        {"serialize_subscribe",   serialize_subscribe},
        {"deserialize_suback",    deserialize_suback},
        {"serialize_unsubscribe", serialize_unsubscribe},
        {"deserialize_ack",       deserialize_ack},
        {"serialize_disconnect",  serialize_disconnect},
        {NULL, NULL}
};

int luaopen_mqttpacket(lua_State *L) {
    luaL_newlib(L, mqttpacket);
    return 1;
}