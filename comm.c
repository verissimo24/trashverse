#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <zmq.h>

#include "comm.h"              
#include "space_trash.pb-c.h"  // generated from space_trash.proto

zmqChannel create_server_channel(int reqrep_port) {
    zmqChannel ch = {0};

    ch.ctx = zmq_ctx_new();
    if (!ch.ctx) {
        fprintf(stderr, "zmq_ctx_new failed\n");
        return ch;
    }

    ch.sock = zmq_socket(ch.ctx, ZMQ_REP);
    if (!ch.sock) {
        fprintf(stderr, "zmq_socket(ZMQ_REP) failed\n");
        zmq_ctx_shutdown (ch.ctx);
        ch.ctx = NULL;
        return ch;
    }

    char endpoint[64];
    snprintf(endpoint, sizeof(endpoint), "tcp://*:%d", reqrep_port);

    if (zmq_bind(ch.sock, endpoint) != 0) {
        fprintf(stderr, "zmq_bind failed\n");
        zmq_close(ch.sock);
        ch.sock = NULL;
        ch.ctx  = NULL;
        return ch;
    }

    int timeout_ms = 100; 
    zmq_setsockopt(ch.sock, ZMQ_RCVTIMEO, &timeout_ms, sizeof(timeout_ms));

    return ch;
}

zmqChannel create_client_channel(const char *address, int reqrep_port)
{
    zmqChannel ch = {0};
    if (!address) return ch;

    ch.ctx = zmq_ctx_new();
    if (!ch.ctx) {
        fprintf(stderr, "zmq_ctx_new failed\n");
        return ch;
    }

    ch.sock = zmq_socket(ch.ctx, ZMQ_REQ);
    if (!ch.sock) {
        fprintf(stderr, "zmq_socket(ZMQ_REQ) failed\n");
        zmq_ctx_shutdown(ch.ctx);
        ch.ctx = NULL;
        return ch;
    }

    char endpoint[128];
    snprintf(endpoint, sizeof(endpoint), "tcp://%s:%d", address, reqrep_port);

    fprintf(stderr, "Client connecting REQ to: %s\n", endpoint);
    if (zmq_connect(ch.sock, endpoint) != 0) {
        fprintf(stderr, "zmq_connect(%s) failed: %s\n", endpoint, zmq_strerror(errno));
        zmq_close(ch.sock);
        zmq_ctx_shutdown(ch.ctx);
        ch.sock = NULL;
        ch.ctx  = NULL;
        return ch;
    }
    int timeout_ms = 500;
    zmq_setsockopt(ch.sock, ZMQ_RCVTIMEO, &timeout_ms, sizeof(timeout_ms));
    zmq_setsockopt(ch.sock, ZMQ_SNDTIMEO, &timeout_ms, sizeof(timeout_ms));

    int linger = 0;
    zmq_setsockopt(ch.sock, ZMQ_LINGER, &linger, sizeof(linger));

    return ch;
}

static Direction dir_char_to_proto(direction_t d) {
    switch (d) {
        case UP:    return DIRECTION__DIR_UP;
        case DOWN:  return DIRECTION__DIR_DOWN;
        case LEFT:  return DIRECTION__DIR_LEFT;
        case RIGHT: return DIRECTION__DIR_RIGHT;
        default:    return DIRECTION__DIR_UP;
    }
}

static direction_t dir_proto_to_char(Direction d) {
    switch (d) {
        case DIRECTION__DIR_UP:    return UP;
        case DIRECTION__DIR_DOWN:  return DOWN;
        case DIRECTION__DIR_LEFT:  return LEFT;
        case DIRECTION__DIR_RIGHT: return RIGHT;
        default:                   return UP;
    }
}


void send_connection_message(void *fd) {

    //Creates connect message
    ClientConnect connect = CLIENT_CONNECT__INIT;
  
    //Creates envelope message clientMessage
    ClientMessage msg = CLIENT_MESSAGE__INIT;
    msg.kind_case = CLIENT_MESSAGE__KIND_CONNECT;
    msg.connect  = &connect;

    //packs message into bytes
    uint8_t buffer[128];
    size_t len = client_message__pack(&msg, buffer);

    //sends message
    int rc = zmq_send(fd, buffer, len, 0);
    if (rc == -1) perror("zmq_send");
}


void send_movement_message(void *fd, char ch, direction_t d, uint32_t token) 
{
    //Creates move message
    ClientMove move = CLIENT_MOVE__INIT;

    //stores ship id as a string
    char id_str[2] = { ch, '\0' };
    move.ship_id = id_str;

   // convert direction character to protobuf enum
    move.dir = dir_char_to_proto(d);

    move.token = token;

    //Creates envelope message clientMessage
    ClientMessage msg = CLIENT_MESSAGE__INIT;
    msg.kind_case = CLIENT_MESSAGE__KIND_MOVE;
    msg.move      = &move;

    //packs message into bytes
    uint8_t buffer[128];
    size_t len = client_message__pack(&msg, buffer);

    //sends message
    int rc = zmq_send(fd, buffer, len, 0);
    if (rc == -1) perror("zmq_send");
}

void send_quit_message(void *fd, char ch, uint32_t token)
{
    ClientQuit q = CLIENT_QUIT__INIT;

    char id_str[2] = { ch, '\0' };
    q.ship_id = id_str;
    q.token   = token;

    ClientMessage msg = CLIENT_MESSAGE__INIT;
    msg.kind_case = CLIENT_MESSAGE__KIND_QUIT;
    msg.quit      = &q;

    uint8_t buffer[128];
    size_t len = client_message__pack(&msg, buffer);

    int rc = zmq_send(fd, buffer, len, 0);
    if (rc == -1) perror("zmq_send");
}


void read_message(void *fd, char *message_type, char * c, direction_t *d, uint32_t *token) {

    //Receive serialized bytes
    uint8_t buffer[128];
    int msg_len = zmq_recv(fd, buffer, sizeof(buffer), 0);
    if (msg_len <= 0) {
        message_type[0] = '\0';
        return;
    }

    //Unpack(convert) received bytes
    ClientMessage *msg = client_message__unpack(NULL, msg_len, buffer);
    if (!msg) {
        fprintf(stderr, "read_message: failed to unpack ClientMessage\n");
        message_type[0] = '\0';
        return;
    }

    //Default values
    *c = '?';
    *d = '\0';  
    *token = 0;

    //Identify message type, CONNECT or MOVE or Invalid
    if (msg->kind_case == CLIENT_MESSAGE__KIND_CONNECT && msg->connect) {
    strcpy(message_type, "CONNECT");

    } else if (msg->kind_case == CLIENT_MESSAGE__KIND_MOVE && msg->move) {
        strcpy(message_type, "MOVE");
        *c = msg->move->ship_id ? msg->move->ship_id[0] : '?';
        *d = dir_proto_to_char(msg->move->dir);
        *token = msg->move->token;

    } else if (msg->kind_case == CLIENT_MESSAGE__KIND_QUIT && msg->quit) {
        strcpy(message_type, "QUIT");
        *c = msg->quit->ship_id ? msg->quit->ship_id[0] : '?';
        *d = 0; // não usado
        *token = msg->quit->token;

    } else {
        message_type[0] = '\0';
    }

    client_message__free_unpacked(msg, NULL);
}


void send_response(void *fd, char *message) {
    ServerAck ack = SERVER_ACK__INIT;
    ack.ok = (strcmp(message, "OK") == 0);
    ack.msg = message;

    uint8_t buffer[128];
    size_t len = server_ack__pack(&ack, buffer);

    zmq_send(fd, buffer, len, 0);
}

void send_connect_response(void *fd, char ship_id, uint32_t token) 
{    
    //Creates response message (ok)
    ServerAck ack = SERVER_ACK__INIT;
    ack.ok = 1;

    //Stores ship_id as a string
    char id_str[2] = { ship_id, '\0' };
    ack.ship_id = id_str;

    //Includes token
    ack.token = token;   
    ack.has_token = 1;   

    //sends message
    uint8_t buffer[128];
    size_t len = server_ack__pack(&ack, buffer);
    zmq_send(fd, buffer, len, 0);
}


void receive_response(void *fd, char *message) 
{
    //receive bytes from socket
    uint8_t buffer[128];
    int len = zmq_recv(fd, buffer, sizeof(buffer), 0);

    //received empty message
    if (len <= 0) {
        message[0] = '\0';
        return;
    }

    ServerAck *ack = server_ack__unpack(NULL, len, buffer);
    if (!ack) {
        fprintf(stderr, "receive_response: failed to unpack ServerAck\n");
        message[0] = '\0';
        return;
    }

    //Extract response message
    if (ack->msg) {
        strncpy(message, ack->msg, 99);
        message[99] = '\0';
    } else {
        strcpy(message, ack->ok ? "OK" : "ERROR");
    }

    server_ack__free_unpacked(ack, NULL);
}


int receive_connect_ack(void *fd, char *ship_id_out, uint32_t *token_out)
{
    //receive bytes from socket
    uint8_t buffer[128];
    int len = zmq_recv(fd, buffer, sizeof(buffer), 0);
    if (len <= 0) return -1;

    ServerAck *ack = server_ack__unpack(NULL, len, buffer);
    if (!ack) return -1;

    //validate message
    if (!ack->ok || !ack->ship_id || !ack->has_token) {
        server_ack__free_unpacked(ack, NULL);
        return -1;
    }

    //extract ship_id and token assigned by the server
    *ship_id_out = ack->ship_id[0];
    *token_out = ack->token;

    server_ack__free_unpacked(ack, NULL);
    return 0;
}


zmqChannel create_server_pub_channel(int pub_port)
{
    zmqChannel ch = (zmqChannel){0};

    if (pub_port <= 0) pub_port = 5556;

    ch.ctx = zmq_ctx_new();
    if (!ch.ctx) {
        fprintf(stderr, "zmq_ctx_new failed\n");
        return ch;
    }

    ch.sock = zmq_socket(ch.ctx, ZMQ_PUB);
    if (!ch.sock) {
        fprintf(stderr, "zmq_socket(ZMQ_PUB) failed\n");
        zmq_ctx_shutdown(ch.ctx);
        ch.ctx = NULL;
        return ch;
    }

    char endpoint[64];
    snprintf(endpoint, sizeof(endpoint), "tcp://*:%d", pub_port);

    if (zmq_bind(ch.sock, endpoint) != 0) {
        fprintf(stderr, "zmq_bind(PUB) failed: %s\n", zmq_strerror(errno));
        zmq_close(ch.sock);
        zmq_ctx_shutdown(ch.ctx);
        ch.sock = NULL;
        ch.ctx  = NULL;
        return ch;
    }

    return ch;
}

zmqChannel create_client_sub_channel(const char *address, int pub_port, const char *topic)
{
    zmqChannel ch = (zmqChannel){0};
    if (!address) return ch;
    if (pub_port <= 0) pub_port = 5556;

    ch.ctx = zmq_ctx_new();
    if (!ch.ctx) {
        fprintf(stderr, "zmq_ctx_new failed\n");
        return ch;
    }

    ch.sock = zmq_socket(ch.ctx, ZMQ_SUB);
    if (!ch.sock) {
        fprintf(stderr, "zmq_socket(ZMQ_SUB) failed\n");
        zmq_ctx_shutdown(ch.ctx);
        ch.ctx = NULL;
        return ch;
    }

    // Subscribe
    if (topic) {
        zmq_setsockopt(ch.sock, ZMQ_SUBSCRIBE, topic, strlen(topic));
    } else {
        zmq_setsockopt(ch.sock, ZMQ_SUBSCRIBE, "", 0);
    }

    int timeout_ms = 2000;
    zmq_setsockopt(ch.sock, ZMQ_RCVTIMEO, &timeout_ms, sizeof(timeout_ms));

    char endpoint[128];
    snprintf(endpoint, sizeof(endpoint), "tcp://%s:%d", address, pub_port);

    if (zmq_connect(ch.sock, endpoint) != 0) {
        fprintf(stderr, "zmq_connect(SUB) failed: %s\n", zmq_strerror(errno));
        zmq_close(ch.sock);
        zmq_ctx_shutdown(ch.ctx);
        ch.sock = NULL;
        ch.ctx  = NULL;
        return ch;
    }

    return ch;
}

static size_t count_active_trash(const Universe *u)
{
    if (!u || !u->trash) return 0;
    size_t n = 0;
    for (int i = 0; i < u->max_trash; ++i)
        if (u->trash[i].active) n++;
    return n;
}

static size_t count_active_ships(const Universe *u)
{
    if (!u || !u->ships) return 0;
    size_t n = 0;
    for (int i = 0; i < MAX_SHIPS; ++i)
        if (u->ships[i].active) n++;
    return n;
}


int comm_pub_send_universe_state(void *pub_sock, const Universe *u, int game_over)
{
    if (!pub_sock || !u) return -1;

    //Initialize protobuf base message
    UniverseState st = UNIVERSE_STATE__INIT;
    st.width            = (uint32_t)u->width;
    st.height           = (uint32_t)u->height;
    st.max_trash        = (uint32_t)u->max_trash;
    st.recycling_planet = (uint32_t)((u->recycling_planet < 0) ? 0 : u->recycling_planet);
    st.game_over = game_over ? 1 : 0;

    // -------- Planets --------
    size_t np = (u->num_planets > 0 && u->planets) ? (size_t)u->num_planets : 0;  //Total number of planets
    PlanetState  *p_msgs = NULL;
    PlanetState **p_ptrs = NULL;

    if (np > 0) {
        p_msgs = calloc(np, sizeof(*p_msgs));
        p_ptrs = calloc(np, sizeof(*p_ptrs));
        if (!p_msgs || !p_ptrs) {
            free(p_msgs); free(p_ptrs);
            return -1;
        }

        for (size_t i = 0; i < np; ++i) {
            p_msgs[i] = (PlanetState)PLANET_STATE__INIT;
            p_ptrs[i] = &p_msgs[i];

            const Planet *p = &u->planets[i];
            p_msgs[i].id          = (uint32_t)i;
            p_msgs[i].name_ascii  = (uint32_t)(unsigned char)p->planet_name;
            p_msgs[i].x           = p->x;
            p_msgs[i].y           = p->y;
            p_msgs[i].recycle     = p->recycle ? 1 : 0;
            p_msgs[i].trash_count = (uint32_t)p->trash_count;
            p_msgs[i].has_trash_count = 1;
        }

        st.n_planets = np;
        st.planets   = p_ptrs;
    }

    // -------- Trash (only active) --------
    size_t nt = count_active_trash(u);
    TrashState  *t_msgs = NULL;
    TrashState **t_ptrs = NULL;

    if (nt > 0) {
        t_msgs = calloc(nt, sizeof(*t_msgs));
        t_ptrs = calloc(nt, sizeof(*t_ptrs));
        if (!t_msgs || !t_ptrs) {
            free(p_msgs); free(p_ptrs);
            free(t_msgs); free(t_ptrs);
            return -1;
        }

        size_t k = 0;
        for (int i = 0; i < u->max_trash; ++i) {
            const Trash *t = &u->trash[i];
            if (!t->active) continue;

            t_msgs[k] = (TrashState)TRASH_STATE__INIT;
            t_ptrs[k] = &t_msgs[k];
            t_msgs[k].x = t->x;
            t_msgs[k].y = t->y;

            if (++k == nt) break;
        }

        st.n_trash = nt;
        st.trash   = t_ptrs;
    }

    // -------- Ships (send all slots with active flag) --------
    size_t ns = MAX_SHIPS;
    ShipState  *s_msgs = calloc(ns, sizeof(*s_msgs));
    ShipState **s_ptrs = calloc(ns, sizeof(*s_ptrs));
    if (!s_msgs || !s_ptrs) {
        free(p_msgs); free(p_ptrs);
        free(t_msgs); free(t_ptrs);
        free(s_msgs); free(s_ptrs);
        return -1;
    }

    for (size_t i = 0; i < ns; ++i) {
        s_msgs[i] = (ShipState)SHIP_STATE__INIT;
        s_ptrs[i] = &s_msgs[i];

        if (u->ships) {
            const Ship *s = &u->ships[i];
            s_msgs[i].ship_ascii  = (uint32_t)(unsigned char)s->ship_id;
            s_msgs[i].active      = s->active ? 1 : 0;
            s_msgs[i].x           = s->x;
            s_msgs[i].y           = s->y;
            s_msgs[i].v_amplitude = s->velocity.amplitude;
            s_msgs[i].v_angle     = s->velocity.angle;
            s_msgs[i].cargo       = (uint32_t)s->cargo;
            s_msgs[i].capacity    = (uint32_t)u->ship_capacity;
        } else {
            s_msgs[i].ship_ascii  = (uint32_t)('A' + i);
            s_msgs[i].active      = 0;
            s_msgs[i].x           = 0.0f;
            s_msgs[i].y           = 0.0f;
            s_msgs[i].v_amplitude = 0.0f;
            s_msgs[i].v_angle     = 0.0f;
            s_msgs[i].cargo       = 0;
            s_msgs[i].capacity    = (uint32_t)u->ship_capacity;
        }
    }

    st.n_ships = ns;
    st.ships   = s_ptrs;

    // -------- pack & send multipart: "STATE" + bytes --------
    size_t len = universe_state__get_packed_size(&st);
    uint8_t *buf = malloc(len);
    if (!buf) {
        free(p_msgs); free(p_ptrs);
        free(t_msgs); free(t_ptrs);
        free(s_msgs); free(s_ptrs);
        return -1;
    }

    universe_state__pack(&st, buf);

    int rc1 = zmq_send(pub_sock, "STATE", 5, ZMQ_SNDMORE);
    int rc2 = zmq_send(pub_sock, buf, len, 0);

    free(buf);
    free(p_msgs); free(p_ptrs);
    free(t_msgs); free(t_ptrs);
    free(s_msgs); free(s_ptrs);

    return (rc1 == -1 || rc2 == -1) ? -1 : 0; //If any zmq_send fails return -1(error)
}


int comm_pub_send_universe_stats(void *pub_sock, const Universe *u, int server_quit)
{
    if (!pub_sock || !u) return -1;

    UniverseStats st = UNIVERSE_STATS__INIT;

    st.roaming     = (uint32_t)count_active_trash(u);
    st.max_roaming = (uint32_t)u->max_trash;
    st.game_over = server_quit ? 1 : 0;

    // -------- Planets --------
    size_t np = (u->num_planets > 0 && u->planets) ? (size_t)u->num_planets : 0;
    PlanetStats  *p_msgs = NULL;
    PlanetStats **p_ptrs = NULL;

    if (np > 0) {
        p_msgs = calloc(np, sizeof(*p_msgs));
        p_ptrs = calloc(np, sizeof(*p_ptrs));
        if (!p_msgs || !p_ptrs) {
            free(p_msgs); free(p_ptrs);
            return -1;
        }

        for (size_t i = 0; i < np; ++i) {
            p_msgs[i] = (PlanetStats)PLANET_STATS__INIT;
            p_ptrs[i] = &p_msgs[i];

            const Planet *p = &u->planets[i];
            p_msgs[i].name_ascii = (uint32_t)(unsigned char)p->planet_name;
            p_msgs[i].recycled   = (uint32_t)p->trash_count;
        }

        st.n_planets = np;
        st.planets   = p_ptrs;
    }

    // -------- Ships (only active) --------
    size_t ns = count_active_ships(u);
    ShipStats  *s_msgs = NULL;
    ShipStats **s_ptrs = NULL;

    if (ns > 0) {
        s_msgs = calloc(ns, sizeof(*s_msgs));
        s_ptrs = calloc(ns, sizeof(*s_ptrs));
        if (!s_msgs || !s_ptrs) {
            free(p_msgs); free(p_ptrs);
            free(s_msgs); free(s_ptrs);
            return -1;
        }

        size_t k = 0;
        for (int i = 0; i < MAX_SHIPS; ++i) {
            const Ship *s = &u->ships[i];
            if (!s->active) continue;

            s_msgs[k] = (ShipStats)SHIP_STATS__INIT;
            s_ptrs[k] = &s_msgs[k];

            s_msgs[k].ship_ascii = (uint32_t)(unsigned char)s->ship_id;
            s_msgs[k].active     = 1;
            s_msgs[k].cargo      = (uint32_t)s->cargo;

            if (++k == ns) break;
        }

        st.n_ships = ns;
        st.ships   = s_ptrs;
    }

    // -------- pack & send multipart: "STATS" + bytes --------
    size_t len = universe_stats__get_packed_size(&st);
    uint8_t *buf = malloc(len);
    if (!buf) {
        free(p_msgs); free(p_ptrs);
        free(s_msgs); free(s_ptrs);
        return -1;
    }

    universe_stats__pack(&st, buf);

    int rc1 = zmq_send(pub_sock, "STATS", 5, ZMQ_SNDMORE);
    int rc2 = zmq_send(pub_sock, buf, len, 0);

    free(buf);
    free(p_msgs); free(p_ptrs);
    free(s_msgs); free(s_ptrs);

    return (rc1 == -1 || rc2 == -1) ? -1 : 0;
}



struct UniverseState *comm_sub_recv_universe_state(void *sub_sock)
{
    if (!sub_sock) return NULL;


    //Receive one frame(topic)
    zmq_msg_t topic;
    zmq_msg_t payload;
    zmq_msg_init(&topic);

    int rc = zmq_msg_recv(&topic, sub_sock, 0);
    if (rc == -1) {
        zmq_msg_close(&topic);
        return NULL; // timeout or error
    }

    const char *tdata = (const char *)zmq_msg_data(&topic);
    size_t tlen = zmq_msg_size(&topic);  //(5 "STATE")

    
    //Receive second frame (payload)
    zmq_msg_init(&payload);
    rc = zmq_msg_recv(&payload, sub_sock, 0);
    if (rc == -1) {
        zmq_msg_close(&topic);
        zmq_msg_close(&payload);
        return NULL;
    }

    UniverseState *st = NULL;
    if (tlen == 5 && memcmp(tdata, "STATE", 5) == 0) {  //Confirm topic
        size_t len = zmq_msg_size(&payload);
        const uint8_t *data = (const uint8_t *)zmq_msg_data(&payload);
        st = universe_state__unpack(NULL, len, data);
    }

    zmq_msg_close(&topic);
    zmq_msg_close(&payload);
    return st;
}



struct UniverseStats *comm_sub_recv_universe_stats(void *sub_sock)
{
    if (!sub_sock) return NULL;

    zmq_msg_t topic;
    zmq_msg_t payload;
    zmq_msg_init(&topic);

    int rc = zmq_msg_recv(&topic, sub_sock, 0);
    if (rc == -1) {
        zmq_msg_close(&topic);
        return NULL;
    }

    const char *tdata = (const char *)zmq_msg_data(&topic);
    size_t tlen = zmq_msg_size(&topic);

    zmq_msg_init(&payload);
    rc = zmq_msg_recv(&payload, sub_sock, 0);
    if (rc == -1) {
        zmq_msg_close(&topic);
        zmq_msg_close(&payload);
        return NULL;
    }

    UniverseStats *st = NULL;
    if (tlen == 5 && memcmp(tdata, "STATS", 5) == 0) {
        size_t len = zmq_msg_size(&payload);
        const uint8_t *data = (const uint8_t *)zmq_msg_data(&payload);
        st = universe_stats__unpack(NULL, len, data);
    }

    zmq_msg_close(&topic);
    zmq_msg_close(&payload);
    return st;
}


void destroy_channel(zmqChannel *ch)
{
    if (!ch) return;

    if (ch->sock) {
        zmq_close(ch->sock);
        ch->sock = NULL;
    }
    if (ch->ctx) {
        zmq_ctx_shutdown(ch->ctx);
        ch->ctx = NULL;
    }
}
